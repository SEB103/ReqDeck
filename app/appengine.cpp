// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appengine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQmlContext>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>
#include <QUrl>
#include <QtGlobal>

#include <cstdio>

#include "appinfo.h"
#include "framework/apppaths.h"
#include "framework/logfiltermodel.h"
#include "framework/logmodel.h"
#include "licensemodel.h"
#include "localecontroller.h"
#include "qmlapi/workspacemanager.h"
#include "updatecontroller.h"

namespace {
constexpr auto kLogFileName = "app.log";
constexpr auto kRotatedLogFileName = "app.log.1";

/*!
 * \internal
 * \brief Size at which the log file is rotated, in bytes.
 *
 * The log used to grow without bound across runs. One rotation step keeps the
 * previous session's tail available while capping the disk footprint.
 */
constexpr qint64 kMaxLogFileBytes = 5 * 1024 * 1024;

QtMessageHandler g_prevQtMessageHandler = nullptr;
QFile  g_logFile;
QMutex g_logMutex;
bool   g_logInitAttempted = false;

/*!
 * \internal
 * \brief In-memory log fed by the message handler; null once the engine is gone.
 *
 * Guarded by \c g_logModelMutex because Qt message handlers run on every thread
 * the application uses, while the model itself lives in the GUI thread and is
 * therefore only ever touched through a queued invocation.
 */
LogModel* g_logModel = nullptr;
QMutex g_logModelMutex;

/*!
 * \internal
 * \brief Maps a Qt message type to the shared diagnostics severity.
 */
int diagnosticsLevelForType(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return Diagnostics::Debug;
    case QtInfoMsg: return Diagnostics::Info;
    case QtWarningMsg: return Diagnostics::Warning;
    case QtCriticalMsg:
    case QtFatalMsg: return Diagnostics::Error;
    }
    return Diagnostics::Info;
}

/*!
 * \internal
 * \brief Forwards one message to the in-memory log model, if one is installed.
 */
void appendLogLineToModel(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (QCoreApplication::closingDown())
        return;

    // Appending emits model signals, so a view reacting to them could log again
    // and re-enter this function. The guard is checked before the mutex is taken
    // because QMutex is not recursive and re-entering would otherwise deadlock.
    static thread_local bool appending = false;
    if (appending)
        return;

    appending = true;

    {
        QMutexLocker locker(&g_logModelMutex);
        if (g_logModel) {
            const QString category =
                context.category ? QString::fromUtf8(context.category) : QString();
            const int level = diagnosticsLevelForType(type);

            if (QThread::currentThread() == g_logModel->thread()) {
                // Deliver directly on the GUI thread. Posting an event would need
                // a live event dispatcher, which is not guaranteed once the
                // application is being torn down.
                g_logModel->appendEntry(level, category, msg);
            } else {
                QMetaObject::invokeMethod(g_logModel, "appendEntry", Qt::QueuedConnection,
                                          Q_ARG(int, level),
                                          Q_ARG(QString, category),
                                          Q_ARG(QString, msg));
            }
        }
    }

    appending = false;
}

/*!
 * \internal
 * \brief Extracts a compact method name from a Qt logging context.
 */
QString formatContextFunction(const QMessageLogContext& context)
{
    const QString functionInfo = context.function ? QString::fromUtf8(context.function) : QString();
    if (functionInfo.isEmpty())
        return QStringLiteral("Unknown method");
    static const QRegularExpression reMethod(QStringLiteral(R"((\w+::\w+))"));
    auto m = reMethod.match(functionInfo);
    if (m.hasMatch())
        return m.captured(1);
    return functionInfo;
}


/*!
 * \internal
 * \brief Returns the textual log level for \a type.
 */
QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("DEBUG");
    case QtInfoMsg: return QStringLiteral("INFO");
    case QtWarningMsg: return QStringLiteral("WARNING");
    case QtCriticalMsg: return QStringLiteral("CRITICAL");
    case QtFatalMsg: return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKNOWN");
}

/*!
 * \internal
 * \brief Opens the release log file while \c g_logMutex is held.
 */
bool ensureLogFileOpenLocked()
{
    if (g_logFile.isOpen())
        return true;
    if (g_logInitAttempted)
        return false;
    g_logInitAttempted = true;
    const QString logDirPath = AppPaths::instance().logDir();
    if (!QDir().mkpath(logDirPath))
        return false;
    const QString logFilePath = QDir(logDirPath).filePath(QString::fromLatin1(kLogFileName));

    // Rotate before appending so a long-lived installation keeps at most the
    // current file plus one previous generation.
    if (QFileInfo(logFilePath).size() >= kMaxLogFileBytes) {
        const QString rotatedPath =
            QDir(logDirPath).filePath(QString::fromLatin1(kRotatedLogFileName));
        QFile::remove(rotatedPath);
        QFile::rename(logFilePath, rotatedPath);
    }

    g_logFile.setFileName(logFilePath);
    return g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

/*!
 * \internal
 * \brief Returns the absolute log file path once the log file has been opened.
 */
QString openedLogFilePath()
{
    QMutexLocker locker(&g_logMutex);
    return g_logFile.isOpen() ? QFileInfo(g_logFile).absoluteFilePath() : QString();
}

/*!
 * \internal
 * \brief Appends one formatted \a line to the release log file.
 */
void appendLogLineToFile(const QString& line)
{
    QMutexLocker locker(&g_logMutex);
    if (ensureLogFileOpenLocked()) {
        QTextStream ts(&g_logFile);
        ts << line << '\n';
        ts.flush();
    }
}

/*!
 * \internal
 * \brief Writes Qt log messages to the application log and forwards them to the previous handler.
 */
void customLogMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (msg.startsWith(QLatin1String("Model size of")))
        return;
    const QString contextString = QStringLiteral("(%1; %2:%3)")
                                      .arg(formatContextFunction(context),
                                           context.file
                                               ? QString::fromUtf8(context.file)
                                               : QStringLiteral("<unknown>"),
                                           QString::number(context.line));
    const QString line = QStringLiteral("%1 [%2] %3 %4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  messageTypeName(type),
                                  msg,
                                  contextString);
    appendLogLineToModel(type, context, msg);

    // The log file stays a release-build facility; a debug session already has
    // the message on stderr and in the debugger.
#ifdef QT_NO_DEBUG
    appendLogLineToFile(line);
#else
    Q_UNUSED(line)
#endif

    if (g_prevQtMessageHandler) {
        g_prevQtMessageHandler(type, context, msg);
        return;
    }

    // qInstallMessageHandler() returns null when the default handler was in
    // place, so there is nothing to chain to. Writing the formatted message out
    // here keeps the console output this handler would otherwise swallow.
    const QByteArray formatted = qFormatLogMessage(type, context, msg).toLocal8Bit();
    std::fputs(formatted.constData(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

} // namespace

/*!
 * \brief Creates the QML engine and registers the C++ objects used by QML.
 *
 * The in-memory log model is published to the message handler before the
 * handler is installed, so no message emitted during startup is lost.
 */
AppEngine::AppEngine(QObject* parent)
    : QQmlApplicationEngine(parent)
    , m_workspaceManager(new WorkspaceManager(this))
    , m_appInfo(new AppInfo(this))
    , m_licenseModel(new LicenseModel(this))
    , m_logModel(new LogModel(2000, this))
    , m_logFilterModel(new LogFilterModel(this))
{
    m_logFilterModel->setSourceModel(m_logModel);

    // Publish the model before installing the handler so no message is lost.
    {
        QMutexLocker locker(&g_logModelMutex);
        g_logModel = m_logModel;
    }

    qmlRegisterUncreatableType<AppEngine>("Cpp.AppEngine", 1, 0, "AppEngine", QStringLiteral("AppEngine is a subclass of QQmlApplicationEngine and should not be created in QML."));
    qmlRegisterUncreatableType<LogFilterModel>("Cpp.AppEngine", 1, 0, "LogFilterModel", QStringLiteral("LogFilterModel is exposed by AppEngine::logModel."));
    rootContext()->setContextProperty("cppAppEngine", this);

    qmlRegisterUncreatableType<WorkspaceManager>("Cpp.WorkspaceManager", 1, 0, "WorkspaceManager", QStringLiteral("WorkspaceManager should not be created in QML."));
    rootContext()->setContextProperty("cppWorkspaceManager", m_workspaceManager);

    // Application/build metadata and the bundled license documents shown by the
    // Help > About dialog. The license texts are embedded as resources under
    // /licenses (see resources/CMakeLists.txt), so the model scans the resource
    // directory rather than a deployed folder.
    m_licenseModel->setDirectory(QStringLiteral("qrc:/licenses/LICENSES"));
    rootContext()->setContextProperty("cppAppInfo", m_appInfo);
    rootContext()->setContextProperty("cppLicenseModel", m_licenseModel);

    // Installed in every configuration: the log panel is a debugging aid the user
    // needs in a development build too. Writing to the log file stays
    // release-only inside the handler itself.
    g_prevQtMessageHandler = qInstallMessageHandler(customLogMessageHandler);

    // First entry of every session: it dates the log, states which build wrote
    // it, and keeps the log panel from opening on an empty list.
    qInfo() << QCoreApplication::applicationName()
            << QCoreApplication::applicationVersion() << "started";

    m_logModel->setLogFilePath(openedLogFilePath());
}

/*!
 * \brief Uninstalls the message handler and closes the log file.
 *
 * The GUI-thread log model is unpublished first, so no message emitted while the
 * owned services are destroyed is posted to a model that is going away.
 */
AppEngine::~AppEngine()
{
    {
        QMutexLocker modelLocker(&g_logModelMutex);
        g_logModel = nullptr;
    }

    qInstallMessageHandler(g_prevQtMessageHandler);
    g_prevQtMessageHandler = nullptr;

    m_logFilterModel = nullptr;
    m_logModel = nullptr;

    QMutexLocker locker(&g_logMutex);
    if (g_logFile.isOpen()) {
        g_logFile.flush();
        g_logFile.close();
    }
    g_logInitAttempted = false;
}

/*!
 * \brief Removes every entry from the in-memory application log.
 */
void AppEngine::clearLog()
{
    if (m_logModel)
        m_logModel->clear();
}

/*!
 * \brief Opens the directory holding the log file in the system file manager.
 * \return \c false when no log file has been written in this session.
 */
bool AppEngine::showLogFileLocation()
{
    // The path is only known once the first message has been written, so refresh
    // it here rather than relying on the value captured at construction time.
    const QString path = openedLogFilePath();
    if (m_logModel)
        m_logModel->setLogFilePath(path);

    if (path.isEmpty())
        return false;

    return QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

/*!
 * \brief Injects the INI settings store into the workspace manager.
 * \param settings Non-owning settings store used for the recent-workspaces list
 *        and the default workspaces directory; may be null to disable persistence.
 */
void AppEngine::setSettings(QSettings* settings)
{
    if (m_workspaceManager)
        m_workspaceManager->setSettings(settings);
}

/*!
 * \brief Publishes the language controller to QML and wires C++-side refresh.
 * \param controller Non-owning language selector, or null to skip wiring.
 *
 * QQmlApplicationEngine::retranslate() refreshes QML bindings that contain
 * translated strings, but C++ facades cache the last status text built with
 * tr(). Refreshing them on languageChanged() keeps the status bar in step with
 * the rest of the UI on a live switch.
 */
void AppEngine::setLocaleController(LocaleController* controller)
{
    m_localeController = controller;
    if (!controller)
        return;

    qmlRegisterUncreatableType<LocaleController>(
        "Cpp.Locale", 1, 0, "LocaleController",
        QStringLiteral("LocaleController is exposed as the cppLocale context property."));
    rootContext()->setContextProperty("cppLocale", controller);

    connect(controller, &LocaleController::languageChanged, this, [this]() {
        if (m_workspaceManager)
            m_workspaceManager->retranslate();
    });
}

/*!
 * \brief Publishes the update \a controller to QML as \c cppUpdate.
 *
 * QML reads the check state through the controller's boolean convenience
 * properties, so no QML type registration is needed for the Status enum.
 */
void AppEngine::setUpdateController(UpdateController* controller)
{
    m_updateController = controller;
    if (!controller)
        return;

    rootContext()->setContextProperty("cppUpdate", controller);
}
