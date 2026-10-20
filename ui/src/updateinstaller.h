/*
  Q Light Controller Plus
  updateinstaller.h

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

#ifndef UPDATEINSTALLER_H
#define UPDATEINSTALLER_H

#include <QObject>
#include <QString>

#include "qlcui_export.h"

class QProcess;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Put a verified download in place -- only ever as qlcconsole quits.
 *
 * macOS: prepare() copies the app out of the DMG to beside the running one
 * and checks its signature (update-helper.sh prepare); launchOnQuit()
 * starts the helper detached, which waits for this process to end, swaps
 * the apps (keeping the old one as a backup) and, for a restart, reopens
 * the same show. Where the app's folder cannot be written (/Applications
 * without admin rights, a translocated or disk-image copy), there is no
 * in-place install: the person opens the DMG instead.
 *
 * Windows: launchOnQuit() runs the NSIS installer silently (/S, into the
 * folder qlcconsole runs from) once this process has ended, then reopens
 * the show for a restart.
 *
 * Linux and Raspberry Pi: no installing; the release page says what to get.
 *
 * Never during a show: safeToInstall() is false in Perform (Operate) mode,
 * in kiosk mode, and while any function runs -- the caller asks it right
 * before launchOnQuit(), and the restart button is disabled while it says no.
 */
class QLC_UI_EXPORT UpdateInstaller : public QObject
{
    Q_OBJECT

public:
    explicit UpdateInstaller(QObject *parent = nullptr);
    ~UpdateInstaller();

    /** Whether installing now could interrupt a show; @p why says what is
     *  in the way. */
    static bool safeToInstall(const Doc *doc, QString *why = nullptr);

    enum Method
    {
        NoInstall,      ///< notify only (Linux, Pi, unknown)
        OpenDownload,   ///< hand the download to the person (no write access)
        InPlace         ///< swap/run the installer at quit
    };
    /** How this copy can be updated, @p why when not InPlace. */
    static Method method(QString *why = nullptr);

    /** The running .app on macOS ("/Applications/qlcconsole.app"); empty
     *  elsewhere or when not run from a bundle. */
    static QString runningBundle();
    /** The folder the running program lives in, Windows' install folder. */
    static QString installFolder();
    /** Can @p bundle be replaced where it is: its folder writable, not
     *  translocated by Gatekeeper, not on a mounted disk image. */
    static bool canReplaceBundle(const QString &bundle, QString *why = nullptr);

    /** The helper script, written to a private temporary file; empty on
     *  failure. */
    static QString writeMacHelper();
    /** Where the staged app goes, beside @p bundle. */
    static QString stagedPathFor(const QString &bundle);
    /** Where the old app is kept until the new one has started. */
    static QString backupPathFor(const QString &bundle);

    /** The Windows hand-off script: waits for @p pid to end, runs
     *  @p installer silently into @p installDir, reopens @p exe on
     *  @p show when @p relaunch. */
    static QString windowsScript(const QString &installer, qint64 pid,
                                 const QString &installDir, const QString &exe,
                                 bool relaunch, const QString &show);

    /** macOS: stage and verify the app inside @p dmg (asynchronous, see
     *  prepared()). Testing: @p bundle replaces the running bundle and
     *  @p allowAdhoc accepts an ad-hoc signed app. */
    void prepare(const QString &dmg, const QString &bundle = QString(), bool allowAdhoc = false);
    bool isPreparing() const { return m_process != nullptr; }
    QString stagedPath() const { return m_staged; }
    QString teamId() const { return m_team; }

    /** Start the install, to run as this process ends. @p file is the
     *  download (Windows) -- on macOS the staged app is used. Returns
     *  false, and does nothing, if it could not be started. */
    bool launchOnQuit(const QString &file, bool relaunch, const QString &show);

signals:
    void prepared(bool ok, const QString &error);

private:
    QProcess *m_process;
    QString m_staged;
    QString m_team;
    QString m_helper;
};

/** @} */

#endif
