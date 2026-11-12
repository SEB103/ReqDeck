// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COOKIEMODEL_H
#define COOKIEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QNetworkCookie>

class WorkspaceManager;

/**
 * Shows the cookies of the active workspace to QML and edits them.
 *
 * Rows follow CookieStore::cookies(): by domain, path, and name. The model
 * resets whenever the store changes, also when a response sets a cookie.
 */
class CookieModel : public QAbstractListModel
{
    Q_OBJECT

    /** Number of cookies. */
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

    /** Whether requests of the active workspace send and store cookies. */
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    /** Roles of a cookie row. */
    enum Role {
        DomainRole = Qt::UserRole + 1,
        NameRole,
        ValueRole,
        PathRole,
        ExpiresRole,
        SecureRole,
        HttpOnlyRole
    };
    Q_ENUM(Role)

    /** Creates a model of the cookies \a manager keeps; \a manager is not owned. */
    explicit CookieModel(WorkspaceManager *manager, QObject *parent = nullptr);

    /** Returns the number of cookies. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** Returns \a role of the cookie at \a index. */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Returns the role names used by QML delegates. */
    QHash<int, QByteArray> roleNames() const override;

    /** Returns whether requests of the active workspace send and store cookies. */
    bool enabled() const;

    /** Sets whether requests of the active workspace send and store cookies. */
    void setEnabled(bool enabled);

    /** Sets the value of the cookie at \a row; false for a row that does not exist. */
    Q_INVOKABLE bool setValue(int row, const QString &value);

    /** Adds a cookie for \a domain and \a path; false without a name or domain. */
    Q_INVOKABLE bool add(const QString &domain, const QString &name, const QString &value, const QString &path);

    /** Removes the cookie at \a row; false for a row that does not exist. */
    Q_INVOKABLE bool remove(int row);

    /** Removes every cookie of the workspace. */
    Q_INVOKABLE void removeAll();

signals:
    /** Emitted when the number of cookies changes. */
    void countChanged();

    /** Emitted when enabled() changes. */
    void enabledChanged();

private:
    /** Reloads the rows from the store. */
    void reload();

    /** Workspace manager owning the store; not owned. */
    WorkspaceManager *m_manager {nullptr};

    /** Cookies as shown. */
    QList<QNetworkCookie> m_cookies;
};

#endif // COOKIEMODEL_H
