// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPENGINE_H
#define APPENGINE_H

#include <QQmlApplicationEngine>

#include "framework/logfiltermodel.h"

class AppInfo;
class CollectionTreeModel;
class EnvironmentModel;
class HelpLibrary;
class HelpLocator;
class HistoryModel;
class LicenseModel;
class LocaleController;
class LogModel;
class RequestEditor;
class ResponseView;
class UpdateController;
class WorkspaceManager;

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

/**
 * QML engine wrapper that owns application-level C++ services.
 *
 * Exposes the services to QML as context properties: \c cppAppEngine,
 * \c cppWorkspaceManager, \c cppCollection, \c cppEnvironments, \c cppHistory, \c cppHelp,
 * \c cppHelpLocator,
 * \c cppRequestEditor, \c cppResponse,
 * \c cppAppInfo, \c cppLicenseModel, and, once injected, \c cppLocale and
 * \c cppUpdate. It also
 * installs the Qt message handler that
 * feeds the in-application log and the release log file.
 */
class AppEngine : public QQmlApplicationEngine
{
    Q_OBJECT
    Q_DISABLE_COPY(AppEngine)

    /** Filtered application log shown by the log panel; owned by this engine. */
    Q_PROPERTY(LogFilterModel *logModel READ logModel CONSTANT)

public:
    /** Creates the engine and exposes application services to QML. */
    explicit AppEngine(QObject* parent = nullptr);

    /** Uninstalls the message handler and closes the log file. */
    ~AppEngine() override;

    /** Injects the INI \a settings store into the workspace manager for persistence. */
    void setSettings(QSettings* settings);

    /**
     * Publishes the language \a controller to QML as \c cppLocale and refreshes
     * C++-side text when it reports a live language switch.
     */
    void setLocaleController(LocaleController* controller);

    /** Publishes the update \a controller to QML as \c cppUpdate. */
    void setUpdateController(UpdateController* controller);

    /** Returns the workspace facade exposed to QML as \c cppWorkspaceManager. */
    WorkspaceManager* workspaceManager() const { return m_workspaceManager; }

    /** Returns the filtered application log exposed to QML. */
    LogFilterModel* logModel() const { return m_logFilterModel; }

    /** Removes every entry from the in-memory application log. */
    Q_INVOKABLE void clearLog();

    /** Places the given text on the system clipboard. */
    Q_INVOKABLE void copyToClipboard(const QString& text);

    /**
     * Opens the directory holding the log file in the system file manager.
     * \return \c false when no log file has been written in this session.
     */
    Q_INVOKABLE bool showLogFileLocation();

private:
    /** Workspace facade exposed to QML as \c cppWorkspaceManager; owned by this engine. */
    WorkspaceManager* m_workspaceManager = nullptr;

    /** Collection of the active workspace exposed to QML as \c cppCollection; owned by this engine. */
    CollectionTreeModel* m_collection = nullptr;

    /** Environments of the active workspace exposed to QML as \c cppEnvironments; owned by this engine. */
    EnvironmentModel* m_environments = nullptr;

    /** Finder of the element F1 refers to, exposed to QML as \c cppHelpLocator; owned by this engine. */
    HelpLocator* m_helpLocator = nullptr;

    /** Help topics exposed to QML as \c cppHelp; owned by this engine. */
    HelpLibrary* m_help = nullptr;

    /** Sent requests exposed to QML as \c cppHistory; owned by this engine. */
    HistoryModel* m_history = nullptr;

    /** Last response exposed to QML as \c cppResponse; owned by this engine. */
    ResponseView* m_response = nullptr;

    /** Open request exposed to QML as \c cppRequestEditor; owned by this engine. */
    RequestEditor* m_requestEditor = nullptr;

    /** Application/build metadata exposed to QML as \c cppAppInfo; owned by this engine. */
    AppInfo* m_appInfo = nullptr;

    /** Bundled license documents exposed to QML as \c cppLicenseModel; owned by this engine. */
    LicenseModel* m_licenseModel = nullptr;

    /** UI language selector exposed to QML as \c cppLocale; owned by main(). */
    LocaleController* m_localeController = nullptr;

    /** Update checker exposed to QML as \c cppUpdate; owned by main(). */
    UpdateController* m_updateController = nullptr;

    /** In-memory application log fed by the installed Qt message handler. */
    LogModel* m_logModel = nullptr;

    /** Severity and text filter over m_logModel, exposed to QML. */
    LogFilterModel* m_logFilterModel = nullptr;
};

#endif // APPENGINE_H
