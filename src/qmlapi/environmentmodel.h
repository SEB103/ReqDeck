// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENVIRONMENTMODEL_H
#define ENVIRONMENTMODEL_H

#include <QAbstractListModel>
#include <QStringList>

#include "core/workspace/workspacedata.h"

class EnvironmentVariablesModel;
class QQuickTextDocument;
class VariableHighlighter;
class WorkspaceManager;

/**
 * The environments of the active workspace, exposed to QML as \c cppEnvironments.
 *
 * Rows are read from WorkspaceManager, and every change goes straight into its
 * in-memory workspace. The model also tracks the environment shown in the
 * editor, which is independent of the active one, and owns the highlighter of
 * the URL field, which colors references by the active environment.
 */
class EnvironmentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EnvironmentModel)
    Q_MOC_INCLUDE("qmlapi/environmentvariablesmodel.h")

    /** Names of the environments in display order. */
    Q_PROPERTY(QStringList names READ names NOTIFY namesChanged)

    /** Row of the active environment, or -1 when none is active. */
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeChanged)

    /** Names of the variables that resolve in the active environment; empty without one. */
    Q_PROPERTY(QStringList activeVariableNames READ activeVariableNames NOTIFY activeVariablesChanged)

    /** Row of the environment shown in the editor, or -1. */
    Q_PROPERTY(int editedRow READ editedRow WRITE setEditedRow NOTIFY editedRowChanged)

    /** Name of the environment shown in the editor. */
    Q_PROPERTY(QString editedName READ editedName NOTIFY editedNameChanged)

    /** Variables of the environment shown in the editor. */
    Q_PROPERTY(EnvironmentVariablesModel *variablesModel READ variablesModel CONSTANT)

    /** Whether the URL highlight uses the palette for dark backgrounds. */
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)

public:
    /** Data roles of a row. */
    enum Role {
        IdRole = Qt::UserRole + 1, /**< Stable identifier. */
        NameRole,                  /**< Display name. */
        ActiveRole                 /**< Whether this is the active environment. */
    };

    /** Creates a model over the active workspace of \a manager. */
    explicit EnvironmentModel(WorkspaceManager *manager, QObject *parent = nullptr);

    /** Returns the number of environments. */
    int rowCount(const QModelIndex &parent = {}) const override;

    /** Returns the value of \a role for \a index. */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Returns the QML names of the roles. */
    QHash<int, QByteArray> roleNames() const override;

    /** Returns the names of the environments. */
    QStringList names() const;

    /** Returns the row of the active environment, or -1. */
    int activeIndex() const;

    /** Activates the environment at \a index; -1 activates none, other values are ignored. */
    void setActiveIndex(int index);

    /** Returns the row shown in the editor, or -1. */
    int editedRow() const;

    /** Shows the environment at \a row in the editor; -1 shows none. */
    void setEditedRow(int row);

    /** Returns the name of the environment shown in the editor. */
    QString editedName() const;

    /** Returns the names of the variables that resolve in the active environment. */
    QStringList activeVariableNames() const;

    /** Returns the variables of the environment shown in the editor. */
    EnvironmentVariablesModel *variablesModel() const;

    /** Returns whether the URL highlight uses the dark palette. */
    bool darkTheme() const;

    /** Selects the palette of the URL highlight. */
    void setDarkTheme(bool dark);

    /** Returns the highlighter of the URL field. */
    VariableHighlighter *urlHighlighter() const;

    /** Appends a new environment, shows it in the editor, and returns its row, or -1. */
    Q_INVOKABLE int addEnvironment();

    /** Appends a copy of \a row, shows it in the editor, and returns its row, or -1. */
    Q_INVOKABLE int duplicateEnvironment(int row);

    /** Renames \a row to the trimmed \a name; returns false for a blank name or invalid row. */
    Q_INVOKABLE bool renameEnvironment(int row, const QString &name);

    /** Removes \a row; an invalid row number is ignored. */
    Q_INVOKABLE void removeEnvironment(int row);

    /**
     * Adds an enabled secret variable with \a value to the active environment, named
     * \a baseKey or \a baseKey with a number when taken, and returns the key. Without
     * an active environment one named "Default" is made active. Returns an empty
     * string without a workspace.
     */
    Q_INVOKABLE QString addSecretVariable(const QString &baseKey, const QString &value);

    /**
     * Sets the variable \a key of the active environment to \a value, adding it, secret
     * when \a secret is true, when missing; an environment named "Default" is made
     * active when none is.
     */
    Q_INVOKABLE void setVariable(const QString &key, const QString &value, bool secret = false);

    /** Appends the imported \a environment; returns its row, or -1 without a workspace. */
    int importEnvironment(const Environment &environment);

    /** Colors the URL held by \a document. */
    Q_INVOKABLE void attachUrlHighlighter(QQuickTextDocument *document);

signals:
    /** Emitted when the list of names changes. */
    void namesChanged();

    /** Emitted when the active environment or its row changes. */
    void activeChanged();

    /** Emitted when the variables of the active environment may have changed. */
    void activeVariablesChanged();

    /** Emitted when the row shown in the editor changes. */
    void editedRowChanged();

    /** Emitted when the name shown in the editor changes. */
    void editedNameChanged();

    /** Emitted when the palette changes. */
    void darkThemeChanged();

private:
    /** Returns the environments of the workspace. */
    const QList<Environment> &environments() const;

    /** Appends \a environment and shows it in the editor; returns its row. */
    int appendEnvironment(const Environment &environment);

    /** Rebuilds everything after another workspace became active. */
    void resetFromWorkspace();

    /** Writes the edited table to its environment. */
    void storeVariables();

    /** Loads \a row into the editor, also when it is the row already shown. */
    void showInEditor(int row);

    /** Passes the active variables to the URL highlight. */
    void refreshHighlighter();

    /** Returns the row of the active environment, making "Default" active when none is; -1 without a workspace. */
    int activeRowForNewVariable();

    /** Workspace holding the environments; not owned. */
    WorkspaceManager *m_manager {nullptr};

    /** Variables of the edited environment; owned through QObject. */
    EnvironmentVariablesModel *m_variables {nullptr};

    /** Highlighter of the URL field; owned through QObject. */
    VariableHighlighter *m_highlighter {nullptr};

    /** Row shown in the editor, or -1. */
    int m_editedRow {-1};

    /** Whether the dark palette is selected. */
    bool m_darkTheme {false};
};

#endif // ENVIRONMENTMODEL_H
