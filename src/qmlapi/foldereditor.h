// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FOLDEREDITOR_H
#define FOLDEREDITOR_H

#include <QObject>
#include <QVariantMap>
#include <QString>

#include "core/workspace/workspacedata.h"

class CollectionTreeModel;
class KeyValueModel;

/** Edits the settings of one folder that its requests inherit; exposed to QML as \c cppFolderEditor. */
class FolderEditor : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FolderEditor)
    Q_MOC_INCLUDE("qmlapi/keyvaluemodel.h")

    /** Id of the folder being edited; empty for none. */
    Q_PROPERTY(QString folderId READ folderId NOTIFY folderChanged)

    /** Name of the folder being edited. */
    Q_PROPERTY(QString name READ name NOTIFY folderChanged)

    /** Authentication: 0 none, 1 basic, 2 bearer, 3 API key, 4 inherit from the folder above. */
    Q_PROPERTY(int authType READ authType WRITE setAuthType NOTIFY folderChanged)

    /** Basic auth user name. */
    Q_PROPERTY(QString authUsername READ authUsername WRITE setAuthUsername NOTIFY folderChanged)

    /** Basic auth password. */
    Q_PROPERTY(QString authPassword READ authPassword WRITE setAuthPassword NOTIFY folderChanged)

    /** Bearer token. */
    Q_PROPERTY(QString authToken READ authToken WRITE setAuthToken NOTIFY folderChanged)

    /** OAuth 2.0 settings: grant (0 authorization code, 1 client credentials), authUrl, tokenUrl, clientId, clientSecret, scope, audience. */
    Q_PROPERTY(QVariantMap oauth2 READ oauth2 NOTIFY folderChanged)

    /** API key name. */
    Q_PROPERTY(QString apiKeyName READ apiKeyName WRITE setApiKeyName NOTIFY folderChanged)

    /** API key value. */
    Q_PROPERTY(QString apiKeyValue READ apiKeyValue WRITE setApiKeyValue NOTIFY folderChanged)

    /** Whether the API key goes into the query string. */
    Q_PROPERTY(bool apiKeyInQuery READ apiKeyInQuery WRITE setApiKeyInQuery NOTIFY folderChanged)

    /** Variables of the folder. */
    Q_PROPERTY(KeyValueModel *variablesModel READ variablesModel CONSTANT)

public:
    /** Creates an editor over the folders of \a collection. */
    explicit FolderEditor(CollectionTreeModel *collection, QObject *parent = nullptr);

    /** Returns the id of the folder being edited. */
    QString folderId() const;

    /** Returns the name of the folder being edited. */
    QString name() const;

    /** Returns the authentication type. */
    int authType() const;

    /** Sets the authentication type; an unknown value is ignored. */
    void setAuthType(int type);

    /** Returns the Basic auth user name. */
    QString authUsername() const;

    /** Sets the Basic auth user name. */
    void setAuthUsername(const QString &username);

    /** Returns the Basic auth password. */
    QString authPassword() const;

    /** Sets the Basic auth password. */
    void setAuthPassword(const QString &password);

    /** Returns the bearer token. */
    QString authToken() const;

    /** Returns the OAuth 2.0 settings as a map. */
    QVariantMap oauth2() const;

    /** Sets the OAuth 2.0 field \a name to \a value. */
    Q_INVOKABLE void setOAuthField(const QString &name, const QVariant &value);

    /** Sets the bearer token. */
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

    /** Returns the variables of the folder. */
    KeyValueModel *variablesModel() const;

    /** Shows the folder \a id; a request or an unknown id leaves the editor empty. */
    Q_INVOKABLE void load(const QString &id);

signals:
    /** Emitted when another folder is shown or a setting changed. */
    void folderChanged();

private:
    /** Sets \a field to \a value and stores the settings when it changed. */
    template<typename T>
    void updateField(T &field, const T &value);

    /** Stores the settings in the collection. */
    void store();

    /** The collection the folders belong to. */
    CollectionTreeModel *m_collection {nullptr};

    /** Id of the folder being edited. */
    QString m_id;

    /** Working copy of its settings. */
    FolderSettings m_settings;

    /** Variables of the folder as a table. */
    KeyValueModel *m_variables {nullptr};
};

#endif // FOLDEREDITOR_H
