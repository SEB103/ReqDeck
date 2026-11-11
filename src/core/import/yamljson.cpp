// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/import/yamljson.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>

#include <sstream>
#include <string>

#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/parser.h>

namespace {
/*!
 * \internal
 * \brief Raised when a document breaks a limit or cannot be represented as JSON.
 */
struct Refused
{
    /*! Why, for the error message. */
    QString what;
};

/*!
 * \internal
 * \brief Returns the JSON value of the plain scalar \a text by the YAML 1.2 core schema.
 *
 * null, ~, and an empty scalar are null; true and false in their three
 * spellings are booleans; decimal, 0o octal, and 0x hexadecimal integers and
 * decimal floats are numbers. Anything else, .inf and .nan included since JSON
 * has no such numbers, is a string.
 */
QJsonValue plainScalar(const QString &text)
{
    static const QRegularExpression decimal(QStringLiteral(R"(^[-+]?[0-9]+$)"));
    static const QRegularExpression octal(QStringLiteral(R"(^0o[0-7]+$)"));
    static const QRegularExpression hexadecimal(QStringLiteral(R"(^0x[0-9a-fA-F]+$)"));
    static const QRegularExpression floating(
        QStringLiteral(R"(^[-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?$)"));

    if (text.isEmpty() || text == u'~' || text == QLatin1StringView("null") || text == QLatin1StringView("Null")
        || text == QLatin1StringView("NULL"))
        return QJsonValue::Null;
    if (text == QLatin1StringView("true") || text == QLatin1StringView("True") || text == QLatin1StringView("TRUE"))
        return true;
    if (text == QLatin1StringView("false") || text == QLatin1StringView("False") || text == QLatin1StringView("FALSE"))
        return false;

    bool ok = false;
    if (decimal.match(text).hasMatch()) {
        const qlonglong value = text.toLongLong(&ok);
        if (ok)
            return value;
    } else if (octal.match(text).hasMatch()) {
        const qlonglong value = text.mid(2).toLongLong(&ok, 8);
        if (ok)
            return value;
    } else if (hexadecimal.match(text).hasMatch()) {
        const qlonglong value = text.mid(2).toLongLong(&ok, 16);
        if (ok)
            return value;
    }
    if (floating.match(text).hasMatch()) {
        const double value = text.toDouble(&ok);
        if (ok)
            return value;
    }
    return text;
}

/*!
 * \internal
 * \brief Builds JSON from the events of the yaml-cpp parser.
 *
 * Anchored values are kept with the number of values they hold, so an alias
 * costs one lookup and the size of the expanded document is known without
 * expanding it. QJsonValue copies share their data.
 */
class JsonBuilder : public YAML::EventHandler
{
public:
    /*!
     * \internal
     * \brief Returns the document, or a null value when the stream held none.
     */
    QJsonValue result() const { return m_result; }

    /*!
     * \internal
     * \brief Starts a document; nothing to do.
     */
    void OnDocumentStart(const YAML::Mark &) override {}

    /*!
     * \internal
     * \brief Ends a document; nothing to do.
     */
    void OnDocumentEnd() override {}

    /*!
     * \internal
     * \brief Adds a null value with \a anchor.
     */
    void OnNull(const YAML::Mark &, YAML::anchor_t anchor) override { add(QJsonValue::Null, 1, anchor); }

    /*!
     * \internal
     * \brief Adds the value anchored as \a anchor again, counting all it holds.
     */
    void OnAlias(const YAML::Mark &, YAML::anchor_t anchor) override
    {
        const auto it = m_anchors.constFind(anchor);
        if (it == m_anchors.constEnd())
            add(QJsonValue::Null, 1, YAML::NullAnchor);
        else
            add(it->value, it->count, YAML::NullAnchor);
    }

    /*!
     * \internal
     * \brief Adds the scalar \a value; quoted and !!str scalars stay strings.
     */
    void OnScalar(const YAML::Mark &, const std::string &tag, YAML::anchor_t anchor, const std::string &value) override
    {
        const QString text = QString::fromStdString(value);
        const bool string = tag == "!" || tag == "tag:yaml.org,2002:str";
        add(string ? QJsonValue(text) : plainScalar(text), 1, anchor);
    }

    /*!
     * \internal
     * \brief Opens a sequence with \a anchor.
     */
    void OnSequenceStart(const YAML::Mark &, const std::string &, YAML::anchor_t anchor, YAML::EmitterStyle::value) override
    {
        open(false, anchor);
    }

    /*!
     * \internal
     * \brief Closes the innermost sequence.
     */
    void OnSequenceEnd() override { close(); }

    /*!
     * \internal
     * \brief Opens a mapping with \a anchor.
     */
    void OnMapStart(const YAML::Mark &, const std::string &, YAML::anchor_t anchor, YAML::EmitterStyle::value) override
    {
        open(true, anchor);
    }

    /*!
     * \internal
     * \brief Closes the innermost mapping.
     */
    void OnMapEnd() override { close(); }

private:
    /*!
     * \internal
     * \brief A mapping or sequence being read.
     */
    struct Frame
    {
        bool map {false};
        YAML::anchor_t anchor {YAML::NullAnchor};
        QJsonObject object;
        QJsonArray array;
        QString key;
        bool haveKey {false};
        qint64 count {1};
    };

    /*!
     * \internal
     * \brief A finished anchored value and the number of values in it.
     */
    struct Anchored
    {
        QJsonValue value;
        qint64 count {1};
    };

    /*!
     * \internal
     * \brief Counts \a count more values and refuses the document past the limit.
     */
    void spend(qint64 count)
    {
        m_total += count;
        if (m_total > YamlJson::kMaxValues)
            throw Refused {QStringLiteral("The YAML file expands to more than %L1 values.").arg(YamlJson::kMaxValues)};
    }

    /*!
     * \internal
     * \brief Opens a mapping when \a map is true, else a sequence.
     */
    void open(bool map, YAML::anchor_t anchor)
    {
        if (!m_stack.isEmpty() && m_stack.last().map && !m_stack.last().haveKey)
            throw Refused {QStringLiteral("The YAML file uses a mapping or list as a key, which JSON cannot hold.")};
        if (m_stack.size() >= YamlJson::kMaxDepth)
            throw Refused {QStringLiteral("The YAML file is nested more than %1 levels deep.").arg(YamlJson::kMaxDepth)};
        spend(1);
        Frame frame;
        frame.map = map;
        frame.anchor = anchor;
        m_stack.append(std::move(frame));
    }

    /*!
     * \internal
     * \brief Closes the innermost mapping or sequence and adds it to its parent.
     */
    void close()
    {
        Frame frame = m_stack.takeLast();
        const QJsonValue value = frame.map ? QJsonValue(frame.object) : QJsonValue(frame.array);
        if (frame.anchor != YAML::NullAnchor)
            m_anchors.insert(frame.anchor, {value, frame.count});
        place(value, frame.count);
    }

    /*!
     * \internal
     * \brief Adds \a value holding \a count values, anchored as \a anchor.
     */
    void add(const QJsonValue &value, qint64 count, YAML::anchor_t anchor)
    {
        spend(count);
        if (anchor != YAML::NullAnchor)
            m_anchors.insert(anchor, {value, count});
        place(value, count);
    }

    /*!
     * \internal
     * \brief Puts \a value into the innermost mapping or sequence, or makes it the result.
     *
     * In a mapping every other value is a key. A "<<" key merges the mapping
     * or mappings it names, as YAML 1.1 merge keys do; keys written in the
     * mapping itself win.
     */
    void place(const QJsonValue &value, qint64 count)
    {
        if (m_stack.isEmpty()) {
            m_result = value;
            return;
        }
        Frame &frame = m_stack.last();
        frame.count += count;
        if (!frame.map) {
            frame.array.append(value);
            return;
        }
        if (!frame.haveKey) {
            frame.key = value.isString() ? value.toString() : value.toVariant().toString();
            frame.haveKey = true;
            return;
        }
        frame.haveKey = false;
        if (frame.key == QLatin1StringView("<<") && (value.isObject() || value.isArray())) {
            const QJsonArray sources = value.isArray() ? value.toArray() : QJsonArray {value};
            for (const QJsonValue &source : sources) {
                const QJsonObject merged = source.toObject();
                for (auto it = merged.begin(); it != merged.end(); ++it) {
                    if (!frame.object.contains(it.key()))
                        frame.object.insert(it.key(), it.value());
                }
            }
            return;
        }
        frame.object.insert(frame.key, value);
    }

    QList<Frame> m_stack;
    QHash<YAML::anchor_t, Anchored> m_anchors;
    QJsonValue m_result;
    qint64 m_total {0};
};
} // namespace

/*!
 * \brief Converts the first document of \a yaml to JSON.
 *
 * The document is read from the parser's events, not from a node tree.
 * Aliases share the value of their anchor; kMaxValues counts every value as
 * if it were written out, because the JSON text made from the result is, and
 * a few lines of aliases can stand for billions of values. kMaxDepth bounds
 * nesting below the parser's own guard. Mapping keys are taken as text;
 * mappings or lists as keys are refused. Tags other than strings are ignored.
 * \param yaml The file content in UTF-8.
 * \param errorString Receives the reason when the conversion fails; may be null.
 * \return The JSON document, an object or an array, or nothing on failure.
 */
std::optional<QJsonDocument> YamlJson::toJson(const QByteArray &yaml, QString *errorString)
{
    auto fail = [errorString](const QString &message) {
        if (errorString)
            *errorString = message;
        return std::nullopt;
    };

    try {
        std::istringstream stream(yaml.toStdString());
        YAML::Parser parser(stream);
        JsonBuilder builder;
        parser.HandleNextDocument(builder);
        const QJsonValue value = builder.result();
        if (value.isObject())
            return QJsonDocument(value.toObject());
        if (value.isArray())
            return QJsonDocument(value.toArray());
        return fail(QStringLiteral("The YAML file holds no mapping or list."));
    } catch (const Refused &refused) {
        return fail(refused.what);
    } catch (const YAML::Exception &exception) {
        const QString message = QString::fromStdString(exception.msg);
        if (exception.mark.is_null())
            return fail(QStringLiteral("The YAML file cannot be read: %1").arg(message));
        return fail(QStringLiteral("The YAML file cannot be read: line %1, column %2: %3")
                        .arg(exception.mark.line + 1)
                        .arg(exception.mark.column + 1)
                        .arg(message));
    } catch (const std::exception &exception) {
        return fail(QStringLiteral("The YAML file cannot be read: %1").arg(QString::fromLocal8Bit(exception.what())));
    }
}
