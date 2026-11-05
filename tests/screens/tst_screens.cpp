// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include "core/workspace/workspaceserializer.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/requesteditor.h"
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
        m_manager.setSecretsDir(m_dir.filePath(QStringLiteral("secrets")));
        context->setContextProperty(QStringLiteral("cppWorkspaceManager"), &m_manager);
        context->setContextProperty(QStringLiteral("cppCollection"), &m_collection);
        context->setContextProperty(QStringLiteral("cppRequestEditor"), &m_editor);
        context->setContextProperty(QStringLiteral("cppTestHost"), this);
    }

    /*! Opens a fresh copy of the test workspace, clean and with nothing selected. */
    Q_INVOKABLE bool reset()
    {
        m_editor.clear();
        m_manager.closeWorkspace();

        WorkspaceData data;
        data.id = newItemId();
        data.displayName = QStringLiteral("Screens");
        data.collection = makeCollection();
        const QString path = m_dir.filePath(QStringLiteral("screens.reqdeck"));
        return WorkspaceSerializer::save(path, data) && m_manager.openWorkspace(path);
    }

private:
    /*! Directory of the test workspace. */
    QTemporaryDir m_dir;

    /*! Real workspace manager. */
    WorkspaceManager m_manager;

    /*! Real collection model. */
    CollectionTreeModel m_collection {&m_manager};

    /*! Real request editor. */
    RequestEditor m_editor {&m_collection};
};

/*! Quick Test setup that installs the facades on the test engine. */
class ScreensSetup : public QObject
{
    Q_OBJECT

public slots:
    /*! Publishes the facades on \a engine. */
    void qmlEngineAvailable(QQmlEngine *engine) { m_host.publish(engine->rootContext()); }

private:
    /*! Host kept alive for the lifetime of the Quick Test setup object. */
    ScreensHost m_host;
};

QUICK_TEST_MAIN_WITH_SETUP(reqdeck_screens, ScreensSetup)

#include "tst_screens.moc"
