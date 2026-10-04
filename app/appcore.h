// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPCORE_H
#define APPCORE_H

#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QString>
#include <QVariant>

#include <limits>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

/**
 * QGuiApplication subclass with settings and logging helpers.
 *
 * The class owns the lazily created QSettings instance and provides small
 * application-level utility functions used during startup and tests.
 */
class AppCore : public QGuiApplication
{
    Q_OBJECT
public:
    /** Creates the application object from command-line arguments \a argc and \a argv. */
    explicit AppCore(int& argc, char** argv);
    /** Destroys the application object and its child-owned settings object. */
    ~AppCore();

    /** Blocks the current thread for \a msec milliseconds while processing queued events. */
    void delay(int msec);

    /** Returns a random unsigned integer in the inclusive range from \a min to \a max. */
    unsigned int randint(unsigned int min = 0, unsigned int max = (std::numeric_limits<unsigned int>::max)());

    /** Creates an INI-based settings store for \a appName under \a iniFileDir. */
    bool createSettings(const QString& appName, const QString& iniFileDir);

    /** Writes \a value to \a name inside \a group and optional \a subdir. */
    void writeSettings(const QVariant& value, const QString& name, const QString& group, const QString& subdir = "");

    /** Removes \a name inside \a group and optional \a subdir. */
    void removeSettings(const QString& name, const QString& group, const QString& subdir = "");

    /** Reads \a name inside \a group and optional \a subdir, or returns \a defaultValue. */
    QVariant readSettings(const QVariant& defaultValue, const QString& name, const QString& group, const QString& subdir = "") const;

    /** Reads \a name or writes and returns \a defaultValue when the key is missing. */
    QVariant readOrSetSettings(const QVariant &defaultValue, const QString &name, const QString &group, const QString &subdir = "");

    /** Returns the non-owning QSettings pointer, or \c nullptr before createSettings(). */
    QSettings* settings();

    /** Installs the default Qt message pattern for console output. */
    static void setMessagePattern();

private:
    /** Settings object parented to this application instance. */
    QSettings* m_settings {};
};

#endif // APPCORE_H
