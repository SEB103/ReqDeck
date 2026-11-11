// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/workspace/workspaceserializer.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/foldereditor.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/workspacemanager.h"

/*! Verifies the editor of folder settings over a real workspace. */
class FolderEditorTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Creates a workspace with a folder holding a request. */
    void init();

    /*! Releases the per-test objects. */
    void cleanup();

    /*! Verifies that loading shows the settings and changes nothing. */
    void loadShowsSettingsWithoutDirty();

    /*! Verifies that edits reach the folder of the workspace. */
    void editsWriteThrough();

    /*! Verifies that a request or an unknown id leaves the editor empty. */
    void requestLeavesEditorEmpty();

private:
    /*! Directory of the test workspace. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Real workspace manager. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Collection of the workspace. */
    std::unique_ptr<CollectionTreeModel> m_collection;

    /*! Editor under test. */
    std::unique_ptr<FolderEditor> m_editor;
};

void FolderEditorTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));

    CollectionNode request;
    request.kind = CollectionNode::Kind::Request;
    request.id = QStringLiteral("r1");
    request.name = QStringLiteral("Request");
    CollectionNode folder;
    folder.id = QStringLiteral("f1");
    folder.name = QStringLiteral("API");
    folder.folder.auth.type = AuthType::Bearer;
    folder.folder.auth.token = QStringLiteral("{{token}}");
    folder.children = {request};
    WorkspaceData data;
    data.id = newItemId();
    data.collection = {folder};
    const QString path = m_dir->filePath(QStringLiteral("folders.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, data));
    QVERIFY(m_manager->openWorkspace(path));

    m_collection = std::make_unique<CollectionTreeModel>(m_manager.get());
    m_editor = std::make_unique<FolderEditor>(m_collection.get());
}

void FolderEditorTest::cleanup()
{
    m_editor.reset();
    m_collection.reset();
    m_manager.reset();
    m_dir.reset();
}

void FolderEditorTest::loadShowsSettingsWithoutDirty()
{
    QSignalSpy changed(m_editor.get(), &FolderEditor::folderChanged);

    m_editor->load(QStringLiteral("f1"));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(m_editor->property("folderId").toString(), QStringLiteral("f1"));
    QCOMPARE(m_editor->property("name").toString(), QStringLiteral("API"));
    QCOMPARE(m_editor->property("authType").toInt(), int(AuthType::Bearer));
    QCOMPARE(m_editor->property("authToken").toString(), QStringLiteral("{{token}}"));
    QVERIFY(!m_manager->dirty());
}

void FolderEditorTest::editsWriteThrough()
{
    m_editor->load(QStringLiteral("f1"));

    m_editor->setAuthType(int(AuthType::ApiKey));
    m_editor->setApiKeyName(QStringLiteral("X-Key"));
    m_editor->setApiKeyInQuery(true);
    m_editor->variablesModel()->addRow();
    KeyValueModel *variables = m_editor->variablesModel();
    variables->setData(variables->index(0), QStringLiteral("base"), KeyValueModel::KeyRole);
    variables->setData(variables->index(0), QStringLiteral("https://h"), KeyValueModel::ValueRole);

    const FolderSettings &stored = m_manager->workspaceData().collection.at(0).folder;
    QCOMPARE(stored.auth.type, AuthType::ApiKey);
    QCOMPARE(stored.auth.apiKeyName, QStringLiteral("X-Key"));
    QCOMPARE(stored.auth.apiKeyLocation, ApiKeyLocation::Query);
    QCOMPARE(stored.variables.size(), 1);
    QCOMPARE(stored.variables.at(0).value, QStringLiteral("https://h"));
    QVERIFY(m_manager->dirty());
}

void FolderEditorTest::requestLeavesEditorEmpty()
{
    m_editor->load(QStringLiteral("r1"));
    QVERIFY(m_editor->property("folderId").toString().isEmpty());

    m_editor->setAuthType(int(AuthType::None));
    QVERIFY(!m_manager->dirty());
}

QTEST_GUILESS_MAIN(FolderEditorTest)

#include "tst_foldereditor.moc"
