// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/import/insomniaimporter.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
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
    /*!
     * \internal
     * \brief Indexes \a resources by their parent.
     */
    explicit Builder(const QJsonArray &resources)
    {
        for (const QJsonValue &value : resources) {
            const QJsonObject resource = value.toObject();
            byParent[resource.value(QLatin1StringView("parentId")).toString()].append(resource);
        }
    }

    /*!
     * \internal
     * \brief Reads the Insomnia auth \a value at \a where; empty inherits, as in Insomnia.
     */
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
        } else if (type == QLatin1StringView("oauth2")
                   && (object.value(QLatin1StringView("grantType")).toString() == QLatin1StringView("authorization_code")
                       || object.value(QLatin1StringView("grantType")).toString() == QLatin1StringView("client_credentials"))) {
            const auto text = [&object](const char *key) {
                return references(object.value(QLatin1StringView(key)).toString());
            };
            result.type = AuthType::OAuth2;
            result.oauth2.grant = object.value(QLatin1StringView("grantType")).toString() == QLatin1StringView("client_credentials")
                                      ? OAuthGrant::ClientCredentials
                                      : OAuthGrant::AuthorizationCode;
            result.oauth2.authUrl = text("authorizationUrl");
            result.oauth2.tokenUrl = text("accessTokenUrl");
            result.oauth2.clientId = text("clientId");
            result.oauth2.clientSecret = text("clientSecret");
            result.oauth2.scope = text("scope");
            result.oauth2.audience = text("audience");
            report.append(QStringLiteral("%1: OAuth 2.0 settings came over; get a token with Get Token").arg(where));
        } else {
            result.type = AuthType::None;
            report.append(QStringLiteral("%1: %2 authorization").arg(where, type));
        }
        return result;
    }

    /*!
     * \internal
     * \brief Reports the file \a path of field \a field at \a where: it is a path of the exporting computer.
     *
     * An empty \a field means the file body.
     */
    void reportFile(const QString &path, const QString &field, const QString &where)
    {
        if (path.trimmed().isEmpty()) {
            report.append(field.isEmpty() ? QStringLiteral("%1: no file for the body; choose one").arg(where)
                                          : QStringLiteral("%1: field %2 has no file; choose one").arg(where, field));
        } else {
            report.append(QStringLiteral("%1: file %2 is a path of the computer the file came from; check it").arg(where, path));
        }
    }

    /*!
     * \internal
     * \brief Reports template tags such as {% uuid %} in \a text at \a where, once each.
     */
    void reportTags(const QString &text, const QString &where)
    {
        static const QRegularExpression tag(QStringLiteral(R"(\{%.*?%\})"));
        for (QRegularExpressionMatchIterator it = tag.globalMatch(text); it.hasNext();) {
            const QString line = QStringLiteral("%1: template tag %2").arg(where, it.next().captured(0));
            if (!report.contains(line))
                report.append(line);
        }
    }

    /*!
     * \internal
     * \brief Reads the request \a resource at \a where.
     */
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
        } else if (mime.startsWith(QLatin1StringView("multipart/form-data"))) {
            spec.body.mode = BodyMode::FormData;
            for (const QJsonValue &entry : body.value(QLatin1StringView("params")).toArray()) {
                const QJsonObject param = entry.toObject();
                FormPart part;
                part.key = references(param.value(QLatin1StringView("name")).toString());
                part.enabled = !param.value(QLatin1StringView("disabled")).toBool();
                part.isFile = param.value(QLatin1StringView("type")).toString() == QLatin1StringView("file");
                part.value = part.isFile ? param.value(QLatin1StringView("fileName")).toString()
                                         : references(param.value(QLatin1StringView("value")).toString());
                if (part.isFile)
                    reportFile(part.value, part.key, where);
                spec.body.parts.append(part);
            }
        } else if (body.contains(QLatin1StringView("fileName"))) {
            spec.body.mode = BodyMode::File;
            spec.body.filePath = body.value(QLatin1StringView("fileName")).toString();
            reportFile(spec.body.filePath, QString(), where);
        } else if (mime.startsWith(QLatin1StringView("multipart/"))) {
            report.append(QStringLiteral("%1: %2 body").arg(where, mime));
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
        for (const FormPart &part : std::as_const(spec.body.parts))
            texts << part.key << part.value;
        reportTags(texts.join(u'\n'), where);
        return spec;
    }

    /*!
     * \internal
     * \brief Reads the children of \a parentId at \a where into \a children.
     *
     * The nodes made are at \a depth in the tree. A group met again, as a
     * file whose parentId links loop would make, and anything deeper than
     * kMaxCollectionDepth are left out and reported. \a visited holds the ids
     * of the groups being read.
     */
    void children(const QString &parentId, const QString &where, QList<CollectionNode> &children, int depth,
                  QSet<QString> &visited)
    {
        for (const QJsonObject &resource : byParent.value(parentId)) {
            const QString type = resource.value(QLatin1StringView("_type")).toString();
            const QString name = resource.value(QLatin1StringView("name")).toString();
            const QString path = where + QStringLiteral(" / ") + name;
            if (type == QLatin1StringView("grpc_request") || type == QLatin1StringView("websocket_request")) {
                report.append(QStringLiteral("%1: %2").arg(path, type == QLatin1StringView("grpc_request")
                                                                     ? QStringLiteral("gRPC request")
                                                                     : QStringLiteral("WebSocket request")));
                continue;
            }
            if (type != QLatin1StringView("request_group") && type != QLatin1StringView("request"))
                continue;
            if (depth > kMaxCollectionDepth) {
                report.append(QStringLiteral("%1: nested deeper than %2 levels, left out").arg(path).arg(kMaxCollectionDepth));
                continue;
            }
            const QString id = resource.value(QLatin1StringView("_id")).toString();
            if (type == QLatin1StringView("request_group") && visited.contains(id)) {
                report.append(QStringLiteral("%1: folder inside itself, left out").arg(path));
                continue;
            }
            CollectionNode node;
            node.id = newItemId();
            node.name = name;
            if (type == QLatin1StringView("request_group")) {
                node.kind = CollectionNode::Kind::Folder;
                node.folder.auth = auth(resource.value(QLatin1StringView("authentication")), path);
                node.folder.variables = environmentRows(resource.value(QLatin1StringView("environment")).toObject());
                for (const KeyValue &row : std::as_const(node.folder.variables)) {
                    if (environmentKeys.contains(row.key))
                        report.append(QStringLiteral("%1: variable %2 is also set by an environment, which wins in ReqDeck")
                                          .arg(path, row.key));
                }
                visited.insert(id);
                this->children(id, path, node.children, depth + 1, visited);
                visited.remove(id);
            } else {
                node.kind = CollectionNode::Kind::Request;
                node.request = request(resource, path);
            }
            children.append(std::move(node));
        }
    }

    /*!
     * \internal
     * \brief Resources by the id of their parent, in file order.
     */
    QHash<QString, QList<QJsonObject>> byParent;

    /*!
     * \internal
     * \brief Names of the variables the environments set.
     */
    QSet<QString> environmentKeys;

    /*!
     * \internal
     * \brief What did not carry over.
     */
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
            for (const KeyValue &row : environmentRows(sub.value(QLatin1StringView("data")).toObject())) {
                environment.variables.append({row.key, row.value, row.enabled, false});
                builder.environmentKeys.insert(row.key);
            }
            result.environments.append(environment);
        }
    }

    // Groups and requests sit below the top folder, which is at depth 1.
    QSet<QString> visited {workspaceId};
    builder.children(workspaceId, result.folder.name, result.folder.children, 2, visited);
    for (const QJsonValue &value : resources) {
        const QJsonObject other = value.toObject();
        if (other.value(QLatin1StringView("_type")).toString() == QLatin1StringView("workspace")
            && other.value(QLatin1StringView("_id")).toString() != workspaceId)
            builder.report.append(QStringLiteral("%1: workspace left out; only the first one is imported")
                                      .arg(other.value(QLatin1StringView("name")).toString()));
    }

    result.report = builder.report;
    result.hasCollection = true;
    result.ok = true;
    return result;
}
