// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QUrl>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QStringListModel>
#include <QTest>
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
#include "qmlapi/foldereditor.h"
#include "qmlapi/cookiemodel.h"
#include "qmlapi/importcontroller.h"
#include "qmlapi/tabsmodel.h"
#include "qmlapi/environmentvariablesmodel.h"
#include "qmlapi/helplibrary.h"
#include "qmlapi/helplocator.h"
#include "qmlapi/historymodel.h"
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
    /*! Takes over opening addresses and points the guide into the test directory. */
    ScreensHost()
    {
        for (const QString &scheme : schemes())
            QDesktopServices::setUrlHandler(scheme, this, "recordUrl");
        m_help.setGuideDirectory(m_dir.filePath(QStringLiteral("help")));
    }

    /*! Gives opening addresses back to the system. */
    ~ScreensHost() override
    {
        for (const QString &scheme : schemes())
            QDesktopServices::unsetUrlHandler(scheme);
    }

    /*! Returns the schemes recordUrl() takes over. */
    static QStringList schemes()
    {
        return {QStringLiteral("file"), QStringLiteral("http"), QStringLiteral("https")};
    }

    /*! Creates the facades and publishes them to QML. */
    void publish(QQmlContext *context)
    {
        m_server.start();
        m_tlsServer.startTls();
        QObject::connect(&m_editor, &RequestEditor::requestFinished, &m_history,
                         &HistoryModel::record);
        const auto followScope = [this]() { m_environments.setVariableScope(m_editor.requestId()); };
        QObject::connect(&m_editor, &RequestEditor::requestChanged, &m_environments, followScope);
        QObject::connect(&m_collection, &CollectionTreeModel::collectionChanged, &m_environments, followScope);
        m_manager.setSecretsDir(m_dir.filePath(QStringLiteral("secrets")));
        context->setContextProperty(QStringLiteral("cppWorkspaceManager"), &m_manager);
        context->setContextProperty(QStringLiteral("cppCollection"), &m_collection);
        context->setContextProperty(QStringLiteral("cppEnvironments"), &m_environments);
        context->setContextProperty(QStringLiteral("cppHistory"), &m_history);
        context->setContextProperty(QStringLiteral("cppHelp"), &m_help);
        context->setContextProperty(QStringLiteral("cppHelpLocator"), &m_helpLocator);
        context->setContextProperty(QStringLiteral("cppRequestEditor"), &m_editor);
        context->setContextProperty(QStringLiteral("cppFolderEditor"), &m_folderEditor);
        context->setContextProperty(QStringLiteral("cppImport"), &m_import);
        context->setContextProperty(QStringLiteral("cppCookies"), &m_cookies);
        context->setContextProperty(QStringLiteral("cppTabs"), &m_tabs);
        m_editor.setTabs(&m_tabs);
        QObject::connect(&m_manager, &WorkspaceManager::workspaceReplaced, &m_tabs, &TabsModel::clear);
        context->setContextProperty(QStringLiteral("cppResponse"), &m_response);
        context->setContextProperty(QStringLiteral("cppTestHost"), this);
        publishMainStubs(context);
    }

    /*!
     * Publishes the services only Main.qml and its dialogs read. They have no part
     * in what the tests of Main check, which is the wiring of the real facades.
     */
    void publishMainStubs(QQmlContext *context)
    {
        auto *info = new QQmlPropertyMap(this);
        for (const char *key : {"appName", "appVersion", "buildTimestamp", "buildType", "compiler",
                                "copyright", "cpuArchitecture", "cxxStandard", "description",
                                "homepageUrl", "licenseName", "licensesRoot", "operatingSystem",
                                "qtVersion"}) {
            info->insert(QLatin1StringView(key), QStringLiteral("x"));
        }
        context->setContextProperty(QStringLiteral("cppAppInfo"), info);

        auto *locale = new QQmlPropertyMap(this);
        locale->insert(QStringLiteral("availableLanguages"), QStringList());
        locale->insert(QStringLiteral("currentFlag"), QString());
        locale->insert(QStringLiteral("currentLanguage"), QStringLiteral("en"));
        context->setContextProperty(QStringLiteral("cppLocale"), locale);

        auto *update = new QQmlPropertyMap(this);
        for (const char *key : {"featureEnabled", "checkAutomatically", "updateAvailable", "checking",
                                "canInstallUpdate"}) {
            update->insert(QLatin1StringView(key), false);
        }
        for (const char *key : {"actionError", "currentVersion", "statusMessage"})
            update->insert(QLatin1StringView(key), QString());
        context->setContextProperty(QStringLiteral("cppUpdate"), update);

        context->setContextProperty(QStringLiteral("cppLicenseModel"), new QStringListModel(this));

        auto *engine = new QQmlPropertyMap(this);
        engine->insert(QStringLiteral("logModel"),
                       QVariant::fromValue<QObject *>(new QStringListModel(this)));
        context->setContextProperty(QStringLiteral("cppAppEngine"), engine);
    }

    /*! Presses \a key with \a modifiers in the window \a window, which tests cannot reach by keyClick(). */
    Q_INVOKABLE void pressKey(QObject *window, int key, int modifiers)
    {
        QTest::keyClick(qobject_cast<QQuickWindow *>(window), Qt::Key(key),
                        Qt::KeyboardModifiers(modifiers));
    }

    /*! Returns the URL of \a path on the mock server. */
    Q_INVOKABLE QString serverUrl(const QString &path) const { return m_server.url(path).toString(); }

    /*! Returns a path for a file named \a name in the test directory. */
    Q_INVOKABLE QString scratchFile(const QString &name) const { return m_dir.filePath(name); }

    /*! Returns whether the file \a path holds exactly \a size bytes of the /bytes/ pattern. */
    Q_INVOKABLE bool fileMatchesPattern(const QString &path, qint64 size) const
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() != size)
            return false;
        for (qint64 offset = 0; offset < size; offset += 1024 * 1024) {
            const QByteArray chunk = file.read(1024 * 1024);
            if (chunk != MockHttpServer::patternBytes(offset, chunk.size()))
                return false;
        }
        return true;
    }

    /*! Returns the address of \a path on the HTTPS server with an untrusted certificate. */
    Q_INVOKABLE QString tlsServerUrl(const QString &path) const { return m_tlsServer.url(path).toString(); }

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

    /*! Adds \a count entries for \a url with \a error and, if given, \a status. */
    Q_INVOKABLE void addHistory(const QString &url, int count, const QString &error = {},
                                int status = 0)
    {
        RequestSpec request;
        request.url = url;
        ResponseData response;
        response.statusCode = status > 0 ? status : (error.isEmpty() ? 200 : 0);
        response.errorString = error;
        for (int i = 0; i < count; ++i)
            m_history.record(request, response);
    }

    /*! Sends \a window the event of the pointer leaving it. */
    Q_INVOKABLE void leaveWindow(QQuickWindow *window)
    {
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(window, &leave);
    }

    /*!
     * Returns whether \a item names its topic itself or takes it from something it
     * is part of: a dialog or a menu names one topic for all it holds, a list or a
     * tree for its rows, and a control such as a tab bar or one of the compound Base
     * controls below for its parts. A plain area such as a pane does not, so a
     * control added there without a topic of its own is caught.
     */
    static bool ownsTopic(const QQuickItem *item)
    {
        static const QStringList compound {
            QStringLiteral("BsKeyValueTable"), QStringLiteral("FormDataTable"), QStringLiteral("BsSegmentedControl"),
            QStringLiteral("BsLogPanel"), QStringLiteral("BsStatusBar"),
            QStringLiteral("BsNotificationBanner")};
        for (const QObject *object = item; object;) {
            if (object != item
                && (object->inherits("QQuickPopup") || object->inherits("QQuickItemView")
                    || object->inherits("QQuickTableView"))) {
                return true;
            }
            if (!object->property("helpTopic").toString().isEmpty()) {
                const QString type =
                    QString::fromLatin1(object->metaObject()->className()).section(u'_', 0, 0);
                return object == item
                    || (object->inherits("QQuickControl") && !object->inherits("QQuickPane"))
                    || compound.contains(type);
            }
            const auto *parent = qobject_cast<const QQuickItem *>(object);
            object = parent && parent->parentItem() && !parent->inherits("QQuickPopupItem")
                ? static_cast<const QObject *>(parent->parentItem())
                : object->parent();
        }
        return false;
    }

    /*!
     * Returns the controls of \a window a user can work with that have no help
     * topic, or one unknown to the help in either language. Closed popups are
     * searched too, through their content.
     */
    Q_INVOKABLE QStringList controlsWithoutHelp(QQuickWindow *window) const
    {
        if (!window)
            return {QStringLiteral("no window")};
        QQuickItem *content = window->contentItem();
        QList<QQuickItem *> roots {content->parentItem() ? content->parentItem() : content};
        const QList<QObject *> objects = window->findChildren<QObject *>();
        for (QObject *object : objects) {
            if (!object->inherits("QQuickPopup"))
                continue;
            for (const char *part : {"contentItem", "header", "footer", "background"}) {
                if (auto *item = object->property(part).value<QQuickItem *>())
                    roots.append(item);
            }
        }

        const HelpLibrary russian(QStringLiteral("ru"), QStringLiteral(REQDECK_HELP_DIR));
        QStringList missing;
        QList<QQuickItem *> pending = roots;
        QSet<QQuickItem *> seen;
        while (!pending.isEmpty()) {
            QQuickItem *item = pending.takeLast();
            if (!item || seen.contains(item))
                continue;
            seen.insert(item);
            pending.append(item->childItems());

            const bool usable = item->inherits("QQuickControl")
                && (item->inherits("QQuickAbstractButton") || item->inherits("QQuickTextField")
                    || item->inherits("QQuickTextArea") || item->inherits("QQuickComboBox")
                    || item->inherits("QQuickSpinBox") || item->inherits("QQuickSwitch")
                    || item->activeFocusOnTab())
                && !item->inherits("QQuickScrollBar");
            const bool view = item->inherits("QQuickListView") || item->inherits("QQuickTableView");
            if (!usable && !view)
                continue;

            const QString topic = HelpLocator::topicOf(item);
            if (topic.isEmpty() || !m_help.hasTopic(topic) || !russian.hasTopic(topic)
                || !ownsTopic(item)) {
                QStringList chain;
                for (const QObject *object = item; object && chain.size() < 4;
                     object = qobject_cast<const QQuickItem *>(object)
                         ? static_cast<const QQuickItem *>(object)->parentItem() : object->parent()) {
                    QString name = object->objectName();
                    if (name.isEmpty())
                        name = QString::fromLatin1(object->metaObject()->className()).section(u'_', 0, 0);
                    chain.append(name);
                }
                missing.append((topic.isEmpty() ? QStringLiteral("none") : topic) + u' '
                               + item->property("text").toString() + u' ' + chain.join(u'<'));
            }
        }
        missing.removeDuplicates();
        return missing;
    }

    /*! Returns the number of requests the mock server received. */
    Q_INVOKABLE int requestCount() const { return m_server.requestCount(); }

    /*! Returns the path of the secrets file of the open workspace. */
    QString secretsFile() const
    {
        return m_dir.filePath(QStringLiteral("secrets/%1.json").arg(m_manager.workspaceData().id));
    }

    /*! Opens the workspace again with its secrets file blocked by a directory. */
    Q_INVOKABLE bool reopenWithLockedSecrets()
    {
        const QString path = m_manager.activeWorkspacePath();
        if (!QDir().mkpath(secretsFile()))
            return false;
        m_editor.clear();
        m_manager.closeWorkspace();
        return m_manager.openWorkspace(path);
    }

    /*! Removes the block of reopenWithLockedSecrets(). */
    Q_INVOKABLE bool unlockSecrets() { return QDir().rmdir(secretsFile()); }

    /*! Returns the addresses the application asked the system to open, oldest first. */
    Q_INVOKABLE QStringList openedUrls() const { return m_openedUrls; }

    /*! Puts a stand-in for the English guide where the application looks for it, or removes it. */
    Q_INVOKABLE bool setGuidePresent(bool present)
    {
        QFile guide(m_dir.filePath(QStringLiteral("help/ReqDeck-Guide.en.pdf")));
        if (!present)
            return !guide.exists() || guide.remove();
        if (!QDir().mkpath(m_dir.filePath(QStringLiteral("help"))) || !guide.open(QIODevice::WriteOnly))
            return false;
        return guide.write("%PDF-1.7\n") > 0;
    }

    /*! Returns the path of the Postman sample collection of the unit tests. */
    Q_INVOKABLE QString importSample() const
    {
        return QStringLiteral(REQDECK_IMPORT_SAMPLE);
    }

    /*! Returns whether the active environment has a secret variable \a key. */
    Q_INVOKABLE bool isSecretVariable(const QString &key) const
    {
        const WorkspaceData &data = m_manager.workspaceData();
        for (const Environment &environment : data.environments) {
            if (environment.id != data.activeEnvironmentId)
                continue;
            for (const EnvironmentVariable &variable : environment.variables) {
                if (variable.key == key)
                    return variable.secret;
            }
        }
        return false;
    }

    /*! Closes the workspace, so the launcher shows, and points Open Samples at a fresh copy. */
    Q_INVOKABLE bool prepareLauncherSamples()
    {
        m_editor.clear();
        m_manager.closeWorkspace();
        const QString path = m_dir.filePath(QStringLiteral("SamplesCopy.reqdeck"));
        QFile::remove(path);
        if (!QFile::copy(QStringLiteral(REQDECK_SAMPLES_FILE), path)
            || !QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner)) {
            return false;
        }
        m_manager.setSamplesPath(path);
        return true;
    }

    /*! Opens a fresh copy of the bundled samples workspace. */
    Q_INVOKABLE bool openSamples()
    {
        m_editor.clear();
        m_manager.closeWorkspace();
        const QString path = m_dir.filePath(QStringLiteral("Samples.reqdeck"));
        QFile::remove(path);
        return QFile::copy(QStringLiteral(REQDECK_SAMPLES_FILE), path)
            && QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner)
            && m_manager.openWorkspace(path);
    }

    /*!
     * Saves the part of the window that \a item covers to \a path, popups above it
     * included, which grabImage() leaves out or misplaces.
     */
    Q_INVOKABLE bool saveGrab(QQuickItem *item, const QString &path) const
    {
        if (!item || !item->window())
            return false;
        return saveGrabArea(item->window(), item->mapRectToScene(QRectF(0, 0, item->width(), item->height())),
                            path);
    }

    /*! Saves the area \a scene of \a window, in scene coordinates, to \a path. */
    Q_INVOKABLE bool saveGrabArea(QQuickWindow *window, const QRectF &scene, const QString &path) const
    {
        if (!window)
            return false;
        const QImage image = window->grabWindow();
        const qreal ratio = image.devicePixelRatio();
        return image.copy(QRectF(scene.topLeft() * ratio, scene.size() * ratio).toAlignedRect()).save(path);
    }

    /*! Returns the folder screenshots for the guide go to, empty unless REQDECK_GUIDE_SHOTS names one. */
    Q_INVOKABLE QString guideShotsDir() const { return qEnvironmentVariable("REQDECK_GUIDE_SHOTS"); }

public slots:
    /*! Records \a url instead of opening it, so tests never start a browser or a viewer. */
    void recordUrl(const QUrl &url) { m_openedUrls.append(url.toString()); }

public:
    /*! Opens a fresh copy of the test workspace, clean and with nothing selected. */
    Q_INVOKABLE bool reset()
    {
        m_openedUrls.clear();
        setGuidePresent(false);
        m_editor.clear();
        m_manager.closeWorkspace();
        m_history.clear();

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

    /*! Addresses recorded by recordUrl(). */
    QStringList m_openedUrls;

    /*! Local server the requests of the tests go to. */
    MockHttpServer m_server;

    /*! Local HTTPS server with an untrusted certificate. */
    MockHttpServer m_tlsServer;

    /*! Real workspace manager. */
    WorkspaceManager m_manager;

    /*! Real collection model. */
    CollectionTreeModel m_collection {&m_manager};

    /*! Real environment model. */
    EnvironmentModel m_environments {&m_manager};

    /*! Real finder of help targets. */
    HelpLocator m_helpLocator;

    /*! Real help topics in English, read from the source tree. */
    HelpLibrary m_help {QStringLiteral("en"), QStringLiteral(REQDECK_HELP_DIR)};

    /*! Real history over a temporary file. */
    HistoryModel m_history {m_dir.filePath(QStringLiteral("history.json"))};

    /*! Real response view. */
    ResponseView m_response;

    /*! Real request editor. */
    RequestEditor m_editor {&m_manager, &m_collection, &m_response};

    /*! Real folder editor. */
    FolderEditor m_folderEditor {&m_collection};

    /*! Real import controller. */
    ImportController m_import {&m_collection, &m_environments};

    /*! Real cookie model. */
    CookieModel m_cookies {&m_manager};

    /*! Real tabs model. */
    TabsModel m_tabs {&m_collection};
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
