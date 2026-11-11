// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest>

#include <algorithm>
#include <memory>

#include "core/workspace/workspaceserializer.h"
#include "qmlapi/environmentmodel.h"
#include "qmlapi/environmentvariablesmodel.h"
#include "qmlapi/variablehighlighter.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Returns the environments Dev, with baseUrl, and Live.
 */
static QList<Environment> makeEnvironments()
{
    Environment dev;
    dev.id = QStringLiteral("dev");
    dev.name = QStringLiteral("Dev");
    dev.variables = {{QStringLiteral("baseUrl"), QStringLiteral("http://dev"), true, false}};

    Environment live;
    live.id = QStringLiteral("live");
    live.name = QStringLiteral("Live");
    live.variables = {{QStringLiteral("baseUrl"), QStringLiteral("http://live"), true, false}};

    return {dev, live};
}

/*!
 * \internal
 * \brief Returns whether the first line of \a document has an underlined range.
 */
static bool hasUnderline(const QTextDocument &document)
{
    const QList<QTextLayout::FormatRange> formats = document.firstBlock().layout()->formats();
    return std::any_of(formats.cbegin(), formats.cend(), [](const QTextLayout::FormatRange &range) {
        return range.format.fontUnderline();
    });
}

/*! Verifies the environments of a real workspace. */
class EnvironmentModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Opens a workspace with Dev and Live for each test. */
    void init();

    /*! Releases the per-test objects. */
    void cleanup();

    /*! Verifies rows, names, and roles. */
    void listsEnvironments();

    /*! Verifies that choosing the active environment reaches the workspace. */
    void settingActiveIndexStoresId();

    /*! Verifies that the active role follows the active environment. */
    void activeRoleFollowsActiveIndex();

    /*! Verifies a file naming an environment it does not contain. */
    void unknownActiveIdReadsAsNone();

    /*! Verifies a new environment. */
    void addAppendsAndSelectsForEditing();

    /*! Verifies that a copy has its own id and the same variables. */
    void duplicateCopiesValuesWithNewId();

    /*! Verifies trimming and blank names. */
    void renameTrimsAndRefusesBlank();

    /*! Verifies that the name of the edited environment reports its change. */
    void renameEditedEmitsEditedName();

    /*! Verifies that removing the active environment leaves none active. */
    void removingActiveClearsIt();

    /*! Verifies that an earlier removal keeps the active environment. */
    void removingBeforeActiveKeepsIt();

    /*! Verifies where editing goes after the edited environment is removed. */
    void removingEditedMovesEditing();

    /*! Verifies that a removal before the edited row keeps editing it. */
    void removingBeforeEditedKeepsIt();

    /*! Verifies that listeners of a removal can read the edited name. */
    void removalSignalsSeeValidEditedRow();

    /*! Verifies that table edits reach the edited environment only. */
    void editingVariablesWritesThrough();

    /*! Verifies that editing does not choose the active environment. */
    void editedIsIndependentOfActive();

    /*! Verifies that the URL highlight follows the active variables. */
    void highlightFollowsActiveEnvironment();

    /*! Verifies that secrets read late reach the URL highlight. */
    void retriedSecretsReachHighlight();

    /*! Verifies that a secret stays out of the file and comes back. */
    void secretStaysOutOfWorkspaceFile();

    /*! Verifies that another workspace replaces everything. */
    void switchingWorkspaceResets();

    /*! Verifies that closing the workspace empties the model. */
    void closingWorkspaceEmptiesModel();

    /*! Verifies that a save keeps rows and selection. */
    void saveDoesNotResetModel();

    /*! Verifies that nothing happens without a workspace. */
    void withoutWorkspaceDoesNothing();

    /*! Verifies the names that resolve in the active environment, and their signal. */
    void activeVariableNamesFollowActive();

    /*! Verifies that the variables of the folders above the scope request are known. */
    void folderVariablesJoinTheScope();

    /*! Verifies that setVariable() replaces a value or adds a plain variable. */
    void setVariableAddsOrReplaces();

    /*! Verifies that a secret variable goes to the active environment under a free name. */
    void addSecretVariableToActive();

    /*! Verifies that without an active environment a Default one is made active for it. */
    void addSecretVariableCreatesDefault();

private:
    /*! Saves \a data and opens it in the manager. */
    bool openWith(const WorkspaceData &data, const QString &fileName);

    /*! Returns the environments held by the workspace. */
    const QList<Environment> &stored() const { return m_manager->workspaceData().environments; }

    /*! Directory of the test workspaces. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Real workspace manager. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Model under test. */
    std::unique_ptr<EnvironmentModel> m_model;

    /*! Consistency checks on the model under test. */
    std::unique_ptr<QAbstractItemModelTester> m_tester;
};

bool EnvironmentModelTest::openWith(const WorkspaceData &data, const QString &fileName)
{
    const QString path = m_dir->filePath(fileName);
    return WorkspaceSerializer::save(path, data) && m_manager->openWorkspace(path);
}

void EnvironmentModelTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));

    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QStringLiteral("Test");
    data.environments = makeEnvironments();
    QVERIFY(openWith(data, QStringLiteral("test.reqdeck")));

    m_model = std::make_unique<EnvironmentModel>(m_manager.get());
    m_tester = std::make_unique<QAbstractItemModelTester>(
        m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
}

void EnvironmentModelTest::cleanup()
{
    m_tester.reset();
    m_model.reset();
    m_manager.reset();
    m_dir.reset();
}

void EnvironmentModelTest::folderVariablesJoinTheScope()
{
    WorkspaceData data = m_manager->workspaceData();
    CollectionNode request;
    request.id = newItemId();
    request.kind = CollectionNode::Kind::Request;
    request.name = QStringLiteral("List");
    CollectionNode folder;
    folder.id = newItemId();
    folder.kind = CollectionNode::Kind::Folder;
    folder.name = QStringLiteral("Api");
    folder.folder.variables = {{QStringLiteral("apiRoot"), QStringLiteral("https://api.test"), true},
                               {QStringLiteral("off"), QStringLiteral("x"), false}};
    folder.children = {request};
    data.collection = {folder};
    QVERIFY(openWith(data, QStringLiteral("folders.reqdeck")));
    QSignalSpy changed(m_model.get(), &EnvironmentModel::activeVariablesChanged);

    m_model->setVariableScope(request.id);

    QVERIFY(changed.count() > 0);
    const QStringList names = m_model->property("activeVariableNames").toStringList();
    QVERIFY2(names.contains(QStringLiteral("apiRoot")), qPrintable(names.join(u',')));
    QVERIFY(!names.contains(QStringLiteral("off")));

    m_model->setVariableScope(QString());
    QVERIFY(!m_model->property("activeVariableNames").toStringList().contains(QStringLiteral("apiRoot")));
}

void EnvironmentModelTest::activeVariableNamesFollowActive()
{
    QSignalSpy changed(m_model.get(), &EnvironmentModel::activeVariablesChanged);
    QCOMPARE(m_model->property("activeVariableNames").toStringList(), QStringList());

    m_model->setActiveIndex(0);

    QCOMPARE(m_model->property("activeVariableNames").toStringList(), QStringList({QStringLiteral("baseUrl")}));
    QVERIFY(changed.count() > 0);
}

void EnvironmentModelTest::setVariableAddsOrReplaces()
{
    m_model->setActiveIndex(0);

    m_model->setVariable(QStringLiteral("baseUrl"), QStringLiteral("http://other"));
    m_model->setVariable(QStringLiteral("page"), QStringLiteral("2"));

    const QList<EnvironmentVariable> &variables = stored().at(0).variables;
    QCOMPARE(variables.size(), 2);
    QCOMPARE(variables.at(0).value, QStringLiteral("http://other"));
    QCOMPARE(variables.at(1).key, QStringLiteral("page"));
    QVERIFY(!variables.at(1).secret);
    QVERIFY(variables.at(1).enabled);
}

void EnvironmentModelTest::addSecretVariableToActive()
{
    m_model->setActiveIndex(1);
    m_model->setEditedRow(1);

    QCOMPARE(m_model->addSecretVariable(QStringLiteral("token"), QStringLiteral("abc")), QStringLiteral("token"));
    QCOMPARE(m_model->addSecretVariable(QStringLiteral("token"), QStringLiteral("def")), QStringLiteral("token2"));

    const QList<EnvironmentVariable> &variables = stored().at(1).variables;
    QCOMPARE(variables.size(), 3);
    QCOMPARE(variables.at(1).key, QStringLiteral("token"));
    QCOMPARE(variables.at(1).value, QStringLiteral("abc"));
    QVERIFY(variables.at(1).enabled);
    QVERIFY(variables.at(1).secret);
    QCOMPARE(variables.at(2).key, QStringLiteral("token2"));
    QCOMPARE(m_model->variablesModel()->rowCount(), 3);
    QCOMPARE(stored().at(0).variables.size(), 1);
    QVERIFY(m_manager->dirty());
}

void EnvironmentModelTest::addSecretVariableCreatesDefault()
{
    QCOMPARE(m_model->activeIndex(), -1);

    QCOMPARE(m_model->addSecretVariable(QStringLiteral("password"), QStringLiteral("p")), QStringLiteral("password"));

    QCOMPARE(m_model->rowCount(), 3);
    QCOMPARE(m_model->activeIndex(), 2);
    QCOMPARE(stored().at(2).name, QStringLiteral("Default"));
    QCOMPARE(stored().at(2).variables.size(), 1);
    QVERIFY(stored().at(2).variables.at(0).secret);

    // A second credential goes to the same environment, now active.
    QCOMPARE(m_model->addSecretVariable(QStringLiteral("token"), QStringLiteral("t")), QStringLiteral("token"));
    QCOMPARE(m_model->rowCount(), 3);
    QCOMPARE(stored().at(2).variables.size(), 2);
}

void EnvironmentModelTest::listsEnvironments()
{
    QCOMPARE(m_model->rowCount(), 2);
    QCOMPARE(m_model->names(), QStringList({QStringLiteral("Dev"), QStringLiteral("Live")}));
    QCOMPARE(m_model->data(m_model->index(1), EnvironmentModel::IdRole).toString(),
             QStringLiteral("live"));
    QCOMPARE(m_model->data(m_model->index(1), EnvironmentModel::NameRole).toString(),
             QStringLiteral("Live"));
    QCOMPARE(m_model->activeIndex(), -1);
    QCOMPARE(m_model->editedRow(), -1);
}

void EnvironmentModelTest::settingActiveIndexStoresId()
{
    QSignalSpy active(m_model.get(), &EnvironmentModel::activeChanged);

    m_model->setActiveIndex(1);

    QCOMPARE(m_manager->workspaceData().activeEnvironmentId, QStringLiteral("live"));
    QVERIFY(m_manager->dirty());
    QCOMPARE(active.count(), 1);

    m_model->setActiveIndex(5);
    QCOMPARE(m_model->activeIndex(), 1);
    QCOMPARE(active.count(), 1);

    m_model->setActiveIndex(-1);
    QVERIFY(m_manager->workspaceData().activeEnvironmentId.isEmpty());
    QCOMPARE(active.count(), 2);
}

void EnvironmentModelTest::activeRoleFollowsActiveIndex()
{
    QSignalSpy changed(m_model.get(), &QAbstractItemModel::dataChanged);

    m_model->setActiveIndex(0);
    m_model->setActiveIndex(1);

    QVERIFY(!m_model->data(m_model->index(0), EnvironmentModel::ActiveRole).toBool());
    QVERIFY(m_model->data(m_model->index(1), EnvironmentModel::ActiveRole).toBool());
    QCOMPARE(changed.count(), 3);
}

void EnvironmentModelTest::unknownActiveIdReadsAsNone()
{
    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QStringLiteral("Other");
    data.environments = makeEnvironments();
    data.activeEnvironmentId = QStringLiteral("ghost");
    QVERIFY(openWith(data, QStringLiteral("ghost.reqdeck")));

    QCOMPARE(m_model->activeIndex(), -1);
}

void EnvironmentModelTest::addAppendsAndSelectsForEditing()
{
    QSignalSpy names(m_model.get(), &EnvironmentModel::namesChanged);

    const int row = m_model->addEnvironment();

    QCOMPARE(row, 2);
    QCOMPARE(m_model->editedRow(), 2);
    QCOMPARE(m_model->editedName(), QStringLiteral("New environment"));
    QCOMPARE(m_model->variablesModel()->rowCount(), 0);
    QVERIFY(!stored().at(2).id.isEmpty());
    QCOMPARE(names.count(), 1);
    QVERIFY(m_manager->dirty());
}

void EnvironmentModelTest::duplicateCopiesValuesWithNewId()
{
    const int row = m_model->duplicateEnvironment(0);

    QCOMPARE(row, 2);
    QCOMPARE(m_model->editedRow(), 2);
    QCOMPARE(stored().at(2).name, QStringLiteral("Dev copy"));
    QVERIFY(stored().at(2).id != stored().at(0).id);
    QVERIFY(stored().at(2).variables == stored().at(0).variables);
    QCOMPARE(m_model->variablesModel()->rowCount(), 1);
    QCOMPARE(m_model->duplicateEnvironment(9), -1);
}

void EnvironmentModelTest::renameTrimsAndRefusesBlank()
{
    QSignalSpy names(m_model.get(), &EnvironmentModel::namesChanged);

    QVERIFY(m_model->renameEnvironment(0, QStringLiteral("  Stage ")));
    QCOMPARE(stored().at(0).name, QStringLiteral("Stage"));
    QCOMPARE(names.count(), 1);

    QVERIFY(!m_model->renameEnvironment(0, QStringLiteral("   ")));
    QCOMPARE(stored().at(0).name, QStringLiteral("Stage"));

    QVERIFY(!m_model->renameEnvironment(7, QStringLiteral("Other")));
    QCOMPARE(names.count(), 1);
}

void EnvironmentModelTest::renameEditedEmitsEditedName()
{
    m_model->setEditedRow(0);
    QSignalSpy name(m_model.get(), &EnvironmentModel::editedNameChanged);

    QVERIFY(m_model->renameEnvironment(1, QStringLiteral("Other row")));
    QCOMPARE(name.count(), 0);

    QVERIFY(m_model->renameEnvironment(0, QStringLiteral("Stage")));
    QCOMPARE(name.count(), 1);
    QCOMPARE(m_model->editedName(), QStringLiteral("Stage"));
}

void EnvironmentModelTest::removingActiveClearsIt()
{
    m_model->setActiveIndex(0);
    QSignalSpy active(m_model.get(), &EnvironmentModel::activeChanged);

    m_model->removeEnvironment(0);

    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->activeIndex(), -1);
    QVERIFY(m_manager->workspaceData().activeEnvironmentId.isEmpty());
    QCOMPARE(active.count(), 1);
}

void EnvironmentModelTest::removingBeforeActiveKeepsIt()
{
    m_model->setActiveIndex(1);
    QSignalSpy active(m_model.get(), &EnvironmentModel::activeChanged);

    m_model->removeEnvironment(0);

    QCOMPARE(m_manager->workspaceData().activeEnvironmentId, QStringLiteral("live"));
    QCOMPARE(m_model->activeIndex(), 0);
    QCOMPARE(active.count(), 1);
}

void EnvironmentModelTest::removingEditedMovesEditing()
{
    m_model->setEditedRow(1);

    m_model->removeEnvironment(1);
    QCOMPARE(m_model->editedRow(), 0);
    QCOMPARE(m_model->editedName(), QStringLiteral("Dev"));
    QCOMPARE(m_model->variablesModel()->rows().constFirst().value, QStringLiteral("http://dev"));

    m_model->removeEnvironment(0);
    QCOMPARE(m_model->editedRow(), -1);
    QVERIFY(m_model->editedName().isEmpty());
    QCOMPARE(m_model->variablesModel()->rowCount(), 0);
}

void EnvironmentModelTest::removingBeforeEditedKeepsIt()
{
    m_model->setEditedRow(1);
    m_model->variablesModel()->addRow();

    m_model->removeEnvironment(0);

    QCOMPARE(m_model->editedRow(), 0);
    QCOMPARE(m_model->editedName(), QStringLiteral("Live"));
    QCOMPARE(m_model->variablesModel()->rowCount(), 2);
}

void EnvironmentModelTest::removalSignalsSeeValidEditedRow()
{
    m_model->setEditedRow(1);
    QStringList seen;
    const auto read = [&] { seen.append(m_model->editedName()); };
    connect(m_model.get(), &QAbstractItemModel::rowsRemoved, this, read);
    connect(m_model.get(), &EnvironmentModel::namesChanged, this, read);

    m_model->removeEnvironment(1);

    QCOMPARE(seen, QStringList({QStringLiteral("Dev"), QStringLiteral("Dev")}));
}

void EnvironmentModelTest::editingVariablesWritesThrough()
{
    m_model->setEditedRow(1);
    EnvironmentVariablesModel *variables = m_model->variablesModel();

    variables->setData(variables->index(0), QStringLiteral("http://new"),
                       EnvironmentVariablesModel::ValueRole);

    QCOMPARE(stored().at(1).variables.constFirst().value, QStringLiteral("http://new"));
    QCOMPARE(stored().at(0).variables.constFirst().value, QStringLiteral("http://dev"));
    QVERIFY(m_manager->dirty());
}

void EnvironmentModelTest::editedIsIndependentOfActive()
{
    m_model->setActiveIndex(0);

    m_model->setEditedRow(1);

    QCOMPARE(m_model->activeIndex(), 0);
    QCOMPARE(m_model->editedName(), QStringLiteral("Live"));
}

void EnvironmentModelTest::highlightFollowsActiveEnvironment()
{
    QTextDocument document(QStringLiteral("{{baseUrl}}"));
    m_model->urlHighlighter()->setDocument(&document);
    m_model->urlHighlighter()->rehighlight();
    QVERIFY(hasUnderline(document));

    m_model->setActiveIndex(0);
    QVERIFY(!hasUnderline(document));

    m_model->setEditedRow(0);
    EnvironmentVariablesModel *variables = m_model->variablesModel();
    variables->setData(variables->index(0), false, EnvironmentVariablesModel::EnabledRole);
    QVERIFY(hasUnderline(document));

    variables->setData(variables->index(0), true, EnvironmentVariablesModel::EnabledRole);
    QVERIFY(!hasUnderline(document));

    m_model->removeEnvironment(0);
    QVERIFY(hasUnderline(document));
}

void EnvironmentModelTest::retriedSecretsReachHighlight()
{
    m_model->setActiveIndex(0);
    m_model->setEditedRow(0);
    EnvironmentVariablesModel *variables = m_model->variablesModel();
    variables->addRow();
    const QModelIndex row = variables->index(1);
    variables->setData(row, QStringLiteral("token"), EnvironmentVariablesModel::KeyRole);
    variables->setData(row, true, EnvironmentVariablesModel::SecretRole);
    variables->setData(row, QStringLiteral("value"), EnvironmentVariablesModel::ValueRole);
    QVERIFY(m_manager->saveWorkspace());

    const QString path = m_manager->activeWorkspacePath();
    const QString secrets = m_dir->filePath(
        QStringLiteral("secrets/%1.json").arg(m_manager->workspaceData().id));
    const QString aside = secrets + QStringLiteral(".aside");
    m_manager->closeWorkspace();
    QVERIFY(QFile::rename(secrets, aside));
    QVERIFY(QDir().mkpath(secrets));
    QVERIFY(m_manager->openWorkspace(path));

    QTextDocument document(QStringLiteral("{{token}}"));
    m_model->urlHighlighter()->setDocument(&document);
    m_model->urlHighlighter()->rehighlight();
    QVERIFY(hasUnderline(document));

    QVERIFY(QDir().rmdir(secrets));
    QVERIFY(QFile::rename(aside, secrets));
    QVERIFY(m_manager->retrySecrets());

    QVERIFY(!hasUnderline(document));
}

void EnvironmentModelTest::secretStaysOutOfWorkspaceFile()
{
    const QString secret = QStringLiteral("s3cret-value");
    m_model->setEditedRow(0);
    EnvironmentVariablesModel *variables = m_model->variablesModel();
    variables->addRow();
    const QModelIndex row = variables->index(1);
    variables->setData(row, QStringLiteral("token"), EnvironmentVariablesModel::KeyRole);
    variables->setData(row, true, EnvironmentVariablesModel::SecretRole);
    variables->setData(row, secret, EnvironmentVariablesModel::ValueRole);

    QVERIFY(m_manager->saveWorkspace());

    QFile file(m_dir->filePath(QStringLiteral("test.reqdeck")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!file.readAll().contains(secret.toUtf8()));

    const QString path = m_dir->filePath(QStringLiteral("test.reqdeck"));
    m_manager->closeWorkspace();
    QVERIFY(m_manager->openWorkspace(path));
    m_model->setEditedRow(0);
    QCOMPARE(variables->rows().at(1).value, secret);
}

void EnvironmentModelTest::switchingWorkspaceResets()
{
    m_model->setEditedRow(1);
    QSignalSpy reset(m_model.get(), &QAbstractItemModel::modelReset);
    QSignalSpy names(m_model.get(), &EnvironmentModel::namesChanged);
    QSignalSpy active(m_model.get(), &EnvironmentModel::activeChanged);

    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QStringLiteral("Other");
    Environment single;
    single.id = QStringLiteral("only");
    single.name = QStringLiteral("Only");
    data.environments = {single};
    data.activeEnvironmentId = QStringLiteral("only");
    QVERIFY(openWith(data, QStringLiteral("other.reqdeck")));

    QCOMPARE(reset.count(), 1);
    QCOMPARE(names.count(), 1);
    QCOMPARE(active.count(), 1);
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->activeIndex(), 0);
    QCOMPARE(m_model->editedRow(), -1);
    QCOMPARE(m_model->variablesModel()->rowCount(), 0);
}

void EnvironmentModelTest::closingWorkspaceEmptiesModel()
{
    m_model->setEditedRow(1);

    m_manager->closeWorkspace();

    QCOMPARE(m_model->rowCount(), 0);
    QVERIFY(m_model->names().isEmpty());
    QCOMPARE(m_model->editedRow(), -1);
    QCOMPARE(m_model->variablesModel()->rowCount(), 0);
}

void EnvironmentModelTest::saveDoesNotResetModel()
{
    m_model->setEditedRow(1);
    m_model->setActiveIndex(1);
    QSignalSpy reset(m_model.get(), &QAbstractItemModel::modelReset);

    QVERIFY(m_manager->saveWorkspace());

    QCOMPARE(reset.count(), 0);
    QCOMPARE(m_model->editedRow(), 1);
    QCOMPARE(m_model->activeIndex(), 1);
}

void EnvironmentModelTest::withoutWorkspaceDoesNothing()
{
    m_manager->closeWorkspace();

    QCOMPARE(m_model->addEnvironment(), -1);
    m_model->removeEnvironment(0);
    m_model->setActiveIndex(0);
    m_model->setEditedRow(0);

    QCOMPARE(m_model->rowCount(), 0);
    QCOMPARE(m_model->activeIndex(), -1);
    QCOMPARE(m_model->editedRow(), -1);
}

QTEST_MAIN(EnvironmentModelTest)

#include "tst_environmentmodel.moc"
