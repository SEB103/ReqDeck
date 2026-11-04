// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef JSONFILE_H
#define JSONFILE_H

#include <QJsonObject>
#include <QString>

/** Writes \a root as indented JSON to \a path atomically, creating parent directories. */
bool writeJsonFile(const QString &path, const QJsonObject &root);

/** Renames the unusable file at \a path to "<path>.corrupt", replacing an older copy. */
void setAsideCorruptFile(const QString &path);

#endif // JSONFILE_H
