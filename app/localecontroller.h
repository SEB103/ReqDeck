// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOCALECONTROLLER_H
#define LOCALECONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QSettings;
class QTranslator;
QT_END_NAMESPACE

/**
 * Selects, applies, and persists the UI language exposed to QML as \c cppLocale.
 *
 * English is the source language baked into the tr()/qsTr() strings and stays the
 * fallback for any message a translation leaves empty. The controller installs the
 * application translator (\c :/translations/ReqDeck_<code>) and the matching
 * Qt-base translator (\c qtbase_<lang>) so both application and standard-control
 * text follow the chosen language.
 *
 * applyInitialLanguage() runs from main() before the QML UI is built; setLanguage()
 * switches live at runtime by swapping translators, retranslating the QML engine,
 * and emitting languageChanged() so C++-side item models can refresh their text.
 */
class LocaleController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LocaleController)

    /** Locale code of the active UI language, for example \c "de_DE". */
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)

    /** Flag image resource (qrc) of the active UI language. */
    Q_PROPERTY(QString currentFlag READ currentFlag NOTIFY currentLanguageChanged)

    /** Endonym of the active UI language, for example \c "Deutsch". */
    Q_PROPERTY(QString currentLanguageName READ currentLanguageName NOTIFY currentLanguageChanged)

    /** Selectable languages as {code, name, flag} rows for a ComboBox model. */
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)

public:
    /** Creates the controller using \a settings for persistence (may be null). */
    explicit LocaleController(QSettings *settings, QObject *parent = nullptr);

    /** Destroys the controller and its owned translators. */
    ~LocaleController() override;

    /** Returns the locale code of the active UI language. */
    QString currentLanguage() const;

    /** Returns the flag image resource (qrc) of the active UI language. */
    QString currentFlag() const;

    /** Returns the endonym of the active UI language. */
    QString currentLanguageName() const;

    /** Returns the selectable languages as {code, name, flag} rows for QML. */
    QVariantList availableLanguages() const;

    /**
     * Resolves and installs the startup language before the UI is built.
     * The resolution order is the saved preference, then the best system-locale
     * match, then English as the default.
     */
    void applyInitialLanguage();

    /** Associates the QML \a engine so live switches can retranslate its bindings. */
    void setEngine(QQmlEngine *engine);

    /** Persists and applies the language \a code live; ignores unknown or current codes. */
    Q_INVOKABLE void setLanguage(const QString &code);

signals:
    /** Emitted when the active UI language changes. */
    void currentLanguageChanged();

    /** Emitted after a live language switch so C++-side text can be refreshed. */
    void languageChanged();

private:
    /** One selectable UI language. */
    struct LanguageEntry
    {
        /** Locale code, for example \c "de_DE". */
        QString code;
        /** Endonym shown in the selector, for example \c "Deutsch". */
        QString nativeName;
        /** Flag image resource (qrc), for example \c "qrc:/images/flags/de.svg". */
        QString flag;
    };

    /** Loads and installs the application and Qt-base translators for \a code. */
    void loadTranslators(const QString &code);

    /** Returns the index of \a code in the language table, or -1 when unknown. */
    int indexOfCode(const QString &code) const;

    /** Returns the saved language, else the best system match, else English. */
    QString resolveInitialCode() const;

    /** Non-owning INI settings store used to persist the language preference. */
    QSettings *m_settings {nullptr};

    /** Non-owning QML engine retranslated on a live language switch. */
    QQmlEngine *m_engine {nullptr};

    /** Application translator ( :/translations/ReqDeck_<code> ); owned. */
    QTranslator *m_appTranslator {nullptr};

    /** Qt-base translator ( qtbase_<lang> ) for standard controls; owned. */
    QTranslator *m_qtTranslator {nullptr};

    /** Locale code of the active UI language. */
    QString m_currentCode;

    /** Supported languages in selector order; English first. */
    QVector<LanguageEntry> m_languages;
};

#endif // LOCALECONTROLLER_H
