// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QString>

#include "appcore.h"
#include "appengine.h"
#include "framework/apppaths.h"
#include "localecontroller.h"
#include "productinfo.h"
#include "qmlapi/workspacemanager.h"
#include "updatecontroller.h"

int main(int argc, char *argv[])
{
    AppCore::setMessagePattern();
    AppCore app(argc, argv);

    // Identity comes from the centralized product metadata. The organization,
    // domain and application name are the STABLE identifier (not the display
    // name), so QSettings paths and per-user data directories are unaffected by
    // a future product rename.
    QCoreApplication::setOrganizationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QCoreApplication::setOrganizationDomain(QStringLiteral(PRODUCT_ORG_DOMAIN));
    QCoreApplication::setApplicationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QCoreApplication::setApplicationVersion(QStringLiteral(PRODUCT_VERSION));

    // Resolve installed/portable data locations once, before anything opens them.
    AppPaths::instance().initialize();

    // Set the application icon used for the window title bar, the taskbar and
    // Alt+Tab. The multi-size .ico is bundled via resources/CMakeLists.txt; Qt
    // selects the frame matching the requested size.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/images/app/ReqDeck.ico")));

    // Create the INI settings store in an ini/ folder under the resolved config
    // directory (next to the executable in portable mode, AppConfigLocation when
    // installed). It persists the recent workspaces and the UI preferences.
    app.createSettings(QCoreApplication::applicationName(),
                       AppPaths::instance().configDir());

    // Install the UI language before any QML is created. The controller resolves
    // the saved preference (falling back to the system locale, then English) and
    // outlives the engine so it can also switch languages live at runtime.
    LocaleController localeController(app.settings());
    localeController.applyInitialLanguage();

    // Update checker exposed to QML as cppUpdate. It persists the automatic-check
    // preference through the same INI store and outlives the engine.
    UpdateController updateController(app.settings());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Desktop client for composing, sending, and saving HTTP/REST requests."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("workspace"),
        QStringLiteral("Workspace file (.reqdeck) to open at startup."),
        QStringLiteral("[workspace]"));
    parser.process(app);

    AppEngine engine;
    engine.setSettings(app.settings());

    // Expose the language selector to QML and let it retranslate the engine on a
    // live switch. Wiring happens before loadFromModule so cppLocale is available
    // to the first objects the engine creates.
    engine.setLocaleController(&localeController);
    localeController.setEngine(&engine);
    engine.setUpdateController(&updateController);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("ReqDeck", "Main");

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Application startup failed because no QML root objects were created.";
        return -1;
    }

    // A workspace passed on the command line (for example through a file
    // association) opens once the UI exists, so its errors reach the user.
    const QString initialWorkspace = parser.positionalArguments().value(0).trimmed();
    if (!initialWorkspace.isEmpty())
        engine.workspaceManager()->openWorkspace(initialWorkspace);

    return app.exec();
}
