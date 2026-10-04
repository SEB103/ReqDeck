// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QEventLoop>
#include <QSettings>
#include <QTimer>

#include <algorithm>
#include <random>

#include "appcore.h"


namespace {

/*!
 * \internal
 * \brief Returns the legacy key format used by older versions of this helper.
 *
 * Historically, writeSettings()/readSettings() stored keys inside \a group with a leading
 * slash, e.g. "/name" or "/subdir/name". Newer code prefers QSettings groups for nesting.
 */
QString legacyKey(const QString& name, const QString& subdir)
{
    if (subdir.isEmpty())
        return QStringLiteral("/") + name;
    return QStringLiteral("/") + subdir + QStringLiteral("/") + name;
}

/*!
 * \internal
 * \brief Enters \a group and optional \a subdir as QSettings groups.
 *
 * The returned pair indicates whether group/subdir were actually entered, so the caller can
 * safely unwind with endGroup().
 */
std::pair<bool, bool> enterGroups(QSettings* s, const QString& group, const QString& subdir)
{
    const bool groupEntered = !group.isEmpty();
    if (groupEntered)
        s->beginGroup(group);

    const bool subdirEntered = !subdir.isEmpty();
    if (subdirEntered)
        s->beginGroup(subdir);

    return {groupEntered, subdirEntered};
}

/*!
 * \internal
 * \brief Leaves groups previously entered via enterGroups().
 */
void leaveGroups(QSettings* s, const std::pair<bool, bool>& entered)
{
    if (entered.second)
        s->endGroup();
    if (entered.first)
        s->endGroup();
}

} // namespace

/*! \brief Constructs AppCore.
 * \param argc Command-line argument count passed to QGuiApplication.
 * \param argv Command-line argument vector passed to QGuiApplication.
 *
 * AppCore is a thin wrapper around QGuiApplication. The settings object (QSettings) is created
 * lazily via createSettings().
 */
AppCore::AppCore(int& argc, char** argv)
    : QGuiApplication(argc, argv)
{}

/*! \brief Destroys AppCore.
 *
 * QSettings is parented to this object (see createSettings()), so it is deleted automatically.
 */
AppCore::~AppCore() {}

/*! \brief Creates an INI-based QSettings instance (lazy; only once).
 *
 * This helper stores settings in an INI file located in:
 * \code
 * <iniFileDir>/ini/<appName>.ini
 * \endcode
 *
 * Call this once during application startup (after Q(Core)Application is constructed) and then
 * use writeSettings()/readSettings()/removeSettings(). If settings already exist, the function
 * does nothing and returns \c false.
 *
 * The \a appName parameter is the base filename of the INI file, typically
 * qApp->applicationName(). The \a iniFileDir parameter is the base directory
 * for the \c ini folder. Returns \c true if QSettings was created, or \c false
 * if it already existed.
 */
bool AppCore::createSettings(const QString& appName, const QString& iniFileDir)
{
    if(m_settings == nullptr)
    {
        const QString iniDirPath  = iniFileDir + "/ini";
        QDir().mkpath(iniDirPath);

        const QString iniFile = QDir::toNativeSeparators(
            iniDirPath + "/" + appName + ".ini"
            );

        m_settings = new QSettings(iniFile, QSettings::IniFormat, this);
        return true;
    }
    return false;
}

/*! \brief Writes a setting value under \a group and optional \a subdir.
 * \param value The value to store.
 *
 * The storage structure maps \a group to a QSettings group, maps \a subdir to
 * an optional nested group, and stores \a name as the key inside the final
 * group.
 *
 * Example:
 * \code
 * // Writes [Ui/Window] width=900 in the INI file
 * writeSettings(900, "width", "Ui", "Window");
 * \endcode
 *
 * \note For backward compatibility, readSettings()/readOrSetSettings() can also read values
 * written by older versions that used a legacy key format ("/subdir/name") inside \a group.
 */
void AppCore::writeSettings(const QVariant& value, const QString& name, const QString& group, const QString& subdir)
{
    if(m_settings != nullptr)
    {
        const auto entered = enterGroups(m_settings, group, subdir);
        m_settings->setValue(name, value);
        leaveGroups(m_settings, entered);
    }
}

/*! \brief Removes a setting under \a group and optional \a subdir.
 *
 * Removes the key \a name in the QSettings group path \c group/subdir.
 * For cleanup, this also removes the legacy key variant ("/subdir/name") under \a group.
 */
void AppCore::removeSettings(const QString& name, const QString& group, const QString& subdir)
{
    if(m_settings != nullptr)
    {
        // Remove new-style location: group/subdir/name
        const auto entered = enterGroups(m_settings, group, subdir);
        m_settings->remove(name);
        leaveGroups(m_settings, entered);

        // Remove legacy location: group + "/subdir/name"
        if (!group.isEmpty())
            m_settings->beginGroup(group);
        m_settings->remove(legacyKey(name, subdir));
        if (!group.isEmpty())
            m_settings->endGroup();
    }
}

/*! \brief Blocks the current thread for \a msec milliseconds.
 *
 * This is a convenience method for short delays (e.g. test code, small UI sequencing).
 * It spins a local event loop, so timers and queued events can still be processed.
 * Avoid using it for long waits in production code.
 */
void AppCore::delay(int msec)
{
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, &QEventLoop::quit);
    loop.exec();
}

/*! \brief Returns a random unsigned integer in the inclusive range [\a min, \a max].
 *
 * Uses std::mt19937 seeded by std::random_device.
 */
unsigned int AppCore::randint(unsigned int min, unsigned int max)
{
    if (max < min)
        std::swap(min, max);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dis(min, max);
    return dis(gen);
}

/*! \brief Reads a setting under \a group and optional \a subdir.
 * \param name The setting key inside the selected group path.
 *
 * The preferred lookup order is the new-style \c group/subdir/name group path
 * first and the legacy \c group + "/subdir/name" key second.
 *
 * Returns the stored value, or \a defaultValue when the key does not exist.
 */
QVariant AppCore::readSettings(const QVariant& defaultValue, const QString& name, const QString& group, const QString& subdir) const
{
    QVariant result {defaultValue};
    if(m_settings != nullptr)
    {
        // New-style: group/subdir/name
        const auto entered = enterGroups(m_settings, group, subdir);
        if (m_settings->contains(name)) {
            result = m_settings->value(name, defaultValue);
            leaveGroups(m_settings, entered);
            return result;
        }
        leaveGroups(m_settings, entered);

        // Legacy: group + "/subdir/name"
        if (!group.isEmpty())
            m_settings->beginGroup(group);
        result = m_settings->value(legacyKey(name, subdir), defaultValue);
        if (!group.isEmpty())
            m_settings->endGroup();
    }
    return result;
}

/*! \brief Reads a setting; if missing, stores \a defaultValue and returns it.
 * \param name The setting key inside the selected group path.
 * \param group The top-level settings group.
 * \param subdir The optional nested settings group.
 *
 * This is a convenience method for "ensure a value exists" logic.
 * It checks both the new-style (group/subdir/name) and the legacy location.
 * If neither exists, it writes \a defaultValue to the new-style location.
 *
 * Example:
 * \code
 * // Ensure a window width exists and read it.
 * const int w = readOrSetSettings(900, "width", "Ui", "Window").toInt();
 * \endcode
 */
QVariant AppCore::readOrSetSettings(const QVariant &defaultValue, const QString &name, const QString &group, const QString &subdir)
{
    if (!m_settings)
        return defaultValue;

    // 1) New-style: group/subdir/name
    {
        const auto entered = enterGroups(m_settings, group, subdir);
        if (m_settings->contains(name)) {
            const QVariant v = m_settings->value(name, defaultValue);
            leaveGroups(m_settings, entered);
            return v;
        }
        leaveGroups(m_settings, entered);
    }

    // 2) Legacy: group + "/subdir/name"
    {
        if (!group.isEmpty())
            m_settings->beginGroup(group);
        const QString key = legacyKey(name, subdir);
        if (m_settings->contains(key)) {
            const QVariant v = m_settings->value(key, defaultValue);
            if (!group.isEmpty())
                m_settings->endGroup();
            return v;
        }
        if (!group.isEmpty())
            m_settings->endGroup();
    }

    // Missing everywhere -> write default to new-style location.
    const auto entered = enterGroups(m_settings, group, subdir);
    if (defaultValue.isValid())
        m_settings->setValue(name, defaultValue);
    m_settings->sync();
    leaveGroups(m_settings, entered);
    return defaultValue;
}

/*! \brief Returns the underlying QSettings instance.
 *
 * Returns \c nullptr until createSettings() was called successfully.
 */
QSettings* AppCore::settings()
{
    return m_settings;
}

/*! \brief Installs the default Qt message pattern used by this application.
 *
 * This pattern affects the default Qt handler output and also qFormatLogMessage().
 * If you write logs to a file using qFormatLogMessage(), this pattern defines the file format.
 */
void AppCore::setMessagePattern()
{
    qSetMessagePattern(
        "%{if-debug}D %{endif}"
        "%{if-info}I %{endif}"
        "%{if-warning}W!%{endif}"
        "%{if-critical}C!%{endif}"
        "%{if-fatal}F!%{endif}"
        "%{time process}  "
        "%{threadid}  "
        "%{if-category}%{category}: %{endif}"
        "%{function}"
        //                       "%{if-debug}(%{file}:%{line})%{endif} "
        "%{if-debug}(l:%{line})%{endif} "
        "- %{message}");
}
