// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/import/insomniaimporter.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
/*!
 * \internal
 * \brief Turns Insomnia references {{ _.name }} into {{name}}.
 */
QString references(QString text)
{
    static const QRegularExpression reference(QStringLiteral(R"(\{\{\s*_\.([A-Za-z_][A-Za-z0-9_.\-]*)\s*\}\})"));
    return text.replace(reference, QStringLiteral("{{\\1}}"));
}

/*!
 * \internal
 * \brief Returns Insomnia rows {name, value, disabled} as key/value rows, references converted.
 */
QList<KeyValue> rows(const QJsonValue &list)
{
    QList<KeyValue> result;
    for (const QJsonValue &entry : list.toArray()) {
        const QJsonObject object = entry.toObject();
        result.append({references(object.value(QLatin1StringView("name")).toString()),
                       references(object.value(QLatin1StringView("value")).toString()),
                       !object.value(QLatin1StringView("disabled")).toBool()});
    }
    return result;
}

/*!
 * \internal
 * \brief Returns the values of environment \a data as rows; nested values become compact JSON.
 */
QList<KeyValue> environmentRows(const QJsonObject &data)
{
    QList<KeyValue> result;
    for (auto it = data.begin(); it != data.end(); ++it) {
        const QJsonValue value = it.value();
        QString text;
        if (value.isObject())
            text = QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        else if (value.isArray())
            text = QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        else
            text = value.toVariant().toString();
        result.append({it.key(), references(text), true});
    }
    return result;
}

/*!
 * \internal
 * \brief Builds the tree of an Insomnia workspace and collects the report.
 */
class Builder
{
public:
    /*! Indexes \a resources by their parent. */
    explicit Builder(const QJsonArray &resources)
    {
        for (const QJsonValue &value : resources) {
            const QJsonObject resource = value.toObject();
            byParent[resource.value(QLatin1StringView("parentId")).toString()].append(resource);
        }
    }

    /*! Reads the Insomnia auth \a value at \a where; empty inherits, as in Insomnia. */
    RequestAuth auth(const QJsonValue &value, const QString &where)
    {
        RequestAuth result;
        result.type = AuthType::Inherit;
        const QJsonObject object = value.toObject();
        const QString type = object.value(QLatin1StringView("type")).toString();
        if (type.isEmpty() || object.value(QLatin1StringView("disabled")).toBool())
            return result;
        if (type == QLatin1StringView("none")) {
            result.type = AuthType::None;
        } else if (type == QLatin1StringView("bearer")) {
            result.type = AuthType::Bearer;
            result.token = references(object.value(QLatin1StringView("token")).toString());
        } else if (type == QLatin1StringView("basic")) {
            result.type = AuthType::Basic;
            result.username = references(object.value(QLatin1StringView("username")).toString());
            result.password = references(object.value(QLatin1StringView("password")).toString());
        } else if (type == QLatin1StringView("apikey")) {
            result.type = AuthType::ApiKey;
            result.apiKeyName = references(object.value(QLatin1StringView("key")).toString());
            result.apiKeyValue = references(object.value(QLatin1StringView("value")).toString());
            if (object.value(QLatin1StringView("addTo")).toString() == QLatin1StringView("queryParams"))
                result.apiKeyLocation = ApiKeyLocation::Query;
        } else {
            result.type = AuthType::None;
            report.append(QStringLiteral("%1: %2 authorization").arg(where, type));
        }
        return result;
    }

    /*! Reports template tags such as {% uuid %} in \a text at \a where, once each. */
    void reportTags(const QString &text, const QString &where)
    {
        static const QRegularExpression tag(QStringLiteral(R"(\{%.*?%\})"));
        for (QRegularExpressionMatchIterator it = tag.globalMatch(text); it.hasNext();) {
            const QString line = QStringLiteral("%1: template tag %2").arg(where, it.next().captured(0));
            if (!report.contains(line))
                report.append(line);
        }
    }

    /*! Reads the request \a resource at \a where. */
    RequestSpec request(const QJsonObject &resource, const QString &where)
    {
        RequestSpec spec;
        const QString method = resource.value(QLatin1StringView("method")).toString(QStringLiteral("GET")).toUpper();
        for (const HttpMethod candidate : {HttpMethod::Get, HttpMethod::Post, HttpMethod::Put, HttpMethod::Patch,
                                           HttpMethod::Delete, HttpMethod::Head, HttpMethod::Options}) {
            if (QString::fromLatin1(httpMethodVerb(candidate)) == method)
                spec.method = candidate;
        }
        if (QString::fromLatin1(httpMethodVerb(spec.method)) != method)
            report.append(QStringLiteral("%1: method %2, sent as GET").arg(where, method));

        spec.url = references(resource.value(QLatin1StringView("url")).toString());
        spec.queryParams = rows(resource.value(QLatin1StringView("parameters")));
        spec.headers = rows(resource.value(QLatin1StringView("headers")));
        spec.auth = auth(resource.value(QLatin1StringView("authentication")), where);

        const QJsonObject body = resource.value(QLatin1StringView("body")).toObject();
        const QString mime = body.value(QLatin1StringView("mimeType")).toString();
        if (mime == QLatin1StringView("application/x-www-form-urlencoded")) {
            spec.body.mode = BodyMode::FormUrlEncoded;
            spec.body.form = rows(body.value(QLatin1StringView("params")));
        } else if (mime.startsWith(QLatin1StringView("multipart/")) || body.contains(QLatin1StringView("fileName"))) {
            report.append(QStringLiteral("%1: %2 body").arg(where, mime.isEmpty() ? QStringLiteral("file") : mime));
        } else if (body.contains(QLatin1StringView("text"))) {
            spec.body.mode = BodyMode::Raw;
            spec.body.raw = references(body.value(QLatin1StringView("text")).toString());
            spec.body.rawType = mime.contains(QLatin1StringView("json")) ? RawBodyType::Json
                                : mime.contains(QLatin1StringView("xml")) ? RawBodyType::Xml
                                                                          : RawBodyType::Text;
        }

        for (const char *script : {"preRequestScript", "afterResponseScript"}) {
            if (!resource.value(QLatin1StringView(script)).toString().trimmed().isEmpty()) {
                report.append(QStringLiteral("%1: scripts").arg(where));
                break;
            }
        }
        QStringList texts {spec.url, spec.body.raw, spec.auth.token, spec.auth.password, spec.auth.apiKeyValue};
        for (const KeyValue &row : spec.queryParams + spec.headers + spec.body.form)
            texts << row.key << row.value;
        reportTags(texts.join(u'\n'), where);
        return spec;
    }

    /*! Reads the children of \a parentId at \a where into \a children. */
    void children(const QString &parentId, const QString &where, QList<CollectionNode> &children)
    {
        for (const QJsonObject &resource : byParent.value(parentId)) {
            const QString type = resource.value(QLatin1StringView("_type")).toString();
            if (type != QLatin1StringView("request_group") && type != QLatin1StringView("request"))
                continue;
            CollectionNode node;
            node.id = newItemId();
            node.name = resource.value(QLatin1StringView("name")).toString();
            const QString path = where + QStringLiteral(" / ") + node.name;
            if (type == QLatin1StringView("request_group")) {
                node.kind = CollectionNode::Kind::Folder;
                node.folder.auth = auth(resource.value(QLatin1StringView("authentication")), path);
                node.folder.variables = environmentRows(resource.value(QLatin1StringView("environment")).toObject());
                this->children(resource.value(QLatin1StringView("_id")).toString(), path, node.children);
            } else {
                node.kind = CollectionNode::Kind::Request;
                node.request = request(resource, path);
            }
            children.append(std::move(node));
        }
    }

    /*! Resources by the id of their parent, in file order. */
    QHash<QString, QList<QJsonObject>> byParent;

    /*! What did not carry over. */
    QStringList report;
};
} // namespace

/*!
 * \brief Returns whether \a root is an Insomnia export of format 4.
 * \param root The JSON object of a file.
 */
bool InsomniaImporter::isExport(const QJsonObject &root)
{
    return root.value(QLatin1StringView("_type")).toString() == QLatin1StringView("export")
           && root.value(QLatin1StringView("__export_format")).toInt() == 4
           && root.value(QLatin1StringView("resources")).isArray();
}

/*!
 * \brief Reads an Insomnia export.
 *
 * Only the first workspace is read; it becomes one folder with its request
 * groups and requests in file order. The base environment of the workspace
 * becomes the folder's variables, its sub-environments ReqDeck environments.
 * Scripts, template tags, multipart and file bodies, and auth ReqDeck does not
 * have are listed in ImportResult::report.
 * \param json The file content.
 * \return The folder, the environments, and the report, or a failure.
 */
ImportResult InsomniaImporter::importExport(const QByteArray &json)
{
    ImportResult result;
    result.format = QStringLiteral("Insomnia export");
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (!isExport(root)) {
        result.errorString = QStringLiteral("The file is not an Insomnia export of format 4.");
        return result;
    }

    const QJsonArray resources = root.value(QLatin1StringView("resources")).toArray();
    Builder builder(resources);
    QJsonObject workspace;
    for (const QJsonValue &value : resources) {
        if (value.toObject().value(QLatin1StringView("_type")).toString() == QLatin1StringView("workspace")) {
            workspace = value.toObject();
            break;
        }
    }
    if (workspace.isEmpty()) {
        result.errorString = QStringLiteral("The export holds no workspace.");
        return result;
    }

    const QString workspaceId = workspace.value(QLatin1StringView("_id")).toString();
    result.folder.id = newItemId();
    result.folder.name = workspace.value(QLatin1StringView("name")).toString(QStringLiteral("Insomnia workspace"));
    builder.children(workspaceId, result.folder.name, result.folder.children);

    for (const QJsonObject &base : builder.byParent.value(workspaceId)) {
        if (base.value(QLatin1StringView("_type")).toString() != QLatin1StringView("environment"))
            continue;
        result.folder.folder.variables += environmentRows(base.value(QLatin1StringView("data")).toObject());
        for (const QJsonObject &sub : builder.byParent.value(base.value(QLatin1StringView("_id")).toString())) {
            if (sub.value(QLatin1StringView("_type")).toString() != QLatin1StringView("environment"))
                continue;
            Environment environment;
            environment.id = newItemId();
            environment.name = sub.value(QLatin1StringView("name")).toString();
            for (const KeyValue &row : environmentRows(sub.value(QLatin1StringView("data")).toObject()))
                environment.variables.append({row.key, row.value, row.enabled, false});
            result.environments.append(environment);
        }
    }

    result.report = builder.report;
    result.hasCollection = true;
    result.ok = true;
    return result;
}
