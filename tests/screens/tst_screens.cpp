// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtQuickTest/quicktest.h>

#include <algorithm>
#include <memory>

#include "core/workspace/workspaceserializer.h"
#include "mockhttpserver.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/environmentmodel.h"
#include "qmlapi/environmentvariablesmodel.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/requesteditor.h"
#include "qmlapi/responseview.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Returns the collection f1(r1) followed by r2.
 */
static QList<CollectionNode> makeCollection()
{
    CollectionNode first;
    first.kind = CollectionNode::Kind::Request;
    first.id = QStringLiteral("r1");
    first.name = QStringLiteral("Create item");
    first.request.method = HttpMethod::Post;
    first.request.url = QStringLiteral("http://h/items");
    first.request.headers = {{QStringLiteral("Accept"), QStringLiteral("*/*"), true}};
    first.request.body.mode = BodyMode::Raw;
    first.request.body.raw = QStringLiteral("{}");
    first.request.auth.type = AuthType::Basic;
    first.request.auth.username = QStringLiteral("user");
    first.request.auth.password = QStringLiteral("pass");

    CollectionNode folder;
    folder.id = QStringLiteral("f1");
    folder.name = QStringLiteral("Items");
    folder.children = {first};

    CollectionNode second;
    second.kind = CollectionNode::Kind::Request;
    second.id = QStringLiteral("r2");
    second.name = QStringLiteral("List items");

    return {folder, second};
}

/*!
 * \internal
 * \brief Returns an environment defining baseUrl as \a baseUrl.
 */
static Environment makeEnvironment(const QString &id, const QString &name, const QString &baseUrl)
{
    Environment environment;
    environment.id = id;
    environment.name = name;
    environment.variables = {{QStringLiteral("baseUrl"), baseUrl, true, false}};
    return environment;
}

/*!
 * \internal
 * \brief Gives the screens real facades over a temporary workspace.
 *
 * Unlike the Base smoke test, these screens read the application's context, so
 * the context properties are the production objects, not mocks.
 */
class ScreensHost : public QObject
{
    Q_OBJECT

public:
    /*! Creates the facades and publishes them to QML. */
    void publish(QQmlContext *context)
    {
        m_server.start();
        m_manager.setSecretsDir(m_dir.filePath(QStringLiteral("secrets")));
        context->setContextProperty(QStringLiteral("cppWorkspaceManager"), &m_manager);
        context->setContextProperty(QStringLiteral("cppCollection"), &m_collection);
        context->setContextProperty(QStringLiteral("cppEnvironments"), &m_environments);
        context->setContextProperty(QStringLiteral("cppRequestEditor"), &m_editor);
        context->setContextProperty(QStringLiteral("cppResponse"), &m_response);
        context->setContextProperty(QStringLiteral("cppTestHost"), this);
    }

    /*! Returns the URL of \a path on the mock server. */
    Q_INVOKABLE QString serverUrl(const QString &path) const { return m_server.url(path).toString(); }

    /*! Returns a URL on a port nothing listens on. */
    Q_INVOKABLE QString closedUrl() const
    {
        QTcpServer probe;
        probe.listen(QHostAddress::LocalHost, 0);
        return QStringLiteral("http://127.0.0.1:%1/").arg(probe.serverPort());
    }

    /*! Returns whether one of the first lines of \a document carries a text format. */
    Q_INVOKABLE bool isColoredBelowFirstLine(QQuickTextDocument *document) const
    {
        QTextBlock block = document->textDocument()->firstBlock();
        for (int line = 0; line < 4 && block.isValid(); ++line, block = block.next()) {
            if (!block.layout()->formats().isEmpty())
                return true;
        }
        return false;
    }

    /*! Returns whether the first line of \a document has an underlined range. */
    Q_INVOKABLE bool hasUnderline(QQuickTextDocument *document) const
    {
        const QList<QTextLayout::FormatRange> formats =
            document->textDocument()->firstBlock().layout()->formats();
        return std::any_of(formats.cbegin(), formats.cend(),
                           [](const QTextLayout::FormatRange &range) {
                               return range.format.fontUnderline();
                           });
    }

    /*! Returns the number of requests the mock server received. */
    Q_INVOKABLE int requestCount() const { return m_server.requestCount(); }

    /*! Opens a fresh copy of the test workspace, clean and with nothing selected. */
    Q_INVOKABLE bool reset()
    {
        m_editor.clear();
        m_manager.closeWorkspace();

        WorkspaceData data;
        data.id = newItemId();
        data.displayName = QStringLiteral("Screens");
        data.collection = makeCollection();
        data.environments = {makeEnvironment(QStringLiteral("e1"), QStringLiteral("Created"),
                                              m_server.url(QStringLiteral("/status/201")).toString()),
                             makeEnvironment(QStringLiteral("e2"), QStringLiteral("Missing"),
                                              m_server.url(QStringLiteral("/status/404")).toString())};
        const QString path = m_dir.filePath(QStringLiteral("screens.reqdeck"));
        return WorkspaceSerializer::save(path, data) && m_manager.openWorkspace(path);
    }

private:
    /*! Directory of the test workspace. */
    QTemporaryDir m_dir;

    /*! Local server the requests of the tests go to. */
    MockHttpServer m_server;

    /*! Real workspace manager. */
    WorkspaceManager m_manager;

    /*! Real collection model. */
    CollectionTreeModel m_collection {&m_manager};

    /*! Real environment model. */
    EnvironmentModel m_environments {&m_manager};

    /*! Real response view. */
    ResponseView m_response;

    /*! Real request editor. */
    RequestEditor m_editor {&m_manager, &m_collection, &m_response};
};

/*! Quick Test setup that installs the facades on the test engine. */
class ScreensSetup : public QObject
{
    Q_OBJECT

public slots:
    /*! Creates the facades, which need the application object, and publishes them. */
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        m_host = std::make_unique<ScreensHost>();
        m_host->publish(engine->rootContext());
    }

private:
    /*! Host kept alive for the lifetime of the Quick Test setup object. */
    std::unique_ptr<ScreensHost> m_host;
};

QUICK_TEST_MAIN_WITH_SETUP(reqdeck_screens, ScreensSetup)

#include "tst_screens.moc"
