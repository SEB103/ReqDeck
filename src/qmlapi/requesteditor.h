// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REQUESTEDITOR_H
#define REQUESTEDITOR_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>

#include <optional>

#include "core/http/oauthclient.h"
#include "core/http/requestbuilder.h"
#include "core/http/requestspec.h"
#include "core/http/responsecache.h"
#include "core/http/responsedata.h"
#include "core/http/variableresolver.h"

class CollectionTreeModel;
class HttpExecutor;
class OAuthClient;
struct OAuthResult;
class TabsModel;
class FormPartModel;
class KeyValueModel;
class ResponseView;
class WorkspaceManager;
struct ResponseData;

/**
 * The request open in the editor, exposed to QML as \c cppRequestEditor.
 *
 * Edits go straight into the workspace through CollectionTreeModel and mark it
 * dirty; File > Save writes them. send() runs the open request with the active
 * environment and reports to a ResponseView. Enum properties use the numeric
 * values of BodyMode, RawBodyType, and AuthType.
 */
class RequestEditor : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RequestEditor)

    /** Whether a request is open. */
    Q_PROPERTY(bool hasRequest READ hasRequest NOTIFY requestChanged)

    /** Whether a request is running. */
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

    /** Whether send() would start a request. */
    Q_PROPERTY(bool canSend READ canSend NOTIFY canSendChanged)

    /** Node id of the open request; empty when none is open. */
    Q_PROPERTY(QString requestId READ requestId NOTIFY requestChanged)

    /** Name of the request in the collection; a blank name is refused. */
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY requestChanged)

    /** Request verb, for example "GET". */
    Q_PROPERTY(QString method READ method WRITE setMethod NOTIFY requestChanged)

    /** Request URL. */
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY requestChanged)

    /** Body mode: 0 none, 1 raw, 2 form. */
    Q_PROPERTY(int bodyMode READ bodyMode WRITE setBodyMode NOTIFY requestChanged)

    /** Raw body flavor: 0 JSON, 1 text, 2 XML. */
    Q_PROPERTY(int rawBodyType READ rawBodyType WRITE setRawBodyType NOTIFY requestChanged)

    /** Raw body text. */
    Q_PROPERTY(QString rawBody READ rawBody WRITE setRawBody NOTIFY requestChanged)

    /** Authentication: 0 none, 1 basic, 2 bearer, 3 API key. */
    Q_PROPERTY(int authType READ authType WRITE setAuthType NOTIFY requestChanged)

    /** Basic auth user name. */
    Q_PROPERTY(QString authUsername READ authUsername WRITE setAuthUsername NOTIFY requestChanged)

    /** Basic auth password. */
    Q_PROPERTY(QString authPassword READ authPassword WRITE setAuthPassword NOTIFY requestChanged)

    /** Bearer token. */
    Q_PROPERTY(QString authToken READ authToken WRITE setAuthToken NOTIFY requestChanged)

    /** OAuth 2.0 settings: grant (0 authorization code, 1 client credentials), authUrl, tokenUrl, clientId, clientSecret, scope, audience. */
    Q_PROPERTY(QVariantMap oauth2 READ oauth2 NOTIFY requestChanged)

    /**
     * Auth field of the current type that holds typed text, not only {{variable}}
     * references: "password", "token", "apiKey", or empty. Such text is saved in the
     * workspace file.
     */
    Q_PROPERTY(QString literalCredential READ literalCredential NOTIFY requestChanged)

    /** Whether an OAuth 2.0 login or token request is running. */
    Q_PROPERTY(bool oauthBusy READ oauthBusy NOTIFY oauthChanged)

    /** State of the OAuth 2.0 token of the open request, for the user. */
    Q_PROPERTY(QString oauthStatus READ oauthStatus NOTIFY oauthChanged)

    /** Why the OAuth 2.0 token of the open request may travel unencrypted, or empty. */
    Q_PROPERTY(QString oauthWarning READ oauthWarning NOTIFY oauthChanged)

    /** API key header or parameter name. */
    Q_PROPERTY(QString apiKeyName READ apiKeyName WRITE setApiKeyName NOTIFY requestChanged)

    /** API key value. */
    Q_PROPERTY(QString apiKeyValue READ apiKeyValue WRITE setApiKeyValue NOTIFY requestChanged)

    /** Whether the API key goes into the query string instead of a header. */
    Q_PROPERTY(bool apiKeyInQuery READ apiKeyInQuery WRITE setApiKeyInQuery NOTIFY requestChanged)

    /** Transfer timeout in milliseconds; 0 uses the default. */
    Q_PROPERTY(int timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY requestChanged)

    /** Query parameter rows. */
    Q_PROPERTY(KeyValueModel *paramsModel READ paramsModel CONSTANT)

    /** Header rows. */
    Q_PROPERTY(KeyValueModel *headersModel READ headersModel CONSTANT)

    /** Form field rows. */
    Q_PROPERTY(KeyValueModel *formModel READ formModel CONSTANT)

    /** Parts of a multipart/form-data body. */
    Q_PROPERTY(FormPartModel *partsModel READ partsModel CONSTANT)
    Q_MOC_INCLUDE("qmlapi/formpartmodel.h")

    /** Path of the file sent as the body. */
    Q_PROPERTY(QString bodyFilePath READ bodyFilePath WRITE setBodyFilePath NOTIFY requestChanged)

    /** Size of the body file, or why it cannot be sent; empty while the path holds variables. */
    Q_PROPERTY(QString bodyFileInfo READ bodyFileInfo NOTIFY requestChanged)

    /** response() templates for the completion, one per other request with a response in this run. */
    Q_PROPERTY(QStringList responseTemplates READ responseTemplates NOTIFY responseTemplatesChanged)

public:
    /** Creates an empty editor; \a manager, \a collection, and \a response must outlive it. */
    RequestEditor(WorkspaceManager *manager, CollectionTreeModel *collection, ResponseView *response,
                  QObject *parent = nullptr);

    /** Sets the defaults requests are built with. */
    void setBuilderOptions(const RequestBuilder::Options &options);

    /** Follows the current tab of \a tabs, which is not owned; sends and responses then belong to tabs. */
    void setTabs(TabsModel *tabs);

    /** Returns whether a request is open. */
    bool hasRequest() const;

    /** Returns whether a request is running. */
    bool busy() const;

    /** Returns whether send() would start a request. */
    bool canSend() const;

    /** Returns the node id of the open request. */
    QString requestId() const;

    /** Returns the name of the request. */
    QString name() const;

    /** Renames the request. */
    void setName(const QString &name);

    /** Returns the request verb. */
    QString method() const;

    /** Sets the request verb; an unknown verb is ignored. */
    void setMethod(const QString &method);

    /** Returns the request URL. */
    QString url() const;

    /** Sets the request URL. */
    void setUrl(const QString &url);

    /** Returns the body mode. */
    int bodyMode() const;

    /** Sets the body mode; an unknown value is ignored. */
    void setBodyMode(int mode);

    /** Returns the raw body flavor. */
    int rawBodyType() const;

    /** Sets the raw body flavor; an unknown value is ignored. */
    void setRawBodyType(int type);

    /** Returns the raw body text. */
    QString rawBody() const;

    /** Sets the raw body text. */
    void setRawBody(const QString &text);

    /** Returns the authentication type. */
    int authType() const;

    /** Sets the authentication type; an unknown value is ignored. */
    void setAuthType(int type);

    /** Returns the auth field that holds typed text, or an empty string. */
    QString literalCredential() const;

    /** Returns the text of the field named by literalCredential(). */
    Q_INVOKABLE QString literalCredentialValue() const;

    /** Replaces the field named by literalCredential() with a reference to \a key. */
    Q_INVOKABLE void useVariableForCredential(const QString &key);

    /** Returns the Basic auth user name. */
    QString authUsername() const;

    /** Sets the Basic auth user name. */
    void setAuthUsername(const QString &username);

    /** Returns the Basic auth password. */
    QString authPassword() const;

    /** Sets the Basic auth password. */
    void setAuthPassword(const QString &password);

    /** Returns the Bearer token. */
    QString authToken() const;

    /** Returns the OAuth 2.0 settings as a map. */
    QVariantMap oauth2() const;

    /** Sets the OAuth 2.0 field \a name to \a value. */
    Q_INVOKABLE void setOAuthField(const QString &name, const QVariant &value);

    /** Sets the Bearer token. */
    void setAuthToken(const QString &token);

    /** Returns the API key name. */
    QString apiKeyName() const;

    /** Sets the API key name. */
    void setApiKeyName(const QString &name);

    /** Returns the API key value. */
    QString apiKeyValue() const;

    /** Sets the API key value. */
    void setApiKeyValue(const QString &value);

    /** Returns whether the API key goes into the query string. */
    bool apiKeyInQuery() const;

    /** Sets whether the API key goes into the query string. */
    void setApiKeyInQuery(bool inQuery);

    /** Returns the transfer timeout in milliseconds. */
    int timeoutMs() const;

    /** Sets the transfer timeout; a negative value is ignored. */
    void setTimeoutMs(int timeoutMs);

    /** Returns the query parameter rows. */
    KeyValueModel *paramsModel() const;

    /** Returns the header rows. */
    KeyValueModel *headersModel() const;

    /** Returns the form field rows. */
    KeyValueModel *formModel() const;

    /** Returns the parts of a multipart/form-data body. */
    FormPartModel *partsModel() const;

    /** Returns the path of the file sent as the body. */
    QString bodyFilePath() const;

    /** Sets the path of the file sent as the body; a file URL is turned into a path. */
    void setBodyFilePath(const QString &pathOrUrl);

    /** Returns the size of the body file, or why it cannot be sent. */
    QString bodyFileInfo() const;

    /** Opens the request \a id; a folder or an unknown id leaves the editor empty. */
    Q_INVOKABLE void load(const QString &id);

    /** Closes the open request. */
    Q_INVOKABLE void clear();

    /** Sends the open request; does nothing without one or while one is running. */
    Q_INVOKABLE void send();

    /** Returns the variables the open request references that do not resolve, each once. */
    Q_INVOKABLE QStringList missingVariables() const;

    /** Returns response() templates for the other requests with a response in this run, newest first. */
    QStringList responseTemplates() const;

    /** Sends the open request with \a values added to the active variables for this send only. */
    Q_INVOKABLE void sendWithValues(const QVariantMap &values);

    /** Returns the files the open request would upload with \a values that were not confirmed yet. */
    Q_INVOKABLE QStringList unconfirmedFiles(const QVariantMap &values = {}) const;

    /** Allows the files at \a paths to go to the server of the open request with \a values until another workspace opens. */
    Q_INVOKABLE void confirmFiles(const QStringList &paths, const QVariantMap &values = {});

    /** Returns the server the open request with \a values would upload its files to. */
    Q_INVOKABLE QString uploadServer(const QVariantMap &values = {}) const;

    /**
     * Sends the open request again without checking the certificate of the host
     * whose check failed for its last response; does nothing otherwise.
     */
    Q_INVOKABLE void sendTrustingCertificate();

    /** Cancels the running request. */
    Q_INVOKABLE void cancel();

    /** Copies the open request to the clipboard as a cURL command. */
    Q_INVOKABLE void copyAsCurl();

    /** Returns whether an OAuth 2.0 login or token request is running. */
    bool oauthBusy() const;

    /** Returns the state of the OAuth 2.0 token of the open request. */
    QString oauthStatus() const;

    /** Returns why the OAuth 2.0 token may travel unencrypted, or an empty string. */
    QString oauthWarning() const;

    /** Gets an OAuth 2.0 token for the open request: in the browser, or with the client's credentials. */
    Q_INVOKABLE void getToken();

    /** Stops a running OAuth 2.0 login. */
    Q_INVOKABLE void cancelLogin();

    /** Removes the OAuth 2.0 tokens of the node whose settings the open request uses. */
    Q_INVOKABLE void resetToken();

    /** Checks a cURL \a command; the map holds ok, error, summary, and ignored. */
    Q_INVOKABLE QVariantMap checkCurl(const QString &command) const;

    /** Adds a request made from a cURL \a command to \a parentId; returns its id or "". */
    Q_INVOKABLE QString importCurl(const QString &command, const QString &parentId);

signals:
    /** Emitted when the open request or any of its fields changes. */
    void requestChanged();

    /** Emitted after the response of \a request reached the view; \a request is as it was sent. */
    void requestFinished(const RequestSpec &request, const ResponseData &response);

    /** Emitted when responseTemplates may have changed. */
    void responseTemplatesChanged();

    /** Emitted after copyAsCurl() wrote to the clipboard. */
    void copiedToClipboard();

    /** Emitted when oauthBusy() or oauthStatus() changes. */
    void oauthChanged();

    /** Emitted when busy() changes. */
    void busyChanged();

    /** Emitted when canSend() changes. */
    void canSendChanged();

private:
    /** Assigns \a value to \a field of the open request and stores the request. */
    template<typename T>
    void updateField(T &field, const T &value);

    /** Stores the working copy in the collection. */
    void store();

    /** Puts the rows of the table models into the working copy and stores it. */
    void storeTables();

    /** Follows changes of the collection made outside the editor. */
    void syncWithCollection();

    /** Shows the working copy in the table models and notifies QML. */
    void showRequest();

    /** Returns the open request with inherited auth, and in \a variables the rows to resolve with. */
    RequestSpec effectiveRequest(QList<KeyValue> &variables) const;

    /** Builds and sends the open request; \a tlsException is a failed check it accepts. */
    void sendBuilt(const TlsCheck &tlsException, const QList<KeyValue> &extraVariables = {});

    /** Passes the result of the running request to the response view. */
    void showResponse(const ResponseData &response);

    /** Ends a running request and removes the response of the old workspace. */
    void dropResponse();

    /** Workspace holding the environments; not owned. */
    WorkspaceManager *m_manager {nullptr};

    /** Collection holding the requests; not owned. */
    CollectionTreeModel *m_collection {nullptr};

    /** Receives the state and result of sending; not owned. */
    ResponseView *m_response {nullptr};

    /** The send of one tab and what it gave. */
    struct TabSend
    {
        /** Executor of the tab; owned through the QObject tree. */
        HttpExecutor *executor {nullptr};
        /** The request as it was sent, which outlives switching to another one. */
        RequestSpec sent;
        /** Name of the request sent. */
        QString name;
        /** Id of the request sent. */
        QString requestId;
        /** Variables of the send that had no value. */
        QStringList unresolved;
        /** One-time values of the send, for Continue past a certificate; never stored. */
        QList<KeyValue> extra;
        /** Last response, or nothing. */
        std::optional<ResponseData> response;
        /** Why the last send could not be built; empty when it was. */
        QString buildError;
    };

    /** Returns the key of the current tab's send: its tab id, or 0 without tabs. */
    int currentKey() const;

    /** Returns the send of \a key, with its executor created on first use. */
    TabSend &sendOf(int key);

    /** Keeps the result of the send of \a key and shows it when its tab is current. */
    void finishSend(int key, const ResponseData &response);

    /** Loads the current tab's request and shows its send. */
    void showCurrentTab();

    /** Shows the send of the current tab in the response view. */
    void showCurrentSend();

    /** Cancels and forgets the send of the closed tab \a tabId. */
    void dropTab(int tabId);

    /** Loads \a id into the editor without touching the tabs. */
    void loadRequest(const QString &id);

    /** Sends per tab; key 0 without tabs. */
    QHash<int, TabSend> m_sends;

    /** Tabs followed, or null; not owned. */
    TabsModel *m_tabs {nullptr};

    /** The OAuth 2.0 settings of the open request, resolved, with their owner and store key. */
    struct OAuthContext
    {
        /** Whether the open request uses OAuth 2.0. */
        bool active {false};
        /** Node whose auth holds the settings. */
        QString owner;
        /** Settings with variables resolved. */
        OAuth2Settings settings;
        /** Key of the token in the store. */
        QString key;
        /** Variables of the settings that have no value, each once. */
        QStringList unresolved;
    };

    /** Returns the message that \a names must have values before a token can be fetched. */
    static QString oauthVariablesMessage(const QStringList &names);

    /** Returns the OAuth context of the open request with the one-time values \a extra. */
    OAuthContext oauthContext(const QList<KeyValue> &extra = {}) const;

    /** Stores the token of \a result, then sends again or reports, as the running OAuth step asked. */
    void finishOAuth(const OAuthResult &result);

    /** Gets tokens; owned through the QObject tree. */
    OAuthClient *m_oauth {nullptr};

    /** Key of the token the running OAuth step gets. */
    QString m_oauthKey;

    /** Whether the running OAuth step should send the request of m_oauthSendKey afterwards. */
    bool m_oauthThenSend {false};

    /** Send key of the request waiting for a token. */
    int m_oauthSendKey {0};

    /** Certificate exception of the request waiting for a token. */
    TlsCheck m_oauthTls;

    /** One-time values of the request waiting for a token; never stored. */
    QList<KeyValue> m_oauthExtra;

    /** Last OAuth error for the user, or empty. */
    QString m_oauthError;

    /** Request and workspace of the running OAuth step, so its token and send land where they began. */
    QString m_oauthRequestId;

    /** Workspace of the running OAuth step. */
    QString m_oauthWorkspaceId;

    /** Whether the running OAuth step is a refresh. */
    bool m_oauthIsRefresh {false};

    /** A token just fetched for the waiting send, used even when it expires soon. */
    std::optional<OAuthToken> m_freshToken;

    /** Last response of each request in this run, for response() templates; never stored. */
    ResponseCache m_responses;

    /** Files the user allowed to upload in this workspace, as fileKey() gives them. */
    QSet<QString> m_confirmedFiles;

    /** Returns the builder options with the workspace folder as the base of relative paths. */
    RequestBuilder::Options builderOptions() const;

    /** Returns the files of \a built that were not confirmed. */
    QStringList unconfirmedFilesOf(const BuiltRequest &built) const;

    /** Returns the comparison key of the file at \a path. */
    static QString fileKey(const QString &path);

    /** Returns the server of \a url as scheme, host, and port. */
    static QString originOf(const QUrl &url);

    /** Builds the open request with the one-time \a values. */
    BuiltRequest buildWithValues(const QVariantMap &values) const;

    /** Returns a resolver over \a rows whose response() templates read m_responses. */
    VariableResolver resolverFor(const QList<KeyValue> &rows) const;

    /** Forgets the responses of requests that no longer exist. */
    void pruneResponses();

    /** Defaults requests are built with. */
    RequestBuilder::Options m_builderOptions;

    /** Query parameter rows; owned through the QObject tree. */
    KeyValueModel *m_params {nullptr};

    /** Header rows; owned through the QObject tree. */
    KeyValueModel *m_headers {nullptr};

    /** Form field rows; owned through the QObject tree. */
    KeyValueModel *m_form {nullptr};

    /** Multipart parts; owned through the QObject tree. */
    FormPartModel *m_parts {nullptr};

    /** Node id of the open request; empty when none is open. */
    QString m_requestId;

    /** Name of the open request. */
    QString m_name;

    /** Working copy of the open request. */
    RequestSpec m_request;
};

#endif // REQUESTEDITOR_H
