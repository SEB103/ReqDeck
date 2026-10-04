// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Read-only application, build, and environment metadata for QML.
 */

#include "appinfo.h"

#include "productinfo.h"

#include <QCoreApplication>
#include <QFile>
#include <QSysInfo>
#include <QtGlobal>

/*!
 * \brief Creates the metadata object.
 */
AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{}

/*!
 * \brief Returns the human-readable application name.
 */
QString AppInfo::appName() const
{
    return QStringLiteral(PRODUCT_DISPLAY_NAME);
}

/*!
 * \brief Returns the application version string.
 *
 * The value comes from QCoreApplication, which main() sets from the CMake
 * project version.
 */
QString AppInfo::appVersion() const
{
    return QCoreApplication::applicationVersion();
}

/*!
 * \brief Returns the organization the application reports itself under.
 */
QString AppInfo::organization() const
{
    return QStringLiteral(PRODUCT_ORGANIZATION);
}

/*!
 * \brief Returns the copyright line for the application's own source code.
 */
QString AppInfo::copyright() const
{
    return QStringLiteral(PRODUCT_COPYRIGHT);
}

/*!
 * \brief Returns the SPDX identifier of the application's own license.
 */
QString AppInfo::licenseName() const
{
    return QStringLiteral("GPL-3.0-or-later");
}

/*!
 * \brief Returns a one-line description of the application.
 */
QString AppInfo::description() const
{
    return QStringLiteral(
        "A desktop client for composing, sending, and saving HTTP/REST requests.");
}

/*!
 * \brief Returns the project homepage URL.
 */
QString AppInfo::homepageUrl() const
{
    return QStringLiteral(PRODUCT_HOMEPAGE);
}

/*!
 * \brief Returns the Qt runtime version the application is linked against.
 */
QString AppInfo::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

/*!
 * \brief Returns the compiler and version the application was built with.
 */
QString AppInfo::compiler() const
{
#if defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#elif defined(__clang__)
    return QStringLiteral("Clang %1.%2.%3")
        .arg(__clang_major__)
        .arg(__clang_minor__)
        .arg(__clang_patchlevel__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2.%3")
        .arg(__GNUC__)
        .arg(__GNUC_MINOR__)
        .arg(__GNUC_PATCHLEVEL__);
#else
    return QStringLiteral("Unknown compiler");
#endif
}

/*!
 * \brief Returns the C++ language standard the application was built with.
 *
 * The project baseline fixes the standard at C++20 in CMake. The value is
 * reported from that baseline rather than from __cplusplus, which MSVC does not
 * report accurately without an extra compiler switch.
 */
QString AppInfo::cxxStandard() const
{
    return QStringLiteral("C++20");
}

/*!
 * \brief Returns the build configuration, "Debug" or "Release".
 */
QString AppInfo::buildType() const
{
#ifdef QT_NO_DEBUG
    return QStringLiteral("Release");
#else
    return QStringLiteral("Debug");
#endif
}

/*!
 * \brief Returns the compile date and time of this build.
 */
QString AppInfo::buildTimestamp() const
{
    return QStringLiteral(__DATE__ " " __TIME__);
}

/*!
 * \brief Returns the pretty name of the operating system.
 */
QString AppInfo::operatingSystem() const
{
    return QSysInfo::prettyProductName();
}

/*!
 * \brief Returns the CPU architecture the application runs on.
 */
QString AppInfo::cpuArchitecture() const
{
    return QSysInfo::currentCpuArchitecture();
}

/*!
 * \brief Returns the resource root holding the bundled license documents.
 */
QString AppInfo::licensesRoot() const
{
    return QStringLiteral("qrc:/licenses");
}

/*!
 * \brief Reads the UTF-8 text file at \a path for display in the About dialog.
 *
 * A "qrc:/" prefix is rewritten to the ":/" resource root so the same call works
 * for embedded resources and for a filesystem path. Returns an empty string when
 * the file cannot be opened.
 */
QString AppInfo::readText(const QString &path) const
{
    QString localPath = path;
    if (localPath.startsWith(QStringLiteral("qrc:/")))
        localPath.remove(0, 3); // "qrc:/..." -> ":/..."

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    return QString::fromUtf8(file.readAll());
}
