/*
  Q Light Controller Plus
  updatechecker.h

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

#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QUrl>

#include "qlcui_export.h"

class QNetworkAccessManager;
class QNetworkReply;

/** @addtogroup ui UI
 * @{
 */

/** One file attached to a GitHub release. */
struct QLC_UI_EXPORT UpdateAsset
{
    QString name;
    QUrl url;           ///< browser_download_url
    qint64 size = 0;
};

/** One GitHub release, as much of it as an update needs. */
struct QLC_UI_EXPORT UpdateRelease
{
    QString tag;        ///< "v0.1.1"
    QString version;    ///< the tag without its "v": "0.1.1"
    QString name;       ///< "qlcconsole v0.1.1", "Show Tree 2"
    QString body;       ///< the release notes, Markdown
    QUrl pageUrl;       ///< html_url
    bool prerelease = false;
    QDateTime published;
    QList<UpdateAsset> assets;

    bool isValid() const { return version.isEmpty() == false; }
};

/**
 * Is there a newer qlcconsole? Asks GitHub Releases, and nothing else.
 *
 * One unauthenticated GET of the repository's releases list
 * (api.github.com/repos/sandinak/qlcconsole/releases): no account, no
 * identifier, nothing about the machine or the show. The answer is cached
 * with its ETag, so a repeat check sends If-None-Match and a "nothing new"
 * costs GitHub's 304 rather than the list again (and does not count
 * against the hourly limit). A 403/429 with the limit spent is remembered
 * until GitHub's reset time; offline or slow, it gives up quietly.
 *
 * Which releases count is the channel: Stable takes only full releases,
 * PreRelease takes pre-releases too (a "Show Tree" build for students).
 * The newest of those, if newer than this build and not the version the
 * person said to skip, is the update.
 *
 * Machine settings (QSettings, never the show): updates/check,
 * updates/channel, updates/autoInstall, updates/skipVersion,
 * updates/lastCheck, updates/etag. Turned off entirely -- no request at
 * all, menu item included -- by QLCCONSOLE_NO_UPDATES=1 in the environment
 * or updates/disabled=true in the system-wide settings, for a school's IT.
 */
class QLC_UI_EXPORT UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker();

    enum Channel { Stable, PreRelease };
    Q_ENUM(Channel)

    enum Status
    {
        Idle,
        Checking,
        UpToDate,
        Available,
        Offline,        ///< no answer (no network, timed out, DNS)
        RateLimited,    ///< GitHub said not now; retryAfter() says when
        Failed,         ///< an answer that made no sense
        Disabled        ///< by policy (isDisabledByPolicy())
    };
    Q_ENUM(Status)

    /*********************************************************************
     * Versions
     *********************************************************************/
public:
    /** This build: the release (VERSION) with the build id's
     *  "+qlcconsole.N", e.g. "0.1.0+qlcconsole.4870". */
    static QString currentVersion();

    /** Compare two versions, SemVer-style: "1.2.3", a leading v allowed,
     *  "-pre.1" sorts before its release, numeric identifiers numerically.
     *  Build metadata ("+qlcconsole.N") only breaks a tie, and only when
     *  both carry a qlcconsole.N. Returns <0, 0 or >0; an unparseable
     *  version sorts below every parseable one. */
    static int compareVersions(const QString &a, const QString &b);
    static bool isValidVersion(const QString &v);
    /** True for a SemVer pre-release ("0.2.0-showtree.1"). */
    static bool isPrereleaseVersion(const QString &v);

    /** What a fresh install follows: PreRelease for a pre-release build
     *  -- one whose own GitHub release carries the pre-release flag (seen
     *  by a check), or with a SemVer pre-release suffix -- Stable otherwise. */
    static Channel defaultChannel();

    /*********************************************************************
     * Releases
     *********************************************************************/
public:
    /** The releases in a GitHub /releases JSON answer (drafts left out). */
    static QList<UpdateRelease> parseReleases(const QByteArray &json);

    /** The newest release on @p channel, or an invalid one. */
    static UpdateRelease newestOn(const QList<UpdateRelease> &releases, Channel channel);

    /** The platform key assets are picked for: "macos", "windows",
     *  "linux" -- and the CPU, "arm64" or "x86_64". */
    static QString currentOs();
    static QString currentArch();

    /** The download for @p os / @p arch among @p assets, -1 if none:
     *  macOS the DMG (universal first, else this arch's), Windows the
     *  win64 installer, Linux x86_64 the AppImage. Raspberry Pi and other
     *  Linux arm64 have none: they build from source. */
    static int pickAsset(const QList<UpdateAsset> &assets,
                         const QString &os, const QString &arch);

    /** The checksum file published with a release. */
    static QString checksumAssetName() { return QStringLiteral("SHA256SUMS"); }
    /** "sha256  filename" lines (sha256sum / shasum -a 256 output; a
     *  leading * for binary mode allowed) as file name -> lowercase hex. */
    static QHash<QString, QString> parseSha256Sums(const QByteArray &text);

    /*********************************************************************
     * Settings
     *********************************************************************/
public:
    /** Off by the environment or the system-wide settings: never checks,
     *  never downloads, the menu item and the settings say so. */
    static bool isDisabledByPolicy();

    static bool checkEnabled();
    static void setCheckEnabled(bool on);
    static Channel channel();
    static void setChannel(Channel channel);
    /** Download and verify in the background, install at the next quit.
     *  Off (the default): ask. */
    static bool autoInstall();
    static void setAutoInstall(bool on);
    static QString skippedVersion();
    static void setSkippedVersion(const QString &version);

    /** When the last check got an answer (any answer, 304 included). */
    static QDateTime lastCheck();

    /*********************************************************************
     * Checking
     *********************************************************************/
public:
    /** The API address -- GitHub's; a test points it at its own server. */
    void setApiUrl(const QUrl &url);
    QUrl apiUrl() const;
    void setTimeout(int msec);

    /** Due: enabled, not disabled by policy, not rate limited, and the
     *  last check over a day ago. */
    bool isCheckDue(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;

    /** Ask GitHub. @p manual (Help → Check for Updates): also when not
     *  due, and the skipped version is offered again. */
    void check(bool manual = false);

    Status status() const { return m_status; }
    /** What went wrong, for the person, when status() is not a success. */
    QString errorText() const { return m_errorText; }
    UpdateRelease available() const { return m_available; }
    QDateTime retryAfter() const;
    bool lastCheckWasManual() const { return m_manual; }

signals:
    void statusChanged(UpdateChecker::Status status);
    /** A check finished, whatever the answer. */
    void checkFinished(UpdateChecker::Status status);

private slots:
    void slotFinished();

private:
    void finish(Status status, const QString &error = QString());
    void evaluate(const QByteArray &json);
    static QString cacheFile();

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;
    QUrl m_apiUrl;
    int m_timeout;
    Status m_status;
    QString m_errorText;
    UpdateRelease m_available;
    bool m_manual;
};

/** @} */

#endif
