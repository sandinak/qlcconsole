/*
  Q Light Controller Plus
  updatemanager.cpp

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QGuiApplication>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QSettings>
#include <QFileInfo>
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QDebug>
#include <QDir>

#include "updatedownloader.h"
#include "updateinstaller.h"
#include "updatemanager.h"
#include "updatedialog.h"
#include "qlcconfig.h"

#define SETTINGS_PENDING_VERSION QStringLiteral("updates/pendingVersion")
#define SETTINGS_PENDING_BACKUP  QStringLiteral("updates/pendingBackup")

UpdateManager::UpdateManager(Doc *doc, QWidget *window)
    : QObject(window)
    , m_doc(doc)
    , m_window(window)
    , m_checker(new UpdateChecker(this))
    , m_downloader(new UpdateDownloader(this))
    , m_installer(new UpdateInstaller(this))
    , m_periodic(nullptr)
    , m_state(NoUpdate)
    , m_percent(0)
{
    connect(m_checker, &UpdateChecker::checkFinished, this, &UpdateManager::slotCheckFinished);
    connect(m_downloader, &UpdateDownloader::finished, this, &UpdateManager::slotDownloadFinished);
    connect(m_downloader, &UpdateDownloader::progress, this, [this](qint64 got, qint64 total) {
        const int pct = total > 0 ? int(qBound(qint64(0), got * 100 / total, qint64(100))) : 0;
        if (pct != m_percent)
        {
            m_percent = pct;
            updateFooter();
            emit stateChanged();
        }
    });
    connect(m_installer, &UpdateInstaller::prepared, this, &UpdateManager::slotPrepared);
}

UpdateManager::~UpdateManager()
{
}

/*****************************************************************************
 * Footer
 *****************************************************************************/

void UpdateManager::setFooterLabel(QLabel *label)
{
    if (m_footer != nullptr)
        m_footer->removeEventFilter(this);
    m_footer = label;
    if (label != nullptr)
    {
        label->setObjectName(QStringLiteral("footerUpdate"));
        label->setCursor(Qt::PointingHandCursor);
        label->setProperty("helpTopic", QStringLiteral("updates.md"));
        label->setStyleSheet(QStringLiteral("QLabel { color: palette(link); font-weight: bold; }"));
        label->installEventFilter(this);
    }
    updateFooter();
}

QString UpdateManager::footerText() const
{
    switch (m_state)
    {
        case Available:
            return tr("Update available: %1").arg(m_release.version);
        case Downloading:
            return tr("Downloading %1… %2%").arg(m_release.version).arg(m_percent);
        case Preparing:
            return tr("Checking update %1…").arg(m_release.version);
        case Ready:
            return tr("Update %1 installs when you quit").arg(m_release.version);
        case Manual:
            return tr("Update %1 downloaded").arg(m_release.version);
        case Failed:
            return tr("Update %1 failed").arg(m_release.version);
        case NoUpdate:
            break;
    }
    if (m_justInstalled.isEmpty() == false)
        return tr("Updated to %1").arg(m_justInstalled);
    return QString();
}

void UpdateManager::updateFooter()
{
    if (m_footer == nullptr)
        return;
    const QString text = footerText();
    m_footer->setText(text);
    m_footer->setToolTip(m_message.isEmpty() ? tr("Click for details.")
                                             : m_message + QLatin1Char('\n') + tr("Click for details."));
    m_footer->setVisible(text.isEmpty() == false);
}

bool UpdateManager::eventFilter(QObject *watched, QEvent *event)
{
    /* The event first: the footer's own teardown passes through here, and
       comparing it with m_footer then downcast a half-destroyed label */
    if (event->type() == QEvent::MouseButtonPress && watched == m_footer)
    {
        if (m_state == NoUpdate)
        {
            // "Updated to X": the click dismisses it.
            m_justInstalled.clear();
            updateFooter();
        }
        else
        {
            showDialog();
        }
        return true;
    }
    return QObject::eventFilter(watched, event);
}

/*****************************************************************************
 * Checking
 *****************************************************************************/

bool UpdateManager::backgroundChecksAllowed()
{
    if (UpdateChecker::isDisabledByPolicy())
        return false;
    const QString platform = QGuiApplication::platformName();
    return platform != QLatin1String("offscreen") && platform != QLatin1String("minimal");
}

void UpdateManager::scheduleChecks(int firstDelayMs)
{
    m_justInstalled = cleanupAfterUpdate();
    updateFooter();
    if (m_justInstalled.isEmpty() == false)
        QTimer::singleShot(120000, this, [this]() { m_justInstalled.clear(); updateFooter(); });

    if (backgroundChecksAllowed() == false)
        return;
    QTimer::singleShot(firstDelayMs, this, &UpdateManager::slotPeriodic);
    m_periodic = new QTimer(this);
    m_periodic->setInterval(3600 * 1000);
    connect(m_periodic, &QTimer::timeout, this, &UpdateManager::slotPeriodic);
    m_periodic->start();
}

void UpdateManager::slotPeriodic()
{
    // Already found (or busy with) one: nothing to ask until it is dealt with.
    if (m_state != NoUpdate && m_state != Failed)
        return;
    if (m_checker->isCheckDue())
        m_checker->check(false);
}

void UpdateManager::checkNow()
{
    showDialog();
    if (m_state == Downloading || m_state == Preparing || m_state == Ready)
        return;     // already on its way: the dialog shows where it is
    m_checker->check(true);
}

void UpdateManager::slotCheckFinished(UpdateChecker::Status status)
{
    if (status != UpdateChecker::Available)
    {
        // Only a definite "up to date" forgets an update already found; a
        // check that could not reach GitHub keeps what is known.
        if (status == UpdateChecker::UpToDate && (m_state == Available || m_state == Failed))
            setState(NoUpdate);
        else
            emit stateChanged();
        return;
    }

    const UpdateRelease found = m_checker->available();
    if (m_state != NoUpdate && m_state != Failed && m_state != Available
        && UpdateChecker::compareVersions(found.version, m_release.version) == 0)
    {
        emit stateChanged();    // already downloading/staged this one
        return;
    }
    if (m_state == Downloading || m_state == Preparing)
    {
        emit stateChanged();
        return;
    }
    m_release = found;
    setState(Available, tr("qlcconsole %1 is available; this is %2.")
                            .arg(found.version, QStringLiteral(QLCCONSOLE_VERSION)));

    // "Install automatically": fetch and verify now, install at quit -- only
    // where that can happen without the person (not Linux, not without
    // write access).
    if (UpdateChecker::autoInstall() && UpdateInstaller::method() == UpdateInstaller::InPlace)
        install();
}

/*****************************************************************************
 * Installing
 *****************************************************************************/

void UpdateManager::install()
{
    if (m_state != Available && m_state != Failed)
        return;
    if (m_release.isValid() == false)
        return;

    QString why;
    if (UpdateInstaller::method(&why) == UpdateInstaller::NoInstall)
    {
        openReleasePage();
        return;
    }
    const int asset = UpdateChecker::pickAsset(m_release.assets, UpdateChecker::currentOs(),
                                               UpdateChecker::currentArch());
    if (asset < 0)
    {
        setState(Failed, tr("Release %1 has no download for this computer (%2, %3).")
                             .arg(m_release.version, UpdateChecker::currentOs(),
                                  UpdateChecker::currentArch()));
        return;
    }
    m_percent = 0;
    m_downloadPath.clear();
    setState(Downloading, tr("Downloading %1…").arg(m_release.assets.at(asset).name));
    m_downloader->start(m_release, asset);
}

void UpdateManager::slotDownloadFinished(bool ok, const QString &error)
{
    if (ok == false)
    {
        setState(Failed, error);
        return;
    }
    m_downloadPath = m_downloader->filePath();

    QString why;
    const UpdateInstaller::Method how = UpdateInstaller::method(&why);
    if (how == UpdateInstaller::OpenDownload)
    {
        setState(Manual, tr("Downloaded and checked. %1 Open the download to install it "
                            "yourself after quitting qlcconsole.").arg(why));
        return;
    }
    if (UpdateChecker::currentOs() == QLatin1String("macos"))
    {
        setState(Preparing, tr("Checking the new app's signature…"));
        m_installer->prepare(m_downloadPath);
        return;
    }
    setState(Ready, tr("qlcconsole %1 is downloaded and checked. It installs when you quit "
                       "qlcconsole -- or restart now.").arg(m_release.version));
}

void UpdateManager::slotPrepared(bool ok, const QString &error)
{
    if (ok == false)
    {
        if (m_downloadPath.isEmpty() == false)
            QFile::remove(m_downloadPath);
        m_downloadPath.clear();
        setState(Failed, tr("The update was not installed: %1.").arg(error));
        return;
    }
    // Staged beside the app: the disk image has done its job.
    if (m_downloadPath.isEmpty() == false)
        QFile::remove(m_downloadPath);
    m_downloadPath.clear();
    setState(Ready, tr("qlcconsole %1 is downloaded and its signature checked. It installs "
                       "when you quit qlcconsole -- or restart now.").arg(m_release.version));
}

void UpdateManager::skipVersion()
{
    if (m_release.isValid() == false)
        return;
    UpdateChecker::setSkippedVersion(m_release.version);
    m_downloader->cancel();
    m_release = UpdateRelease();
    setState(NoUpdate);
}

void UpdateManager::openDownload()
{
    if (m_downloadPath.isEmpty() || QFile::exists(m_downloadPath) == false)
    {
        openReleasePage();
        return;
    }
    if (UpdateChecker::currentOs() == QLatin1String("windows"))
    {
        // Not the installer itself while qlcconsole runs: its files are in use.
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_downloadPath).absolutePath()));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadPath));
}

void UpdateManager::openReleasePage()
{
    QUrl url = m_release.pageUrl;
    if (url.isValid() == false)
        url = QUrl(QStringLiteral("https://github.com/sandinak/qlcconsole/releases"));
    QDesktopServices::openUrl(url);
}

bool UpdateManager::canInstallNow(QString *why) const
{
    return UpdateInstaller::safeToInstall(m_doc, why);
}

bool UpdateManager::restartNow()
{
    if (m_state != Ready || canInstallNow() == false)
        return false;
    emit restartRequested();
    return true;
}

bool UpdateManager::aboutToQuit(bool relaunch, const QString &show)
{
    if (m_state != Ready)
        return false;
    QString why;
    if (UpdateInstaller::safeToInstall(m_doc, &why) == false)
    {
        qDebug() << "[updates] not installing at quit:" << why;
        return false;
    }
    if (launchInstaller(relaunch, show) == false)
    {
        qWarning() << "[updates] the installer could not be started";
        return false;
    }
    QSettings settings;
    settings.setValue(SETTINGS_PENDING_VERSION, m_release.version);
    const QString bundle = UpdateInstaller::runningBundle();
    if (bundle.isEmpty() == false)
        settings.setValue(SETTINGS_PENDING_BACKUP, UpdateInstaller::backupPathFor(bundle));
    else
        settings.remove(SETTINGS_PENDING_BACKUP);
    return true;
}

bool UpdateManager::launchInstaller(bool relaunch, const QString &show)
{
    return m_installer->launchOnQuit(m_downloadPath, relaunch, show);
}

QString UpdateManager::cleanupAfterUpdate()
{
    QSettings settings;
    QString installed;
    const QString pending = settings.value(SETTINGS_PENDING_VERSION).toString();
    const QString backup = settings.value(SETTINGS_PENDING_BACKUP).toString();
    if (pending.isEmpty() == false)
    {
        if (UpdateChecker::compareVersions(UpdateChecker::currentVersion(), pending) >= 0)
        {
            installed = pending;
            // The new version has started: the old one can go. Only ever
            // the backup's own name, whatever the setting says.
            if (QFileInfo(backup).fileName() == QLatin1String(".qlcconsole-previous.app")
                && QFileInfo(backup).isDir())
            {
                QDir(backup).removeRecursively();
            }
            settings.remove(SETTINGS_PENDING_VERSION);
            settings.remove(SETTINGS_PENDING_BACKUP);
        }
        else
        {
            qWarning() << "[updates] expected version" << pending << "but this is"
                       << UpdateChecker::currentVersion() << "-- keeping the backup";
        }
    }

    // A staged copy that never got installed (quit during a show, a failed
    // swap): the next update stages afresh.
    const QString bundle = UpdateInstaller::runningBundle();
    if (bundle.isEmpty() == false)
    {
        const QString staged = UpdateInstaller::stagedPathFor(bundle);
        if (QFileInfo(staged).isDir())
            QDir(staged).removeRecursively();
    }
    return installed;
}

/*****************************************************************************
 * Dialog
 *****************************************************************************/

void UpdateManager::showDialog()
{
    if (m_dialog == nullptr)
    {
        m_dialog = new UpdateDialog(this, m_window);
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}

void UpdateManager::setState(State state, const QString &message)
{
    m_state = state;
    m_message = message;
    if (state == Failed)
        qWarning() << "[updates]" << message;
    updateFooter();
    emit stateChanged();
}
