// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/importcontroller.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include "core/import/insomniaimporter.h"
#include "core/import/openapiimporter.h"
#include "core/import/postmanimporter.h"
#include "core/import/yamljson.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/environmentmodel.h"

namespace {
/*!
 * \internal
 * \brief Largest file an import reads; a collection of thousands of requests stays far below.
 */
constexpr qint64 kMaxImportBytes = 64 * 1024 * 1024;

/*!
 * \internal
 * \brief Counts the requests and folders below \a node into \a requests and \a folders.
 */
void count(const CollectionNode &node, int &requests, int &folders)
{
    for (const CollectionNode &child : node.children) {
        if (child.kind == CollectionNode::Kind::Request) {
            ++requests;
        } else {
            ++folders;
            count(child, requests, folders);
        }
    }
}
} // namespace

/*!
    \class ImportController
    \brief Brings collections and environments of other tools into the workspace.

    \internal
*/

/*!
 * \brief Creates a controller that adds to \a collection and \a environments.
 * \param collection The collection model of the active workspace.
 * \param environments The environment model of the active workspace.
 * \param parent The QObject parent.
 */
ImportController::ImportController(CollectionTreeModel *collection, EnvironmentModel *environments, QObject *parent)
    : QObject(parent)
    , m_collection(collection)
    , m_environments(environments)
{
}

bool ImportController::ready() const
{
    return m_result.ok;
}

QString ImportController::errorString() const
{
    return m_result.errorString;
}

/*!
 * \brief Returns one line on what the previewed file holds.
 */
QString ImportController::summary() const
{
    if (!m_result.ok)
        return {};
    QStringList parts;
    if (m_result.hasCollection) {
        int requests = 0;
        int folders = 0;
        count(m_result.folder, requests, folders);
        parts.append(tr("%1 in %2").arg(tr("%n request(s)", "", requests), tr("%n folder(s)", "", folders + 1)));
    }
    if (!m_result.environments.isEmpty())
        parts.append(tr("%n environment(s)", "", int(m_result.environments.size())));
    return tr("%1: %2").arg(m_result.format, parts.join(QStringLiteral(", ")));
}

QStringList ImportController::report() const
{
    return m_result.report;
}

/*!
 * \brief Reads the file at \a pathOrUrl.
 *
 * The file is only read; nothing in it runs, and nothing changes in the
 * workspace until apply().
 * \param pathOrUrl A local path or file URL, as a file dialog gives it.
 * \return Whether the file can be imported.
 */
bool ImportController::preview(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    QFile file(path);
    if (QFileInfo(path).size() > kMaxImportBytes) {
        m_result = ImportResult();
        m_result.errorString = tr("The file is too large to import.");
        emit previewChanged();
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_result = ImportResult();
        m_result.errorString = tr("The file could not be read: %1").arg(file.errorString());
        emit previewChanged();
        return false;
    }
    return previewData(file.readAll());
}

/*!
 * \brief Recognizes the format of \a data and reads it.
 *
 * JSON is read as it is. Anything that does not start like JSON is read as
 * YAML and converted to JSON first, so every format can come in either form.
 * \param data The content of a file.
 * \return Whether it can be imported.
 */
bool ImportController::previewData(const QByteArray &data)
{
    QByteArray json = data;
    const QByteArray start = data.trimmed().left(1);
    if (start != "{" && start != "[") {
        QString yamlError;
        const std::optional<QJsonDocument> converted = YamlJson::toJson(data, &yamlError);
        if (!converted) {
            m_result = ImportResult();
            m_result.errorString = tr("ReqDeck could not read this file as JSON or YAML. %1").arg(yamlError);
            emit previewChanged();
            return false;
        }
        json = converted->toJson(QJsonDocument::Compact);
    }

    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (PostmanImporter::isCollection(root)) {
        m_result = PostmanImporter::importCollection(json);
    } else if (PostmanImporter::isEnvironment(root)) {
        m_result = PostmanImporter::importEnvironment(json);
    } else if (InsomniaImporter::isExport(root)) {
        m_result = InsomniaImporter::importExport(json);
    } else if (OpenApiImporter::isDocument(root)) {
        m_result = OpenApiImporter::importDocument(json);
    } else {
        m_result = ImportResult();
        m_result.errorString = tr("ReqDeck does not know this file. It reads Postman collections and environments, Insomnia exports, and OpenAPI 3 documents.");
    }
    emit previewChanged();
    return m_result.ok;
}

/*!
 * \brief Forgets the preview and its error.
 *
 * The import dialog calls it when it opens and when it is cancelled, so that
 * Import never adds a file chosen for another workspace.
 */
void ImportController::reset()
{
    m_result = ImportResult();
    emit previewChanged();
}

/*!
 * \brief Adds the previewed content to the workspace.
 *
 * Nothing is replaced: the collection becomes a new folder at the end of the
 * tree, and an environment whose name is taken gets " (imported)". The
 * workspace becomes dirty and is saved only when the user saves it.
 * \return The id of the new folder, or an empty string when the file held no collection.
 */
QString ImportController::apply()
{
    if (!m_result.ok)
        return {};
    QString folderId;
    if (m_result.hasCollection) {
        folderId = m_collection->addImportedFolder(m_result.folder);
        if (folderId.isEmpty()) {
            m_result.ok = false;
            m_result.errorString = tr("The collection could not be added to the workspace.");
            emit previewChanged();
            return {};
        }
    }
    for (Environment environment : std::as_const(m_result.environments)) {
        const QStringList names = m_environments->names();
        if (names.contains(environment.name))
            environment.name = tr("%1 (imported)").arg(environment.name);
        m_environments->importEnvironment(environment);
    }
    m_result = ImportResult();
    emit previewChanged();
    return folderId;
}
