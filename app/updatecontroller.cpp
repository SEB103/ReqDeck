// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "updatecontroller.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QUrl>

#include "framework/apppaths.h"
#include "productinfo.h"

#include <utility>

namespace {
/*! \internal Settings key holding the automatic startup-check preference. */
constexpr auto kCheckAutomaticallyKey = "updates/checkAutomatically";

/*! \internal Maximum time a release query may take before it is aborted. */
constexpr int kRequestTimeoutMs = 10000;

/*!
 * \internal
 * \brief Qt Installer Framework option that starts the Maintenance Tool in
 *        updater mode (documented as \c --su / \c --start-updater).
 */
constexpr auto kStartUpdaterOption = "--start-updater";
} // namespace

/*!
 * \brief Creates the controller and loads the persisted startup-check preference.
 * \param settings Non-owning INI store for the preference, or null to disable
 *        persistence.
 * \param parent Optional QObject parent.
 *
 * The network manager is created lazily on the first checkNow() so that a run
 * that never checks for updates allocates nothing.
 */
UpdateController::UpdateController(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (m_settings)
        m_checkAutomatically = m_settings->value(QLatin1String(kCheckAutomaticallyKey), false).toBool();
}

UpdateController::~UpdateController() = default;

/*!
 * \brief Returns the current state of the update check.
 */
UpdateController::Status UpdateController::status() const
{
    return m_status;
}

/*!
 * \brief Returns whether a check is currently in progress.
 */
bool UpdateController::isChecking() const
{
    return m_status == Status::Checking;
}

/*!
 * \brief Returns whether a newer version is available.
 */
bool UpdateController::isUpdateAvailable() const
{
    return m_status == Status::UpdateAvailable;
}

/*!
 * \brief Returns whether an available update can be installed through the
 *        Maintenance Tool next to the executable.
 */
bool UpdateController::canInstallUpdate() const
{
    return updateAction() == UpdateAction::InstallUpdate;
}

/*!
 * \brief Returns the action offered for an available update.
 *
 * The installation mode is probed on every call (a cheap file-existence check)
 * so the answer stays correct if the Maintenance Tool appears or disappears
 * while the application runs.
 */
UpdateController::UpdateAction UpdateController::updateAction() const
{
    if (!isUpdateAvailable())
        return UpdateAction::None;
    return resolveUpdateAction(isPortable(), QFileInfo::exists(maintenanceToolPath()));
}

/*!
 * \brief Returns whether the in-app update check is enabled in the configuration.
 *
 * The value comes from the compile-time PRODUCT_UPDATE_ENABLED flag, which is
 * generated from the \c update.enabled field of packaging/product.json.
 */
bool UpdateController::featureEnabled() const
{
    return PRODUCT_UPDATE_ENABLED;
}

/*!
 * \brief Returns whether an update check runs automatically at startup.
 */
bool UpdateController::checkAutomatically() const
{
    return m_checkAutomatically;
}

/*!
 * \brief Returns the running application version.
 */
QString UpdateController::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

/*!
 * \brief Returns the latest version reported by the release server, or empty.
 */
QString UpdateController::latestVersion() const
{
    return m_latestVersion;
}

/*!
 * \brief Returns the release web page to open, or an empty string.
 */
QString UpdateController::releaseUrl() const
{
    return m_releaseUrl;
}

/*!
 * \brief Returns a human-readable message describing the current status.
 */
QString UpdateController::statusMessage() const
{
    return m_statusMessage;
}

/*!
 * \brief Returns the error of the last update action, or an empty string.
 */
QString UpdateController::actionError() const
{
    return m_actionError;
}

/*!
 * \brief Returns the absolute path of the Maintenance Tool of an installed copy.
 *
 * The installer places the tool next to the executable under the name taken
 * from packaging/product.json (PRODUCT_MAINTENANCE_TOOL_NAME); the launch
 * environment may override the whole path for tests.
 */
QString UpdateController::maintenanceToolPath() const
{
    if (!m_environment.maintenanceToolPath.isEmpty())
        return m_environment.maintenanceToolPath;
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral(PRODUCT_MAINTENANCE_TOOL_NAME ".exe"));
}

/*!
 * \brief Persists and applies whether the startup check runs.
 *
 * Ignores an unchanged value. The preference is written through the injected
 * settings store, mirroring how the UI language is persisted.
 * \param enabled Whether the automatic startup update check is turned on.
 */
void UpdateController::setCheckAutomatically(bool enabled)
{
    if (enabled == m_checkAutomatically)
        return;

    m_checkAutomatically = enabled;

    if (m_settings) {
        m_settings->setValue(QLatin1String(kCheckAutomaticallyKey), enabled);
        m_settings->sync();
    }

    emit checkAutomaticallyChanged();
}

/*!
 * \brief Replaces the launch environment used by installUpdate() and
 *        openDownloadPage(); intended for tests.
 * \param environment Overrides; members left unset keep the production defaults.
 */
void UpdateController::setLaunchEnvironment(LaunchEnvironment environment)
{
    m_environment = std::move(environment);
}

/*!
 * \brief Starts an update check against the configured release server.
 *
 * When the feature is disabled the check resolves immediately to
 * Status::NotConfigured without any network access. A check already in progress
 * is left running rather than restarted. The request carries a User-Agent and a
 * transfer timeout because the GitHub API rejects requests without the former and
 * a stalled connection must not leave the dialog spinning forever.
 */
void UpdateController::checkNow()
{
    if (!featureEnabled()) {
        setStatus(Status::NotConfigured,
                  tr("Update checking is not yet available."));
        return;
    }

    if (m_reply)
        return;

    setActionError(QString());
    setStatus(Status::Checking, tr("Checking for updates…"));

    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl(QStringLiteral(PRODUCT_UPDATE_RELEASES_API)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral(PRODUCT_IDENTIFIER "/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(kRequestTimeoutMs);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        handleReply(reply);
    });
}

/*!
 * \brief Starts the Maintenance Tool in updater mode and asks the application to quit.
 *
 * The tool is started detached with the working directory set to its own
 * folder, so it keeps running after this process exits and the update can
 * replace every file of the installation. Nothing is downloaded or written by
 * the application itself. The caller is expected to run the unsaved-changes
 * guards before calling this, and to quit when quitRequested() is emitted.
 * \return True when the tool was started; false (with actionError() set) when
 *         no update is available, the copy is portable, the tool is missing or
 *         the process could not be started.
 */
bool UpdateController::installUpdate()
{
    if (!canInstallUpdate()) {
        setActionError(tr("The update cannot be installed from this copy; download it from the release page instead."));
        return false;
    }

    const QString tool = maintenanceToolPath();
    const QString workingDir = QFileInfo(tool).absolutePath();
    const QStringList arguments {QString::fromLatin1(kStartUpdaterOption)};

    const bool started = m_environment.startProcess
        ? m_environment.startProcess(tool, arguments, workingDir)
        : QProcess::startDetached(tool, arguments, workingDir);

    if (!started) {
        setActionError(tr("Could not start the Maintenance Tool (%1).").arg(QDir::toNativeSeparators(tool)));
        return false;
    }

    setActionError(QString());
    emit quitRequested();
    return true;
}

/*!
 * \brief Opens the release page of the newer version in the system browser.
 *
 * Only an HTTPS URL is accepted, so a malformed or downgraded link from the
 * release metadata can never be handed to the system.
 * \return True when the URL was accepted by the system handler.
 */
bool UpdateController::openDownloadPage()
{
    const QUrl url(m_releaseUrl);
    if (!isUpdateAvailable() || !url.isValid()
        || url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
        setActionError(tr("The download page address is not a valid HTTPS link."));
        return false;
    }

    const bool opened = m_environment.openUrl
        ? m_environment.openUrl(url)
        : QDesktopServices::openUrl(url);

    if (!opened) {
        setActionError(tr("Could not open the download page."));
        return false;
    }

    setActionError(QString());
    return true;
}

/*!
 * \internal
 * \brief Parses the finished GitHub Releases \a reply and updates the state.
 *
 * A network error (including an aborted transfer or a 404 when no release exists
 * yet) becomes Status::Error; otherwise the body is handed to
 * applyReleasePayload().
 */
void UpdateController::handleReply(QNetworkReply *reply)
{
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus(Status::Error,
                  tr("Could not check for updates: %1").arg(reply->errorString()));
        return;
    }

    applyReleasePayload(reply->readAll());
}

/*!
 * \brief Applies a GitHub Releases JSON document as the result of a check.
 *
 * The \c tag_name field is compared against the running version to choose
 * between Status::UpToDate and Status::UpdateAvailable; the release page comes
 * from the reply's \c html_url, falling back to the configured releases page.
 * A document without a tag becomes Status::Error. This is the parse step of a
 * network check, exposed so the result handling can be exercised offline.
 * \param payload The raw JSON body of a GitHub "latest release" response.
 */
void UpdateController::applyReleasePayload(const QByteArray &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    const QJsonObject release = doc.object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) {
        setStatus(Status::Error, tr("Could not read the latest version."));
        return;
    }

    if (compareVersions(tag, currentVersion()) > 0) {
        m_latestVersion = tag;
        const QString htmlUrl = release.value(QStringLiteral("html_url")).toString();
        m_releaseUrl = htmlUrl.isEmpty()
                           ? QStringLiteral(PRODUCT_UPDATE_RELEASES_PAGE)
                           : htmlUrl;
        emit latestVersionChanged();
        emit releaseUrlChanged();
        setStatus(Status::UpdateAvailable,
                  tr("A new version (%1) is available.").arg(m_latestVersion));
    } else {
        m_latestVersion = tag;
        emit latestVersionChanged();
        setStatus(Status::UpToDate,
                  tr("You are running the latest version."));
    }
}

/*!
 * \internal
 * \brief Sets status() and statusMessage() together and notifies once.
 */
void UpdateController::setStatus(Status status, const QString &message)
{
    m_status = status;
    m_statusMessage = message;
    emit statusChanged();
}

/*!
 * \internal
 * \brief Sets actionError() and notifies only when the text changes.
 */
void UpdateController::setActionError(const QString &message)
{
    if (message == m_actionError)
        return;
    m_actionError = message;
    emit actionErrorChanged();
}

/*!
 * \internal
 * \brief Returns whether the running copy is portable, honouring the test override.
 */
bool UpdateController::isPortable() const
{
    if (m_environment.portable.has_value())
        return *m_environment.portable;
    return AppPaths::instance().isPortable();
}

/*!
 * \brief Chooses how an available update is obtained.
 *
 * A portable copy is never updated in place: its files live wherever the user
 * unpacked them and there is no Maintenance Tool, so the release page is
 * offered for a manual download. An installed copy uses the Maintenance Tool
 * when it exists next to the executable; when it is missing (typically a build
 * started from Qt Creator or another development tree) the release page is
 * offered as well instead of failing.
 * \param portable Whether the copy runs in portable mode.
 * \param maintenanceToolExists Whether the Maintenance Tool file is present.
 * \return The action to offer to the user.
 */
UpdateController::UpdateAction UpdateController::resolveUpdateAction(bool portable, bool maintenanceToolExists)
{
    if (portable)
        return UpdateAction::OpenDownloadPage;
    return maintenanceToolExists ? UpdateAction::InstallUpdate : UpdateAction::OpenDownloadPage;
}

/*!
 * \brief Compares dotted numeric versions, ignoring a leading "v".
 *
 * Only the numeric MAJOR.MINOR.PATCH segments are compared; a missing segment
 * counts as zero and any trailing pre-release suffix is ignored. This is enough
 * for the semantic versions the packaging layer produces.
 * \param lhs Left-hand version string.
 * \param rhs Right-hand version string.
 * \return A negative value if \a lhs precedes \a rhs, zero if they are equal, and
 *         a positive value if \a lhs follows \a rhs.
 */
int UpdateController::compareVersions(const QString &lhs, const QString &rhs)
{
    const auto parse = [](const QString &value) {
        QString trimmed = value.trimmed();
        if (trimmed.startsWith(QLatin1Char('v')) || trimmed.startsWith(QLatin1Char('V')))
            trimmed.remove(0, 1);
        QList<int> parts;
        const QStringList tokens = trimmed.split(QLatin1Char('.'));
        for (const QString &token : tokens) {
            int digits = 0;
            while (digits < token.size() && token.at(digits).isDigit())
                ++digits;
            parts.append(token.left(digits).toInt());
        }
        return parts;
    };

    const QList<int> a = parse(lhs);
    const QList<int> b = parse(rhs);
    const int count = qMax(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        const int av = i < a.size() ? a.at(i) : 0;
        const int bv = i < b.size() ? b.at(i) : 0;
        if (av != bv)
            return av < bv ? -1 : 1;
    }
    return 0;
}
