// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/import/postmanimporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <optional>

namespace {
/*!
 * \internal
 * \brief Names of Postman auth types that ReqDeck does not have, for the report.
 */
QString authName(const QString &type)
{
    if (type == QLatin1StringView("oauth2"))
        return QStringLiteral("OAuth 2.0");
    if (type == QLatin1StringView("oauth1"))
        return QStringLiteral("OAuth 1.0");
    if (type == QLatin1StringView("awsv4"))
        return QStringLiteral("AWS Signature");
    return type;
}

/*!
 * \internal
 * \brief Collects what a Postman file holds and what of it was dropped.
 */
class Reader
{
public:
    /*!
     * \internal
     * \brief Returns the value of the entry \a key of Postman auth attributes \a list.
     *
     * Collection v2.1 lists {key, value} objects; v2.0 has one object with the
     * keys as members.
     */
    static QString valueOf(const QJsonValue &list, QLatin1StringView key)
    {
        if (list.isObject())
            return list.toObject().value(key).toVariant().toString();
        for (const QJsonValue &entry : list.toArray()) {
            const QJsonObject object = entry.toObject();
            if (object.value(QLatin1StringView("key")).toString() == key)
                return object.value(QLatin1StringView("value")).toVariant().toString();
        }
        return {};
    }

    /*!
     * \internal
     * \brief Returns Postman rows {key, value, disabled} as key/value rows.
     */
    static QList<KeyValue> rows(const QJsonValue &list)
    {
        QList<KeyValue> result;
        for (const QJsonValue &entry : list.toArray()) {
            const QJsonObject object = entry.toObject();
            result.append({object.value(QLatin1StringView("key")).toString(),
                           object.value(QLatin1StringView("value")).toVariant().toString(),
                           !object.value(QLatin1StringView("disabled")).toBool()});
        }
        return result;
    }

    /*!
     * \internal
     * \brief Reads the Postman auth \a value at \a where; absent or "inherit" inherits.
     */
    RequestAuth auth(const QJsonValue &value, const QString &where)
    {
        RequestAuth result;
        result.type = AuthType::Inherit;
        if (!value.isObject())
            return result;
        const QJsonObject object = value.toObject();
        const QString type = object.value(QLatin1StringView("type")).toString();
        if (type == QLatin1StringView("noauth")) {
            result.type = AuthType::None;
        } else if (type == QLatin1StringView("bearer")) {
            result.type = AuthType::Bearer;
            result.token = valueOf(object.value(QLatin1StringView("bearer")), QLatin1StringView("token"));
        } else if (type == QLatin1StringView("basic")) {
            result.type = AuthType::Basic;
            result.username = valueOf(object.value(QLatin1StringView("basic")), QLatin1StringView("username"));
            result.password = valueOf(object.value(QLatin1StringView("basic")), QLatin1StringView("password"));
        } else if (type == QLatin1StringView("apikey")) {
            const QJsonValue list = object.value(QLatin1StringView("apikey"));
            result.type = AuthType::ApiKey;
            result.apiKeyName = valueOf(list, QLatin1StringView("key"));
            result.apiKeyValue = valueOf(list, QLatin1StringView("value"));
            if (valueOf(list, QLatin1StringView("in")) == QLatin1StringView("query"))
                result.apiKeyLocation = ApiKeyLocation::Query;
        } else if (!type.isEmpty() && type != QLatin1StringView("inherit")) {
            // An auth ReqDeck cannot send must not fall back to the folder's.
            result.type = AuthType::None;
            report.append(QStringLiteral("%1: %2 authorization").arg(where, authName(type)));
        }
        return result;
    }

    /*!
     * \internal
     * \brief Reports the scripts and examples of \a item at \a where.
     */
    void reportExtras(const QJsonObject &item, const QString &where)
    {
        for (const QJsonValue &event : item.value(QLatin1StringView("event")).toArray()) {
            const QJsonValue exec = event.toObject().value(QLatin1StringView("script")).toObject().value(QLatin1StringView("exec"));
            const bool empty = exec.isArray() ? exec.toArray().isEmpty() : exec.toString().trimmed().isEmpty();
            if (!empty) {
                report.append(QStringLiteral("%1: scripts").arg(where));
                break;
            }
        }
        if (!item.value(QLatin1StringView("response")).toArray().isEmpty())
            report.append(QStringLiteral("%1: saved example responses").arg(where));
    }

    /*!
     * \internal
     * \brief Reports dynamic variables such as {{$guid}} in \a text at \a where, once each.
     */
    void reportDynamic(const QString &text, const QString &where)
    {
        static const QRegularExpression dynamic(QStringLiteral(R"(\{\{\s*(\$[A-Za-z0-9_.\-]+)\s*\}\})"));
        for (QRegularExpressionMatchIterator it = dynamic.globalMatch(text); it.hasNext();) {
            const QString line = QStringLiteral("%1: dynamic variable {{%2}}").arg(where, it.next().captured(1));
            if (!report.contains(line))
                report.append(line);
        }
    }

    /*!
     * \internal
     * \brief Reads the request of \a item at \a where.
     */
    RequestSpec request(const QJsonObject &item, const QString &where)
    {
        RequestSpec spec;
        const QJsonValue requestValue = item.value(QLatin1StringView("request"));
        // Postman allows a request that is only a URL.
        if (requestValue.isString()) {
            spec.url = requestValue.toString();
            spec.auth.type = AuthType::Inherit;
            return spec;
        }
        const QJsonObject request = requestValue.toObject();

        const QString method = request.value(QLatin1StringView("method")).toString(QStringLiteral("GET")).toUpper();
        for (const HttpMethod candidate : {HttpMethod::Get, HttpMethod::Post, HttpMethod::Put, HttpMethod::Patch,
                                           HttpMethod::Delete, HttpMethod::Head, HttpMethod::Options}) {
            if (QString::fromLatin1(httpMethodVerb(candidate)) == method)
                spec.method = candidate;
        }
        if (QString::fromLatin1(httpMethodVerb(spec.method)) != method)
            report.append(QStringLiteral("%1: method %2, sent as GET").arg(where, method));

        const QJsonValue url = request.value(QLatin1StringView("url"));
        QString raw = url.isObject() ? url.toObject().value(QLatin1StringView("raw")).toString() : url.toString();
        const qsizetype question = raw.indexOf(u'?');
        if (url.isObject() && url.toObject().contains(QLatin1StringView("query"))) {
            spec.queryParams = rows(url.toObject().value(QLatin1StringView("query")));
            if (question >= 0)
                raw.truncate(question);
        }
        spec.url = raw;
        spec.headers = rows(request.value(QLatin1StringView("header")));
        spec.auth = auth(request.value(QLatin1StringView("auth")), where);

        const QJsonObject body = request.value(QLatin1StringView("body")).toObject();
        const QString mode = body.value(QLatin1StringView("mode")).toString();
        if (mode == QLatin1StringView("raw")) {
            spec.body.mode = BodyMode::Raw;
            spec.body.raw = body.value(QLatin1StringView("raw")).toString();
            const QString language = body.value(QLatin1StringView("options")).toObject().value(QLatin1StringView("raw"))
                                         .toObject().value(QLatin1StringView("language")).toString();
            spec.body.rawType = language == QLatin1StringView("json") ? RawBodyType::Json
                                : language == QLatin1StringView("xml") ? RawBodyType::Xml
                                                                       : RawBodyType::Text;
        } else if (mode == QLatin1StringView("urlencoded")) {
            spec.body.mode = BodyMode::FormUrlEncoded;
            spec.body.form = rows(body.value(QLatin1StringView("urlencoded")));
        } else if (!mode.isEmpty() && mode != QLatin1StringView("none")) {
            report.append(QStringLiteral("%1: %2 body").arg(where, mode == QLatin1StringView("formdata") ? QStringLiteral("form-data")
                                                                                                         : mode));
        }

        QStringList texts {spec.url, spec.body.raw, spec.auth.token, spec.auth.password, spec.auth.apiKeyValue};
        for (const KeyValue &row : spec.queryParams + spec.headers + spec.body.form)
            texts << row.key << row.value;
        reportDynamic(texts.join(u'\n'), where);
        return spec;
    }

    /*!
     * \internal
     * \brief Reads the items of a folder at \a where into \a children.
     *
     * The nodes made are at \a depth in the tree; anything deeper than
     * kMaxCollectionDepth is left out and reported.
     */
    void items(const QJsonArray &list, const QString &where, QList<CollectionNode> &children, int depth)
    {
        for (const QJsonValue &value : list) {
            const QJsonObject item = value.toObject();
            CollectionNode node;
            node.id = newItemId();
            node.name = item.value(QLatin1StringView("name")).toString();
            const QString path = where + QStringLiteral(" / ") + node.name;
            if (depth > kMaxCollectionDepth) {
                report.append(QStringLiteral("%1: nested deeper than %2 levels, left out").arg(path).arg(kMaxCollectionDepth));
                continue;
            }
            reportExtras(item, path);
            if (item.contains(QLatin1StringView("item"))) {
                node.kind = CollectionNode::Kind::Folder;
                node.folder.auth = auth(item.value(QLatin1StringView("auth")), path);
                items(item.value(QLatin1StringView("item")).toArray(), path, node.children, depth + 1);
            } else {
                node.kind = CollectionNode::Kind::Request;
                node.request = request(item, path);
            }
            children.append(std::move(node));
        }
    }

    /*!
     * \internal
     * \brief What did not carry over.
     */
    QStringList report;
};

/*!
 * \internal
 * \brief Parses \a json into an object, or fails \a result with a message.
 */
std::optional<QJsonObject> parseObject(const QByteArray &json, ImportResult &result)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        result.errorString = QStringLiteral("The file is not valid JSON.");
        return std::nullopt;
    }
    return document.object();
}
} // namespace

/*!
 * \brief Returns whether \a root is a Postman collection, by its "info.schema".
 * \param root The JSON object of a file.
 */
bool PostmanImporter::isCollection(const QJsonObject &root)
{
    const QString schema = root.value(QLatin1StringView("info")).toObject().value(QLatin1StringView("schema")).toString();
    return schema.contains(QLatin1StringView("schema.getpostman.com")) && schema.contains(QLatin1StringView("collection"))
           && root.value(QLatin1StringView("item")).isArray();
}

/*!
 * \brief Returns whether \a root is a Postman environment.
 * \param root The JSON object of a file.
 */
bool PostmanImporter::isEnvironment(const QJsonObject &root)
{
    return root.value(QLatin1StringView("values")).isArray()
           && (root.value(QLatin1StringView("_postman_variable_scope")).toString() == QLatin1StringView("environment")
               || root.contains(QLatin1StringView("_postman_exported_using")));
}

/*!
 * \brief Reads a Postman collection.
 *
 * The collection becomes one folder named after it, with its auth and its
 * variables as folder settings; folders and requests keep their order. A
 * request or folder without auth inherits, as in Postman. Scripts, example
 * responses, auth and bodies that ReqDeck does not have, and Postman's dynamic
 * variables are listed in ImportResult::report.
 * \param json The file content.
 * \return The folder and the report, or a failure.
 */
ImportResult PostmanImporter::importCollection(const QByteArray &json)
{
    ImportResult result;
    result.format = QStringLiteral("Postman collection");
    const std::optional<QJsonObject> root = parseObject(json, result);
    if (!root)
        return result;
    if (!isCollection(*root)) {
        result.errorString = QStringLiteral("The file is not a Postman collection.");
        return result;
    }

    Reader reader;
    const QString name = root->value(QLatin1StringView("info")).toObject().value(QLatin1StringView("name")).toString();
    result.folder.id = newItemId();
    result.folder.name = name.isEmpty() ? QStringLiteral("Postman collection") : name;
    result.folder.folder.auth = reader.auth(root->value(QLatin1StringView("auth")), result.folder.name);
    result.folder.folder.variables = Reader::rows(root->value(QLatin1StringView("variable")));
    reader.reportExtras(*root, result.folder.name);
    // Items sit below the top folder, which is at depth 1.
    reader.items(root->value(QLatin1StringView("item")).toArray(), result.folder.name, result.folder.children, 2);

    result.report = reader.report;
    result.hasCollection = true;
    result.ok = true;
    return result;
}

/*!
 * \brief Reads a Postman environment.
 *
 * Variables of type "secret" stay secret, so their values go to the encrypted
 * secrets file, not into the workspace file.
 * \param json The file content.
 * \return One environment, or a failure.
 */
ImportResult PostmanImporter::importEnvironment(const QByteArray &json)
{
    ImportResult result;
    result.format = QStringLiteral("Postman environment");
    const std::optional<QJsonObject> root = parseObject(json, result);
    if (!root)
        return result;
    if (!isEnvironment(*root)) {
        result.errorString = QStringLiteral("The file is not a Postman environment.");
        return result;
    }

    Environment environment;
    environment.id = newItemId();
    environment.name = root->value(QLatin1StringView("name")).toString(QStringLiteral("Postman environment"));
    for (const QJsonValue &value : root->value(QLatin1StringView("values")).toArray()) {
        const QJsonObject object = value.toObject();
        environment.variables.append({object.value(QLatin1StringView("key")).toString(),
                                      object.value(QLatin1StringView("value")).toVariant().toString(),
                                      object.value(QLatin1StringView("enabled")).toBool(true),
                                      object.value(QLatin1StringView("type")).toString() == QLatin1StringView("secret")});
    }
    result.environments = {environment};
    result.ok = true;
    return result;
}
