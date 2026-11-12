// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/requesteditor.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QUrl>

#include "core/http/oauthfields.h"
#include "core/http/curlconverter.h"
#include "core/http/httpexecutor.h"
#include "core/http/oauthclient.h"
#include "core/http/oauthtokenstore.h"
#include "core/http/variableresolver.h"
#include "core/workspace/collectiontree.h"
#include "core/workspace/inheritance.h"
#include "core/workspace/workspacedata.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/formpartmodel.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/responseview.h"
#include "qmlapi/tabsmodel.h"
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
    , m_params(new KeyValueModel(this))
    , m_headers(new KeyValueModel(this))
    , m_form(new KeyValueModel(this))
    , m_parts(new FormPartModel(this))
    , m_oauth(new OAuthClient(this))
{
    connect(m_oauth, &OAuthClient::finished, this, &RequestEditor::finishOAuth);
    connect(m_manager->tokenStore(), &OAuthTokenStore::changed, this, &RequestEditor::oauthChanged);
    connect(this, &RequestEditor::requestChanged, this, &RequestEditor::oauthChanged);
    connect(m_collection, &CollectionTreeModel::collectionChanged, this,
            &RequestEditor::syncWithCollection);
    connect(m_params, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_headers, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_form, &KeyValueModel::edited, this, &RequestEditor::storeTables);
    connect(m_parts, &FormPartModel::edited, this, &RequestEditor::storeTables);

    connect(m_manager, &WorkspaceManager::workspaceReplaced, this, &RequestEditor::dropResponse);
}

/*!
 * \brief Follows the current tab of \a tabs.
 *
 * Each tab then has its own send: requests of different tabs run at once,
 * and the response view shows the current tab's last result. load() of a
 * request goes through the preview tab. Without tabs there is one send, as
 * before.
 * \param tabs The tabs of the workspace; not owned.
 */
void RequestEditor::setTabs(TabsModel *tabs)
{
    m_tabs = tabs;
    connect(m_tabs, &TabsModel::currentChanged, this, &RequestEditor::showCurrentTab);
    connect(m_tabs, &TabsModel::tabClosed, this, &RequestEditor::dropTab);
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
    const auto it = m_sends.constFind(currentKey());
    return it != m_sends.constEnd() && it->executor->isBusy();
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
    if (mode >= int(BodyMode::None) && mode <= int(BodyMode::File))
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

/*!
 * \brief Returns the parts of a multipart/form-data body.
 */
FormPartModel *RequestEditor::partsModel() const
{
    return m_parts;
}

/*!
 * \brief Returns the path of the file sent as the body.
 */
QString RequestEditor::bodyFilePath() const
{
    return m_request.body.filePath;
}

/*!
 * \brief Sets the path of the file sent as the body.
 * \param pathOrUrl A path, a path with {{variables}}, or a file URL from a file dialog.
 */
void RequestEditor::setBodyFilePath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    updateField(m_request.body.filePath, url.isLocalFile() ? url.toLocalFile() : pathOrUrl);
}

/*!
 * \brief Returns the size of the body file, or why it cannot be sent.
 *
 * A path with {{variables}} is checked only when sending, since its file
 * depends on the environment.
 */
QString RequestEditor::bodyFileInfo() const
{
    const QString path = m_request.body.filePath.trimmed();
    if (path.isEmpty())
        return tr("No file chosen.");
    if (path.contains(QLatin1StringView("{{")))
        return QString();
    const QFileInfo info(path);
    if (!info.isFile())
        return tr("The file was not found.");
    return QLocale().formattedDataSize(info.size());
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
    if (type >= int(AuthType::None) && type <= int(AuthType::OAuth2))
        updateField(m_request.auth.type, AuthType(type));
}

/*!
 * \brief Returns the auth field of the current type that holds typed text.
 *
 * A field counts when something remains after removing every {{name}}
 * reference and surrounding spaces; the user name of Basic auth and the name of
 * an API key are no secrets and never count.
 * \return "password", "token", "apiKey", or an empty string.
 */
QString RequestEditor::literalCredential() const
{
    static const QRegularExpression reference(QStringLiteral("\\{\\{[^{}]*\\}\\}"));
    const auto literal = [](QString text) { return !text.remove(reference).trimmed().isEmpty(); };
    switch (m_request.auth.type) {
    case AuthType::Basic:
        return literal(m_request.auth.password) ? QStringLiteral("password") : QString();
    case AuthType::Bearer:
        return literal(m_request.auth.token) ? QStringLiteral("token") : QString();
    case AuthType::ApiKey:
        return literal(m_request.auth.apiKeyValue) ? QStringLiteral("apiKey") : QString();
    case AuthType::None:
    case AuthType::Inherit:
    case AuthType::OAuth2: // The client secret gets its own offer with the OAuth fields.
        break;
    }
    return {};
}

/*!
 * \internal
 * \brief Returns the open request as it is sent: with the auth its folders give it.
 *
 * The history keeps the request as edited, so an inherited token never reaches it.
 * \param variables Receives the variables to resolve with: those of the folders
 *        above, then the active environment, which wins.
 */
RequestSpec RequestEditor::effectiveRequest(QList<KeyValue> &variables) const
{
    const WorkspaceData &data = m_manager->workspaceData();
    const InheritedSettings inherited = inheritedSettings(data.collection, m_requestId, m_request.auth);
    RequestSpec request = m_request;
    request.auth = inherited.auth;
    variables = inherited.variables + activeEnvironmentVariables(data);
    return request;
}

/*!
 * \brief Returns the text of the field literalCredential() names, or an empty string.
 */
QString RequestEditor::literalCredentialValue() const
{
    const QString field = literalCredential();
    if (field == QLatin1StringView("password"))
        return m_request.auth.password;
    if (field == QLatin1StringView("token"))
        return m_request.auth.token;
    if (field == QLatin1StringView("apiKey"))
        return m_request.auth.apiKeyValue;
    return {};
}

/*!
 * \brief Replaces the typed credential with a reference to the variable \a key.
 *
 * The caller has stored the value in that variable first.
 * \param key The variable; an empty key changes nothing.
 */
void RequestEditor::useVariableForCredential(const QString &key)
{
    if (key.isEmpty())
        return;
    const QString reference = QStringLiteral("{{%1}}").arg(key);
    const QString field = literalCredential();
    if (field == QLatin1StringView("password"))
        setAuthPassword(reference);
    else if (field == QLatin1StringView("token"))
        setAuthToken(reference);
    else if (field == QLatin1StringView("apiKey"))
        setApiKeyValue(reference);
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

/*!
 * \brief Returns the OAuth 2.0 settings as a map for QML.
 */
QVariantMap RequestEditor::oauth2() const
{
    return oauthMap(m_request.auth.oauth2);
}

/*!
 * \brief Sets the OAuth 2.0 field \a name to \a value, as the auth fields do.
 */
void RequestEditor::setOAuthField(const QString &name, const QVariant &value)
{
    updateField(m_request.auth.oauth2, withOAuthField(m_request.auth.oauth2, name, value));
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
    // With tabs the request opens in the preview tab, which then loads it; a
    // folder or nothing leaves the current tab shown.
    if (m_tabs && m_collection->request(id)) {
        if (m_tabs->currentRequestId() != id)
            m_tabs->preview(id);
        else if (m_requestId != id)
            loadRequest(id);
        return;
    }
    if (m_tabs && m_tabs->rowCount() > 0)
        return;
    loadRequest(id);
}

/*!
 * \internal
 * \brief Loads the request \a id into the editor; a folder or unknown id leaves it empty.
 */
void RequestEditor::loadRequest(const QString &id)
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
    sendBuilt({});
}

/*!
 * \brief Returns the variables the open request references that do not resolve.
 *
 * The application asks for them before sending, so a request never leaves with
 * a literal {{name}} the user did not see.
 * \return The names in the order the builder meets them, each once.
 */
QStringList RequestEditor::missingVariables() const
{
    if (!hasRequest())
        return {};
    QList<KeyValue> rows;
    const RequestSpec request = effectiveRequest(rows);
    const VariableResolver variables = VariableResolver::fromKeyValues(rows);
    const BuiltRequest built = RequestBuilder::build(request, variables, m_builderOptions);
    QStringList names = built.unresolvedVariables;
    if (!built.ok) {
        // The builder stops at a URL it cannot use; the other fields are checked
        // with a stand-in address, so all names are asked for at once.
        RequestSpec rest = request;
        rest.url = QStringLiteral("http://placeholder.invalid/");
        names += RequestBuilder::build(rest, variables, m_builderOptions).unresolvedVariables;
    }
    names.removeDuplicates();
    return names;
}

/*!
 * \brief Sends the open request with \a values for variables it lacks.
 *
 * The values count for this send only; nothing is stored, and the response
 * shows no warning for them.
 * \param values Variable names mapped to their values.
 */
void RequestEditor::sendWithValues(const QVariantMap &values)
{
    QList<KeyValue> extra;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        extra.append({it.key(), it.value().toString(), true});
    sendBuilt({}, extra);
}

/*!
 * \brief Sends the open request again, trusting the certificate that failed.
 *
 * Offered after a response failed only its certificate check. The exception
 * covers the server of that check, its errors for the same certificate, and this
 * one send; nothing about it is stored. The response must belong to the open request, so that a click cannot
 * apply to a request the user did not see fail.
 */
void RequestEditor::sendTrustingCertificate()
{
    if (!m_response->canTrustCertificate() || m_response->requestId() != m_requestId)
        return;
    // The values entered for the failed send count for its repetition too.
    sendBuilt(m_response->certificateFailure(), sendOf(currentKey()).extra);
}

/*!
 * \internal
 * \brief Builds the open request and sends it.
 * \param tlsException A failed certificate check the request accepts; empty for none.
 */
void RequestEditor::sendBuilt(const TlsCheck &tlsException, const QList<KeyValue> &extraVariables)
{
    if (!canSend())
        return;

    // Copied first: a repetition passes the send's own values.
    const QList<KeyValue> extra = extraVariables;
    if (m_tabs)
        m_tabs->keepCurrent();
    TabSend &send = sendOf(currentKey());
    send.extra = extra;
    send.name = m_name;
    send.requestId = m_requestId;
    QList<KeyValue> rows;
    RequestSpec request = effectiveRequest(rows);
    const VariableResolver variables = VariableResolver::fromKeyValues(rows + extra);

    // OAuth 2.0: a usable token goes in as a header; otherwise one is fetched
    // first when that needs no user, and the send comes back here.
    if (request.auth.type == AuthType::OAuth2) {
        const OAuthContext oauth = oauthContext(extra);
        const std::optional<OAuthToken> token = m_manager->tokenStore()->token(oauth.key);
        if (token && token->isUsable()) {
            request.auth.type = AuthType::None;
            request.headers.append({QStringLiteral("Authorization"),
                                    (token->tokenType.isEmpty() ? QStringLiteral("Bearer") : token->tokenType) + u' '
                                        + token->accessToken,
                                    true});
        } else if (!m_oauth->isBusy()
                   && ((token && !token->refreshToken.isEmpty()) || oauth.settings.grant == OAuthGrant::ClientCredentials)) {
            m_response->setBusy(m_name, m_requestId);
            m_oauthKey = oauth.key;
            m_oauthThenSend = true;
            m_oauthSendKey = currentKey();
            m_oauthTls = tlsException;
            m_oauthExtra = extra;
            const bool started = token && !token->refreshToken.isEmpty()
                                     ? m_oauth->refresh(oauth.settings, token->refreshToken)
                                     : m_oauth->requestClientCredentials(oauth.settings);
            if (!started) {
                m_oauthThenSend = false;
                send.buildError = tr("The OAuth 2.0 token URL is not valid.");
                m_response->setBuildError(send.buildError, {});
            }
            emit oauthChanged();
            return;
        } else {
            m_response->setBusy(m_name, m_requestId);
            send.response.reset();
            send.buildError = m_oauth->isBusy() ? tr("A login is running; send again when it is done.")
                                                : tr("Log in first: press Get Token in the OAuth 2.0 settings.");
            m_response->setBuildError(send.buildError, {});
            return;
        }
    }

    BuiltRequest built = RequestBuilder::build(request, variables, m_builderOptions);
    built.tlsException = tlsException;

    m_response->setBusy(m_name, m_requestId);
    send.unresolved = built.unresolvedVariables;
    if (!built.ok) {
        send.response.reset();
        send.buildError = built.errorString;
        m_response->setBuildError(built.errorString, built.unresolvedVariables);
        return;
    }
    send.buildError.clear();
    send.sent = m_request;
    send.executor->setCookieStore(m_manager->cookiesEnabled() ? m_manager->cookieStore() : nullptr);
    send.executor->send(built);
}

/*!
 * \brief Cancels the running request, which finishes as aborted.
 */
void RequestEditor::cancel()
{
    const auto it = m_sends.constFind(currentKey());
    if (it != m_sends.constEnd())
        it->executor->abort();
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
    QList<KeyValue> rows;
    QGuiApplication::clipboard()->setText(CurlConverter::toCurl(effectiveRequest(rows), m_builderOptions.followRedirects,
                                                                 m_builderOptions.defaultTimeoutMs));
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
    // A skipped certificate check is a security decision, so the log keeps it.
    for (const TlsCheck &check : response.tlsChecks) {
        if (check.ignored) {
            qWarning().noquote() << QStringLiteral("The certificate of %1 was not checked: %2")
                                        .arg(check.server(), check.messages.join(QStringLiteral("; ")));
        }
    }
    // Without tabs the one send has key 0.
    finishSend(0, response);
}

/*!
 * \brief Returns whether an OAuth 2.0 login or token request is running.
 */
bool RequestEditor::oauthBusy() const
{
    return m_oauth->isBusy();
}

/*!
 * \brief Returns the state of the OAuth 2.0 token of the open request, for the user.
 *
 * Empty when the request does not use OAuth 2.0.
 */
QString RequestEditor::oauthStatus() const
{
    const OAuthContext oauth = oauthContext();
    if (!oauth.active)
        return {};
    if (m_oauth->isBusy())
        return tr("Waiting for the login…");
    if (!m_oauthError.isEmpty())
        return m_oauthError;
    const std::optional<OAuthToken> token = m_manager->tokenStore()->token(oauth.key);
    if (!token)
        return tr("No token yet.");
    if (token->isUsable()) {
        return token->expiresAt.isValid()
                   ? tr("Token valid until %1.").arg(QLocale().toString(token->expiresAt.toLocalTime().time(), QLocale::ShortFormat))
                   : tr("Token ready.");
    }
    return token->refreshToken.isEmpty() ? tr("The token expired; get a new one.")
                                         : tr("The token expired; it is renewed on the next send.");
}

/*!
 * \brief Gets an OAuth 2.0 token for the open request.
 *
 * Authorization Code opens the system browser at the login page and waits
 * for the callback; Client Credentials asks the token endpoint directly.
 */
void RequestEditor::getToken()
{
    const OAuthContext oauth = oauthContext();
    if (!oauth.active || m_oauth->isBusy())
        return;
    m_oauthError.clear();
    m_oauthKey = oauth.key;
    m_oauthThenSend = false;
    if (oauth.settings.grant == OAuthGrant::ClientCredentials) {
        if (!m_oauth->requestClientCredentials(oauth.settings))
            m_oauthError = tr("The OAuth 2.0 token URL is not valid.");
    } else {
        QString error;
        const QUrl url = m_oauth->startAuthorization(oauth.settings, &error);
        if (url.isEmpty())
            m_oauthError = error;
        else
            QDesktopServices::openUrl(url);
    }
    emit oauthChanged();
}

/*!
 * \brief Stops a running OAuth 2.0 login.
 */
void RequestEditor::cancelLogin()
{
    m_oauthThenSend = false;
    m_oauth->cancel();
}

/*!
 * \brief Removes the OAuth 2.0 tokens of the node whose settings the open request uses.
 */
void RequestEditor::resetToken()
{
    const OAuthContext oauth = oauthContext();
    if (!oauth.active)
        return;
    m_oauthError.clear();
    m_manager->tokenStore()->removeNode(oauth.owner);
    emit oauthChanged();
}

/*!
 * \internal
 * \brief Returns the OAuth context of the open request.
 *
 * The settings come from the request or the folder it inherits from, with
 * variables resolved as a send would resolve them, \a extra included.
 */
RequestEditor::OAuthContext RequestEditor::oauthContext(const QList<KeyValue> &extra) const
{
    OAuthContext context;
    if (!hasRequest())
        return context;
    const WorkspaceData &data = m_manager->workspaceData();
    const InheritedSettings inherited = inheritedSettings(data.collection, m_requestId, m_request.auth);
    if (inherited.auth.type != AuthType::OAuth2)
        return context;
    const VariableResolver variables =
        VariableResolver::fromKeyValues(inherited.variables + activeEnvironmentVariables(data) + extra);
    const OAuth2Settings &raw = inherited.auth.oauth2;
    context.active = true;
    context.owner = inherited.authOwnerId;
    context.settings = raw;
    for (QString *field : {&context.settings.authUrl, &context.settings.tokenUrl, &context.settings.clientId,
                           &context.settings.clientSecret, &context.settings.scope, &context.settings.audience})
        *field = variables.resolve(*field);
    context.key = OAuthTokenStore::keyFor(context.owner, context.settings);
    return context;
}

/*!
 * \internal
 * \brief Stores the token of \a result, then sends again or reports.
 *
 * A send that waited for the token goes out only when its tab is still the
 * current one; otherwise it is dropped, and the next send uses the token.
 */
void RequestEditor::finishOAuth(const OAuthResult &result)
{
    const bool thenSend = std::exchange(m_oauthThenSend, false);
    if (result.ok) {
        m_oauthError.clear();
        m_manager->tokenStore()->setToken(m_oauthKey, result.token);
    } else {
        m_oauthError = result.errorString;
    }
    emit oauthChanged();
    if (!thenSend)
        return;
    if (!result.ok) {
        TabSend &send = sendOf(m_oauthSendKey);
        send.buildError = result.errorString;
        if (m_oauthSendKey == currentKey())
            m_response->setBuildError(result.errorString, {});
        return;
    }
    if (m_oauthSendKey == currentKey())
        sendBuilt(std::exchange(m_oauthTls, {}), std::exchange(m_oauthExtra, {}));
}

/*!
 * \internal
 * \brief Returns the key of the current tab's send: its tab id, or 0 without tabs.
 */
int RequestEditor::currentKey() const
{
    return m_tabs ? m_tabs->currentTabId() : 0;
}

/*!
 * \internal
 * \brief Returns the send of \a key, creating its executor on first use.
 *
 * Each executor reports to its own key, so a response reaches the tab that
 * sent it even after another tab became current.
 */
RequestEditor::TabSend &RequestEditor::sendOf(int key)
{
    TabSend &send = m_sends[key];
    if (!send.executor) {
        send.executor = new HttpExecutor(this);
        connect(send.executor, &HttpExecutor::finished, this, [this, key](const ResponseData &response) {
            if (key == 0)
                showResponse(response);
            else
                finishSend(key, response);
        });
        connect(send.executor, &HttpExecutor::busyChanged, this, [this, key](bool busy) {
            if (m_tabs)
                m_tabs->setBusy(key, busy);
            if (key == currentKey()) {
                emit busyChanged();
                emit canSendChanged();
            }
        });
    }
    return send;
}

/*!
 * \internal
 * \brief Keeps \a response of the send of \a key and shows it when its tab is current.
 *
 * Every result goes to the history, also one of a tab in the background.
 */
void RequestEditor::finishSend(int key, const ResponseData &response)
{
    auto it = m_sends.find(key);
    if (it == m_sends.end())
        return;
    if (key != 0) {
        for (const TlsCheck &check : response.tlsChecks) {
            if (check.ignored) {
                qWarning().noquote() << QStringLiteral("The certificate of %1 was not checked: %2")
                                            .arg(check.server(), check.messages.join(QStringLiteral("; ")));
            }
        }
    }
    it->response = response;
    const RequestSpec sent = it->sent;
    if (key == currentKey())
        m_response->setResponse(response, it->unresolved);
    emit requestFinished(sent, response);
}

/*!
 * \internal
 * \brief Loads the current tab's request and shows its send.
 */
void RequestEditor::showCurrentTab()
{
    const QString id = m_tabs->currentRequestId();
    if (id.isEmpty())
        clear();
    else if (id != m_requestId)
        loadRequest(id);
    showCurrentSend();
    emit busyChanged();
    emit canSendChanged();
}

/*!
 * \internal
 * \brief Shows the send of the current tab: running, its result, or nothing.
 */
void RequestEditor::showCurrentSend()
{
    const auto it = m_sends.constFind(currentKey());
    if (it == m_sends.constEnd()) {
        m_response->clear();
    } else if (it->executor->isBusy()) {
        m_response->setBusy(it->name, it->requestId);
    } else if (it->response) {
        m_response->setBusy(it->name, it->requestId);
        m_response->setResponse(*it->response, it->unresolved);
    } else if (!it->buildError.isEmpty()) {
        m_response->setBusy(it->name, it->requestId);
        m_response->setBuildError(it->buildError, it->unresolved);
    } else {
        m_response->clear();
    }
}

/*!
 * \internal
 * \brief Cancels and forgets the send of the closed tab \a tabId.
 */
void RequestEditor::dropTab(int tabId)
{
    const auto it = m_sends.find(tabId);
    if (it == m_sends.end())
        return;
    HttpExecutor *executor = it->executor;
    m_sends.erase(it);
    // Erased first, so the abort's finished() finds no send to show.
    executor->abort();
    executor->deleteLater();
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
    const QHash<int, TabSend> sends = std::exchange(m_sends, {});
    for (const TabSend &send : sends) {
        send.executor->abort();
        send.executor->deleteLater();
    }
    m_response->clear();
    emit busyChanged();
    emit canSendChanged();
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
    // An edit keeps the preview tab.
    if (m_tabs)
        m_tabs->keepCurrent();
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
    m_request.body.parts = m_parts->rows();
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
        loadRequest(m_requestId);
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
    m_parts->setRows(m_request.body.parts);
    emit requestChanged();
    emit canSendChanged();
}
