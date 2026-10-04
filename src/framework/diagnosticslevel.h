// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DIAGNOSTICSLEVEL_H
#define DIAGNOSTICSLEVEL_H

#include <QObject>

/**
 * Shared severity vocabulary for user-facing diagnostics.
 *
 * The same four levels classify entries in the in-application log and transient
 * notifications, so the status bar, the notification banner, and the log panel
 * can colour and filter them consistently. Notifications never use Debug.
 */
namespace Diagnostics {
Q_NAMESPACE

/** Severity of a log entry or a notification. */
enum Level {
    /** Developer-oriented detail; shown only when the log filter asks for it. */
    Debug = 0,
    /** Normal progress information, such as a completed operation. */
    Info,
    /** Something unexpected that did not stop the operation. */
    Warning,
    /** An operation failed and the user has to react. */
    Error
};
Q_ENUM_NS(Level)

} // namespace Diagnostics

#endif // DIAGNOSTICSLEVEL_H
