// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMPORTCONTROLLER_H
#define IMPORTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "core/import/importresult.h"

class CollectionTreeModel;
class EnvironmentModel;

/**
 * Imports files of other tools into the active workspace; exposed to QML as
 * \c cppImport. preview() reads a file and says what it holds and what will not
 * carry over; apply() adds it without replacing anything.
 */
class ImportController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ImportController)

    /** Whether the previewed file can be imported. */
    Q_PROPERTY(bool ready READ ready NOTIFY previewChanged)

    /** Why the previewed file cannot be imported; empty otherwise. */
    Q_PROPERTY(QString errorString READ errorString NOTIFY previewChanged)

    /** One line on what the file holds, for example "Postman collection: 12 requests in 3 folders". */
    Q_PROPERTY(QString summary READ summary NOTIFY previewChanged)

    /** What will not carry over, one line per item. */
    Q_PROPERTY(QStringList report READ report NOTIFY previewChanged)

public:
    /** Creates a controller that adds to \a collection and \a environments. */
    ImportController(CollectionTreeModel *collection, EnvironmentModel *environments, QObject *parent = nullptr);

    /** Returns whether the previewed file can be imported. */
    bool ready() const;

    /** Returns why the previewed file cannot be imported. */
    QString errorString() const;

    /** Returns what the previewed file holds. */
    QString summary() const;

    /** Returns what will not carry over. */
    QStringList report() const;

    /** Reads the file at \a pathOrUrl and recognizes its format; returns ready(). */
    Q_INVOKABLE bool preview(const QString &pathOrUrl);

    /** Reads \a data, JSON or YAML, as if it came from a file; returns ready(). */
    bool previewData(const QByteArray &data);

    /** Adds the previewed content to the workspace; returns the id of the new folder, or an empty string. */
    Q_INVOKABLE QString apply();

signals:
    /** Emitted when another file was previewed or the preview was used. */
    void previewChanged();

private:
    /** Where the content goes. */
    CollectionTreeModel *m_collection {nullptr};

    /** Where the environments go. */
    EnvironmentModel *m_environments {nullptr};

    /** The previewed content. */
    ImportResult m_result;
};

#endif // IMPORTCONTROLLER_H
