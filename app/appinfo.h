// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPINFO_H
#define APPINFO_H

#include <QObject>
#include <QString>

/**
 * Read-only application, build, and environment metadata exposed to QML.
 *
 * The About dialog binds to these properties to show the application version,
 * license, and the versions of the toolchain and third-party components the
 * application was built against. readText() loads a bundled UTF-8 document (a
 * license or notice file) for display. All properties are constant for the
 * lifetime of the process.
 */
class AppInfo : public QObject
{
    Q_OBJECT

    /** Human-readable application name. */
    Q_PROPERTY(QString appName READ appName CONSTANT)
    /** Application version string. */
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    /** Organization the application reports itself under. */
    Q_PROPERTY(QString organization READ organization CONSTANT)
    /** Copyright line for the application's own source code. */
    Q_PROPERTY(QString copyright READ copyright CONSTANT)
    /** SPDX identifier of the application's own license. */
    Q_PROPERTY(QString licenseName READ licenseName CONSTANT)
    /** One-line description of the application. */
    Q_PROPERTY(QString description READ description CONSTANT)
    /** Project homepage URL. */
    Q_PROPERTY(QString homepageUrl READ homepageUrl CONSTANT)
    /** Qt runtime version the application is linked against. */
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    /** Compiler and version the application was built with. */
    Q_PROPERTY(QString compiler READ compiler CONSTANT)
    /** C++ language standard the application was built with. */
    Q_PROPERTY(QString cxxStandard READ cxxStandard CONSTANT)
    /** Build configuration, "Debug" or "Release". */
    Q_PROPERTY(QString buildType READ buildType CONSTANT)
    /** Compile date and time of this build. */
    Q_PROPERTY(QString buildTimestamp READ buildTimestamp CONSTANT)
    /** Pretty name of the operating system the application runs on. */
    Q_PROPERTY(QString operatingSystem READ operatingSystem CONSTANT)
    /** CPU architecture the application runs on. */
    Q_PROPERTY(QString cpuArchitecture READ cpuArchitecture CONSTANT)
    /** Resource root holding the bundled license and notice documents. */
    Q_PROPERTY(QString licensesRoot READ licensesRoot CONSTANT)

public:
    /** Creates the metadata object. */
    explicit AppInfo(QObject *parent = nullptr);

    /** Returns the human-readable application name. */
    QString appName() const;
    /** Returns the application version string. */
    QString appVersion() const;
    /** Returns the organization the application reports itself under. */
    QString organization() const;
    /** Returns the copyright line for the application's own source code. */
    QString copyright() const;
    /** Returns the SPDX identifier of the application's own license. */
    QString licenseName() const;
    /** Returns a one-line description of the application. */
    QString description() const;
    /** Returns the project homepage URL. */
    QString homepageUrl() const;
    /** Returns the Qt runtime version the application is linked against. */
    QString qtVersion() const;
    /** Returns the compiler and version the application was built with. */
    QString compiler() const;
    /** Returns the C++ language standard the application was built with. */
    QString cxxStandard() const;
    /** Returns the build configuration, "Debug" or "Release". */
    QString buildType() const;
    /** Returns the compile date and time of this build. */
    QString buildTimestamp() const;
    /** Returns the pretty name of the operating system. */
    QString operatingSystem() const;
    /** Returns the CPU architecture the application runs on. */
    QString cpuArchitecture() const;
    /** Returns the resource root holding the bundled license documents. */
    QString licensesRoot() const;

    /**
     * Reads the UTF-8 text file at \a path for display in the About dialog.
     * \param path Filesystem path or "qrc:/" resource path.
     * \return The file contents, or an empty string when the file cannot be read.
     */
    Q_INVOKABLE QString readText(const QString &path) const;
};

#endif // APPINFO_H
