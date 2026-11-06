// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/environmentmodel.h"

#include <QQuickTextDocument>

#include <algorithm>

#include "core/http/variableresolver.h"
#include "qmlapi/environmentvariablesmodel.h"
#include "qmlapi/variablehighlighter.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \brief Creates a model over the active workspace of \a manager.
 *
 * WorkspaceManager reports every save as a change of the active workspace too,
 * so the model resets only on workspaceReplaced(). A save can still fill in
 * secret values that could not be read earlier, which is why the highlight is
 * refreshed on both.
 * \param manager The workspace manager.
 * \param parent The QObject parent.
 */
EnvironmentModel::EnvironmentModel(WorkspaceManager *manager, QObject *parent)
    : QAbstractListModel(parent)
    , m_manager(manager)
    , m_variables(new EnvironmentVariablesModel(this))
    , m_highlighter(new VariableHighlighter(this))
{
    connect(m_manager, &WorkspaceManager::workspaceReplaced, this,
            &EnvironmentModel::resetFromWorkspace);
    connect(m_manager, &WorkspaceManager::activeWorkspaceChanged, this,
            &EnvironmentModel::refreshHighlighter);
    connect(m_variables, &EnvironmentVariablesModel::edited, this,
            &EnvironmentModel::storeVariables);
    refreshHighlighter();
}

/*!
 * \brief Returns the number of environments.
 */
int EnvironmentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(environments().size());
}

/*!
 * \brief Returns the value of \a role for \a index.
 */
QVariant EnvironmentModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const Environment &environment = environments().at(index.row());
    switch (role) {
    case IdRole:
        return environment.id;
    case NameRole:
        return environment.name;
    case ActiveRole:
        return index.row() == activeIndex();
    default:
        return {};
    }
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> EnvironmentModel::roleNames() const
{
    return {{IdRole, QByteArrayLiteral("environmentId")},
            {NameRole, QByteArrayLiteral("name")},
            {ActiveRole, QByteArrayLiteral("active")}};
}

/*!
 * \brief Returns the names of the environments.
 */
QStringList EnvironmentModel::names() const
{
    QStringList names;
    names.reserve(environments().size());
    for (const Environment &environment : environments())
        names.append(environment.name);
    return names;
}

/*!
 * \brief Returns the row of the active environment.
 * \return The row, or -1 when none is active or the workspace names an unknown one.
 */
int EnvironmentModel::activeIndex() const
{
    const QString &id = m_manager->workspaceData().activeEnvironmentId;
    if (id.isEmpty())
        return -1;
    const QList<Environment> &rows = environments();
    for (int row = 0; row < rows.size(); ++row) {
        if (rows.at(row).id == id)
            return row;
    }
    return -1;
}

/*!
 * \brief Activates the environment at \a index.
 * \param index The row; -1 activates none, and other values are ignored.
 */
void EnvironmentModel::setActiveIndex(int index)
{
    const int previous = activeIndex();
    if (index == previous || index < -1 || index >= rowCount())
        return;

    WorkspaceData data = m_manager->workspaceData();
    data.activeEnvironmentId = index < 0 ? QString() : environments().at(index).id;
    m_manager->updateWorkspaceData(data);

    for (const int row : {previous, index}) {
        if (row >= 0)
            emit dataChanged(this->index(row), this->index(row), {ActiveRole});
    }
    emit activeChanged();
    refreshHighlighter();
}

/*!
 * \brief Returns the row shown in the editor, or -1.
 */
int EnvironmentModel::editedRow() const
{
    return m_editedRow;
}

/*!
 * \brief Shows the environment at \a row in the editor.
 * \param row The row; -1 shows none, and other values are ignored.
 */
void EnvironmentModel::setEditedRow(int row)
{
    if (row != m_editedRow && row >= -1 && row < rowCount())
        showInEditor(row);
}

/*!
 * \brief Returns the name of the environment shown in the editor.
 */
QString EnvironmentModel::editedName() const
{
    return m_editedRow < 0 ? QString() : environments().at(m_editedRow).name;
}

/*!
 * \brief Returns the variables of the environment shown in the editor.
 */
EnvironmentVariablesModel *EnvironmentModel::variablesModel() const
{
    return m_variables;
}

/*!
 * \brief Returns whether the URL highlight uses the dark palette.
 */
bool EnvironmentModel::darkTheme() const
{
    return m_darkTheme;
}

/*!
 * \brief Selects the palette of the URL highlight.
 * \param dark True for the dark palette.
 */
void EnvironmentModel::setDarkTheme(bool dark)
{
    if (m_darkTheme == dark)
        return;
    m_darkTheme = dark;
    m_highlighter->setDark(dark);
    emit darkThemeChanged();
}

/*!
 * \brief Returns the highlighter of the URL field.
 */
VariableHighlighter *EnvironmentModel::urlHighlighter() const
{
    return m_highlighter;
}

/*!
 * \brief Appends a new environment and shows it in the editor.
 * \return Its row, or -1 without a workspace.
 */
int EnvironmentModel::addEnvironment()
{
    Environment environment;
    environment.id = newItemId();
    environment.name = tr("New environment");
    return appendEnvironment(environment);
}

/*!
 * \brief Appends a copy of \a row and shows it in the editor.
 * \param row The row to copy.
 * \return The new row, or -1 for an invalid row.
 */
int EnvironmentModel::duplicateEnvironment(int row)
{
    if (row < 0 || row >= rowCount())
        return -1;

    Environment copy = environments().at(row);
    copy.id = newItemId();
    copy.name = tr("%1 copy").arg(copy.name);
    return appendEnvironment(copy);
}

/*!
 * \brief Renames \a row to the trimmed \a name.
 * \param row The row.
 * \param name The new name.
 * \return False for a blank name or an invalid row.
 */
bool EnvironmentModel::renameEnvironment(int row, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (row < 0 || row >= rowCount() || trimmed.isEmpty())
        return false;
    if (environments().at(row).name == trimmed)
        return true;

    WorkspaceData data = m_manager->workspaceData();
    data.environments[row].name = trimmed;
    m_manager->updateWorkspaceData(data);

    emit dataChanged(index(row), index(row), {NameRole});
    emit namesChanged();
    if (row == m_editedRow)
        emit editedNameChanged();
    return true;
}

/*!
 * \brief Removes \a row.
 *
 * Removing the active environment leaves none active. The editor moves to the
 * row that took its place, or to the last row. A row after the removed one keeps
 * showing the same environment.
 * \param row The row; an invalid number is ignored.
 */
void EnvironmentModel::removeEnvironment(int row)
{
    if (row < 0 || row >= rowCount())
        return;

    const int previousActive = activeIndex();
    const int previousEdited = m_editedRow;
    WorkspaceData data = m_manager->workspaceData();

    // The edited row moves before any signal, so a listener never reads past the end.
    beginRemoveRows({}, row, row);
    if (row == previousActive)
        data.activeEnvironmentId.clear();
    data.environments.removeAt(row);
    m_manager->updateWorkspaceData(data);
    if (row < m_editedRow)
        --m_editedRow;
    else if (row == m_editedRow)
        m_editedRow = std::min(row, rowCount() - 1);
    endRemoveRows();

    emit namesChanged();
    if (activeIndex() != previousActive)
        emit activeChanged();

    if (row == previousEdited)
        showInEditor(m_editedRow);
    else if (m_editedRow != previousEdited)
        emit editedRowChanged();
    refreshHighlighter();
}

/*!
 * \brief Colors the URL held by \a document.
 *
 * The highlighter serves one field. A later call moves it to the new document.
 * \param document The text document of the field.
 */
void EnvironmentModel::attachUrlHighlighter(QQuickTextDocument *document)
{
    m_highlighter->setDocument(document ? document->textDocument() : nullptr);
}

/*!
 * \internal
 * \brief Returns the environments of the workspace.
 */
const QList<Environment> &EnvironmentModel::environments() const
{
    return m_manager->workspaceData().environments;
}

/*!
 * \internal
 * \brief Appends \a environment and shows it in the editor.
 * \return Its row, or -1 without a workspace.
 */
int EnvironmentModel::appendEnvironment(const Environment &environment)
{
    if (!m_manager->hasActiveWorkspace())
        return -1;

    const int row = rowCount();
    WorkspaceData data = m_manager->workspaceData();
    data.environments.append(environment);

    beginInsertRows({}, row, row);
    m_manager->updateWorkspaceData(data);
    endInsertRows();

    emit namesChanged();
    showInEditor(row);
    return row;
}

/*!
 * \internal
 * \brief Rebuilds everything after another workspace became active.
 */
void EnvironmentModel::resetFromWorkspace()
{
    beginResetModel();
    endResetModel();

    emit namesChanged();
    emit activeChanged();
    showInEditor(-1);
    refreshHighlighter();
}

/*!
 * \internal
 * \brief Writes the edited table to its environment.
 */
void EnvironmentModel::storeVariables()
{
    if (m_editedRow < 0)
        return;

    WorkspaceData data = m_manager->workspaceData();
    data.environments[m_editedRow].variables = m_variables->rows();
    m_manager->updateWorkspaceData(data);

    if (m_editedRow == activeIndex())
        refreshHighlighter();
}

/*!
 * \internal
 * \brief Loads \a row into the editor, also when it is the row already shown.
 * \param row The row, or -1.
 */
void EnvironmentModel::showInEditor(int row)
{
    m_editedRow = row;
    m_variables->setRows(row < 0 ? QList<EnvironmentVariable>() : environments().at(row).variables);
    emit editedRowChanged();
    emit editedNameChanged();
}

/*!
 * \internal
 * \brief Passes the active variables to the URL highlight.
 */
void EnvironmentModel::refreshHighlighter()
{
    m_highlighter->setVariables(
        VariableResolver::fromKeyValues(activeEnvironmentVariables(m_manager->workspaceData())));
}
