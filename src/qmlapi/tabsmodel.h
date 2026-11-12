// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABSMODEL_H
#define TABSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

class CollectionTreeModel;
QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

/**
 * The open request tabs of the active workspace.
 *
 * At most one tab is the preview tab: previewing another request replaces
 * its request, and keeping it makes it a normal tab. Tabs of requests that
 * leave the collection close by themselves.
 */
class TabsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TabsModel)

    /** Row of the current tab, or -1 without tabs. */
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)

    /** Request id of the current tab, or an empty string. */
    Q_PROPERTY(QString currentRequestId READ currentRequestId NOTIFY currentChanged)

    /** Id of the current tab, or 0. */
    Q_PROPERTY(int currentTabId READ currentTabId NOTIFY currentChanged)

    /** Number of tabs. */
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

    /** Whether the tab bar shows; off keeps one preview tab, as without tabs. */
    Q_PROPERTY(bool showTabs READ showTabs WRITE setShowTabs NOTIFY showTabsChanged)

public:
    /** Data roles of a tab. */
    enum Role {
        TabIdRole = Qt::UserRole + 1, /**< Id of the tab, unique while the model lives. */
        RequestIdRole,                /**< Id of the request shown. */
        NameRole,                     /**< Name of the request. */
        MethodRole,                   /**< HTTP method of the request, for example "GET". */
        PreviewRole,                  /**< Whether the tab is the preview tab. */
        BusyRole                      /**< Whether a send of the tab is running. */
    };
    Q_ENUM(Role)

    /** Most tabs open at once. */
    static constexpr int kMaxTabs = 30;

    /** Creates an empty model of tabs for requests of \a collection, which is not owned. */
    explicit TabsModel(CollectionTreeModel *collection, QObject *parent = nullptr);

    /** Returns the number of tabs. */
    int rowCount(const QModelIndex &parent = {}) const override;

    /** Returns \a role of the tab at \a index. */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Returns the QML names of the roles. */
    QHash<int, QByteArray> roleNames() const override;

    /** Returns the row of the current tab, or -1. */
    int currentIndex() const;

    /** Makes the tab at \a row current; an invalid row is ignored. */
    void setCurrentIndex(int row);

    /** Returns the request id of the current tab, or an empty string. */
    QString currentRequestId() const;

    /** Returns the id of the current tab, or 0. */
    int currentTabId() const;

    /** Shows \a requestId in the preview tab, or in its own tab when one is open. */
    Q_INVOKABLE void preview(const QString &requestId);

    /** Shows \a requestId in a tab that stays, opening one when needed. */
    Q_INVOKABLE void open(const QString &requestId);

    /** Keeps the preview tab, if it is current, as a normal tab. */
    Q_INVOKABLE void keepCurrent();

    /** Closes the tab at \a row. */
    Q_INVOKABLE void close(int row);

    /** Closes every tab but the one at \a row. */
    Q_INVOKABLE void closeOthers(int row);

    /** Moves the tab at \a from to \a to. */
    Q_INVOKABLE void move(int from, int to);

    /** Makes the next tab current, wrapping around; \a step -1 goes back. */
    Q_INVOKABLE void cycle(int step);

    /** Returns the row of the tab with id \a tabId, or -1. */
    int rowOfTab(int tabId) const;

    /** Marks the tab \a tabId as running a send or not. */
    void setBusy(int tabId, bool busy);

    /** Returns the request ids of the tabs, in order, for restoring them later. */
    QStringList requestIds() const;

    /** Returns the request id of the preview tab, or an empty string. */
    QString previewRequestId() const;

    /** Replaces the tabs with \a requestIds, \a preview as the preview tab, and \a current current; unknown ids are left out. */
    void restore(const QStringList &requestIds, const QString &preview, const QString &current);

    /** Closes every tab. */
    Q_INVOKABLE void clear();

    /** Returns whether the tab bar shows; true unless turned off in the settings. */
    bool showTabs() const;

    /** Shows or hides the tab bar; hiding keeps only the current tab, as a preview tab. */
    void setShowTabs(bool show);

    /** Keeps the tabs of each workspace in \a settings, which is not owned; null keeps nothing. */
    void setSettings(QSettings *settings);

    /** Switches to the tabs of the workspace \a workspaceId, as kept in the settings; empty for none. */
    void attachWorkspace(const QString &workspaceId);

    /** Sets \a source, which returns the id of the open workspace, so tabs save only for the attached one. */
    void setWorkspaceIdSource(std::function<QString()> source);

signals:
    /** Emitted when the current tab or its request changes. */
    void currentChanged();

    /** Emitted when the number of tabs changes. */
    void countChanged();

    /** Emitted when showTabs() changes. */
    void showTabsChanged();

    /** Emitted when the tab \a tabId closes, so its response and send can go. */
    void tabClosed(int tabId);

private:
    /** One open tab. */
    struct Tab
    {
        int id {0};
        QString requestId;
        bool preview {false};
    };

    /** Inserts a tab for \a requestId after the current one and makes it current. */
    void insertTab(const QString &requestId, bool preview);

    /** Removes the tab at \a row with \a nextCurrent as the current row afterwards. */
    void removeTab(int row, int nextCurrent);

    /** Keeps only the current tab, as the preview tab. */
    void reduceToPreview();

    /** Closes tabs of requests that left the collection and refreshes names. */
    void followCollection();

    /** Returns the row of the first tab of \a requestId, or -1. */
    int rowOfRequest(const QString &requestId) const;

    /** Writes the tabs of the attached workspace to the settings. */
    void save() const;

    /** Settings the tabs are kept in, or null; not owned. */
    QSettings *m_settings {nullptr};

    /** Workspace whose tabs these are; empty for none. */
    QString m_workspaceId;

    /** Whether tabs are being restored, so nothing is saved meanwhile. */
    bool m_restoring {false};

    /** Whether the tab bar shows. */
    bool m_showTabs {true};

    /** Returns the id of the open workspace, or nothing to save always. */
    std::function<QString()> m_workspaceIdSource;

    /** Collection of the requests; not owned. */
    CollectionTreeModel *m_collection {nullptr};

    /** Tabs in order. */
    QList<Tab> m_tabs;

    /** Ids of tabs with a running send. */
    QSet<int> m_busy;

    /** Row of the current tab, or -1. */
    int m_current {-1};

    /** Id the next tab gets. */
    int m_nextId {1};
};

#endif // TABSMODEL_H
