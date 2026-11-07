// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/requesteditor.h"

#include <QClipboard>
#include <QGuiApplication>

#include "core/http/curlconverter.h"
#include "core/http/httpexecutor.h"
#include "core/http/variableresolver.h"
#include "core/workspace/collectiontree.h"
#include "core/workspace/workspacedata.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/responseview.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \brief Creates an empty editor.
 * \param manager The workspace manager, source of the active environment.
 * \param collection The collection holding the requests.
 * \param response The view that shows the state and result of sending.
 * \param parent The QObject parent.
 */
RequestEditor::RequestEditor(WorkspaceManager *manager, CollectionTreeModel *collection,
                             ResponseView *response, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_collection(collection)
    , m_response(response)
    , m_executor(new HttpExecutor(this))
    , m_params(new KeyValueModel(this))
    , m_headers(new KeyValueModel(this))
    , m_form(new KeyValueModel(this))
{
    connect(m_collection, &CollectionTreeModel::collectionChanged, this,
            &RequestEditor::syncWithCollection);
    connect(m_params, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_headers, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_form, &KeyValueModel::edited, this, &RequestEditor::storeTables);

    connect(m_manager, &WorkspaceManager::workspaceReplaced, this, &RequestEditor::dropResponse);
    connect(m_executor, &HttpExecutor::finished, this, &RequestEditor::showResponse);
    connect(m_executor, &HttpExecutor::busyChanged, this, [this]() {
        emit busyChanged();
        emit canSendChanged();
    });
}

/*!
 * \brief Sets the defaults requests are built with.
 * \param options User-Agent, default timeout, and redirect handling.
 */
void RequestEditor::setBuilderOptions(const RequestBuilder::Options &options)
{
    m_builderOptions = options;
}

bool RequestEditor::hasRequest() const
{
    return !m_requestId.isEmpty();
}

bool RequestEditor::busy() const
{
    return m_executor->isBusy();
}

bool RequestEditor::canSend() const
{
    return hasRequest() && !busy();
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
 * \brief Sends the open request.
 *
 * The working copy is built with the variables of the active environment. A
 * request that cannot be built is reported to the response view and not sent.
 * The response goes to the view even when another request was opened meanwhile;
 * the view names the request it belongs to.
 */
void RequestEditor::send()
{
    if (!canSend())
        return;

    const VariableResolver variables =
        VariableResolver::fromKeyValues(activeEnvironmentVariables(m_manager->workspaceData()));
    const BuiltRequest built = RequestBuilder::build(m_request, variables, m_builderOptions);

    m_response->setBusy(m_name, m_requestId);
    if (!built.ok) {
        m_response->setBuildError(built.errorString, built.unresolvedVariables);
        return;
    }
    m_unresolved = built.unresolvedVariables;
    m_sentRequest = m_request;
    m_executor->send(built);
}

/*!
 * \brief Cancels the running request, which finishes as aborted.
 */
void RequestEditor::cancel()
{
    m_executor->abort();
}

/*!
 * \brief Copies the open request to the clipboard as a cURL command.
 *
 * Variables stay unresolved, so no secret value of an environment gets out.
 * Credentials typed into the request itself are part of the command.
 */
void RequestEditor::copyAsCurl()
{
    if (!hasRequest())
        return;
    QGuiApplication::clipboard()->setText(CurlConverter::toCurl(m_request));
    emit copiedToClipboard();
}

/*!
 * \brief Checks a cURL \a command.
 * \param command The command.
 * \return A map with \c ok, \c error, \c summary (the name the request would get),
 * and \c ignored (the options without an equivalent).
 */
QVariantMap RequestEditor::checkCurl(const QString &command) const
{
    const CurlConverter::ParseResult result = CurlConverter::parse(command);
    return {{QStringLiteral("ok"), result.ok},
            {QStringLiteral("error"), result.errorString},
            {QStringLiteral("summary"), result.ok ? requestDisplayName(result.request) : QString()},
            {QStringLiteral("ignored"), result.ignoredOptions}};
}

/*!
 * \brief Adds a request made from a cURL \a command.
 * \param command The command.
 * \param parentId The folder, or an empty string for the top level.
 * \return The id of the request, or an empty string when nothing was added.
 */
QString RequestEditor::importCurl(const QString &command, const QString &parentId)
{
    const CurlConverter::ParseResult result = CurlConverter::parse(command);
    if (!result.ok)
        return {};
    return m_collection->addRequestFrom(parentId, result.request,
                                        requestDisplayName(result.request));
}

/*!
 * \internal
 * \brief Passes the result of the running request to the response view.
 */
void RequestEditor::showResponse(const ResponseData &response)
{
    m_response->setResponse(response, m_unresolved);
    emit requestFinished(m_sentRequest, response);
}

/*!
 * \internal
 * \brief Ends a running request and removes the response of the old workspace.
 *
 * A save also reports a changed workspace, so this listens only to the signal
 * for a real change of workspace.
 */
void RequestEditor::dropResponse()
{
    m_executor->abort();
    m_response->clear();
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
    emit canSendChanged();
}
