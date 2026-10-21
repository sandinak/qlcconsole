/*
  Q Light Controller Plus
  updatemanager.h

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

#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QPointer>
#include <QObject>

#include "updatechecker.h"
#include "qlcui_export.h"

class UpdateDownloader;
class UpdateInstaller;
class UpdateDialog;
class QWidget;
class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The app's side of updating: when to check, what the footer chip says,
 * the dialog, and the hand-off to the installer at quit.
 *
 * Checks a few seconds after the window is up and then at most once a day
 * (UpdateChecker::isCheckDue()), in the background; Help → Check for
 * Updates… checks now. An update shows as a footer chip, never a pop-up.
 * Install (or "Install updates automatically", which starts this on its
 * own) downloads and verifies in the background and, on macOS, stages the
 * new app; it is then put in place when the person quits -- or restarts
 * from the dialog -- and never while the show is running
 * (UpdateInstaller::safeToInstall()).
 */
class QLC_UI_EXPORT UpdateManager : public QObject
{
    Q_OBJECT
    friend class Updates_Test;

public:
    UpdateManager(Doc *doc, QWidget *window);
    ~UpdateManager();

    enum State
    {
        NoUpdate,
        Available,      ///< found; nothing downloaded
        Downloading,
        Preparing,      ///< macOS: copying out of the DMG and checking it
        Ready,          ///< verified and staged: installs at quit
        Manual,         ///< verified; the person installs it (no write access)
        Failed
    };
    Q_ENUM(State)

    UpdateChecker *checker() const { return m_checker; }
    UpdateDownloader *downloader() const { return m_downloader; }
    UpdateInstaller *installer() const { return m_installer; }

    State state() const { return m_state; }
    UpdateRelease release() const { return m_release; }
    /** What to tell the person about the current state. */
    QString message() const { return m_message; }
    int progressPercent() const { return m_percent; }
    QString downloadPath() const { return m_downloadPath; }

    /** The footer chip this drives (hidden while there is nothing to say;
     *  a click opens the dialog). */
    void setFooterLabel(QLabel *label);
    QString footerText() const;

    /** Background checks: the first after @p firstDelayMs, then hourly
     *  looking whether one is due. Not when disabled, or on a headless
     *  (offscreen/minimal) platform -- tests and the help's screenshots
     *  never reach GitHub. */
    void scheduleChecks(int firstDelayMs = 8000);
    static bool backgroundChecksAllowed();

    /** Help → Check for Updates…: check now and show the answer. */
    void checkNow();
    void showDialog();

    /** Download, verify and (macOS) stage the update. */
    void install();
    /** Don't offer this release again (a newer one still is). */
    void skipVersion();
    /** Open the verified download for the person (Manual), or the
     *  release page when there is nothing to open. */
    void openDownload();
    void openReleasePage();

    /** Ready and nothing on stage: asks the app to close, which installs
     *  and reopens the show (restartRequested()). */
    bool restartNow();
    bool canInstallNow(QString *why = nullptr) const;

    /** From App::closeEvent(), once the close is accepted: start the
     *  staged install if there is one and it is safe now. Returns true if
     *  the installer was started (the app must then really quit). */
    bool aboutToQuit(bool relaunch, const QString &show);

    /** At start: remove the previous version kept by the last install, and
     *  any staged copy that never got installed. Returns the version just
     *  installed, if this start is its first. */
    static QString cleanupAfterUpdate();

signals:
    void stateChanged();
    void restartRequested();

protected:
    /** Start the installer (UpdateInstaller::launchOnQuit()); a test
     *  replaces it to see whether it would have been. */
    virtual bool launchInstaller(bool relaunch, const QString &show);

private slots:
    void slotCheckFinished(UpdateChecker::Status status);
    void slotDownloadFinished(bool ok, const QString &error);
    void slotPrepared(bool ok, const QString &error);
    void slotPeriodic();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setState(State state, const QString &message = QString());
    void updateFooter();

    Doc *m_doc;
    QPointer<QWidget> m_window;
    UpdateChecker *m_checker;
    UpdateDownloader *m_downloader;
    UpdateInstaller *m_installer;
    QPointer<UpdateDialog> m_dialog;
    QPointer<QLabel> m_footer;
    QTimer *m_periodic;
    State m_state;
    UpdateRelease m_release;
    QString m_message;
    QString m_downloadPath;
    QString m_justInstalled;
    int m_percent;
};

/** @} */

#endif
