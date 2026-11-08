// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick

/*!
    \qmltype BsVariables
    \inqmlmodule Base
    \brief Finds {{variable}} references in text, with the syntax the request builder uses.
*/
QtObject {
    /*!
        Returns the names referenced in \a text that are not in \a known, each once,
        in the order they first appear. A null \a known checks nothing.
    */
    function unknownIn(text, known) {
        if (known === null || known === undefined || text === undefined || text.length === 0)
            return []
        // Same pattern as VariableResolver in C++.
        const pattern = /\{\{\s*([A-Za-z_][A-Za-z0-9_.\-]*)\s*\}\}/g
        const unknown = []
        let match
        while ((match = pattern.exec(text)) !== null) {
            const name = match[1]
            if (known.indexOf(name) < 0 && unknown.indexOf(name) < 0)
                unknown.push(name)
        }
        return unknown
    }
}
