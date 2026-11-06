// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/variablehighlighter.h"

#include <QColor>

/*!
 * \brief Creates a highlighter without a document or variables.
 * \param parent The QObject parent.
 */
VariableHighlighter::VariableHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    m_unresolvedFormat.setFontUnderline(true);
    updateFormats();
}

/*!
 * \brief Sets the variables references resolve from and recolors the document.
 * \param variables The variables of the active environment.
 */
void VariableHighlighter::setVariables(const VariableResolver &variables)
{
    m_variables = variables;
    rehighlight();
}

/*!
 * \brief Selects the palette for a dark background and recolors the document.
 *
 * The document holds a single line, so recoloring it in place stays cheap.
 * \param dark True for the dark palette.
 */
void VariableHighlighter::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    updateFormats();
    rehighlight();
}

/*!
 * \brief Colors the block \a text.
 * \param text The text of the block.
 */
void VariableHighlighter::highlightBlock(const QString &text)
{
    const QList<VariableReference> references = VariableResolver::references(text);
    for (const VariableReference &reference : references) {
        setFormat(int(reference.start), int(reference.length),
                  m_variables.contains(reference.name) ? m_knownFormat : m_unresolvedFormat);
    }
}

/*!
 * \internal
 * \brief Applies the colors of the active palette to the formats.
 */
void VariableHighlighter::updateFormats()
{
    m_knownFormat.setForeground(QColor(m_dark ? 0x90CAF9 : 0x1565C0));
    m_unresolvedFormat.setForeground(QColor(m_dark ? 0xEF9A9A : 0xC62828));
}
