// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include "framework/logfiltermodel.h"
#include "framework/logmodel.h"

/*!
 * \internal
 * \brief Mock application engine exposing the log the log panel reads.
 *
 * Stands in for the \c cppAppEngine context property of the application so the
 * Base components can be created without the application's context.
 */
class MockAppEngine : public QObject
{
    Q_OBJECT

    /*! Real filtered log model so the panel is exercised against genuine roles. */
    Q_PROPERTY(LogFilterModel *logModel READ logModel CONSTANT)

    /*! Text most recently passed to copyToClipboard(). */
    Q_PROPERTY(QString copiedText READ copiedText NOTIFY copiedTextChanged)

public:
    /*! Creates the mock and wires the filter proxy onto the log model. */
    explicit MockAppEngine(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_logFilterModel.setSourceModel(&m_logModel);
    }

    /*! Returns the real filtered log model. */
    LogFilterModel *logModel() { return &m_logFilterModel; }

    /*! Returns the text most recently passed to copyToClipboard(). */
    QString copiedText() const { return m_copiedText; }

    /*! Appends \a message at \a level so a test can populate the log. */
    Q_INVOKABLE void log(int level, const QString &message)
    {
        m_logModel.appendEntry(level, QString(), message);
    }

    /*! Removes every log entry. */
    Q_INVOKABLE void clearLog() { m_logModel.clear(); }

    /*! Mock no-op that reports that no log file exists. */
    Q_INVOKABLE bool showLogFileLocation() { return false; }

    /*! Records \a text instead of touching the system clipboard. */
    Q_INVOKABLE void copyToClipboard(const QString &text)
    {
        m_copiedText = text;
        emit copiedTextChanged();
    }

signals:
    /*! Emitted when copiedText changes. */
    void copiedTextChanged();

private:
    /*! Real log storage backing the panel under test. */
    LogModel m_logModel;

    /*! Real severity and text filter shown by the panel under test. */
    LogFilterModel m_logFilterModel;

    /*! Text most recently passed to copyToClipboard(). */
    QString m_copiedText;
};

/*!
 * \internal
 * \brief Quick Test setup object that injects the mock context into each engine.
 */
class QmlTestSetup : public QObject
{
    Q_OBJECT

public slots:
    /*! Adds the mock engine to \a engine as \c cppAppEngine. */
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(QStringLiteral("cppAppEngine"), &m_appEngine);
    }

private:
    /*! Mock engine kept alive for the lifetime of the Quick Test setup object. */
    MockAppEngine m_appEngine;
};

QUICK_TEST_MAIN_WITH_SETUP(reqdeck_qml, QmlTestSetup)

#include "tst_qml.moc"
