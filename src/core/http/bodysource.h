// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BODYSOURCE_H
#define BODYSOURCE_H

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QString>

#include <memory>

/** One piece of a streamed body: bytes held in memory, or a file read while sending. */
struct BodySegment
{
    /** Bytes of the piece; used when filePath is empty. */
    QByteArray bytes;
    /** Absolute path of a file whose content is the piece; empty for bytes. */
    QString filePath;

    bool operator==(const BodySegment &) const = default;
};

/**
 * Reads a list of body segments as one device, files included.
 *
 * Files are opened when the device opens and read in chunks as the network
 * asks for data, so a large upload is never held in memory. The device is
 * random-access with a known size, which gives the request its
 * Content-Length; a new device is made for every send of the same body.
 */
class SegmentedBodyDevice : public QIODevice
{
    Q_OBJECT

public:
    /** Creates a closed device over \a segments. */
    explicit SegmentedBodyDevice(QList<BodySegment> segments, QObject *parent = nullptr);

    /** Destroys the device and closes its files. */
    ~SegmentedBodyDevice() override;

    /** Opens the device for reading; fails with errorString() when a file cannot be read. */
    bool open(OpenMode mode) override;

    /** Closes the device and its files. */
    void close() override;

    /** Returns false: the device can seek, so the network can resend from the start. */
    bool isSequential() const override;

    /** Returns the total size of all segments, as measured when the device opened. */
    qint64 size() const override;

    /** Moves to \a pos; false beyond the end. */
    bool seek(qint64 pos) override;

protected:
    /** Reads up to \a maxSize bytes at the current position into \a data. */
    qint64 readData(char *data, qint64 maxSize) override;

    /** Refuses writing; the device is read-only. */
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    /** Segments in order. */
    QList<BodySegment> m_segments;
    /** Open file per segment; null for bytes. */
    QList<std::shared_ptr<QFile>> m_files;
    /** Offset of the start of each segment. */
    QList<qint64> m_offsets;
    /** Size of all segments. */
    qint64 m_size {0};
};

#endif // BODYSOURCE_H
