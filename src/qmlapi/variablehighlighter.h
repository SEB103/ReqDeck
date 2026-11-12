// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VARIABLEHIGHLIGHTER_H
#define VARIABLEHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include "core/http/variableresolver.h"

/** Colors {{name}} references by whether they resolve; unresolved ones are also underlined. */
class VariableHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VariableHighlighter)

public:
    /** Creates a highlighter without a document or variables. */
    explicit VariableHighlighter(QObject *parent = nullptr);

    /** Sets the variables references resolve from and recolors the document. */
    void setVariables(const VariableResolver &variables);

    /** Selects the palette for a dark background and recolors the document. */
    void setDark(bool dark);

protected:
    /** Colors the block \a text. */
    void highlightBlock(const QString &text) override;

private:
    /** Applies the colors of the active palette to the formats. */
    void updateFormats();

    /** The variables references resolve from. */
    VariableResolver m_variables;

    /** Whether the dark palette is active. */
    bool m_dark {false};

    /** Format of a reference that resolves. */
    QTextCharFormat m_knownFormat;

    /** Format of a reference that does not resolve. */
    QTextCharFormat m_unresolvedFormat;

    /** Format of a response() template, whose value is known only when the request is sent. */
    QTextCharFormat m_responseFormat;
};

#endif // VARIABLEHIGHLIGHTER_H
