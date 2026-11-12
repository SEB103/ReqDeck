// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/tabsmodel.h"

#include <QSettings>

#include "core/http/requestspec.h"
#include "qmlapi/collectiontreemodel.h"

/*!
 * \class TabsModel
 * \inmodule ReqDeckQmlApi
 * \brief The open request tabs of the active workspace.
 */

/*!
 * \brief Creates an empty model of tabs for requests of \a collection, owned by \a parent.
 */
TabsModel::TabsModel(CollectionTreeModel *collection, QObject *parent)
    : QAbstractListModel(parent)
    , m_collection(collection)
{
    connect(m_collection, &CollectionTreeModel::collectionChanged, this, &TabsModel::followCollection);
    // A method or name changed in the editor shows on its tab too.
    connect(m_collection, &QAbstractItemModel::dataChanged, this, [this]() {
        if (!m_tabs.isEmpty())
            emit dataChanged(index(0), index(int(m_tabs.size()) - 1), {NameRole, MethodRole});
    });
    connect(this, &TabsModel::countChanged, this, &TabsModel::save);
    connect(this, &TabsModel::currentChanged, this, &TabsModel::save);
    connect(this, &QAbstractItemModel::rowsMoved, this, &TabsModel::save);
    connect(this, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
                if (roles.isEmpty() || roles.contains(PreviewRole) || roles.contains(RequestIdRole))
                    save();
            });
}

/*!
 * \brief Keeps the tabs of each workspace in \a settings.
 *
 * The tabs are UI state of this computer, not part of the workspace, so
 * opening a tab never makes the workspace dirty.
 * \param settings The settings store, or null to keep nothing.
 */
void TabsModel::setSettings(QSettings *settings)
{
    m_settings = settings;
    if (m_settings && !m_settings->value(QStringLiteral("Tabs/show"), true).toBool())
        setShowTabs(false);
}

/*!
 * \brief Tells the model which workspace is open right now, through \a source.
 *
 * Tabs are saved only while it is the attached workspace: when another
 * workspace opens, the collection empties before attachWorkspace() runs, and
 * the tabs closing then must not overwrite what the old workspace kept.
 * \param source Returns the id of the open workspace.
 */
void TabsModel::setWorkspaceIdSource(std::function<QString()> source)
{
    m_workspaceIdSource = std::move(source);
}

/*!
 * \brief Returns whether the tab bar shows.
 */
bool TabsModel::showTabs() const
{
    return m_showTabs;
}

/*!
 * \brief Shows or hides the tab bar, and keeps the choice in the settings.
 *
 * Hidden, ReqDeck works with one request at a time as before tabs: the other
 * tabs close, the current one becomes the preview tab, and open() and edits
 * no longer keep tabs.
 * \param show True to show the tabs.
 */
void TabsModel::setShowTabs(bool show)
{
    if (show == m_showTabs)
        return;
    m_showTabs = show;
    if (m_settings)
        m_settings->setValue(QStringLiteral("Tabs/show"), show);
    if (!show)
        reduceToPreview();
    emit showTabsChanged();
}

/*!
 * \brief Switches to the tabs of the workspace \a workspaceId.
 *
 * Called after another workspace opened: the old tabs were saved as they
 * changed, so they are only closed; the new workspace's tabs come back with
 * restore(), which leaves out requests the workspace no longer has.
 * \param workspaceId The id of the open workspace, or empty for none.
 */
void TabsModel::attachWorkspace(const QString &workspaceId)
{
    m_restoring = true;
    m_workspaceId.clear();
    clear();
    if (m_settings && !workspaceId.isEmpty()) {
        const QString group = QStringLiteral("Tabs/") + workspaceId + u'/';
        restore(m_settings->value(group + QStringLiteral("requests")).toStringList(),
                m_settings->value(group + QStringLiteral("preview")).toString(),
                m_settings->value(group + QStringLiteral("current")).toString());
    }
    m_workspaceId = workspaceId;
    m_restoring = false;
}

/*!
 * \internal
 * \brief Writes the tabs of the attached workspace to the settings.
 */
void TabsModel::save() const
{
    if (!m_settings || m_workspaceId.isEmpty() || m_restoring)
        return;
    if (m_workspaceIdSource && m_workspaceIdSource() != m_workspaceId)
        return;
    const QString group = QStringLiteral("Tabs/") + m_workspaceId + u'/';
    m_settings->setValue(group + QStringLiteral("requests"), requestIds());
    m_settings->setValue(group + QStringLiteral("preview"), previewRequestId());
    m_settings->setValue(group + QStringLiteral("current"), currentRequestId());
}

/*!
 * \brief Returns the number of tabs; 0 for a valid \a parent.
 */
int TabsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_tabs.size());
}

/*!
 * \brief Returns \a role of the tab at \a index.
 *
 * Name and method are read from the collection, so a rename shows at once.
 */
QVariant TabsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Tab &tab = m_tabs.at(index.row());
    switch (role) {
    case TabIdRole:
        return tab.id;
    case RequestIdRole:
        return tab.requestId;
    case Qt::DisplayRole:
    case NameRole:
        return m_collection->indexOfNode(tab.requestId).data(CollectionTreeModel::NameRole);
    case MethodRole: {
        const RequestSpec *request = m_collection->request(tab.requestId);
        return request ? QString::fromLatin1(httpMethodVerb(request->method)) : QString();
    }
    case PreviewRole:
        return tab.preview;
    case BusyRole:
        return m_busy.contains(tab.id);
    default:
        return {};
    }
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> TabsModel::roleNames() const
{
    return {{TabIdRole, "tabId"}, {RequestIdRole, "requestId"}, {NameRole, "name"},
            {MethodRole, "method"}, {PreviewRole, "preview"}, {BusyRole, "busy"}};
}

/*!
 * \brief Returns the row of the current tab, or -1.
 */
int TabsModel::currentIndex() const
{
    return m_current;
}

/*!
 * \brief Makes the tab at \a row current.
 */
void TabsModel::setCurrentIndex(int row)
{
    if (row < 0 || row >= m_tabs.size() || row == m_current)
        return;
    m_current = row;
    emit currentChanged();
}

/*!
 * \brief Returns the request id of the current tab, or an empty string.
 */
QString TabsModel::currentRequestId() const
{
    return m_current < 0 || m_current >= m_tabs.size() ? QString() : m_tabs.at(m_current).requestId;
}

/*!
 * \brief Returns the id of the current tab, or 0.
 */
int TabsModel::currentTabId() const
{
    return m_current < 0 || m_current >= m_tabs.size() ? 0 : m_tabs.at(m_current).id;
}

/*!
 * \brief Shows \a requestId in the preview tab, as a single click in the tree does.
 *
 * A request that already has a tab goes to that tab. Otherwise the preview
 * tab shows it, or a new preview tab opens after the current one. A running
 * preview tab stays, unless the tabs are hidden: then the one tab always
 * shows the request, and the running send ends in it, as before tabs.
 * \param requestId The request; a folder or an unknown id is ignored.
 */
void TabsModel::preview(const QString &requestId)
{
    if (!m_collection->request(requestId))
        return;
    if (const int row = rowOfRequest(requestId); row >= 0) {
        setCurrentIndex(row);
        return;
    }
    for (int row = 0; row < m_tabs.size(); ++row) {
        if (m_tabs.at(row).preview && (!m_showTabs || !m_busy.contains(m_tabs.at(row).id))) {
            m_tabs[row].requestId = requestId;
            m_current = row;
            emit dataChanged(index(row), index(row));
            emit currentChanged();
            return;
        }
    }
    insertTab(requestId, true);
}

/*!
 * \brief Shows \a requestId in a tab that stays, as a double click does.
 *
 * With the tabs hidden this is a preview.
 * \param requestId The request; a folder or an unknown id is ignored.
 */
void TabsModel::open(const QString &requestId)
{
    if (!m_showTabs) {
        preview(requestId);
        return;
    }
    if (!m_collection->request(requestId))
        return;
    if (const int row = rowOfRequest(requestId); row >= 0) {
        setCurrentIndex(row);
        keepCurrent();
        return;
    }
    insertTab(requestId, false);
}

/*!
 * \brief Keeps the current tab, when it is the preview tab, as a normal tab.
 *
 * Called when the request is edited or sent, so it is never replaced by the
 * next preview. Nothing is kept while the tabs are hidden.
 */
void TabsModel::keepCurrent()
{
    if (!m_showTabs || m_current < 0 || m_current >= m_tabs.size() || !m_tabs.at(m_current).preview)
        return;
    m_tabs[m_current].preview = false;
    emit dataChanged(index(m_current), index(m_current), {PreviewRole});
}

/*!
 * \brief Closes the tab at \a row.
 *
 * When it was current, the tab now at its place becomes current, else the one
 * before it.
 */
void TabsModel::close(int row)
{
    if (row < 0 || row >= m_tabs.size())
        return;
    const int oldId = currentTabId();
    const int oldRow = m_current;
    int next = m_current;
    if (row == m_current)
        next = m_tabs.size() == 1 ? -1 : std::min(row, int(m_tabs.size()) - 2);
    else if (m_current > row)
        next = m_current - 1;
    removeTab(row, next);
    if (currentTabId() != oldId || m_current != oldRow)
        emit currentChanged();
}

/*!
 * \brief Closes every tab but the one at \a row, which becomes current.
 */
void TabsModel::closeOthers(int row)
{
    if (row < 0 || row >= m_tabs.size())
        return;
    const int keep = m_tabs.at(row).id;
    const int oldId = currentTabId();
    const int oldRow = m_current;
    for (int i = int(m_tabs.size()) - 1; i >= 0; --i) {
        if (m_tabs.at(i).id == keep)
            continue;
        const int keptRow = rowOfTab(keep);
        removeTab(i, i < keptRow ? keptRow - 1 : keptRow);
    }
    if (currentTabId() != oldId || m_current != oldRow)
        emit currentChanged();
}

/*!
 * \brief Moves the tab at \a from to the row \a to; the current tab stays current.
 */
void TabsModel::move(int from, int to)
{
    if (from < 0 || from >= m_tabs.size() || to < 0 || to >= m_tabs.size() || from == to)
        return;
    const int currentId = currentTabId();
    const int oldRow = m_current;
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_tabs.move(from, to);
    m_current = rowOfTab(currentId);
    endMoveRows();
    if (m_current != oldRow)
        emit currentChanged();
}

/*!
 * \brief Makes the tab \a step places away current, wrapping around.
 */
void TabsModel::cycle(int step)
{
    if (m_tabs.size() < 2)
        return;
    const int size = int(m_tabs.size());
    setCurrentIndex(((m_current + step) % size + size) % size);
}

/*!
 * \brief Returns the row of the tab \a tabId, or -1.
 */
int TabsModel::rowOfTab(int tabId) const
{
    for (int row = 0; row < m_tabs.size(); ++row) {
        if (m_tabs.at(row).id == tabId)
            return row;
    }
    return -1;
}

/*!
 * \brief Marks the tab \a tabId as running a send when \a busy is true.
 *
 * A running tab is never replaced by a preview or closed by the tab limit
 * while the tabs show.
 */
void TabsModel::setBusy(int tabId, bool busy)
{
    const bool changed = busy ? !m_busy.contains(tabId) : m_busy.remove(tabId);
    if (busy)
        m_busy.insert(tabId);
    if (const int row = rowOfTab(tabId); changed && row >= 0)
        emit dataChanged(index(row), index(row), {BusyRole});
}

/*!
 * \brief Returns the request ids of the tabs, in order.
 */
QStringList TabsModel::requestIds() const
{
    QStringList ids;
    for (const Tab &tab : m_tabs)
        ids.append(tab.requestId);
    return ids;
}

/*!
 * \brief Returns the request id of the preview tab, or an empty string.
 */
QString TabsModel::previewRequestId() const
{
    for (const Tab &tab : m_tabs) {
        if (tab.preview)
            return tab.requestId;
    }
    return {};
}

/*!
 * \brief Replaces the tabs with those of \a requestIds.
 *
 * Ids the collection no longer has, repeated ids, and ids past kMaxTabs are
 * left out. \a preview names the preview tab and \a current the current one;
 * without a match the first tab is current. With the tabs hidden only the
 * current one stays, as the preview tab.
 */
void TabsModel::restore(const QStringList &requestIds, const QString &preview, const QString &current)
{
    clear();
    beginResetModel();
    for (const QString &id : requestIds) {
        if (m_tabs.size() >= kMaxTabs || !m_collection->request(id) || rowOfRequest(id) >= 0)
            continue;
        m_tabs.append({m_nextId++, id, id == preview});
    }
    m_current = m_tabs.isEmpty() ? -1 : std::max(0, rowOfRequest(current));
    endResetModel();
    if (!m_showTabs)
        reduceToPreview();
    emit countChanged();
    emit currentChanged();
}

/*!
 * \brief Closes every tab.
 */
void TabsModel::clear()
{
    if (m_tabs.isEmpty())
        return;
    const QList<Tab> closed = m_tabs;
    beginResetModel();
    m_tabs.clear();
    m_busy.clear();
    m_current = -1;
    endResetModel();
    for (const Tab &tab : closed)
        emit tabClosed(tab.id);
    emit countChanged();
    emit currentChanged();
}

/*!
 * \internal
 * \brief Keeps only the current tab, as the preview tab.
 */
void TabsModel::reduceToPreview()
{
    if (m_current < 0)
        return;
    closeOthers(m_current);
    if (!m_tabs.at(0).preview) {
        m_tabs[0].preview = true;
        emit dataChanged(index(0), index(0), {PreviewRole});
    }
}

/*!
 * \internal
 * \brief Inserts a tab for \a requestId after the current one and makes it current.
 *
 * Beyond kMaxTabs the first tab that is neither current nor running closes.
 */
void TabsModel::insertTab(const QString &requestId, bool preview)
{
    if (m_tabs.size() >= kMaxTabs) {
        for (int row = 0; row < m_tabs.size(); ++row) {
            if (row != m_current && !m_busy.contains(m_tabs.at(row).id)) {
                removeTab(row, m_current > row ? m_current - 1 : m_current);
                break;
            }
        }
    }
    const int row = m_current < 0 ? int(m_tabs.size()) : m_current + 1;
    beginInsertRows({}, row, row);
    m_tabs.insert(row, {m_nextId++, requestId, preview});
    m_current = row;
    endInsertRows();
    emit countChanged();
    emit currentChanged();
}

/*!
 * \internal
 * \brief Removes the tab at \a row, makes \a nextCurrent the current row, and announces it.
 *
 * The current row is set before the removal is announced, so slots of
 * tabClosed() and countChanged(), saving among them, see a consistent model.
 */
void TabsModel::removeTab(int row, int nextCurrent)
{
    const int id = m_tabs.at(row).id;
    beginRemoveRows({}, row, row);
    m_tabs.removeAt(row);
    m_current = nextCurrent;
    endRemoveRows();
    m_busy.remove(id);
    emit tabClosed(id);
    emit countChanged();
}

/*!
 * \internal
 * \brief Closes tabs of requests that left the collection and refreshes the rest.
 */
void TabsModel::followCollection()
{
    const int oldId = currentTabId();
    const int oldRow = m_current;
    for (int row = int(m_tabs.size()) - 1; row >= 0; --row) {
        if (m_collection->request(m_tabs.at(row).requestId))
            continue;
        int next = m_current;
        if (row == m_current)
            next = m_tabs.size() == 1 ? -1 : std::min(row, int(m_tabs.size()) - 2);
        else if (m_current > row)
            next = m_current - 1;
        removeTab(row, next);
    }
    if (!m_tabs.isEmpty())
        emit dataChanged(index(0), index(int(m_tabs.size()) - 1), {NameRole, MethodRole});
    if (currentTabId() != oldId || m_current != oldRow)
        emit currentChanged();
}

/*!
 * \internal
 * \brief Returns the row of the first tab of \a requestId, or -1.
 */
int TabsModel::rowOfRequest(const QString &requestId) const
{
    for (int row = 0; row < m_tabs.size(); ++row) {
        if (m_tabs.at(row).requestId == requestId)
            return row;
    }
    return -1;
}
