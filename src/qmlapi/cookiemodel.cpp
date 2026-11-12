// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/cookiemodel.h"

#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

#include "core/http/cookiestore.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \class CookieModel
 * \inmodule ReqDeckQmlApi
 * \brief Shows the cookies of the active workspace to QML and edits them.
 */

/*!
 * \brief Creates a model of the cookies \a manager keeps, owned by \a parent.
 */
CookieModel::CookieModel(WorkspaceManager *manager, QObject *parent)
    : QAbstractListModel(parent)
    , m_manager(manager)
{
    connect(m_manager->cookieStore(), &CookieStore::changed, this, &CookieModel::reload);
    connect(m_manager, &WorkspaceManager::cookiesEnabledChanged, this, &CookieModel::enabledChanged);
    reload();
}

/*!
 * \brief Returns the number of cookies; 0 for a valid \a parent.
 */
int CookieModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_cookies.size());
}

/*!
 * \brief Returns \a role of the cookie at \a index.
 *
 * ExpiresRole is the local date and time of expiry, or an empty string for a
 * session cookie, which ends with the workspace.
 */
QVariant CookieModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const QNetworkCookie &cookie = m_cookies.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return QString::fromUtf8(cookie.name());
    case DomainRole:
        return cookie.domain();
    case ValueRole:
        return QString::fromUtf8(cookie.value());
    case PathRole:
        return cookie.path();
    case ExpiresRole:
        return cookie.isSessionCookie()
                   ? QString()
                   : QLocale().toString(cookie.expirationDate().toLocalTime(), QLocale::ShortFormat);
    case SecureRole:
        return cookie.isSecure();
    case HttpOnlyRole:
        return cookie.isHttpOnly();
    default:
        return {};
    }
}

/*!
 * \brief Returns the role names used by QML delegates.
 */
QHash<int, QByteArray> CookieModel::roleNames() const
{
    return {{DomainRole, "domain"},     {NameRole, "name"},     {ValueRole, "value"},       {PathRole, "path"},
            {ExpiresRole, "expires"},   {SecureRole, "secure"}, {HttpOnlyRole, "httpOnly"}};
}

/*!
 * \brief Returns whether requests of the active workspace send and store cookies.
 */
bool CookieModel::enabled() const
{
    return m_manager->cookiesEnabled();
}

/*!
 * \brief Sets whether requests of the active workspace send and store cookies.
 * \param enabled True to send and store cookies.
 */
void CookieModel::setEnabled(bool enabled)
{
    m_manager->setCookiesEnabled(enabled);
}

/*!
 * \brief Sets the value of the cookie at \a row to \a value.
 *
 * Domain, path, expiry, and flags stay as they were.
 * \return False for a row that does not exist, or a value with ';' or a control character.
 */
bool CookieModel::setValue(int row, const QString &value)
{
    if (row < 0 || row >= m_cookies.size() || !isValidValue(value))
        return false;
    QNetworkCookie cookie = m_cookies.at(row);
    if (cookie.value() == value.toUtf8())
        return true;
    cookie.setValue(value.toUtf8());
    return m_manager->cookieStore()->setCookie(cookie);
}

/*!
 * \brief Adds a session cookie \a name = \a value for \a domain and \a path.
 *
 * A cookie with the same name, domain, and path is replaced. A leading dot of
 * the domain, as in ".example.com", makes it a domain cookie for subdomains
 * too; without one it is sent to that host only.
 * \param path The path; empty means "/".
 * \return False without a name or domain, or with characters a cookie cannot hold.
 */
bool CookieModel::add(const QString &domain, const QString &name, const QString &value, const QString &path)
{
    const QString trimmedName = name.trimmed();
    const QString trimmedDomain = domain.trimmed().toLower();
    if (trimmedName.isEmpty() || trimmedDomain.isEmpty()
        || trimmedName.contains(QRegularExpression(QStringLiteral(R"([\s;=,])"))) || !isValidValue(value))
        return false;
    QNetworkCookie cookie(trimmedName.toUtf8(), value.toUtf8());
    cookie.setDomain(trimmedDomain);
    cookie.setPath(path.trimmed().isEmpty() ? QStringLiteral("/") : path.trimmed());
    return m_manager->cookieStore()->setCookie(cookie);
}

/*!
 * \brief Removes the cookie at \a row.
 * \return False for a row that does not exist.
 */
bool CookieModel::remove(int row)
{
    if (row < 0 || row >= m_cookies.size())
        return false;
    return m_manager->cookieStore()->removeCookie(m_cookies.at(row));
}

/*!
 * \brief Removes every cookie of the active workspace.
 */
void CookieModel::removeAll()
{
    m_manager->cookieStore()->clear();
}

/*!
 * \internal
 * \brief Returns whether \a value can be a cookie value.
 *
 * A ';' would start another cookie in the Cookie header, and a control
 * character would break the header or the saved file.
 */
bool CookieModel::isValidValue(const QString &value)
{
    return std::none_of(value.cbegin(), value.cend(), [](QChar c) {
        return c == u';' || c.unicode() < 0x20 || c.unicode() == 0x7F;
    });
}

/*!
 * \internal
 * \brief Reloads the rows from the store.
 */
void CookieModel::reload()
{
    const int before = int(m_cookies.size());
    beginResetModel();
    m_cookies = m_manager->cookieStore()->cookies();
    endResetModel();
    if (m_cookies.size() != before)
        emit countChanged();
}
