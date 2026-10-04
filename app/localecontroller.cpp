// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "localecontroller.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <QTranslator>
#include <QVariantMap>

namespace {
/*! \internal Settings key holding the selected UI language locale code. */
constexpr auto kLanguageSettingsKey = "ui/language";

/*! \internal Resource stem the application translations are embedded under. */
constexpr auto kAppTranslationStem = ":/translations/ReqDeck_";

/*! \internal Default language used when no preference and no system match exist. */
constexpr auto kDefaultLanguageCode = "en_GB";
} // namespace

/*!
 * \brief Creates the controller and the reusable translator objects.
 * \param settings Non-owning INI store for the language preference, or null to
 *        disable persistence.
 * \param parent Optional QObject parent.
 *
 * The language table is fixed and matches the I18N_TRANSLATED_LANGUAGES declared
 * in CMakeLists.txt. The UI currently ships in English only (the en_GB entry);
 * further languages are added here together with their .ts catalog. Untranslated
 * strings fall back to the English source text regardless of the selection.
 */
LocaleController::LocaleController(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_appTranslator(new QTranslator(this))
    , m_qtTranslator(new QTranslator(this))
{
    m_languages = {
        {QStringLiteral("en_GB"), QStringLiteral("English"), QStringLiteral("qrc:/images/flags/gb.svg")},
    };
}

LocaleController::~LocaleController() = default;

/*!
 * \brief Returns the locale code of the active UI language.
 */
QString LocaleController::currentLanguage() const
{
    return m_currentCode;
}

/*!
 * \brief Returns the flag image resource (qrc) of the active UI language.
 * \return The flag path, or an empty string when the current code is unknown.
 */
QString LocaleController::currentFlag() const
{
    const int index = indexOfCode(m_currentCode);
    return index >= 0 ? m_languages.at(index).flag : QString();
}

/*!
 * \brief Returns the endonym of the active UI language.
 * \return The native name, or an empty string when the current code is unknown.
 */
QString LocaleController::currentLanguageName() const
{
    const int index = indexOfCode(m_currentCode);
    return index >= 0 ? m_languages.at(index).nativeName : QString();
}

/*!
 * \brief Returns the selectable languages as {code, name} rows for QML.
 *
 * Each row is a QVariantMap with a \c code key (the locale code passed to
 * setLanguage()), a \c name key (the endonym shown in the selector) and a
 * \c flag key (the qrc path of the country flag image).
 */
QVariantList LocaleController::availableLanguages() const
{
    QVariantList rows;
    rows.reserve(m_languages.size());
    for (const LanguageEntry &entry : m_languages) {
        QVariantMap row;
        row.insert(QStringLiteral("code"), entry.code);
        row.insert(QStringLiteral("name"), entry.nativeName);
        row.insert(QStringLiteral("flag"), entry.flag);
        rows.append(row);
    }
    return rows;
}

/*!
 * \brief Resolves and installs the startup language before the UI is built.
 *
 * Called from main() ahead of QQmlApplicationEngine::loadFromModule(), so the very
 * first objects the engine creates already read the chosen language. No signal is
 * emitted here because no bindings or models exist yet.
 */
void LocaleController::applyInitialLanguage()
{
    m_currentCode = resolveInitialCode();
    loadTranslators(m_currentCode);
}

/*!
 * \brief Associates the QML \a engine retranslated on a live language switch.
 */
void LocaleController::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

/*!
 * \brief Persists and applies the language \a code live.
 *
 * Ignores an unknown code and a code that is already active. Swapping the
 * translators, retranslating the engine, and emitting languageChanged() together
 * refresh the whole UI without a restart.
 */
void LocaleController::setLanguage(const QString &code)
{
    if (code == m_currentCode || indexOfCode(code) < 0)
        return;

    m_currentCode = code;
    loadTranslators(code);

    if (m_settings) {
        m_settings->setValue(QLatin1String(kLanguageSettingsKey), code);
        m_settings->sync();
    }

    // Re-evaluate every QML binding that contains a translated string, then let
    // C++-side item models refresh the text they cache.
    if (m_engine)
        m_engine->retranslate();

    emit currentLanguageChanged();
    emit languageChanged();
}

/*!
 * \brief Loads and installs the application and Qt-base translators for \a code.
 *
 * The previously installed pair is removed first so translations never stack. A
 * translator whose file is absent (for example a Qt-base file that ships only for
 * some locales) is simply left uninstalled, and the affected strings fall back to
 * the English source text.
 */
void LocaleController::loadTranslators(const QString &code)
{
    QCoreApplication::removeTranslator(m_appTranslator);
    QCoreApplication::removeTranslator(m_qtTranslator);

    if (m_appTranslator->load(QLatin1String(kAppTranslationStem) + code))
        QCoreApplication::installTranslator(m_appTranslator);

    // Qt ships its base translations keyed by language only (qtbase_de.qm), so map
    // the full locale code to the language part when loading them.
    const QLocale locale(code);
    if (m_qtTranslator->load(locale,
                             QStringLiteral("qtbase"),
                             QStringLiteral("_"),
                             QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(m_qtTranslator);
    }
}

/*!
 * \brief Returns the index of \a code in the language table, or -1 when unknown.
 */
int LocaleController::indexOfCode(const QString &code) const
{
    for (int i = 0; i < m_languages.size(); ++i) {
        if (m_languages.at(i).code == code)
            return i;
    }
    return -1;
}

/*!
 * \brief Returns the saved language, else the best system match, else English.
 *
 * A stored preference always wins. Otherwise the system UI languages are matched
 * against the supported set, first by exact locale code and then by language, so a
 * system set to plain "de" or "ru" still selects the matching entry.
 */
QString LocaleController::resolveInitialCode() const
{
    if (m_settings) {
        const QString saved =
            m_settings->value(QLatin1String(kLanguageSettingsKey)).toString();
        if (indexOfCode(saved) >= 0)
            return saved;
    }

    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &ui : uiLanguages) {
        const QString name = QLocale(ui).name();
        if (indexOfCode(name) >= 0)
            return name;
    }
    for (const QString &ui : uiLanguages) {
        const QString language = QLocale(ui).name().section(QLatin1Char('_'), 0, 0);
        for (const LanguageEntry &entry : m_languages) {
            if (entry.code.section(QLatin1Char('_'), 0, 0) == language)
                return entry.code;
        }
    }

    return QLatin1String(kDefaultLanguageCode);
}
