// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/import/openapiimporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
/*!
 * \internal
 * \brief Deepest nesting followed when a schema is turned into an example.
 *
 * It also stops schemas that refer to themselves.
 */
constexpr int kMaxSchemaDepth = 6;

/*!
 * \internal
 * \brief Most values one example built from a schema may hold.
 *
 * Depth alone does not bound an example: schemas with many properties that
 * refer to further such schemas grow with the power of the depth.
 */
constexpr int kMaxExampleValues = 2000;

/*!
 * \internal
 * \brief Returns \a value as text: strings as they are, other values as compact JSON.
 */
QString text(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isNull() || value.isUndefined())
        return QString();
    return value.toVariant().toString();
}

/*!
 * \internal
 * \brief Turns OpenAPI path templates /pets/{petId} into /pets/{{petId}}.
 */
QString pathTemplate(QString path)
{
    static const QRegularExpression parameter(QStringLiteral(R"(\{([^{}/]+)\})"));
    return path.replace(parameter, QStringLiteral("{{\\1}}"));
}

/*!
 * \internal
 * \brief Reads one OpenAPI document and collects the report.
 */
class Reader
{
public:
    /*!
     * \internal
     * \brief Prepares reading of the document \a root.
     */
    explicit Reader(const QJsonObject &root)
        : m_root(root)
        , m_schemes(root.value(QLatin1StringView("components")).toObject().value(QLatin1StringView("securitySchemes")).toObject())
    {
    }

    /*!
     * \internal
     * \brief Follows local "#/..." references in \a value at \a where; external or
     * broken references are reported and give an empty object.
     */
    QJsonValue resolve(QJsonValue value, const QString &where)
    {
        for (int hops = 0; hops < 16; ++hops) {
            const QString ref = value.toObject().value(QLatin1StringView("$ref")).toString();
            if (ref.isEmpty())
                return value;
            if (!ref.startsWith(QLatin1StringView("#/"))) {
                note(where, QStringLiteral("external reference %1").arg(ref));
                return QJsonObject();
            }
            QJsonValue target = m_root;
            for (QString part : ref.mid(2).split(u'/')) {
                part.replace(QLatin1StringView("~1"), QLatin1StringView("/")).replace(QLatin1StringView("~0"), QLatin1StringView("~"));
                target = target.toObject().value(part);
            }
            if (target.isUndefined()) {
                note(where, QStringLiteral("unresolved reference %1").arg(ref));
                return QJsonObject();
            }
            value = target;
        }
        return QJsonObject();
    }

    /*!
     * \internal
     * \brief Returns an example value for \a schema, from its example, default, or type.
     */
    QJsonValue example(const QJsonValue &schemaValue, const QString &where, int depth = 0)
    {
        // A schema met again inside itself ends as null instead of nesting on.
        const QString ref = schemaValue.toObject().value(QLatin1StringView("$ref")).toString();
        if (!ref.isEmpty() && m_schemaChain.contains(ref))
            return QJsonValue();
        if (!ref.isEmpty())
            m_schemaChain.append(ref);
        const QJsonValue value = exampleOfSchema(resolve(schemaValue, where).toObject(), where, depth);
        if (!ref.isEmpty())
            m_schemaChain.removeLast();
        return value;
    }

    /*!
     * \internal
     * \brief Returns an example value for the resolved \a schema.
     */
    QJsonValue exampleOfSchema(const QJsonObject &schema, const QString &where, int depth)
    {
        if (--m_exampleBudget < 0)
            return QJsonValue();
        for (const char *key : {"example", "default"}) {
            if (schema.contains(QLatin1StringView(key)))
                return schema.value(QLatin1StringView(key));
        }
        const QJsonArray choices = schema.value(QLatin1StringView("enum")).toArray();
        if (!choices.isEmpty())
            return choices.first();
        if (depth >= kMaxSchemaDepth)
            return QJsonValue();
        for (const char *key : {"oneOf", "anyOf"}) {
            const QJsonArray options = schema.value(QLatin1StringView(key)).toArray();
            if (!options.isEmpty())
                return example(options.first(), where, depth + 1);
        }

        QString type = schema.value(QLatin1StringView("type")).toString();
        if (schema.value(QLatin1StringView("type")).isArray())
            type = schema.value(QLatin1StringView("type")).toArray().at(0).toString();
        const QJsonArray parts = schema.value(QLatin1StringView("allOf")).toArray();
        if (type.isEmpty() && (schema.contains(QLatin1StringView("properties")) || !parts.isEmpty()))
            type = QStringLiteral("object");

        if (type == QLatin1StringView("object")) {
            QJsonObject object;
            for (const QJsonValue &part : parts) {
                const QJsonObject piece = example(part, where, depth + 1).toObject();
                for (auto it = piece.begin(); it != piece.end(); ++it)
                    object.insert(it.key(), it.value());
            }
            const QJsonObject properties = schema.value(QLatin1StringView("properties")).toObject();
            for (auto it = properties.begin(); it != properties.end(); ++it) {
                const QJsonValue value = example(it.value(), where, depth + 1);
                if (!value.isUndefined())
                    object.insert(it.key(), value);
            }
            return object;
        }
        if (type == QLatin1StringView("array")) {
            const QJsonValue item = example(schema.value(QLatin1StringView("items")), where, depth + 1);
            return item.isUndefined() ? QJsonArray() : QJsonArray {item};
        }
        if (type == QLatin1StringView("integer") || type == QLatin1StringView("number"))
            return 0;
        if (type == QLatin1StringView("boolean"))
            return false;
        if (type == QLatin1StringView("string"))
            return QString();
        return QJsonValue();
    }

    /*!
     * \internal
     * \brief Returns the example of the parameter or media object \a object, falling back to its schema.
     */
    QJsonValue exampleOf(const QJsonObject &object, const QString &where)
    {
        if (object.contains(QLatin1StringView("example")))
            return object.value(QLatin1StringView("example"));
        const QJsonObject examples = object.value(QLatin1StringView("examples")).toObject();
        if (!examples.isEmpty())
            return resolve(examples.begin().value(), where).toObject().value(QLatin1StringView("value"));
        m_exampleBudget = kMaxExampleValues;
        const QJsonValue value = example(object.value(QLatin1StringView("schema")), where);
        if (m_exampleBudget < 0)
            note(where, QStringLiteral("example cut short at %1 values").arg(kMaxExampleValues));
        return value;
    }

    /*!
     * \internal
     * \brief Returns the auth of the security requirement list \a requirements at
     * \a where. An empty list means no auth. The auth refers to {{token}},
     * {{username}}, {{password}}, or {{apiKey}}; they are not defined, so Send
     * asks for their values.
     */
    RequestAuth auth(const QJsonArray &requirements, const QString &where)
    {
        RequestAuth result;
        result.type = AuthType::None;
        if (requirements.isEmpty())
            return result;
        const QJsonObject requirement = requirements.first().toObject();
        if (requirement.isEmpty())
            return result;
        const QString name = requirement.begin().key();
        const QJsonObject scheme = resolve(m_schemes.value(name), where).toObject();
        const QString type = scheme.value(QLatin1StringView("type")).toString();
        const QString httpScheme = scheme.value(QLatin1StringView("scheme")).toString().toLower();
        const QString in = scheme.value(QLatin1StringView("in")).toString();
        if (type == QLatin1StringView("http") && httpScheme == QLatin1StringView("bearer")) {
            result.type = AuthType::Bearer;
            result.token = QStringLiteral("{{token}}");
        } else if (type == QLatin1StringView("http") && httpScheme == QLatin1StringView("basic")) {
            result.type = AuthType::Basic;
            result.username = QStringLiteral("{{username}}");
            result.password = QStringLiteral("{{password}}");
        } else if (type == QLatin1StringView("apiKey") && (in == QLatin1StringView("header") || in == QLatin1StringView("query"))) {
            result.type = AuthType::ApiKey;
            result.apiKeyName = scheme.value(QLatin1StringView("name")).toString();
            result.apiKeyValue = QStringLiteral("{{apiKey}}");
            result.apiKeyLocation = in == QLatin1StringView("query") ? ApiKeyLocation::Query : ApiKeyLocation::Header;
        } else if (type == QLatin1StringView("apiKey")) {
            note(where, QStringLiteral("API key in %1").arg(in));
        } else {
            note(where, QStringLiteral("%1 authorization").arg(type.isEmpty() ? name : type));
        }
        return result;
    }

    /*!
     * \internal
     * \brief Reads the operation \a operation of \a path with the path-level \a shared parameters.
     */
    RequestSpec request(const QString &path, HttpMethod method, const QJsonObject &operation,
                        const QJsonArray &shared, const QString &where)
    {
        RequestSpec spec;
        spec.method = method;
        spec.url = QStringLiteral("{{baseUrl}}") + pathTemplate(path);

        // Operation parameters replace path-level ones of the same name and place.
        QList<QJsonObject> parameters;
        auto add = [&](const QJsonArray &list) {
            for (const QJsonValue &value : list) {
                const QJsonObject parameter = resolve(value, where).toObject();
                const auto same = [&](const QJsonObject &other) {
                    return other.value(QLatin1StringView("name")) == parameter.value(QLatin1StringView("name"))
                           && other.value(QLatin1StringView("in")) == parameter.value(QLatin1StringView("in"));
                };
                parameters.removeIf(same);
                parameters.append(parameter);
            }
        };
        add(shared);
        add(operation.value(QLatin1StringView("parameters")).toArray());

        for (const QJsonObject &parameter : std::as_const(parameters)) {
            const QString name = parameter.value(QLatin1StringView("name")).toString();
            const QString in = parameter.value(QLatin1StringView("in")).toString();
            const QString value = text(exampleOf(parameter, where));
            const bool required = parameter.value(QLatin1StringView("required")).toBool();
            if (in == QLatin1StringView("query"))
                spec.queryParams.append({name, value, required});
            else if (in == QLatin1StringView("header"))
                spec.headers.append({name, value, required});
            else if (in == QLatin1StringView("path") && !value.isEmpty())
                addVariable(name, value); // Without an example, Send asks for the value.
            else if (in != QLatin1StringView("path"))
                note(where, QStringLiteral("%1 parameter %2").arg(in, name));
        }

        const QJsonObject content = resolve(operation.value(QLatin1StringView("requestBody")), where)
                                        .toObject().value(QLatin1StringView("content")).toObject();
        if (!content.isEmpty()) {
            // Prefer JSON, then form fields, then any text.
            QString mime;
            for (auto it = content.begin(); it != content.end() && mime.isEmpty(); ++it) {
                if (it.key().contains(QLatin1StringView("json")))
                    mime = it.key();
            }
            if (mime.isEmpty() && content.contains(QLatin1StringView("application/x-www-form-urlencoded")))
                mime = QStringLiteral("application/x-www-form-urlencoded");
            if (mime.isEmpty())
                mime = content.begin().key();
            const QJsonObject media = content.value(mime).toObject();
            if (mime == QLatin1StringView("application/x-www-form-urlencoded")) {
                spec.body.mode = BodyMode::FormUrlEncoded;
                const QJsonObject fields = exampleOf(media, where).toObject();
                for (auto it = fields.begin(); it != fields.end(); ++it)
                    spec.body.form.append({it.key(), text(it.value()), true});
            } else if (mime.contains(QLatin1StringView("json")) || mime.contains(QLatin1StringView("xml"))
                       || mime.startsWith(QLatin1StringView("text/"))) {
                spec.body.mode = BodyMode::Raw;
                const QJsonValue value = exampleOf(media, where);
                if (mime.contains(QLatin1StringView("json"))) {
                    spec.body.rawType = RawBodyType::Json;
                    spec.body.raw = value.isObject() ? QString::fromUtf8(QJsonDocument(value.toObject()).toJson())
                                    : value.isArray() ? QString::fromUtf8(QJsonDocument(value.toArray()).toJson())
                                                      : text(value);
                } else {
                    spec.body.rawType = mime.contains(QLatin1StringView("xml")) ? RawBodyType::Xml : RawBodyType::Text;
                    spec.body.raw = value.isString() ? value.toString() : QString();
                }
            } else {
                note(where, QStringLiteral("%1 body").arg(mime));
            }
        }

        spec.auth.type = AuthType::Inherit;
        if (operation.contains(QLatin1StringView("security")))
            spec.auth = auth(operation.value(QLatin1StringView("security")).toArray(), where);
        return spec;
    }

    /*!
     * \internal
     * \brief Adds the top folder variable \a key with \a value unless it is there already.
     */
    void addVariable(const QString &key, const QString &value)
    {
        for (const KeyValue &row : std::as_const(variables)) {
            if (row.key == key)
                return;
        }
        variables.append({key, value, true});
    }

    /*!
     * \internal
     * \brief Adds "\a where: \a what" to the report once.
     */
    void note(const QString &where, const QString &what)
    {
        const QString line = where + QStringLiteral(": ") + what;
        if (!report.contains(line))
            report.append(line);
    }

    /*!
     * \internal
     * \brief Variables of the top folder, in the order they were met.
     */
    QList<KeyValue> variables;

    /*!
     * \internal
     * \brief What did not carry over.
     */
    QStringList report;

private:
    /*!
     * \internal
     * \brief The whole document, for resolving references.
     */
    QJsonObject m_root;

    /*!
     * \internal
     * \brief components.securitySchemes of the document.
     */
    QJsonObject m_schemes;

    /*!
     * \internal
     * \brief References of the schemas being turned into an example, outermost first.
     */
    QStringList m_schemaChain;

    /*!
     * \internal
     * \brief Values the example being built may still hold.
     */
    int m_exampleBudget {kMaxExampleValues};
};

/*!
 * \internal
 * \brief Returns the URL of \a server with its variables set to their defaults.
 */
QString serverUrl(const QJsonObject &server)
{
    QString url = server.value(QLatin1StringView("url")).toString();
    const QJsonObject variables = server.value(QLatin1StringView("variables")).toObject();
    for (auto it = variables.begin(); it != variables.end(); ++it)
        url.replace(u'{' + it.key() + u'}', text(it.value().toObject().value(QLatin1StringView("default"))));
    while (url.endsWith(u'/'))
        url.chop(1);
    return url;
}
} // namespace

/*!
 * \brief Returns whether \a root looks like an OpenAPI 3 or Swagger 2.0 document.
 *
 * Swagger 2.0 is recognized so that importDocument() can explain that it is
 * not supported, rather than the file being unknown.
 * \param root The JSON object of a file.
 */
bool OpenApiImporter::isDocument(const QJsonObject &root)
{
    const bool openApi = root.value(QLatin1StringView("openapi")).toString().startsWith(QLatin1StringView("3."));
    const bool swagger = root.value(QLatin1StringView("swagger")).toString() == QLatin1StringView("2.0");
    return (openApi || swagger) && root.value(QLatin1StringView("paths")).isObject();
}

/*!
 * \brief Reads an OpenAPI 3 document.
 *
 * Each operation becomes a request named by its summary, else its
 * operationId, else "METHOD path". Requests sit in a folder named by their
 * first tag (tags in the order the document lists them), untagged ones at the
 * top. URLs start with {{baseUrl}}; path parameters become top folder
 * variables with their example values. Query and header parameters carry
 * their examples and are enabled when required. A JSON body comes from the
 * example or is built from the schema. The document's security becomes the
 * top folder's auth; an operation with its own security gets its own auth.
 * \param json The file content.
 * \return The folder, one environment per server when there are several, and
 *         the report, or a failure.
 */
ImportResult OpenApiImporter::importDocument(const QByteArray &json)
{
    ImportResult result;
    result.format = QStringLiteral("OpenAPI document");
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (!isDocument(root)) {
        result.errorString = QStringLiteral("The file is not an OpenAPI 3 document.");
        return result;
    }
    if (root.contains(QLatin1StringView("swagger"))) {
        result.errorString = QStringLiteral("Swagger 2.0 is not supported. Convert the document to OpenAPI 3 first.");
        return result;
    }

    Reader reader(root);
    const QString title = root.value(QLatin1StringView("info")).toObject().value(QLatin1StringView("title")).toString(QStringLiteral("OpenAPI"));
    result.folder.id = newItemId();
    result.folder.name = title;

    const QJsonArray servers = root.value(QLatin1StringView("servers")).toArray();
    if (servers.isEmpty())
        reader.note(title, QStringLiteral("no servers; Send asks for {{baseUrl}}"));
    else
        reader.addVariable(QStringLiteral("baseUrl"), serverUrl(servers.first().toObject()));
    if (servers.size() > 1) {
        for (const QJsonValue &value : servers) {
            const QJsonObject server = value.toObject();
            Environment environment;
            environment.id = newItemId();
            environment.name = server.value(QLatin1StringView("description")).toString(serverUrl(server));
            environment.variables.append({QStringLiteral("baseUrl"), serverUrl(server), true, false});
            result.environments.append(environment);
        }
    }
    result.folder.folder.auth = reader.auth(root.value(QLatin1StringView("security")).toArray(), title);

    QList<CollectionNode> tagFolders;
    for (const QJsonValue &tag : root.value(QLatin1StringView("tags")).toArray()) {
        CollectionNode folder;
        folder.kind = CollectionNode::Kind::Folder;
        folder.name = tag.toObject().value(QLatin1StringView("name")).toString();
        tagFolders.append(folder);
    }
    QList<CollectionNode> untagged;

    static const QList<std::pair<const char *, HttpMethod>> methods {
        {"get", HttpMethod::Get},       {"put", HttpMethod::Put},         {"post", HttpMethod::Post},
        {"delete", HttpMethod::Delete}, {"options", HttpMethod::Options}, {"head", HttpMethod::Head},
        {"patch", HttpMethod::Patch}};
    const QJsonObject paths = root.value(QLatin1StringView("paths")).toObject();
    for (auto it = paths.begin(); it != paths.end(); ++it) {
        const QString path = it.key();
        const QJsonObject item = reader.resolve(it.value(), title + QStringLiteral(" / ") + path).toObject();
        const QJsonArray shared = item.value(QLatin1StringView("parameters")).toArray();
        if (item.contains(QLatin1StringView("trace")))
            reader.note(title + QStringLiteral(" / ") + path, QStringLiteral("TRACE operation"));
        for (const auto &[key, method] : methods) {
            if (!item.contains(QLatin1StringView(key)))
                continue;
            const QJsonObject operation = item.value(QLatin1StringView(key)).toObject();
            CollectionNode node;
            node.id = newItemId();
            node.kind = CollectionNode::Kind::Request;
            node.name = operation.value(QLatin1StringView("summary")).toString().trimmed();
            if (node.name.isEmpty())
                node.name = operation.value(QLatin1StringView("operationId")).toString();
            if (node.name.isEmpty())
                node.name = QString::fromLatin1(httpMethodVerb(method)) + u' ' + path;

            const QString tag = operation.value(QLatin1StringView("tags")).toArray().at(0).toString();
            CollectionNode *folder = nullptr;
            if (!tag.isEmpty()) {
                for (CollectionNode &candidate : tagFolders) {
                    if (candidate.name == tag)
                        folder = &candidate;
                }
                if (!folder) {
                    CollectionNode added;
                    added.kind = CollectionNode::Kind::Folder;
                    added.name = tag;
                    tagFolders.append(added);
                    folder = &tagFolders.last();
                }
            }
            const QString where = title + QStringLiteral(" / ") + (folder ? tag + QStringLiteral(" / ") : QString()) + node.name;
            node.request = reader.request(path, method, operation, shared, where);
            (folder ? folder->children : untagged).append(std::move(node));
        }
    }

    for (CollectionNode &folder : tagFolders) {
        if (folder.children.isEmpty())
            continue;
        folder.id = newItemId();
        result.folder.children.append(std::move(folder));
    }
    result.folder.children += untagged;
    result.folder.folder.variables = reader.variables;
    result.report = reader.report;
    result.hasCollection = true;
    result.ok = true;
    return result;
}
