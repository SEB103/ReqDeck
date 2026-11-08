// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REQUESTEDITOR_H
#define REQUESTEDITOR_H

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "core/http/requestbuilder.h"
#include "core/http/requestspec.h"
#include "core/http/responsedata.h"

class CollectionTreeModel;
class HttpExecutor;
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

    /**
     * Auth field of the current type that holds typed text, not only {{variable}}
     * references: "password", "token", "apiKey", or empty. Such text is saved in the
     * workspace file.
     */
    Q_PROPERTY(QString literalCredential READ literalCredential NOTIFY requestChanged)

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

public:
    /** Creates an empty editor; \a manager, \a collection, and \a response must outlive it. */
    RequestEditor(WorkspaceManager *manager, CollectionTreeModel *collection, ResponseView *response,
                  QObject *parent = nullptr);

    /** Sets the defaults requests are built with. */
    void setBuilderOptions(const RequestBuilder::Options &options);

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

    /** Opens the request \a id; a folder or an unknown id leaves the editor empty. */
    Q_INVOKABLE void load(const QString &id);

    /** Closes the open request. */
    Q_INVOKABLE void clear();

    /** Sends the open request; does nothing without one or while one is running. */
    Q_INVOKABLE void send();

    /** Returns the variables the open request references that do not resolve, each once. */
    Q_INVOKABLE QStringList missingVariables() const;

    /** Sends the open request with \a values added to the active variables for this send only. */
    Q_INVOKABLE void sendWithValues(const QVariantMap &values);

    /**
     * Sends the open request again without checking the certificate of the host
     * whose check failed for its last response; does nothing otherwise.
     */
    Q_INVOKABLE void sendTrustingCertificate();

    /** Cancels the running request. */
    Q_INVOKABLE void cancel();

    /** Copies the open request to the clipboard as a cURL command. */
    Q_INVOKABLE void copyAsCurl();

    /** Checks a cURL \a command; the map holds ok, error, summary, and ignored. */
    Q_INVOKABLE QVariantMap checkCurl(const QString &command) const;

    /** Adds a request made from a cURL \a command to \a parentId; returns its id or "". */
    Q_INVOKABLE QString importCurl(const QString &command, const QString &parentId);

signals:
    /** Emitted when the open request or any of its fields changes. */
    void requestChanged();

    /** Emitted after the response of \a request reached the view; \a request is as it was sent. */
    void requestFinished(const RequestSpec &request, const ResponseData &response);

    /** Emitted after copyAsCurl() wrote to the clipboard. */
    void copiedToClipboard();

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

    /** Sends requests; owned through the QObject tree. */
    HttpExecutor *m_executor {nullptr};

    /** Defaults requests are built with. */
    RequestBuilder::Options m_builderOptions;

    /** Variables of the running request that had no value. */
    QStringList m_unresolved;

    /** The request as it was sent, which outlives switching to another one. */
    RequestSpec m_sentRequest;

    /** Query parameter rows; owned through the QObject tree. */
    KeyValueModel *m_params {nullptr};

    /** Header rows; owned through the QObject tree. */
    KeyValueModel *m_headers {nullptr};

    /** Form field rows; owned through the QObject tree. */
    KeyValueModel *m_form {nullptr};

    /** Node id of the open request; empty when none is open. */
    QString m_requestId;

    /** Name of the open request. */
    QString m_name;

    /** Working copy of the open request. */
    RequestSpec m_request;
};

#endif // REQUESTEDITOR_H
