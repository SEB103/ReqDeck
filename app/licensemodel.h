// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LICENSEMODEL_H
#define LICENSEMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>

/**
 * List of bundled third-party license documents shown by the About dialog.
 *
 * setDirectory() scans a directory of "*.txt" files (typically the Qt resource
 * path "qrc:/licenses/LICENSES") and exposes each file's SPDX-identifier title
 * and path to QML. The model is read-only and is rebuilt only when
 * setDirectory() is called.
 */
class LicenseModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /** Roles exposed to QML license delegates. */
    enum Role {
        /** License title (the file's SPDX identifier, e.g. "MPL-2.0"). */
        TitleRole = Qt::UserRole + 1,
        /** Path of the license text file, usable with AppInfo::readText(). */
        PathRole
    };
    Q_ENUM(Role)

    /** Creates an empty model; call setDirectory() to populate it. */
    explicit LicenseModel(QObject *parent = nullptr);

    /** Returns data for \a index and \a role. */
    QVariant data(const QModelIndex &index, int role) const override;
    /** Returns the number of rows below \a parent. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    /** Returns role names exposed to QML. */
    QHash<int, QByteArray> roleNames() const override;

    /** Scans \a directory for "*.txt" files and rebuilds the model. Accepts a
     *  filesystem path or a "qrc:/" resource path. */
    Q_INVOKABLE void setDirectory(const QString &directory);

    /** Returns the file path of the entry at \a index, or an empty string. */
    Q_INVOKABLE QString pathAt(int index) const;

    /** Returns the title of the entry at \a index, or an empty string. */
    Q_INVOKABLE QString titleAt(int index) const;

private:
    /** One license document entry. */
    struct Entry
    {
        /** License title (the file's SPDX identifier). */
        QString title;
        /** Path of the license text file. */
        QString path;
    };

    /** License documents in display order. */
    QList<Entry> m_entries;
};

#endif // LICENSEMODEL_H
