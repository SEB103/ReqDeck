// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QtTest>

#include <memory>

#include "core/http/curlconverter.h"
#include "core/workspace/collectiontree.h"
#include "core/workspace/collectiontree.h"
#include "core/workspace/workspaceserializer.h"
#include "mockhttpserver.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/requesteditor.h"
#include "qmlapi/responseview.h"
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

    /*! Verifies which auth field is reported as holding typed text. */
    void literalCredentialFollowsAuthField();

    /*! Verifies that the typed credential is replaced by a reference to a variable. */
    void credentialBecomesVariableReference();

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

    /*! Verifies that a sent request ends in the response view. */
    void sendShowsResponse();

    /*! Verifies that an edit counts without saving first. */
    void sendUsesWorkingCopy();

    /*! Verifies that the open request reaches the clipboard unresolved. */
    void copyAsCurlWritesClipboard();

    /*! Verifies that nothing is copied without an open request. */
    void copyAsCurlWithoutRequestDoesNothing();

    /*! Verifies that a command becomes a request in the collection. */
    void importCurlAddsRequest();

    /*! Verifies that a broken command adds nothing. */
    void importCurlRefusesBrokenCommand();

    /*! Verifies that nothing is added without a workspace. */
    void importCurlAddsNothingWithoutWorkspace();

    /*! Verifies what the check reports. */
    void checkCurlReportsOutcome();

    /*! Verifies that options without an equivalent are listed. */
    void checkCurlListsIgnoredOptions();

    /*! Verifies that a finished request is reported with its response. */
    void finishedRequestIsReported();

    /*! Verifies that the report names the request that was sent. */
    void finishedReportsSentRequestAfterSwitching();

    /*! Verifies that a request that never left is not reported. */
    void buildErrorIsNotReported();

    /*! Verifies that variables come from the active environment. */
    void sendResolvesActiveEnvironment();

    /*! Verifies the builder options. */
    void sendSetsUserAgentFromOptions();

    /*! Verifies that an unknown variable in the URL sends nothing. */
    void unresolvedUrlVariableReportsBuildError();

    /*! Verifies that an unknown variable elsewhere sends and warns. */
    void unresolvedHeaderVariableSendsAndWarns();

    /*! Verifies the names a send would leave unresolved, each once, in order. */
    void missingVariablesListsUnresolved();

    /*! Verifies that one-time values resolve the request and are not stored. */
    void sendWithValuesUsesThemOnce();

    /*! Verifies that a request takes the auth and variables of its folder. */
    void sendUsesFolderAuthAndVariables();

    /*! Verifies that Continue past a certificate sends with the one-time values again. */
    void trustingCertificateKeepsOneTimeValues();

    /*! Verifies that nothing is sent without a request. */
    void sendWithoutRequestDoesNothing();

    /*! Verifies that a running request blocks a second one. */
    void sendWhileBusyIsIgnored();

    /*! Verifies cancelling. */
    void cancelAbortsAndReportsIt();

    /*! Verifies cancelling without a running request. */
    void cancelWhenIdleDoesNothing();

    /*! Verifies busy and canSend during a request. */
    void busyFollowsRequest();

    /*! Verifies that selecting another request lets the response arrive. */
    void selectingAnotherRequestKeepsResponseComing();

    /*! Verifies that another workspace ends a running request. */
    void workspaceSwitchAbortsRequest();

    /*! Verifies that closing the workspace ends a running request. */
    void closingWorkspaceAbortsRequest();

    /*! Verifies that a save leaves a running request alone. */
    void saveDoesNotAbortRequest();

    /*! Verifies that deleting the open request during a request is safe. */
    void deletingOpenRequestKeepsRunningRequestSafe();

    /*! Verifies that sending changes nothing in the workspace. */
    void sendDoesNotMarkDirty();

    /*! Verifies that Continue sends the failed request again, trusting only that host. */
    void trustingCertificateResendsToThatHost();

    /*! Verifies that Continue does nothing once another request is open. */
    void trustingCertificateNeedsItsRequest();

    /*! Verifies that Continue does nothing without a failed certificate check. */
    void trustingWithoutTlsFailureDoesNothing();

private:
    /*! Waits until the running request has finished. */
    static bool waitForResponse(const ResponseView &response);

    /*! Points the request r on the mock server at \a path, keeping the workspace clean. */
    void openRequestAt(const QString &path);

    /*! Mock server of the send tests. */
    MockHttpServer m_server;

    /*! HTTPS mock server with an untrusted certificate. */
    MockHttpServer m_tlsServer;

    /*! Opens the request "other" with \a url as its address. */
    void openRequestAtUrl(const QUrl &url);

    /*! Response view the editor reports to. */
    std::unique_ptr<ResponseView> m_response;

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
    m_response = std::make_unique<ResponseView>();
    m_editor = std::make_unique<RequestEditor>(m_manager.get(), m_collection.get(), m_response.get());
    if (m_server.port() == 0)
        QVERIFY(m_server.start());
    if (m_tlsServer.port() == 0)
        QVERIFY(m_tlsServer.startTls());
    QVERIFY(m_manager->openWorkspace(writeWorkspace(QStringLiteral("Main"), makeCollection())));
}

void RequestEditorTest::cleanup()
{
    m_editor.reset();
    m_response.reset();
    m_collection.reset();
    m_manager.reset();
    m_dir.reset();
}

void RequestEditorTest::literalCredentialFollowsAuthField()
{
    m_editor->load(QStringLiteral("full"));
    QCOMPARE(m_editor->property("literalCredential").toString(), QStringLiteral("apiKey"));
    QCOMPARE(m_editor->literalCredentialValue(), QStringLiteral("value"));

    m_editor->setApiKeyValue(QStringLiteral("{{key}}"));
    QCOMPARE(m_editor->property("literalCredential").toString(), QString());
    m_editor->setApiKeyValue(QStringLiteral(" {{a}}{{b}} "));
    QCOMPARE(m_editor->property("literalCredential").toString(), QString());
    m_editor->setApiKeyValue(QStringLiteral("x{{key}}"));
    QCOMPARE(m_editor->property("literalCredential").toString(), QStringLiteral("apiKey"));

    m_editor->setAuthType(2);
    QCOMPARE(m_editor->property("literalCredential").toString(), QStringLiteral("token"));
    m_editor->setAuthType(1);
    QCOMPARE(m_editor->property("literalCredential").toString(), QStringLiteral("password"));
    m_editor->setAuthPassword(QString());
    QCOMPARE(m_editor->property("literalCredential").toString(), QString());
    m_editor->setAuthType(0);
    QCOMPARE(m_editor->property("literalCredential").toString(), QString());
}

void RequestEditorTest::credentialBecomesVariableReference()
{
    m_editor->load(QStringLiteral("full"));

    m_editor->useVariableForCredential(QStringLiteral("apiKey2"));

    QCOMPARE(m_editor->property("apiKeyValue").toString(), QStringLiteral("{{apiKey2}}"));
    QCOMPARE(m_editor->property("literalCredential").toString(), QString());
    QVERIFY(m_manager->dirty());
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
    ResponseView readerResponse;
    RequestEditor reader(m_manager.get(), m_collection.get(), &readerResponse);
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
    m_editor->setProperty("authType", 5); // 4 is AuthType::Inherit
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

bool RequestEditorTest::waitForResponse(const ResponseView &response)
{
    return QTest::qWaitFor([&response]() { return !response.busy(); }, 8000);
}

void RequestEditorTest::openRequestAt(const QString &path)
{
    openRequestAtUrl(m_server.url(path));
}

void RequestEditorTest::openRequestAtUrl(const QUrl &url)
{
    WorkspaceData data = m_manager->workspaceData();
    RequestSpec spec;
    spec.url = url.toString();
    setNodeRequest(data.collection, QStringLiteral("other"), spec);
    const QString file = m_dir->filePath(QStringLiteral("sent.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(file, data));
    QVERIFY(m_manager->openWorkspace(file));
    m_editor->load(QStringLiteral("other"));
}

void RequestEditorTest::sendShowsResponse()
{
    openRequestAt(QStringLiteral("/status/201"));
    m_editor->send();

    QVERIFY(m_response->busy());
    QCOMPARE(m_response->requestName(), QStringLiteral("Other"));
    QVERIFY(waitForResponse(*m_response));
    QVERIFY(m_response->hasResponse());
    QCOMPARE(m_response->statusCode(), 201);
    QCOMPARE(m_response->bodyText(), QStringLiteral("status 201"));
}

void RequestEditorTest::trustingCertificateResendsToThatHost()
{
    openRequestAtUrl(m_tlsServer.url(QStringLiteral("/echo")));
    m_editor->send();
    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->property("canTrustCertificate").toBool(), true);
    QCOMPARE(m_response->property("certificateHost").toString(), QStringLiteral("127.0.0.1"));
    const int before = m_tlsServer.requestCount();

    m_editor->sendTrustingCertificate();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
    QCOMPARE(m_tlsServer.requestCount(), before + 1);
    QVERIFY(m_response->warningText().contains(QStringLiteral("127.0.0.1")));
    QCOMPARE(m_response->property("canTrustCertificate").toBool(), false);
}

void RequestEditorTest::trustingCertificateNeedsItsRequest()
{
    openRequestAtUrl(m_tlsServer.url(QStringLiteral("/echo")));
    m_editor->send();
    QVERIFY(waitForResponse(*m_response));
    m_editor->load(QStringLiteral("full"));
    const int before = m_tlsServer.requestCount();

    m_editor->sendTrustingCertificate();

    QVERIFY(!m_response->busy());
    QCOMPARE(m_tlsServer.requestCount(), before);
}

void RequestEditorTest::trustingWithoutTlsFailureDoesNothing()
{
    openRequestAt(QStringLiteral("/status/201"));
    m_editor->send();
    QVERIFY(waitForResponse(*m_response));
    const int before = m_server.requestCount();

    m_editor->sendTrustingCertificate();

    QVERIFY(!m_response->busy());
    QCOMPARE(m_server.requestCount(), before);
}

void RequestEditorTest::sendUsesWorkingCopy()
{
    openRequestAt(QStringLiteral("/status/201"));
    m_editor->setProperty("url", m_server.url(QStringLiteral("/status/404")).toString());
    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 404);
}

void RequestEditorTest::copyAsCurlWritesClipboard()
{
    m_editor->load(QStringLiteral("full"));
    QSignalSpy copied(m_editor.get(), &RequestEditor::copiedToClipboard);

    m_editor->copyAsCurl();

    QCOMPARE(copied.count(), 1);
    const QString command = QGuiApplication::clipboard()->text();
    QVERIFY(command.contains(QStringLiteral("{{baseUrl}}/items")));
    const CurlConverter::ParseResult parsed = CurlConverter::parse(command);
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.request.method, HttpMethod::Post);
    QVERIFY(parsed.request.url.startsWith(QStringLiteral("{{baseUrl}}/items")));
}

void RequestEditorTest::copyAsCurlWithoutRequestDoesNothing()
{
    QGuiApplication::clipboard()->setText(QStringLiteral("before"));
    m_editor->clear();
    QSignalSpy copied(m_editor.get(), &RequestEditor::copiedToClipboard);

    m_editor->copyAsCurl();

    QCOMPARE(copied.count(), 0);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("before"));
}

void RequestEditorTest::importCurlAddsRequest()
{
    const QString id = m_editor->importCurl(
        QStringLiteral("curl -X POST http://x/items -H 'A: b' -d '{}'"), QStringLiteral("folder"));

    QVERIFY(!id.isEmpty());
    const CollectionNode *node = findNode(m_manager->workspaceData().collection, id);
    QVERIFY(node);
    QCOMPARE(node->name, QStringLiteral("POST http://x/items"));
    QCOMPARE(node->request.url, QStringLiteral("http://x/items"));
    QCOMPARE(node->request.headers.constFirst().key, QStringLiteral("A"));
    QCOMPARE(m_collection->parentForNewItem(id), QStringLiteral("folder"));
    QVERIFY(m_manager->dirty());
}

void RequestEditorTest::importCurlRefusesBrokenCommand()
{
    QVERIFY(m_editor->importCurl(QStringLiteral("not a command"), {}).isEmpty());
    QVERIFY(!m_manager->dirty());
}

void RequestEditorTest::importCurlAddsNothingWithoutWorkspace()
{
    m_manager->closeWorkspace();

    QVERIFY(m_editor->importCurl(QStringLiteral("curl http://x"), {}).isEmpty());
}

void RequestEditorTest::checkCurlReportsOutcome()
{
    const QVariantMap good = m_editor->checkCurl(QStringLiteral("curl -X PUT http://x"));
    QCOMPARE(good.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(good.value(QStringLiteral("summary")).toString(), QStringLiteral("PUT http://x"));

    const QVariantMap bad = m_editor->checkCurl(QStringLiteral("not a command"));
    QCOMPARE(bad.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(!bad.value(QStringLiteral("error")).toString().isEmpty());
}

void RequestEditorTest::checkCurlListsIgnoredOptions()
{
    const QVariantMap result = m_editor->checkCurl(QStringLiteral("curl --compressed http://x"));

    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(result.value(QStringLiteral("ignored")).toStringList(),
             QStringList({QStringLiteral("--compressed")}));
}

void RequestEditorTest::finishedRequestIsReported()
{
    openRequestAt(QStringLiteral("/status/201"));
    QSignalSpy finished(m_editor.get(), &RequestEditor::requestFinished);

    m_editor->send();
    QVERIFY(waitForResponse(*m_response));

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).value<RequestSpec>().url,
             m_server.url(QStringLiteral("/status/201")).toString());
    QCOMPARE(finished.at(0).at(1).value<ResponseData>().statusCode, 201);
}

void RequestEditorTest::finishedReportsSentRequestAfterSwitching()
{
    openRequestAt(QStringLiteral("/delay/100"));
    const QString sent = m_server.url(QStringLiteral("/delay/100")).toString();
    QSignalSpy finished(m_editor.get(), &RequestEditor::requestFinished);

    m_editor->send();
    m_editor->load(QStringLiteral("full"));
    QVERIFY(waitForResponse(*m_response));

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(0).value<RequestSpec>().url, sent);
}

void RequestEditorTest::buildErrorIsNotReported()
{
    openRequestAt(QStringLiteral("/status/201"));
    m_editor->setProperty("url", QStringLiteral("{{missing}}/x"));
    QSignalSpy finished(m_editor.get(), &RequestEditor::requestFinished);

    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(finished.count(), 0);
}

void RequestEditorTest::sendResolvesActiveEnvironment()
{
    openRequestAt(QStringLiteral("/echo"));

    Environment environment;
    environment.id = QStringLiteral("e1");
    environment.name = QStringLiteral("Env");
    environment.variables = {{QStringLiteral("base"), m_server.url(QString()).toString(), true, false}};
    WorkspaceData data = m_manager->workspaceData();
    data.environments = {environment};
    data.activeEnvironmentId = environment.id;
    m_manager->updateWorkspaceData(data);

    m_editor->setProperty("url", QStringLiteral("{{base}}/status/202"));
    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 202);
    QVERIFY(m_response->warningText().isEmpty());
}

void RequestEditorTest::sendSetsUserAgentFromOptions()
{
    openRequestAt(QStringLiteral("/echo"));
    RequestBuilder::Options options;
    options.userAgent = QStringLiteral("ReqDeck/test");
    m_editor->setBuilderOptions(options);
    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QVERIFY(m_response->bodyText().contains(QStringLiteral("ReqDeck/test")));
}

void RequestEditorTest::unresolvedUrlVariableReportsBuildError()
{
    openRequestAt(QStringLiteral("/echo"));
    const int requests = m_server.requestCount();
    m_editor->setProperty("url", QStringLiteral("{{nope}}/x"));
    m_editor->send();

    QVERIFY(!m_response->busy());
    QVERIFY(m_response->hasResponse());
    QVERIFY(m_response->errorText().contains(QStringLiteral("{{nope}}")));
    QCOMPARE(m_server.requestCount(), requests);
    QVERIFY(m_editor->property("canSend").toBool());
}

void RequestEditorTest::unresolvedHeaderVariableSendsAndWarns()
{
    openRequestAt(QStringLiteral("/status/200"));
    m_editor->headersModel()->addRow();
    KeyValueModel *headers = m_editor->headersModel();
    headers->setData(headers->index(0), QStringLiteral("X-A"), KeyValueModel::KeyRole);
    headers->setData(headers->index(0), QStringLiteral("{{missing}}"), KeyValueModel::ValueRole);
    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
    QCOMPARE(m_response->warningText(), QStringLiteral("Unresolved variables: {{missing}}"));
}

void RequestEditorTest::missingVariablesListsUnresolved()
{
    openRequestAt(QStringLiteral("/status/200"));
    QCOMPARE(m_editor->missingVariables(), QStringList());
    m_editor->setProperty("url", QStringLiteral("{{host}}/x/{{host}}"));
    m_editor->headersModel()->addRow();
    KeyValueModel *headers = m_editor->headersModel();
    headers->setData(headers->index(0), QStringLiteral("X-A"), KeyValueModel::KeyRole);
    headers->setData(headers->index(0), QStringLiteral("{{token}}"), KeyValueModel::ValueRole);

    QCOMPARE(m_editor->missingVariables(), QStringList({QStringLiteral("host"), QStringLiteral("token")}));
}

void RequestEditorTest::sendWithValuesUsesThemOnce()
{
    openRequestAt(QStringLiteral("/status/200"));
    m_editor->setProperty("url", QStringLiteral("{{base}}/status/203"));
    const WorkspaceData before = m_manager->workspaceData();

    m_editor->sendWithValues({{QStringLiteral("base"), m_server.url(QString()).toString()}});

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 203);
    QVERIFY(m_response->warningText().isEmpty());
    QCOMPARE(m_manager->workspaceData().environments, before.environments);
    QCOMPARE(m_editor->missingVariables(), QStringList({QStringLiteral("base")}));
}

void RequestEditorTest::sendUsesFolderAuthAndVariables()
{
    openRequestAt(QStringLiteral("/echo"));
    WorkspaceData data = m_manager->workspaceData();
    data.collection[0].folder.auth.type = AuthType::Bearer;
    data.collection[0].folder.auth.token = QStringLiteral("{{token}}");
    data.collection[0].folder.variables = {{QStringLiteral("token"), QStringLiteral("from-folder"), true}};
    m_manager->updateWorkspaceData(data);
    m_editor->setAuthType(int(AuthType::Inherit));

    QCOMPARE(m_editor->missingVariables(), QStringList());
    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QVERIFY2(m_response->bodyText().contains(QStringLiteral("Bearer from-folder")), qPrintable(m_response->bodyText()));
}

void RequestEditorTest::trustingCertificateKeepsOneTimeValues()
{
    openRequestAtUrl(m_tlsServer.url(QStringLiteral("/status/200")));
    m_editor->setProperty("url", m_tlsServer.url(QString()).toString() + QStringLiteral("/{{path}}"));
    m_editor->sendWithValues({{QStringLiteral("path"), QStringLiteral("echo")}});
    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->property("canTrustCertificate").toBool(), true);
    const int before = m_tlsServer.requestCount();

    m_editor->sendTrustingCertificate();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
    QCOMPARE(m_tlsServer.requestCount(), before + 1);
}

void RequestEditorTest::sendWithoutRequestDoesNothing()
{
    m_editor->send();
    QVERIFY(!m_response->busy());
    QVERIFY(!m_response->hasResponse());
    QVERIFY(!m_editor->property("canSend").toBool());
}

void RequestEditorTest::sendWhileBusyIsIgnored()
{
    openRequestAt(QStringLiteral("/delay/200"));
    const int requests = m_server.requestCount();
    m_editor->send();
    m_editor->send();

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
    QCOMPARE(m_server.requestCount(), requests + 1);
}

void RequestEditorTest::cancelAbortsAndReportsIt()
{
    openRequestAt(QStringLiteral("/delay/3000"));
    m_editor->send();
    QTest::qWait(50);
    m_editor->cancel();

    QVERIFY(!m_response->busy());
    QVERIFY(m_response->hasResponse());
    QVERIFY(!m_response->errorText().isEmpty());
    QVERIFY(!m_editor->property("busy").toBool());
}

void RequestEditorTest::cancelWhenIdleDoesNothing()
{
    openRequestAt(QStringLiteral("/echo"));
    m_editor->cancel();
    QVERIFY(!m_response->hasResponse());
}

void RequestEditorTest::busyFollowsRequest()
{
    openRequestAt(QStringLiteral("/delay/100"));
    QVERIFY(m_editor->property("canSend").toBool());
    QSignalSpy spy(m_editor.get(), &RequestEditor::busyChanged);

    m_editor->send();
    QVERIFY(m_editor->property("busy").toBool());
    QVERIFY(!m_editor->property("canSend").toBool());

    QVERIFY(waitForResponse(*m_response));
    QVERIFY(!m_editor->property("busy").toBool());
    QVERIFY(m_editor->property("canSend").toBool());
    QCOMPARE(spy.count(), 2);
}

void RequestEditorTest::selectingAnotherRequestKeepsResponseComing()
{
    openRequestAt(QStringLiteral("/delay/200"));
    m_editor->send();
    m_editor->load(QStringLiteral("full"));

    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
    QCOMPARE(m_response->requestName(), QStringLiteral("Other"));
}

void RequestEditorTest::workspaceSwitchAbortsRequest()
{
    openRequestAt(QStringLiteral("/delay/3000"));
    m_editor->send();
    QVERIFY(m_response->busy());

    QVERIFY(m_manager->openWorkspace(writeWorkspace(QStringLiteral("Other"), {})));

    QVERIFY(!m_response->busy());
    QVERIFY(!m_response->hasResponse());
    QVERIFY(!m_editor->property("busy").toBool());
}

void RequestEditorTest::closingWorkspaceAbortsRequest()
{
    openRequestAt(QStringLiteral("/delay/3000"));
    m_editor->send();

    m_manager->closeWorkspace();

    QVERIFY(!m_response->busy());
    QVERIFY(!m_response->hasResponse());
}

void RequestEditorTest::saveDoesNotAbortRequest()
{
    openRequestAt(QStringLiteral("/delay/200"));
    m_editor->send();

    QVERIFY(m_manager->saveWorkspace());
    QVERIFY(m_manager->saveWorkspaceAs(m_dir->filePath(QStringLiteral("Copy.reqdeck"))));

    QVERIFY(m_response->busy());
    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
}

void RequestEditorTest::deletingOpenRequestKeepsRunningRequestSafe()
{
    openRequestAt(QStringLiteral("/delay/100"));
    m_editor->send();
    QVERIFY(m_collection->remove(QStringLiteral("other")));

    QVERIFY(!m_editor->property("hasRequest").toBool());
    QVERIFY(waitForResponse(*m_response));
    QCOMPARE(m_response->statusCode(), 200);
}

void RequestEditorTest::sendDoesNotMarkDirty()
{
    openRequestAt(QStringLiteral("/status/200"));
    QVERIFY(!m_manager->dirty());
    m_editor->send();
    QVERIFY(waitForResponse(*m_response));
    QVERIFY(!m_manager->dirty());
}

/*!
 * \brief Runs the tests on the offscreen platform.
 *
 * Its clipboard lives in this process, so the tests leave the clipboard of the
 * desktop alone.
 */
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    RequestEditorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_requesteditor.moc"
