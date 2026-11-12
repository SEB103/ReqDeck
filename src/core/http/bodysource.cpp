// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/bodysource.h"

#include <algorithm>
#include <cstring>

/*!
 * \class SegmentedBodyDevice
 * \inmodule ReqDeckCore
 * \brief Reads a list of body segments as one device, files included.
 */

/*!
 * \brief Creates a closed device over \a segments, owned by \a parent.
 */
SegmentedBodyDevice::SegmentedBodyDevice(QList<BodySegment> segments, QObject *parent)
    : QIODevice(parent)
    , m_segments(std::move(segments))
{
}

/*!
 * \brief Closes the files and destroys the device.
 */
SegmentedBodyDevice::~SegmentedBodyDevice()
{
    SegmentedBodyDevice::close();
}

/*!
 * \brief Opens the device for reading.
 *
 * Every file is opened and measured now; the size stays what it was here
 * even when a file grows while it is sent, and a file that shrinks ends
 * the body early, which the server sees as a short body.
 * \param mode Must be read-only.
 * \return False with errorString() naming the file that cannot be read.
 */
bool SegmentedBodyDevice::open(OpenMode mode)
{
    if ((mode & WriteOnly) || !(mode & ReadOnly)) {
        setErrorString(QStringLiteral("The body can only be read."));
        return false;
    }
    m_files.clear();
    m_offsets.clear();
    m_size = 0;
    for (const BodySegment &segment : std::as_const(m_segments)) {
        m_offsets.append(m_size);
        if (segment.filePath.isEmpty()) {
            m_files.append(nullptr);
            m_size += segment.bytes.size();
            continue;
        }
        auto file = std::make_shared<QFile>(segment.filePath);
        if (!file->open(QIODevice::ReadOnly)) {
            setErrorString(QStringLiteral("The file %1 could not be read: %2").arg(segment.filePath, file->errorString()));
            m_files.clear();
            m_offsets.clear();
            m_size = 0;
            return false;
        }
        m_size += file->size();
        m_files.append(std::move(file));
    }
    return QIODevice::open(mode | Unbuffered);
}

/*!
 * \brief Closes the device and its files.
 */
void SegmentedBodyDevice::close()
{
    m_files.clear();
    QIODevice::close();
}

/*!
 * \brief Returns false: the device can seek.
 */
bool SegmentedBodyDevice::isSequential() const
{
    return false;
}

/*!
 * \brief Returns the total size of all segments.
 */
qint64 SegmentedBodyDevice::size() const
{
    return m_size;
}

/*!
 * \brief Moves to \a pos.
 * \return False when \a pos is negative or beyond the end.
 */
bool SegmentedBodyDevice::seek(qint64 pos)
{
    if (pos < 0 || pos > m_size)
        return false;
    return QIODevice::seek(pos);
}

/*!
 * \brief Reads up to \a maxSize bytes at pos() into \a data.
 *
 * Fills \a data across segment borders, so one call returns as much as asked
 * up to the end of the body; a file is read only for the part requested.
 * \return The number of bytes read, 0 at the end, or -1 when a file failed.
 */
qint64 SegmentedBodyDevice::readData(char *data, qint64 maxSize)
{
    qint64 position = pos();
    qint64 total = 0;
    while (total < maxSize && position < m_size) {
        const auto next = std::upper_bound(m_offsets.cbegin(), m_offsets.cend(), position);
        const qsizetype index = std::distance(m_offsets.cbegin(), next) - 1;
        const qint64 offset = position - m_offsets.at(index);
        const qint64 segmentEnd = index + 1 < m_offsets.size() ? m_offsets.at(index + 1) : m_size;
        const qint64 wanted = std::min<qint64>(maxSize - total, segmentEnd - position);

        qint64 read = 0;
        if (!m_files.at(index)) {
            read = wanted;
            std::memcpy(data + total, m_segments.at(index).bytes.constData() + offset, size_t(read));
        } else {
            QFile &file = *m_files.at(index);
            if (file.pos() != offset && !file.seek(offset))
                return total > 0 ? total : -1;
            read = file.read(data + total, wanted);
            if (read <= 0) {
                // A file that shrank since open() ends the body early.
                setErrorString(file.errorString());
                return total > 0 ? total : -1;
            }
        }
        total += read;
        position += read;
    }
    return total;
}

/*!
 * \brief Refuses writing.
 * \return -1.
 */
qint64 SegmentedBodyDevice::writeData(const char *, qint64)
{
    return -1;
}
