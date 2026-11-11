// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/workspace/workspaceserializer.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/environmentmodel.h"
#include "qmlapi/importcontroller.h"
#include "qmlapi/workspacemanager.h"

/*! Verifies the preview and the import of files of other tools. */
class ImportControllerTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Opens an empty workspace with one environment named Staging. */
    void init();

    /*! Releases the per-test objects. */
    void cleanup();

    /*! Verifies that a preview says what the collection holds and changes nothing. */
    void previewSummarizesWithoutChanges();

    /*! Verifies that apply adds the collection as a new folder and leaves the workspace unsaved. */
    void applyAddsFolder();

    /*! Verifies that an environment whose name is taken gets another name. */
    void environmentNameIsKeptApart();

    /*! Verifies that an unknown file is refused with a message. */
    void unknownFileIsRefused();

    /*! Verifies that a large collection imports quickly. */
    void importLargeCollection();

private:
    /*! Directory of the test workspace. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Real workspace manager. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Collection of the workspace. */
    std::unique_ptr<CollectionTreeModel> m_collection;

    /*! Environments of the workspace. */
    std::unique_ptr<EnvironmentModel> m_environments;

    /*! Controller under test. */
    std::unique_ptr<ImportController> m_import;
};

void ImportControllerTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));
    WorkspaceData data;
    data.id = newItemId();
    Environment staging;
    staging.id = QStringLiteral("e1");
    staging.name = QStringLiteral("Staging");
    data.environments = {staging};
    const QString path = m_dir->filePath(QStringLiteral("import.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, data));
    QVERIFY(m_manager->openWorkspace(path));
    m_collection = std::make_unique<CollectionTreeModel>(m_manager.get());
    m_environments = std::make_unique<EnvironmentModel>(m_manager.get());
    m_import = std::make_unique<ImportController>(m_collection.get(), m_environments.get());
}

void ImportControllerTest::cleanup()
{
    m_import.reset();
    m_environments.reset();
    m_collection.reset();
    m_manager.reset();
    m_dir.reset();
}

void ImportControllerTest::previewSummarizesWithoutChanges()
{
    QVERIFY(m_import->preview(QFINDTESTDATA("data/import/postman-shop.postman_collection.json")));

    QCOMPARE(m_import->summary(), QStringLiteral("Postman collection: 5 request(s) in 2 folder(s)"));
    QVERIFY(!m_import->report().isEmpty());
    QVERIFY(m_manager->workspaceData().collection.isEmpty());
    QVERIFY(!m_manager->dirty());
}

void ImportControllerTest::applyAddsFolder()
{
    QVERIFY(m_import->preview(QFINDTESTDATA("data/import/postman-shop.postman_collection.json")));

    const QString id = m_import->apply();

    QVERIFY(!id.isEmpty());
    QCOMPARE(m_collection->rowCount(), 1);
    QCOMPARE(m_manager->workspaceData().collection.at(0).name, QStringLiteral("Shop API"));
    QVERIFY(m_manager->dirty());
    QVERIFY(!m_import->ready());
}

void ImportControllerTest::environmentNameIsKeptApart()
{
    QVERIFY(m_import->preview(QFINDTESTDATA("data/import/staging.postman_environment.json")));

    QVERIFY(m_import->apply().isEmpty());

    QCOMPARE(m_environments->names(), QStringList({QStringLiteral("Staging"), QStringLiteral("Staging (imported)")}));
    QVERIFY(m_manager->workspaceData().environments.at(1).variables.at(1).secret);
}

void ImportControllerTest::unknownFileIsRefused()
{
    QVERIFY(!m_import->previewData("{\"openapi_like\": true}"));
    QVERIFY(!m_import->errorString().isEmpty());
    QVERIFY(m_import->apply().isEmpty());
    QVERIFY(!m_manager->dirty());
}

void ImportControllerTest::importLargeCollection()
{
    QByteArray items;
    for (int i = 0; i < 1000; ++i) {
        if (i > 0)
            items += ',';
        items += "{\"name\": \"r" + QByteArray::number(i) + "\", \"request\": {\"method\": \"GET\", \"url\": \"https://h/"
                 + QByteArray::number(i) + "\"}}";
    }
    const QByteArray json = "{\"info\": {\"name\": \"Big\", \"schema\": "
                            "\"https://schema.getpostman.com/json/collection/v2.1.0/collection.json\"}, \"item\": ["
                            + items + "]}";
    QElapsedTimer timer;
    timer.start();

    QVERIFY(m_import->previewData(json));
    QVERIFY(!m_import->apply().isEmpty());

    QVERIFY2(timer.elapsed() < 3000, qPrintable(QString::number(timer.elapsed())));
    QCOMPARE(m_manager->workspaceData().collection.at(0).children.size(), 1000);
}

QTEST_GUILESS_MAIN(ImportControllerTest)

#include "tst_importcontroller.moc"
