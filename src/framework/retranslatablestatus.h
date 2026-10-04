// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RETRANSLATABLESTATUS_H
#define RETRANSLATABLESTATUS_H

#include <QString>

#include <functional>

#include "framework/diagnosticslevel.h"

/**
 * Remembers the last user-facing status message so it can be rebuilt after a
 * live UI language switch.
 *
 * The status bar keeps the most recent outcome on screen, but a notification is
 * emitted as an already-translated string, so retranslating the QML bindings
 * cannot refresh it. Instead of storing the built text, a controller stores the
 * renderer closure that produced it (with its runtime arguments captured by
 * value); re-invoking the closure re-runs its tr() calls in the now-active
 * language. See the \c statusRetranslated signals in the QML-facing controllers.
 */
struct RetranslatableStatus
{
    /** Severity of the stored message as a Diagnostics::Level value. */
    int level = Diagnostics::Info;

    /** Rebuilds the message text in the active language, or empty when unset. */
    std::function<QString()> render;

    /** Whether a renderer has been stored yet. */
    bool isValid() const { return static_cast<bool>(render); }
};

#endif // RETRANSLATABLESTATUS_H
