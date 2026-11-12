// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "core/http/bodysource.h"

/*!
 * \internal
 * \brief Writes \a content to \a name in \a dir and returns the path.
 */
static QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(content);
    return path;
}

/*! Verifies the device that streams body segments. */
class BodySourceTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that bytes and files are read in order with the right size. */
    void readsSegmentsInOrder();

    /*! Verifies that the device can go back to the start and read again. */
    void seeksAndRereads();

    /*! Verifies that empty segments are skipped. */
    void skipsEmptySegments();

    /*! Verifies that a missing file fails to open with its path in the message. */
    void missingFileFailsToOpen();

    /*! Verifies that a large file is read in chunks the caller asks for. */
    void readsLargeFileInChunks();
};

void BodySourceTest::readsSegmentsInOrder()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, QStringLiteral("a.txt"), "FILE");
    SegmentedBodyDevice device({{"head-", {}}, {{}, path}, {"-tail", {}}});

    QVERIFY(device.open(QIODevice::ReadOnly));
    QCOMPARE(device.size(), 14);
    QVERIFY(!device.isSequential());
    QCOMPARE(device.readAll(), QByteArray("head-FILE-tail"));
    QVERIFY(device.atEnd());
}

void BodySourceTest::seeksAndRereads()
{
    QTemporaryDir dir;
    const QString path = writeFile(dir, QStringLiteral("a.txt"), "0123456789");
    SegmentedBodyDevice device({{"ab", {}}, {{}, path}});
    QVERIFY(device.open(QIODevice::ReadOnly));
    QCOMPARE(device.read(5), QByteArray("ab012"));

    QVERIFY(device.seek(0));
    QCOMPARE(device.readAll(), QByteArray("ab0123456789"));
    QVERIFY(device.seek(7));
    QCOMPARE(device.read(3), QByteArray("567"));
    QVERIFY(!device.seek(100));
}

void BodySourceTest::skipsEmptySegments()
{
    QTemporaryDir dir;
    const QString empty = writeFile(dir, QStringLiteral("empty.txt"), QByteArray());
    SegmentedBodyDevice device({{QByteArray(), {}}, {{}, empty}, {"x", {}}, {QByteArray(), {}}});
    QVERIFY(device.open(QIODevice::ReadOnly));
    QCOMPARE(device.size(), 1);
    QCOMPARE(device.readAll(), QByteArray("x"));
}

void BodySourceTest::missingFileFailsToOpen()
{
    SegmentedBodyDevice device({{"a", {}}, {{}, QStringLiteral("Z:/nowhere/missing.bin")}});
    QVERIFY(!device.open(QIODevice::ReadOnly));
    QVERIFY2(device.errorString().contains(QStringLiteral("missing.bin")), qPrintable(device.errorString()));
    QVERIFY(!device.isOpen());
}

void BodySourceTest::readsLargeFileInChunks()
{
    QTemporaryDir dir;
    QByteArray content(5 * 1024 * 1024, 'q');
    content[0] = 'A';
    content[content.size() - 1] = 'Z';
    const QString path = writeFile(dir, QStringLiteral("big.bin"), content);
    SegmentedBodyDevice device({{{}, path}});
    QVERIFY(device.open(QIODevice::ReadOnly));

    qint64 total = 0;
    QByteArray first;
    QByteArray last;
    while (!device.atEnd()) {
        const QByteArray chunk = device.read(64 * 1024);
        QVERIFY(!chunk.isEmpty());
        QVERIFY(chunk.size() <= 64 * 1024);
        if (first.isEmpty())
            first = chunk.left(1);
        last = chunk.right(1);
        total += chunk.size();
    }
    QCOMPARE(total, content.size());
    QCOMPARE(first, QByteArray("A"));
    QCOMPARE(last, QByteArray("Z"));
}

QTEST_GUILESS_MAIN(BodySourceTest)

#include "tst_bodysource.moc"
