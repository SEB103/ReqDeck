// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/jsonpath.h"

#include <QJsonObject>
#include <QList>

#include <algorithm>
#include <optional>

namespace {
/*!
 * \internal
 * \brief Raised when the path cannot be read or the query visits too much.
 */
struct PathError
{
    /*!
     * \internal
     * \brief Why, for the user.
     */
    QString what;
};

/*!
 * \internal
 * \brief One selector inside brackets or after a dot.
 */
struct Selector
{
    /*!
     * \internal
     * \brief What the selector picks.
     */
    enum class Kind { Name, Wildcard, Index, Slice, Filter };
    Kind kind {Kind::Name};
    QString name;
    qint64 index {0};
    std::optional<qint64> start;
    std::optional<qint64> end;
    qint64 step {1};
    QStringList filterPath;
    QString op;
    QJsonValue literal;
};

/*!
 * \internal
 * \brief A step of the path: its selectors, and whether it searches all descendants.
 */
struct Segment
{
    bool descendants {false};
    QList<Selector> selectors;
};

/*!
 * \internal
 * \brief Reads a path into segments.
 */
class Parser
{
public:
    /*!
     * \internal
     * \brief Prepares reading \a path.
     */
    explicit Parser(const QString &path)
        : m_text(path.trimmed())
    {
    }

    /*!
     * \internal
     * \brief Returns the segments of the path.
     */
    QList<Segment> parse()
    {
        if (!m_text.startsWith(u'$'))
            fail(QStringLiteral("A path starts with $."));
        m_pos = 1;
        QList<Segment> segments;
        while (!atEnd()) {
            Segment segment;
            if (take(QStringLiteral(".."))) {
                segment.descendants = true;
                if (peek() == u'[')
                    segment.selectors = bracket();
                else
                    segment.selectors.append(dotSelector());
            } else if (take(QStringLiteral("."))) {
                segment.selectors.append(dotSelector());
            } else if (peek() == u'[') {
                segment.selectors = bracket();
            } else {
                fail(QStringLiteral("Unexpected \"%1\" at %2.").arg(peek()).arg(m_pos + 1));
            }
            segments.append(segment);
        }
        return segments;
    }

private:
    [[noreturn]] void fail(const QString &message) const { throw PathError {message}; }
    bool atEnd() const { return m_pos >= m_text.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_text.at(m_pos); }
    bool take(const QString &token)
    {
        if (m_text.mid(m_pos, token.size()) != token)
            return false;
        m_pos += token.size();
        return true;
    }
    void skipSpaces()
    {
        while (!atEnd() && peek().isSpace())
            ++m_pos;
    }

    Selector dotSelector()
    {
        Selector selector;
        if (take(QStringLiteral("*"))) {
            selector.kind = Selector::Kind::Wildcard;
            return selector;
        }
        selector.name = identifier();
        return selector;
    }

    QString identifier()
    {
        const qsizetype from = m_pos;
        while (!atEnd() && (peek().isLetterOrNumber() || peek() == u'_' || peek() == u'-' || peek() == u'$'))
            ++m_pos;
        if (m_pos == from)
            fail(QStringLiteral("A name is missing at %1.").arg(from + 1));
        return m_text.mid(from, m_pos - from);
    }

    QString quoted()
    {
        const QChar quote = peek();
        ++m_pos;
        QString value;
        while (!atEnd() && peek() != quote) {
            if (peek() == u'\\' && m_pos + 1 < m_text.size())
                ++m_pos;
            value.append(peek());
            ++m_pos;
        }
        if (atEnd())
            fail(QStringLiteral("A quote is not closed."));
        ++m_pos;
        return value;
    }

    std::optional<qint64> number()
    {
        skipSpaces();
        const qsizetype from = m_pos;
        if (peek() == u'-')
            ++m_pos;
        while (!atEnd() && peek().isDigit())
            ++m_pos;
        if (m_pos == from || (m_pos == from + 1 && m_text.at(from) == u'-'))
            return std::nullopt;
        bool ok = false;
        const qint64 value = m_text.mid(from, m_pos - from).toLongLong(&ok);
        if (!ok)
            fail(QStringLiteral("The number at %1 is too large.").arg(from + 1));
        return value;
    }

    QList<Selector> bracket()
    {
        ++m_pos; // '['
        QList<Selector> selectors;
        while (true) {
            skipSpaces();
            Selector selector;
            if (peek() == u'\'' || peek() == u'"') {
                selector.name = quoted();
            } else if (take(QStringLiteral("*"))) {
                selector.kind = Selector::Kind::Wildcard;
            } else if (take(QStringLiteral("?"))) {
                selector = filter();
            } else {
                const std::optional<qint64> first = number();
                skipSpaces();
                if (peek() == u':') {
                    selector.kind = Selector::Kind::Slice;
                    selector.start = first;
                    ++m_pos;
                    selector.end = number();
                    skipSpaces();
                    if (take(QStringLiteral(":"))) {
                        if (const std::optional<qint64> step = number())
                            selector.step = *step;
                    }
                    if (selector.step <= 0)
                        fail(QStringLiteral("A slice step must be positive."));
                } else if (first) {
                    selector.kind = Selector::Kind::Index;
                    selector.index = *first;
                } else {
                    fail(QStringLiteral("Unexpected \"%1\" in brackets at %2.").arg(peek()).arg(m_pos + 1));
                }
            }
            selectors.append(selector);
            skipSpaces();
            if (take(QStringLiteral(",")))
                continue;
            if (take(QStringLiteral("]")))
                return selectors;
            fail(QStringLiteral("A bracket is not closed."));
        }
    }

    Selector filter()
    {
        Selector selector;
        selector.kind = Selector::Kind::Filter;
        skipSpaces();
        const bool parenthesis = take(QStringLiteral("("));
        skipSpaces();
        if (!take(QStringLiteral("@")))
            fail(QStringLiteral("A filter starts with @."));
        while (true) {
            if (take(QStringLiteral(".")))
                selector.filterPath.append(identifier());
            else if (peek() == u'[') {
                ++m_pos;
                skipSpaces();
                if (peek() != u'\'' && peek() != u'"')
                    fail(QStringLiteral("A filter names its members in quotes."));
                selector.filterPath.append(quoted());
                skipSpaces();
                if (!take(QStringLiteral("]")))
                    fail(QStringLiteral("A bracket is not closed."));
            } else {
                break;
            }
        }
        skipSpaces();
        for (const char *op : {"==", "!=", "<=", ">=", "<", ">"}) {
            if (take(QString::fromLatin1(op))) {
                selector.op = QString::fromLatin1(op);
                break;
            }
        }
        if (!selector.op.isEmpty()) {
            skipSpaces();
            if (peek() == u'\'' || peek() == u'"') {
                selector.literal = quoted();
            } else if (take(QStringLiteral("true"))) {
                selector.literal = true;
            } else if (take(QStringLiteral("false"))) {
                selector.literal = false;
            } else if (take(QStringLiteral("null"))) {
                selector.literal = QJsonValue::Null;
            } else {
                const qsizetype from = m_pos;
                while (!atEnd() && (peek().isDigit() || peek() == u'-' || peek() == u'.' || peek() == u'e' || peek() == u'E' || peek() == u'+'))
                    ++m_pos;
                bool ok = false;
                const double value = m_text.mid(from, m_pos - from).toDouble(&ok);
                if (!ok)
                    fail(QStringLiteral("A filter compares with a number, a string, true, false, or null."));
                selector.literal = value;
            }
        }
        skipSpaces();
        if (parenthesis && !take(QStringLiteral(")")))
            fail(QStringLiteral("A filter parenthesis is not closed."));
        return selector;
    }

    QString m_text;
    qsizetype m_pos {0};
};

/*!
 * \internal
 * \brief Evaluates segments while counting visited values.
 */
class Evaluator
{
public:
    /*!
     * \internal
     * \brief Returns the children of \a value that \a selector picks, appended to \a out.
     */
    void select(const QJsonValue &value, const Selector &selector, QJsonArray &out)
    {
        switch (selector.kind) {
        case Selector::Kind::Name:
            if (value.isObject() && value.toObject().contains(selector.name))
                add(out, value.toObject().value(selector.name));
            break;
        case Selector::Kind::Wildcard:
            for (const QJsonValue &child : children(value))
                add(out, child);
            break;
        case Selector::Kind::Index:
            if (value.isArray()) {
                const QJsonArray array = value.toArray();
                const qint64 index = selector.index < 0 ? array.size() + selector.index : selector.index;
                if (index >= 0 && index < array.size())
                    add(out, array.at(index));
            }
            break;
        case Selector::Kind::Slice:
            if (value.isArray()) {
                const QJsonArray array = value.toArray();
                const qint64 size = array.size();
                const auto clamp = [size](qint64 i) { return std::clamp<qint64>(i < 0 ? size + i : i, 0, size); };
                const qint64 from = selector.start ? clamp(*selector.start) : 0;
                const qint64 to = selector.end ? clamp(*selector.end) : size;
                // Stepping stops before it could pass the end, so a huge step cannot overflow.
                for (qint64 i = from; i < to; i += selector.step) {
                    add(out, array.at(i));
                    if (to - i <= selector.step)
                        break;
                }
            }
            break;
        case Selector::Kind::Filter:
            for (const QJsonValue &child : children(value)) {
                if (matches(child, selector))
                    add(out, child);
            }
            break;
        }
    }

    /*!
     * \internal
     * \brief Appends \a value and all values inside it to \a out, parents before children.
     */
    void descendants(const QJsonValue &value, QJsonArray &out)
    {
        add(out, value);
        for (const QJsonValue &child : children(value))
            descendants(child, out);
    }

private:
    QList<QJsonValue> children(const QJsonValue &value)
    {
        QList<QJsonValue> result;
        if (value.isObject()) {
            const QJsonObject object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
                result.append(it.value());
        } else if (value.isArray()) {
            for (const QJsonValue &item : value.toArray())
                result.append(item);
        }
        return result;
    }

    void add(QJsonArray &out, const QJsonValue &value)
    {
        if (++m_visited > JsonPath::kMaxVisited)
            throw PathError {QStringLiteral("The query visits more than %L1 values.").arg(JsonPath::kMaxVisited)};
        out.append(value);
    }

    bool matches(const QJsonValue &item, const Selector &selector) const
    {
        QJsonValue value = item;
        for (const QString &name : selector.filterPath) {
            if (!value.isObject() || !value.toObject().contains(name))
                return false;
            value = value.toObject().value(name);
        }
        if (selector.op.isEmpty())
            return true;
        const QJsonValue &literal = selector.literal;
        if (selector.op == QLatin1StringView("=="))
            return value == literal;
        if (selector.op == QLatin1StringView("!="))
            return value != literal;
        if (value.isDouble() && literal.isDouble()) {
            const double a = value.toDouble();
            const double b = literal.toDouble();
            return selector.op == QLatin1StringView("<") ? a < b
                 : selector.op == QLatin1StringView("<=") ? a <= b
                 : selector.op == QLatin1StringView(">") ? a > b
                                                          : a >= b;
        }
        if (value.isString() && literal.isString()) {
            const int c = value.toString().compare(literal.toString());
            return selector.op == QLatin1StringView("<") ? c < 0
                 : selector.op == QLatin1StringView("<=") ? c <= 0
                 : selector.op == QLatin1StringView(">") ? c > 0
                                                          : c >= 0;
        }
        return false;
    }

    int m_visited {0};
};
} // namespace

/*!
 * \class JsonPath
 * \inmodule ReqDeckCore
 * \brief Evaluates a subset of JSONPath without any script.
 */

/*!
 * \brief Returns the values \a path selects in \a root.
 *
 * Each segment applies to every value the previous one selected. A query that
 * visits more than kMaxVisited values stops with an error, so a "$..*" over a
 * huge body cannot hang the window.
 * \param root The parsed JSON document.
 * \param path The JSONPath, starting with $.
 * \return The matches, or an error for a path outside the supported subset.
 */
JsonPathResult JsonPath::evaluate(const QJsonValue &root, const QString &path)
{
    JsonPathResult result;
    try {
        const QList<Segment> segments = Parser(path).parse();
        Evaluator evaluator;
        QJsonArray current {root};
        for (const Segment &segment : segments) {
            QJsonArray sources;
            if (segment.descendants) {
                for (const QJsonValue &value : std::as_const(current))
                    evaluator.descendants(value, sources);
            } else {
                sources = current;
            }
            QJsonArray next;
            for (const QJsonValue &value : std::as_const(sources)) {
                for (const Selector &selector : segment.selectors)
                    evaluator.select(value, selector, next);
            }
            current = next;
        }
        result.ok = true;
        result.matches = current;
    } catch (const PathError &error) {
        result.errorString = error.what;
    }
    return result;
}
