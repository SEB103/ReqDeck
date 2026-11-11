// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/foldereditor.h"

#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/keyvaluemodel.h"

/*!
    \class FolderEditor
    \brief Edits the auth and variables of a folder, which its requests inherit.

    \internal
*/

/*!
 * \brief Creates an editor over the folders of \a collection.
 *
 * Another workspace or a removed folder empties the editor, so it never writes
 * into a folder the user no longer sees.
 * \param collection The collection model.
 * \param parent The QObject parent.
 */
FolderEditor::FolderEditor(CollectionTreeModel *collection, QObject *parent)
    : QObject(parent)
    , m_collection(collection)
    , m_variables(new KeyValueModel(this))
{
    connect(m_variables, &KeyValueModel::edited, this, [this] {
        m_settings.variables = m_variables->rows();
        store();
    });
    connect(m_collection, &CollectionTreeModel::collectionChanged, this, [this] {
        if (!m_id.isEmpty() && !m_collection->folderSettings(m_id))
            load(QString());
    });
}

QString FolderEditor::folderId() const
{
    return m_id;
}

QString FolderEditor::name() const
{
    return m_id.isEmpty() ? QString() : m_collection->nodeName(m_id);
}

int FolderEditor::authType() const
{
    return int(m_settings.auth.type);
}

void FolderEditor::setAuthType(int type)
{
    if (type >= int(AuthType::None) && type <= int(AuthType::Inherit))
        updateField(m_settings.auth.type, AuthType(type));
}

QString FolderEditor::authUsername() const
{
    return m_settings.auth.username;
}

void FolderEditor::setAuthUsername(const QString &username)
{
    updateField(m_settings.auth.username, username);
}

QString FolderEditor::authPassword() const
{
    return m_settings.auth.password;
}

void FolderEditor::setAuthPassword(const QString &password)
{
    updateField(m_settings.auth.password, password);
}

QString FolderEditor::authToken() const
{
    return m_settings.auth.token;
}

void FolderEditor::setAuthToken(const QString &token)
{
    updateField(m_settings.auth.token, token);
}

QString FolderEditor::apiKeyName() const
{
    return m_settings.auth.apiKeyName;
}

void FolderEditor::setApiKeyName(const QString &name)
{
    updateField(m_settings.auth.apiKeyName, name);
}

QString FolderEditor::apiKeyValue() const
{
    return m_settings.auth.apiKeyValue;
}

void FolderEditor::setApiKeyValue(const QString &value)
{
    updateField(m_settings.auth.apiKeyValue, value);
}

bool FolderEditor::apiKeyInQuery() const
{
    return m_settings.auth.apiKeyLocation == ApiKeyLocation::Query;
}

void FolderEditor::setApiKeyInQuery(bool inQuery)
{
    updateField(m_settings.auth.apiKeyLocation, inQuery ? ApiKeyLocation::Query : ApiKeyLocation::Header);
}

KeyValueModel *FolderEditor::variablesModel() const
{
    return m_variables;
}

/*!
 * \brief Shows the folder \a id.
 *
 * This only reads, so opening the settings never marks the workspace dirty.
 * \param id The folder; a request or an unknown id leaves the editor empty.
 */
void FolderEditor::load(const QString &id)
{
    const FolderSettings *settings = id.isEmpty() ? nullptr : m_collection->folderSettings(id);
    m_id = settings ? id : QString();
    m_settings = settings ? *settings : FolderSettings();
    m_variables->setRows(m_settings.variables);
    emit folderChanged();
}

/*!
 * \internal
 * \brief Sets \a field to \a value and stores the settings when it changed.
 */
template<typename T>
void FolderEditor::updateField(T &field, const T &value)
{
    if (m_id.isEmpty() || field == value)
        return;
    field = value;
    store();
    emit folderChanged();
}

/*!
 * \internal
 * \brief Writes the working copy into the folder of the collection.
 */
void FolderEditor::store()
{
    if (!m_id.isEmpty())
        m_collection->setFolderSettings(m_id, m_settings);
}
