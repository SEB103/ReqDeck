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
    connect(this, &TabsModel::countChanged, this, &TabsModel::save);
    connect(this, &TabsModel::currentChanged, this, &TabsModel::save);
    connect(this, &QAbstractItemModel::dataChanged, this, &TabsModel::save);
    connect(this, &QAbstractItemModel::rowsMoved, this, &TabsModel::save);
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
    // Slots of tabClosed() run while a removal has not chosen the new current tab.
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
 * tab shows it, or a new preview tab opens after the current one.
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
        if (m_tabs.at(row).preview && !m_busy.contains(m_tabs.at(row).id)) {
            m_tabs[row].requestId = requestId;
            emit dataChanged(index(row), index(row));
            m_current = row;
            emit currentChanged();
            return;
        }
    }
    insertTab(requestId, true);
}

/*!
 * \brief Shows \a requestId in a tab that stays, as a double click does.
 * \param requestId The request; a folder or an unknown id is ignored.
 */
void TabsModel::open(const QString &requestId)
{
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
 * Called when the request is edited, so an edit is never replaced by the
 * next preview.
 */
void TabsModel::keepCurrent()
{
    if (m_current < 0 || !m_tabs.at(m_current).preview)
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
    const bool wasCurrent = row == m_current;
    const int before = m_current;
    removeTab(row);
    if (wasCurrent)
        m_current = m_tabs.isEmpty() ? -1 : std::min(row, int(m_tabs.size()) - 1);
    else if (before > row)
        m_current = before - 1;
    if (wasCurrent)
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
    const bool changed = row != m_current;
    for (int i = int(m_tabs.size()) - 1; i >= 0; --i) {
        if (m_tabs.at(i).id != keep)
            removeTab(i);
    }
    m_current = 0;
    if (changed)
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
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_tabs.move(from, to);
    endMoveRows();
    m_current = rowOfTab(currentId);
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
 * A running tab is never replaced by a preview or closed by the tab limit.
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
 * without a match the first tab is current.
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
    endResetModel();
    m_current = m_tabs.isEmpty() ? -1 : std::max(0, rowOfRequest(current));
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
    endResetModel();
    m_current = -1;
    for (const Tab &tab : closed)
        emit tabClosed(tab.id);
    emit countChanged();
    emit currentChanged();
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
                removeTab(row);
                if (m_current > row)
                    --m_current;
                break;
            }
        }
    }
    const int row = m_current < 0 ? int(m_tabs.size()) : m_current + 1;
    beginInsertRows({}, row, row);
    m_tabs.insert(row, {m_nextId++, requestId, preview});
    endInsertRows();
    m_current = row;
    emit countChanged();
    emit currentChanged();
}

/*!
 * \internal
 * \brief Removes the tab at \a row and announces it, without choosing a current tab.
 */
void TabsModel::removeTab(int row)
{
    const int id = m_tabs.at(row).id;
    beginRemoveRows({}, row, row);
    m_tabs.removeAt(row);
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
    const QString currentRequest = currentRequestId();
    bool currentClosed = false;
    for (int row = int(m_tabs.size()) - 1; row >= 0; --row) {
        if (m_collection->request(m_tabs.at(row).requestId))
            continue;
        if (row == m_current)
            currentClosed = true;
        removeTab(row);
        if (m_current > row)
            --m_current;
    }
    if (currentClosed || m_current >= m_tabs.size()) {
        m_current = m_tabs.isEmpty() ? -1 : std::min(std::max(m_current, 0), int(m_tabs.size()) - 1);
        emit currentChanged();
    }
    if (!m_tabs.isEmpty())
        emit dataChanged(index(0), index(int(m_tabs.size()) - 1), {NameRole, MethodRole});
    if (!currentClosed && currentRequestId() != currentRequest)
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
