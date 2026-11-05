// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/requesteditor.h"

#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/keyvaluemodel.h"

/*!
 * \brief Creates an empty editor over \a collection.
 * \param collection The collection holding the requests.
 * \param parent The QObject parent.
 */
RequestEditor::RequestEditor(CollectionTreeModel *collection, QObject *parent)
    : QObject(parent)
    , m_collection(collection)
    , m_params(new KeyValueModel(this))
    , m_headers(new KeyValueModel(this))
    , m_form(new KeyValueModel(this))
{
    connect(m_collection, &CollectionTreeModel::collectionChanged, this,
            &RequestEditor::syncWithCollection);
    connect(m_params, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_headers, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_form, &KeyValueModel::edited, this, &RequestEditor::storeTables);
}

bool RequestEditor::hasRequest() const
{
    return !m_requestId.isEmpty();
}

QString RequestEditor::requestId() const
{
    return m_requestId;
}

QString RequestEditor::name() const
{
    return m_name;
}

/*!
 * \brief Renames the request.
 *
 * The collection trims the name and refuses a blank one. The signal is emitted
 * in that case too, so that the name field shows the valid name again.
 * \param name The new name.
 */
void RequestEditor::setName(const QString &name)
{
    if (!hasRequest())
        return;
    if (!m_collection->rename(m_requestId, name))
        emit requestChanged();
}

QString RequestEditor::method() const
{
    return QString::fromLatin1(httpMethodVerb(m_request.method));
}

void RequestEditor::setMethod(const QString &method)
{
    if (const std::optional<HttpMethod> parsed = httpMethodFromString(method))
        updateField(m_request.method, *parsed);
}

QString RequestEditor::url() const
{
    return m_request.url;
}

void RequestEditor::setUrl(const QString &url)
{
    updateField(m_request.url, url);
}

int RequestEditor::bodyMode() const
{
    return int(m_request.body.mode);
}

void RequestEditor::setBodyMode(int mode)
{
    if (mode >= int(BodyMode::None) && mode <= int(BodyMode::FormUrlEncoded))
        updateField(m_request.body.mode, BodyMode(mode));
}

int RequestEditor::rawBodyType() const
{
    return int(m_request.body.rawType);
}

void RequestEditor::setRawBodyType(int type)
{
    if (type >= int(RawBodyType::Json) && type <= int(RawBodyType::Xml))
        updateField(m_request.body.rawType, RawBodyType(type));
}

QString RequestEditor::rawBody() const
{
    return m_request.body.raw;
}

void RequestEditor::setRawBody(const QString &text)
{
    updateField(m_request.body.raw, text);
}

int RequestEditor::authType() const
{
    return int(m_request.auth.type);
}

void RequestEditor::setAuthType(int type)
{
    if (type >= int(AuthType::None) && type <= int(AuthType::ApiKey))
        updateField(m_request.auth.type, AuthType(type));
}

QString RequestEditor::authUsername() const
{
    return m_request.auth.username;
}

void RequestEditor::setAuthUsername(const QString &username)
{
    updateField(m_request.auth.username, username);
}

QString RequestEditor::authPassword() const
{
    return m_request.auth.password;
}

void RequestEditor::setAuthPassword(const QString &password)
{
    updateField(m_request.auth.password, password);
}

QString RequestEditor::authToken() const
{
    return m_request.auth.token;
}

void RequestEditor::setAuthToken(const QString &token)
{
    updateField(m_request.auth.token, token);
}

QString RequestEditor::apiKeyName() const
{
    return m_request.auth.apiKeyName;
}

void RequestEditor::setApiKeyName(const QString &name)
{
    updateField(m_request.auth.apiKeyName, name);
}

QString RequestEditor::apiKeyValue() const
{
    return m_request.auth.apiKeyValue;
}

void RequestEditor::setApiKeyValue(const QString &value)
{
    updateField(m_request.auth.apiKeyValue, value);
}

bool RequestEditor::apiKeyInQuery() const
{
    return m_request.auth.apiKeyLocation == ApiKeyLocation::Query;
}

void RequestEditor::setApiKeyInQuery(bool inQuery)
{
    updateField(m_request.auth.apiKeyLocation,
                inQuery ? ApiKeyLocation::Query : ApiKeyLocation::Header);
}

int RequestEditor::timeoutMs() const
{
    return m_request.timeoutMs;
}

void RequestEditor::setTimeoutMs(int timeoutMs)
{
    if (timeoutMs >= 0)
        updateField(m_request.timeoutMs, timeoutMs);
}

KeyValueModel *RequestEditor::paramsModel() const
{
    return m_params;
}

KeyValueModel *RequestEditor::headersModel() const
{
    return m_headers;
}

KeyValueModel *RequestEditor::formModel() const
{
    return m_form;
}

/*!
 * \brief Opens the request \a id.
 *
 * This only reads, so showing a request never marks the workspace dirty.
 * \param id The node id; a folder or an unknown id leaves the editor empty.
 */
void RequestEditor::load(const QString &id)
{
    const RequestSpec *request = m_collection->request(id);
    if (!request) {
        clear();
        return;
    }

    m_requestId = id;
    m_name = m_collection->indexOfNode(id).data(CollectionTreeModel::NameRole).toString();
    m_request = *request;
    showRequest();
}

/*!
 * \brief Closes the open request.
 */
void RequestEditor::clear()
{
    m_requestId.clear();
    m_name.clear();
    m_request = RequestSpec();
    showRequest();
}

/*!
 * \internal
 * \brief Assigns \a value to \a field of the open request and stores the request.
 *
 * Without an open request the field stays as it is, so a late edit from QML
 * cannot change anything after the request was closed.
 */
template<typename T>
void RequestEditor::updateField(T &field, const T &value)
{
    if (!hasRequest() || field == value)
        return;
    field = value;
    store();
    emit requestChanged();
}

/*!
 * \internal
 * \brief Stores the working copy in the collection.
 */
void RequestEditor::store()
{
    m_collection->updateRequest(m_requestId, m_request);
}

/*!
 * \internal
 * \brief Puts the rows of the table models into the working copy and stores it.
 */
void RequestEditor::storeTables()
{
    if (!hasRequest())
        return;
    m_request.queryParams = m_params->rows();
    m_request.headers = m_headers->rows();
    m_request.body.form = m_form->rows();
    store();
}

/*!
 * \internal
 * \brief Follows changes of the collection made outside the editor.
 *
 * The open request may have been removed, also together with its folder or by
 * a workspace switch, renamed in the tree, or replaced by another workspace
 * that holds the same id.
 */
void RequestEditor::syncWithCollection()
{
    if (!hasRequest())
        return;

    const RequestSpec *stored = m_collection->request(m_requestId);
    if (!stored) {
        clear();
        return;
    }

    const QString name =
        m_collection->indexOfNode(m_requestId).data(CollectionTreeModel::NameRole).toString();
    if (*stored != m_request) {
        load(m_requestId);
    } else if (name != m_name) {
        m_name = name;
        emit requestChanged();
    }
}

/*!
 * \internal
 * \brief Shows the working copy in the table models and notifies QML.
 */
void RequestEditor::showRequest()
{
    m_params->setRows(m_request.queryParams);
    m_headers->setRows(m_request.headers);
    m_form->setRows(m_request.body.form);
    emit requestChanged();
}
