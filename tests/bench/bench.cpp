// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#include "core/http/httpexecutor.h"
#include "core/http/requestbuilder.h"
#include "core/workspace/workspacedata.h"
#include "core/workspace/workspaceserializer.h"
#include "mockhttpserver.h"
#include "qmlapi/responseview.h"

namespace {
/*!
 * \internal
 * \brief Returns the private memory of this process in bytes, or 0 where it is not known.
 */
qint64 privateBytes()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters {};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                             sizeof(counters)))
        return qint64(counters.PrivateUsage);
#endif
    return 0;
}

/*!
 * \internal
 * \brief Prints one measurement as "BENCH name value unit", which measure.ps1 reads.
 */
void report(const char *name, double value, const char *unit)
{
    QTextStream(stdout) << "BENCH " << name << ' ' << QString::number(value, 'f', 1) << ' ' << unit << Qt::endl;
}

/*!
 * \internal
 * \brief Returns a workspace of \a count requests in folders of 100.
 */
WorkspaceData bigWorkspace(int count)
{
    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QStringLiteral("Benchmark");
    CollectionNode folder;
    for (int i = 0; i < count; ++i) {
        if (i % 100 == 0) {
            if (i > 0)
                data.collection.append(folder);
            folder = CollectionNode();
            folder.id = newItemId();
            folder.name = QStringLiteral("Folder %1").arg(i / 100 + 1);
        }
        CollectionNode node;
        node.kind = CollectionNode::Kind::Request;
        node.id = newItemId();
        node.name = QStringLiteral("Request %1").arg(i + 1);
        node.request.method = i % 3 == 0 ? HttpMethod::Post : HttpMethod::Get;
        node.request.url = QStringLiteral("{{baseUrl}}/items/%1?page=2").arg(i);
        node.request.headers = {{QStringLiteral("Accept"), QStringLiteral("application/json"), true}};
        if (node.request.method == HttpMethod::Post) {
            node.request.body.mode = BodyMode::Raw;
            node.request.body.rawType = RawBodyType::Json;
            node.request.body.raw = QStringLiteral(R"({"name":"item %1","active":true})").arg(i);
        }
        folder.children.append(node);
    }
    data.collection.append(folder);
    return data;
}

/*!
 * \internal
 * \brief Returns a JSON array of about \a bytes bytes.
 */
QByteArray bigJson(qint64 bytes)
{
    QByteArray body = "[";
    for (int i = 0; body.size() < bytes; ++i) {
        if (i > 0)
            body += ',';
        body += R"({"id":)" + QByteArray::number(i) + R"(,"name":"item )" + QByteArray::number(i)
                + R"(","tags":["alpha","beta"],"active":true,"price":12.5})";
    }
    return body + ']';
}
} // namespace

/*!
 * \internal
 * \brief Measures workspace loading, a large JSON response, and memory over 100 requests.
 *
 * With a path argument, the 3,000-request workspace is also written there, so
 * measure.ps1 can open it in the application.
 */
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString workspacePath = app.arguments().value(1);
    QTemporaryDir dir;

    // 1. A workspace of 3,000 requests.
    const WorkspaceData data = bigWorkspace(3000);
    const QString file = workspacePath.isEmpty() ? dir.filePath(QStringLiteral("bench.reqdeck")) : workspacePath;
    QElapsedTimer timer;
    timer.start();
    if (!WorkspaceSerializer::save(file, data))
        return 1;
    report("workspace3000.save", double(timer.elapsed()), "ms");
    timer.restart();
    const WorkspaceSerializer::LoadResult loaded = WorkspaceSerializer::load(file);
    if (!loaded.ok)
        return 1;
    report("workspace3000.load", double(timer.elapsed()), "ms");

    // 2. A 20 MB JSON response: shown and filtered; pretty-printing covers bodies up to the display limit.
    ResponseData response;
    response.statusCode = 200;
    response.reasonPhrase = QStringLiteral("OK");
    response.contentType = QStringLiteral("application/json");
    response.body = bigJson(20 * 1024 * 1024);
    response.sizeBytes = response.body.size();
    ResponseView view;
    timer.restart();
    view.setResponse(response, {});
    (void)view.bodyText();
    report("json20mb.show", double(timer.elapsed()), "ms");
    timer.restart();
    view.setFilterText(QStringLiteral("$[*].id"));
    (void)view.bodyText();
    report("json20mb.filter", double(timer.elapsed()), "ms");
    view.setFilterText(QString());
    response.body = bigJson(900 * 1024);
    response.sizeBytes = response.body.size();
    view.setPretty(false);
    view.setResponse(response, {});
    timer.restart();
    view.setPretty(true);
    (void)view.bodyText();
    report("json1mb.pretty", double(timer.elapsed()), "ms");
    view.clear();
    response = ResponseData();

    // 3. 100 requests to a local server.
    MockHttpServer server;
    if (!server.start())
        return 1;
    RequestSpec spec;
    spec.url = server.url(QStringLiteral("/json/50")).toString();
    const BuiltRequest built = RequestBuilder::build(spec, {}, {});
    HttpExecutor executor;
    const qint64 before = privateBytes();
    timer.restart();
    for (int i = 0; i < 100; ++i) {
        QEventLoop loop;
        QObject::connect(&executor, &HttpExecutor::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        if (!executor.send(built))
            return 1;
        loop.exec();
    }
    report("requests100.time", double(timer.elapsed()), "ms");
    report("requests100.memoryGrowth", double(privateBytes() - before) / (1024.0 * 1024.0), "MB");
    report("process.privateMemory", double(privateBytes()) / (1024.0 * 1024.0), "MB");
    return 0;
}
