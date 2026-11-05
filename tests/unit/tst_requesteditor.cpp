// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/workspace/collectiontree.h"
#include "core/workspace/workspaceserializer.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/requesteditor.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Returns the collection folder(full, other) with one fully set request.
 */
static QList<CollectionNode> makeCollection()
{
    CollectionNode full;
    full.kind = CollectionNode::Kind::Request;
    full.id = QStringLiteral("full");
    full.name = QStringLiteral("Full request");
    full.request.method = HttpMethod::Post;
    full.request.url = QStringLiteral("{{baseUrl}}/items");
    full.request.queryParams = {{QStringLiteral("q"), QStringLiteral("1"), true}};
    full.request.headers = {{QStringLiteral("Accept"), QStringLiteral("*/*"), false}};
    full.request.body.mode = BodyMode::Raw;
    full.request.body.rawType = RawBodyType::Xml;
    full.request.body.raw = QStringLiteral("<a/>");
    full.request.body.form = {{QStringLiteral("f"), QStringLiteral("v"), true}};
    full.request.auth.type = AuthType::ApiKey;
    full.request.auth.username = QStringLiteral("user");
    full.request.auth.password = QStringLiteral("pass");
    full.request.auth.token = QStringLiteral("tok");
    full.request.auth.apiKeyName = QStringLiteral("key");
    full.request.auth.apiKeyValue = QStringLiteral("value");
    full.request.auth.apiKeyLocation = ApiKeyLocation::Query;
    full.request.timeoutMs = 5000;

    CollectionNode other;
    other.kind = CollectionNode::Kind::Request;
    other.id = QStringLiteral("other");
    other.name = QStringLiteral("Other");

    CollectionNode folder;
    folder.id = QStringLiteral("folder");
    folder.name = QStringLiteral("Folder");
    folder.children = {full, other};
    return {folder};
}

/*! Verifies the editor facade of the open request. */
class RequestEditorTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Opens a workspace with a known collection for each test. */
    void init();

    /*! Releases the per-test objects. */
    void cleanup();

    /*! Verifies that every field of the request is exposed. */
    void loadExposesAllFields();

    /*! Verifies that showing a request changes nothing. */
    void loadDoesNotMarkDirty();

    /*! Verifies that a folder or an unknown id leaves the editor empty. */
    void loadFolderClearsEditor();

    /*! Verifies that each setter reaches the stored request and marks it dirty. */
    void settersWriteThroughAndMarkDirty_data();
    /*! Runs one setter case. */
    void settersWriteThroughAndMarkDirty();

    /*! Verifies that an unchanged value leaves the workspace clean. */
    void unchangedValueStaysClean();

    /*! Verifies that values outside the allowed range are ignored. */
    void invalidValuesAreIgnored();

    /*! Verifies that the name goes to the tree node. */
    void nameRenamesTreeNode();

    /*! Verifies that a blank name is refused and the old one stays. */
    void blankNameIsRefused();

    /*! Verifies that a rename in the tree reaches the open editor. */
    void renamingInTreeUpdatesEditorName();

    /*! Verifies that a changed method shows in the tree. */
    void methodChangeUpdatesTreeRole();

    /*! Verifies that the parameter table writes through. */
    void paramsEditWritesThrough();

    /*! Verifies that the header table writes through. */
    void headersEditWritesThrough();

    /*! Verifies that the form table writes through. */
    void formEditWritesThrough();

    /*! Verifies that switching between requests keeps both sets of edits. */
    void switchingRequestsKeepsBothEdits();

    /*! Verifies that deleting the open request closes the editor. */
    void removingOpenRequestClearsEditor();

    /*! Verifies that deleting the folder of the open request closes the editor. */
    void removingParentFolderClearsEditor();

    /*! Verifies that another workspace does not inherit the open request. */
    void switchingWorkspaceClearsEditor();

    /*! Verifies that a save keeps the request open. */
    void saveKeepsEditorOpen();

    /*! Verifies that edits reach the file and come back. */
    void editsSurviveSaveAndReopen();

private:
    /*! Writes a workspace file holding \a collection; returns its path. */
    QString writeWorkspace(const QString &name, const QList<CollectionNode> &collection);

    /*! Directory of the per-test workspace files. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Workspace manager with the open test workspace. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Collection model the editor works on. */
    std::unique_ptr<CollectionTreeModel> m_collection;

    /*! Editor under test. */
    std::unique_ptr<RequestEditor> m_editor;
};

QString RequestEditorTest::writeWorkspace(const QString &name, const QList<CollectionNode> &collection)
{
    WorkspaceData data;
    data.id = newItemId();
    data.displayName = name;
    data.collection = collection;
    const QString path = m_dir->filePath(name + QStringLiteral(".reqdeck"));
    return WorkspaceSerializer::save(path, data) ? path : QString();
}

void RequestEditorTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));
    m_collection = std::make_unique<CollectionTreeModel>(m_manager.get());
    m_editor = std::make_unique<RequestEditor>(m_collection.get());
    QVERIFY(m_manager->openWorkspace(writeWorkspace(QStringLiteral("Main"), makeCollection())));
}

void RequestEditorTest::cleanup()
{
    m_editor.reset();
    m_collection.reset();
    m_manager.reset();
    m_dir.reset();
}

void RequestEditorTest::loadExposesAllFields()
{
    QSignalSpy spy(m_editor.get(), &RequestEditor::requestChanged);
    m_editor->load(QStringLiteral("full"));

    QVERIFY(!spy.isEmpty());
    QVERIFY(m_editor->property("hasRequest").toBool());
    QCOMPARE(m_editor->property("requestId").toString(), QStringLiteral("full"));
    QCOMPARE(m_editor->property("name").toString(), QStringLiteral("Full request"));
    QCOMPARE(m_editor->property("method").toString(), QStringLiteral("POST"));
    QCOMPARE(m_editor->property("url").toString(), QStringLiteral("{{baseUrl}}/items"));
    QCOMPARE(m_editor->property("bodyMode").toInt(), 1);
    QCOMPARE(m_editor->property("rawBodyType").toInt(), 2);
    QCOMPARE(m_editor->property("rawBody").toString(), QStringLiteral("<a/>"));
    QCOMPARE(m_editor->property("authType").toInt(), 3);
    QCOMPARE(m_editor->property("authUsername").toString(), QStringLiteral("user"));
    QCOMPARE(m_editor->property("authPassword").toString(), QStringLiteral("pass"));
    QCOMPARE(m_editor->property("authToken").toString(), QStringLiteral("tok"));
    QCOMPARE(m_editor->property("apiKeyName").toString(), QStringLiteral("key"));
    QCOMPARE(m_editor->property("apiKeyValue").toString(), QStringLiteral("value"));
    QVERIFY(m_editor->property("apiKeyInQuery").toBool());
    QCOMPARE(m_editor->property("timeoutMs").toInt(), 5000);
    QCOMPARE(m_editor->paramsModel()->rowCount(), 1);
    QCOMPARE(m_editor->headersModel()->rowCount(), 1);
    QCOMPARE(m_editor->formModel()->rowCount(), 1);
}

void RequestEditorTest::loadDoesNotMarkDirty()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->load(QStringLiteral("other"));
    QVERIFY(!m_manager->dirty());
}

void RequestEditorTest::loadFolderClearsEditor()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->load(QStringLiteral("folder"));
    QVERIFY(!m_editor->property("hasRequest").toBool());
    QVERIFY(m_editor->property("url").toString().isEmpty());
    QCOMPARE(m_editor->paramsModel()->rowCount(), 0);

    m_editor->load(QStringLiteral("full"));
    m_editor->load(QStringLiteral("nope"));
    QVERIFY(!m_editor->property("hasRequest").toBool());
}

void RequestEditorTest::settersWriteThroughAndMarkDirty_data()
{
    QTest::addColumn<QByteArray>("name");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("method") << QByteArray("method") << QVariant(QStringLiteral("DELETE"));
    QTest::newRow("url") << QByteArray("url") << QVariant(QStringLiteral("http://new"));
    QTest::newRow("bodyMode") << QByteArray("bodyMode") << QVariant(2);
    QTest::newRow("rawBodyType") << QByteArray("rawBodyType") << QVariant(1);
    QTest::newRow("rawBody") << QByteArray("rawBody") << QVariant(QStringLiteral("{}"));
    QTest::newRow("authType") << QByteArray("authType") << QVariant(1);
    QTest::newRow("authUsername") << QByteArray("authUsername") << QVariant(QStringLiteral("u2"));
    QTest::newRow("authPassword") << QByteArray("authPassword") << QVariant(QStringLiteral("p2"));
    QTest::newRow("authToken") << QByteArray("authToken") << QVariant(QStringLiteral("t2"));
    QTest::newRow("apiKeyName") << QByteArray("apiKeyName") << QVariant(QStringLiteral("k2"));
    QTest::newRow("apiKeyValue") << QByteArray("apiKeyValue") << QVariant(QStringLiteral("v2"));
    QTest::newRow("apiKeyInQuery") << QByteArray("apiKeyInQuery") << QVariant(false);
    QTest::newRow("timeoutMs") << QByteArray("timeoutMs") << QVariant(1500);
}

void RequestEditorTest::settersWriteThroughAndMarkDirty()
{
    QFETCH(QByteArray, name);
    QFETCH(QVariant, value);

    m_editor->load(QStringLiteral("full"));
    QSignalSpy spy(m_editor.get(), &RequestEditor::requestChanged);
    QVERIFY(m_editor->setProperty(name.constData(), value));

    QCOMPARE(m_editor->property(name.constData()), value);
    QCOMPARE(spy.count(), 1);
    QVERIFY(m_manager->dirty());

    // A second editor sees only what was stored.
    RequestEditor reader(m_collection.get());
    reader.load(QStringLiteral("full"));
    QCOMPARE(reader.property(name.constData()), value);
}

void RequestEditorTest::unchangedValueStaysClean()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("url", QStringLiteral("{{baseUrl}}/items"));
    m_editor->setProperty("timeoutMs", 5000);
    QVERIFY(!m_manager->dirty());
}

void RequestEditorTest::invalidValuesAreIgnored()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("method", QStringLiteral("FETCH"));
    m_editor->setProperty("bodyMode", 3);
    m_editor->setProperty("rawBodyType", -1);
    m_editor->setProperty("authType", 4);
    m_editor->setProperty("timeoutMs", -1);

    QCOMPARE(m_editor->property("method").toString(), QStringLiteral("POST"));
    QCOMPARE(m_editor->property("bodyMode").toInt(), 1);
    QCOMPARE(m_editor->property("rawBodyType").toInt(), 2);
    QCOMPARE(m_editor->property("authType").toInt(), 3);
    QCOMPARE(m_editor->property("timeoutMs").toInt(), 5000);
    QVERIFY(!m_manager->dirty());
}

void RequestEditorTest::nameRenamesTreeNode()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("name", QStringLiteral("  Renamed  "));

    QCOMPARE(m_editor->property("name").toString(), QStringLiteral("Renamed"));
    QCOMPARE(m_collection->indexOfNode(QStringLiteral("full")).data(CollectionTreeModel::NameRole).toString(),
             QStringLiteral("Renamed"));
    QVERIFY(m_manager->dirty());
}

void RequestEditorTest::blankNameIsRefused()
{
    m_editor->load(QStringLiteral("full"));
    QSignalSpy spy(m_editor.get(), &RequestEditor::requestChanged);
    m_editor->setProperty("name", QStringLiteral("   "));

    QCOMPARE(m_editor->property("name").toString(), QStringLiteral("Full request"));
    QCOMPARE(spy.count(), 1); // Tells the name field to show the old value again.
    QVERIFY(!m_manager->dirty());
}

void RequestEditorTest::renamingInTreeUpdatesEditorName()
{
    m_editor->load(QStringLiteral("full"));
    QVERIFY(m_collection->rename(QStringLiteral("full"), QStringLiteral("From tree")));
    QCOMPARE(m_editor->property("name").toString(), QStringLiteral("From tree"));
}

void RequestEditorTest::methodChangeUpdatesTreeRole()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("method", QStringLiteral("PATCH"));
    QCOMPARE(m_collection->indexOfNode(QStringLiteral("full")).data(CollectionTreeModel::MethodRole).toString(),
             QStringLiteral("PATCH"));
}

void RequestEditorTest::paramsEditWritesThrough()
{
    m_editor->load(QStringLiteral("full"));
    KeyValueModel *model = m_editor->paramsModel();
    QVERIFY(model->setData(model->index(0), QStringLiteral("changed"), KeyValueModel::ValueRole));

    QCOMPARE(m_collection->request(QStringLiteral("full"))->queryParams.at(0).value,
             QStringLiteral("changed"));
    QVERIFY(m_manager->dirty());
}

void RequestEditorTest::headersEditWritesThrough()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->headersModel()->addRow();
    QCOMPARE(m_collection->request(QStringLiteral("full"))->headers.size(), 2);
    QVERIFY(m_manager->dirty());
}

void RequestEditorTest::formEditWritesThrough()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->formModel()->removeRow(0);
    QVERIFY(m_collection->request(QStringLiteral("full"))->body.form.isEmpty());
    QVERIFY(m_manager->dirty());
}

void RequestEditorTest::switchingRequestsKeepsBothEdits()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("url", QStringLiteral("http://first"));
    m_editor->load(QStringLiteral("other"));
    m_editor->setProperty("url", QStringLiteral("http://second"));

    QCOMPARE(m_collection->request(QStringLiteral("full"))->url, QStringLiteral("http://first"));
    QCOMPARE(m_collection->request(QStringLiteral("other"))->url, QStringLiteral("http://second"));

    m_editor->load(QStringLiteral("full"));
    QCOMPARE(m_editor->property("url").toString(), QStringLiteral("http://first"));
}

void RequestEditorTest::removingOpenRequestClearsEditor()
{
    m_editor->load(QStringLiteral("full"));
    QVERIFY(m_collection->remove(QStringLiteral("full")));

    QVERIFY(!m_editor->property("hasRequest").toBool());
    m_editor->setProperty("url", QStringLiteral("http://ignored"));
    QVERIFY(!m_collection->request(QStringLiteral("full")));
    QVERIFY(m_editor->property("url").toString().isEmpty());
}

void RequestEditorTest::removingParentFolderClearsEditor()
{
    m_editor->load(QStringLiteral("other"));
    QVERIFY(m_collection->remove(QStringLiteral("folder")));
    QVERIFY(!m_editor->property("hasRequest").toBool());
}

void RequestEditorTest::switchingWorkspaceClearsEditor()
{
    m_editor->load(QStringLiteral("full"));

    CollectionNode unrelated;
    unrelated.kind = CollectionNode::Kind::Request;
    unrelated.id = QStringLiteral("unrelated");
    QVERIFY(m_manager->openWorkspace(writeWorkspace(QStringLiteral("Second"), {unrelated})));
    QVERIFY(!m_editor->property("hasRequest").toBool());

    m_editor->load(QStringLiteral("unrelated"));
    m_manager->closeWorkspace();
    QVERIFY(!m_editor->property("hasRequest").toBool());
}

void RequestEditorTest::saveKeepsEditorOpen()
{
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("url", QStringLiteral("http://edited"));
    QVERIFY(m_manager->saveWorkspace());

    QCOMPARE(m_editor->property("requestId").toString(), QStringLiteral("full"));
    QCOMPARE(m_editor->property("url").toString(), QStringLiteral("http://edited"));
}

void RequestEditorTest::editsSurviveSaveAndReopen()
{
    const QString path = m_manager->activeWorkspacePath();
    m_editor->load(QStringLiteral("full"));
    m_editor->setProperty("url", QStringLiteral("http://persisted"));
    m_editor->setProperty("authType", 2);
    m_editor->headersModel()->addRow();
    QVERIFY(m_manager->saveWorkspace());
    m_manager->closeWorkspace();

    QVERIFY(m_manager->openWorkspace(path));
    m_editor->load(QStringLiteral("full"));
    QCOMPARE(m_editor->property("url").toString(), QStringLiteral("http://persisted"));
    QCOMPARE(m_editor->property("authType").toInt(), 2);
    QCOMPARE(m_editor->headersModel()->rowCount(), 2);
}

QTEST_GUILESS_MAIN(RequestEditorTest)

#include "tst_requesteditor.moc"
