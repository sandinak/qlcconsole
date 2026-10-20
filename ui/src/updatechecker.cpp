/*
  Q Light Controller Plus
  updatechecker.cpp

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

#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSysInfo>
#include <QTimeZone>
#include <QLocale>
#include <QSettings>
#include <QDebug>
#include <QFile>
#include <QDir>

#include "updatechecker.h"
#include "qlcconfig.h"

#define SETTINGS_CHECK        QStringLiteral("updates/check")
#define SETTINGS_CHANNEL      QStringLiteral("updates/channel")
#define SETTINGS_AUTOINSTALL  QStringLiteral("updates/autoInstall")
#define SETTINGS_SKIP         QStringLiteral("updates/skipVersion")
#define SETTINGS_LASTCHECK    QStringLiteral("updates/lastCheck")
#define SETTINGS_ETAG         QStringLiteral("updates/etag")
#define SETTINGS_RETRYAFTER   QStringLiteral("updates/retryAfter")
#define SETTINGS_DISABLED     QStringLiteral("updates/disabled")

#define DEFAULT_API_URL "https://api.github.com/repos/sandinak/qlcconsole/releases?per_page=20"

/*****************************************************************************
 * Versions
 *****************************************************************************/

namespace
{
struct ParsedVersion
{
    bool valid = false;
    QList<qlonglong> core;
    QStringList pre;
    QString build;
};

ParsedVersion parseVersion(QString s)
{
    ParsedVersion v;
    s = s.trimmed();
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V')))
        s = s.mid(1);
    if (s.isEmpty())
        return v;

    const int plus = s.indexOf(QLatin1Char('+'));
    if (plus >= 0)
    {
        v.build = s.mid(plus + 1);
        s = s.left(plus);
    }
    const int dash = s.indexOf(QLatin1Char('-'));
    if (dash >= 0)
    {
        v.pre = s.mid(dash + 1).split(QLatin1Char('.'));
        s = s.left(dash);
        for (const QString &id : std::as_const(v.pre))
            if (id.isEmpty())
                return v;
    }
    const QStringList parts = s.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.count() > 4)
        return v;
    for (const QString &p : parts)
    {
        bool ok = false;
        const qlonglong n = p.toLongLong(&ok);
        if (!ok || n < 0 || p.startsWith(QLatin1Char('+')))
            return v;
        v.core << n;
    }
    while (v.core.count() < 3)
        v.core << 0;
    v.valid = true;
    return v;
}

int cmp(qlonglong a, qlonglong b)
{
    return a < b ? -1 : (a > b ? 1 : 0);
}

int comparePre(const QStringList &a, const QStringList &b)
{
    // No pre-release identifiers sorts after any (1.0.0-rc.1 < 1.0.0).
    if (a.isEmpty() && b.isEmpty())
        return 0;
    if (a.isEmpty())
        return 1;
    if (b.isEmpty())
        return -1;
    for (int i = 0; i < qMin(a.count(), b.count()); i++)
    {
        bool an = false, bn = false;
        const qlonglong ai = a.at(i).toLongLong(&an);
        const qlonglong bi = b.at(i).toLongLong(&bn);
        if (an && bn)
        {
            if (const int c = cmp(ai, bi))
                return c;
        }
        else if (an != bn)
        {
            return an ? -1 : 1;     // numeric identifiers sort first
        }
        else if (const int c = QString::compare(a.at(i), b.at(i), Qt::CaseInsensitive))
        {
            return c < 0 ? -1 : 1;
        }
    }
    return cmp(a.count(), b.count());
}

/** The N of a "qlcconsole.N" build id, or -1. */
qlonglong buildNumber(const QString &build)
{
    static const QRegularExpression re(QStringLiteral("(?:^|\\.)qlcconsole\\.(\\d+)(?:$|\\.)"));
    const QRegularExpressionMatch m = re.match(build);
    return m.hasMatch() ? m.captured(1).toLongLong() : -1;
}
}

QString UpdateChecker::currentVersion()
{
    const QString app = QStringLiteral(APPVERSION);
    const int plus = app.indexOf(QLatin1Char('+'));
    QString v = QStringLiteral(QLCCONSOLE_VERSION);
    if (plus >= 0 && v.contains(QLatin1Char('+')) == false)
        v += app.mid(plus);
    return v;
}

int UpdateChecker::compareVersions(const QString &a, const QString &b)
{
    const ParsedVersion va = parseVersion(a);
    const ParsedVersion vb = parseVersion(b);
    if (!va.valid || !vb.valid)
        return cmp(va.valid ? 1 : 0, vb.valid ? 1 : 0);

    const int n = qMax(va.core.count(), vb.core.count());
    for (int i = 0; i < n; i++)
    {
        if (const int c = cmp(va.core.value(i, 0), vb.core.value(i, 0)))
            return c;
    }
    if (const int c = comparePre(va.pre, vb.pre))
        return c;

    const qlonglong ba = buildNumber(va.build);
    const qlonglong bb = buildNumber(vb.build);
    if (ba >= 0 && bb >= 0)
        return cmp(ba, bb);
    return 0;
}

bool UpdateChecker::isValidVersion(const QString &v)
{
    return parseVersion(v).valid;
}

bool UpdateChecker::isPrereleaseVersion(const QString &v)
{
    const ParsedVersion p = parseVersion(v);
    return p.valid && p.pre.isEmpty() == false;
}

UpdateChecker::Channel UpdateChecker::defaultChannel()
{
    return isPrereleaseVersion(QStringLiteral(QLCCONSOLE_VERSION)) ? PreRelease : Stable;
}

/*****************************************************************************
 * Releases
 *****************************************************************************/

QList<UpdateRelease> UpdateChecker::parseReleases(const QByteArray &json)
{
    QList<UpdateRelease> out;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || doc.isArray() == false)
        return out;

    const QJsonArray list = doc.array();
    for (const QJsonValue &v : list)
    {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("draft")).toBool())
            continue;
        UpdateRelease r;
        r.tag = o.value(QStringLiteral("tag_name")).toString();
        if (isValidVersion(r.tag) == false)
        {
            qDebug() << "[updates] ignoring release with a tag that is no version:" << r.tag;
            continue;
        }
        r.version = r.tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive) ? r.tag.mid(1) : r.tag;
        r.name = o.value(QStringLiteral("name")).toString();
        if (r.name.isEmpty())
            r.name = r.tag;
        r.body = o.value(QStringLiteral("body")).toString();
        r.pageUrl = QUrl(o.value(QStringLiteral("html_url")).toString());
        r.prerelease = o.value(QStringLiteral("prerelease")).toBool();
        r.published = QDateTime::fromString(o.value(QStringLiteral("published_at")).toString(),
                                            Qt::ISODate);
        const QJsonArray assets = o.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &av : assets)
        {
            const QJsonObject a = av.toObject();
            UpdateAsset asset;
            asset.name = a.value(QStringLiteral("name")).toString();
            asset.url = QUrl(a.value(QStringLiteral("browser_download_url")).toString());
            asset.size = qint64(a.value(QStringLiteral("size")).toDouble());
            if (asset.name.isEmpty() == false && asset.url.isValid())
                r.assets << asset;
        }
        out << r;
    }
    return out;
}

UpdateRelease UpdateChecker::newestOn(const QList<UpdateRelease> &releases, Channel channel)
{
    UpdateRelease best;
    for (const UpdateRelease &r : releases)
    {
        // GitHub's flag decides; a pre-release version marked as a full
        // release by mistake still stays off the Stable channel.
        const bool pre = r.prerelease || isPrereleaseVersion(r.version);
        if (channel == Stable && pre)
            continue;
        if (best.isValid() == false || compareVersions(r.version, best.version) > 0)
            best = r;
    }
    return best;
}

QString UpdateChecker::currentOs()
{
#if defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#else
    return QStringLiteral("linux");
#endif
}

QString UpdateChecker::currentArch()
{
    const QString cpu = QSysInfo::currentCpuArchitecture();
    if (cpu == QLatin1String("arm64") || cpu == QLatin1String("aarch64"))
        return QStringLiteral("arm64");
    if (cpu == QLatin1String("x86_64") || cpu == QLatin1String("amd64"))
        return QStringLiteral("x86_64");
    return cpu;
}

int UpdateChecker::pickAsset(const QList<UpdateAsset> &assets, const QString &os, const QString &arch)
{
    auto find = [&assets](const QRegularExpression &re) {
        for (int i = 0; i < assets.count(); i++)
            if (re.match(assets.at(i).name).hasMatch())
                return i;
        return -1;
    };
    const QRegularExpression::PatternOption ci = QRegularExpression::CaseInsensitiveOption;

    if (os == QLatin1String("macos"))
    {
        // qlcconsole-<version>-universal-<date>-<rev>.dmg; a single-arch
        // build (UNIVERSAL=0) names its arch instead.
        int i = find(QRegularExpression(QStringLiteral("^qlcconsole-.*-universal(-.*)?\\.dmg$"), ci));
        if (i < 0 && arch.isEmpty() == false)
            i = find(QRegularExpression(QStringLiteral("^qlcconsole-.*-%1(-.*)?\\.dmg$")
                                        .arg(QRegularExpression::escape(arch)), ci));
        return i;
    }
    if (os == QLatin1String("windows"))
    {
        // x64 only; Windows on Arm runs it emulated.
        return find(QRegularExpression(QStringLiteral("^qlcconsole-.*-win64-setup\\.exe$"), ci));
    }
    if (os == QLatin1String("linux") && arch == QLatin1String("x86_64"))
        return find(QRegularExpression(QStringLiteral("^qlcconsole-.*-x86_64\\.AppImage$"), ci));
    return -1;
}

QHash<QString, QString> UpdateChecker::parseSha256Sums(const QByteArray &text)
{
    QHash<QString, QString> out;
    static const QRegularExpression line(QStringLiteral("^([0-9a-fA-F]{64})\\s+\\*?(.+)$"));
    for (const QByteArray &raw : text.split('\n'))
    {
        const QString l = QString::fromUtf8(raw).trimmed();
        const QRegularExpressionMatch m = line.match(l);
        if (m.hasMatch() == false)
            continue;
        QString name = m.captured(2).trimmed();
        // "dist/foo.dmg" -> "foo.dmg": the asset is published by its name.
        const int slash = name.lastIndexOf(QLatin1Char('/'));
        if (slash >= 0)
            name = name.mid(slash + 1);
        out.insert(name, m.captured(1).toLower());
    }
    return out;
}

/*****************************************************************************
 * Settings
 *****************************************************************************/

bool UpdateChecker::isDisabledByPolicy()
{
    const QByteArray env = qgetenv("QLCCONSOLE_NO_UPDATES").trimmed().toLower();
    if (env.isEmpty() == false && env != "0" && env != "false" && env != "no")
        return true;
    // Machine-wide, for IT: /Library/Preferences on macOS, HKLM on
    // Windows, /etc/xdg on Linux. The person's own settings cannot undo it.
    // macOS names the file from the organisation's domain, as QSettings()
    // does: /Library/Preferences/org.qlcplus.qlcconsole.plist.
    QString org = QCoreApplication::organizationName();
#if defined(Q_OS_MACOS)
    if (QCoreApplication::organizationDomain().isEmpty() == false)
        org = QCoreApplication::organizationDomain();
#endif
    QSettings system(QSettings::SystemScope, org, QCoreApplication::applicationName());
    return system.value(SETTINGS_DISABLED, false).toBool();
}

bool UpdateChecker::checkEnabled()
{
    return QSettings().value(SETTINGS_CHECK, true).toBool();
}

void UpdateChecker::setCheckEnabled(bool on)
{
    QSettings().setValue(SETTINGS_CHECK, on);
}

UpdateChecker::Channel UpdateChecker::channel()
{
    const QString c = QSettings().value(SETTINGS_CHANNEL).toString();
    if (c == QLatin1String("stable"))
        return Stable;
    if (c == QLatin1String("prerelease"))
        return PreRelease;
    return defaultChannel();
}

void UpdateChecker::setChannel(Channel channel)
{
    // The build's own channel is not written down: a student moving from a
    // pre-release to a stable build then follows Stable by itself.
    if (channel == defaultChannel())
    {
        QSettings().remove(SETTINGS_CHANNEL);
        return;
    }
    QSettings().setValue(SETTINGS_CHANNEL, channel == Stable ? QStringLiteral("stable")
                                                             : QStringLiteral("prerelease"));
}

bool UpdateChecker::autoInstall()
{
    return QSettings().value(SETTINGS_AUTOINSTALL, false).toBool();
}

void UpdateChecker::setAutoInstall(bool on)
{
    QSettings().setValue(SETTINGS_AUTOINSTALL, on);
}

QString UpdateChecker::skippedVersion()
{
    return QSettings().value(SETTINGS_SKIP).toString();
}

void UpdateChecker::setSkippedVersion(const QString &version)
{
    if (version.isEmpty())
        QSettings().remove(SETTINGS_SKIP);
    else
        QSettings().setValue(SETTINGS_SKIP, version);
}

QDateTime UpdateChecker::lastCheck()
{
    return QSettings().value(SETTINGS_LASTCHECK).toDateTime();
}

/*****************************************************************************
 * Checking
 *****************************************************************************/

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_apiUrl(QUrl(QStringLiteral(DEFAULT_API_URL)))
    , m_timeout(15000)
    , m_status(Idle)
    , m_manual(false)
{
}

UpdateChecker::~UpdateChecker()
{
    if (m_reply != nullptr)
    {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void UpdateChecker::setApiUrl(const QUrl &url)
{
    m_apiUrl = url;
}

QUrl UpdateChecker::apiUrl() const
{
    return m_apiUrl;
}

void UpdateChecker::setTimeout(int msec)
{
    m_timeout = msec;
}

QDateTime UpdateChecker::retryAfter() const
{
    return QSettings().value(SETTINGS_RETRYAFTER).toDateTime();
}

bool UpdateChecker::isCheckDue(const QDateTime &now) const
{
    if (isDisabledByPolicy() || checkEnabled() == false)
        return false;
    const QDateTime retry = retryAfter();
    if (retry.isValid() && now < retry)
        return false;
    const QDateTime last = lastCheck();
    return last.isValid() == false || last.secsTo(now) >= 24 * 3600 || last > now;
}

QString UpdateChecker::cacheFile()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return dir.isEmpty() ? QString() : dir + QStringLiteral("/updates/releases.json");
}

void UpdateChecker::check(bool manual)
{
    if (m_reply != nullptr)
        return;     // one at a time

    m_manual = manual;
    if (isDisabledByPolicy())
    {
        finish(Disabled, tr("Update checks are turned off on this computer."));
        return;
    }
    // Rate limited: a manual check still waits -- asking again only
    // extends GitHub's block.
    const QDateTime retry = retryAfter();
    if (retry.isValid() && QDateTime::currentDateTimeUtc() < retry)
    {
        finish(RateLimited, tr("GitHub asked for a pause; try again after %1.")
                                .arg(QLocale().toString(retry.toLocalTime(), QLocale::ShortFormat)));
        return;
    }

    QNetworkRequest req(m_apiUrl);
    // GitHub requires a User-Agent; this one names the program and nothing
    // more (no version, no machine).
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qlcconsole"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setTransferTimeout(m_timeout);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    const QString etag = QSettings().value(SETTINGS_ETAG).toString();
    if (etag.isEmpty() == false && QFile::exists(cacheFile()))
        req.setRawHeader("If-None-Match", etag.toUtf8());

    m_status = Checking;
    emit statusChanged(m_status);
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateChecker::slotFinished);
}

void UpdateChecker::slotFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (reply == nullptr)
        return;
    reply->deleteLater();

    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QSettings settings;

    if (code == 403 || code == 429)
    {
        // Rate limited (or the API refusing us): wait for GitHub's reset,
        // or an hour when it does not say.
        QDateTime until = QDateTime::currentDateTimeUtc().addSecs(3600);
        const QByteArray reset = reply->rawHeader("X-RateLimit-Reset");
        const QByteArray after = reply->rawHeader("Retry-After");
        bool ok = false;
        if (reset.isEmpty() == false)
        {
            const qint64 epoch = reset.toLongLong(&ok);
            if (ok)
                until = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
        }
        else if (after.isEmpty() == false)
        {
            const qint64 secs = after.toLongLong(&ok);
            if (ok)
                until = QDateTime::currentDateTimeUtc().addSecs(secs);
        }
        settings.setValue(SETTINGS_RETRYAFTER, until);
        finish(RateLimited, tr("GitHub asked for a pause; try again after %1.")
                                .arg(QLocale().toString(until.toLocalTime(), QLocale::ShortFormat)));
        return;
    }

    if (code == 0 || (reply->error() != QNetworkReply::NoError && code < 400))
    {
        // No HTTP answer at all: offline, DNS, timeout. Quiet.
        qDebug() << "[updates] no answer:" << reply->errorString();
        finish(Offline, tr("Could not reach GitHub (%1).").arg(reply->errorString()));
        return;
    }

    if (code == 304)
    {
        settings.setValue(SETTINGS_LASTCHECK, QDateTime::currentDateTimeUtc());
        settings.remove(SETTINGS_RETRYAFTER);
        QFile f(cacheFile());
        if (f.open(QIODevice::ReadOnly))
        {
            evaluate(f.readAll());
            return;
        }
        // The cache went away under us: forget the ETag, ask in full next time.
        settings.remove(SETTINGS_ETAG);
        finish(Failed, tr("The cached release list is missing; check again."));
        return;
    }

    if (code != 200)
    {
        finish(Failed, tr("GitHub answered %1.").arg(code));
        return;
    }

    const QByteArray body = reply->readAll();
    if (QJsonDocument::fromJson(body).isArray() == false)
    {
        finish(Failed, tr("GitHub's answer could not be read."));
        return;
    }
    settings.setValue(SETTINGS_LASTCHECK, QDateTime::currentDateTimeUtc());
    settings.remove(SETTINGS_RETRYAFTER);

    const QString file = cacheFile();
    const QByteArray etag = reply->rawHeader("ETag");
    if (file.isEmpty() == false && QDir().mkpath(QFileInfo(file).absolutePath()))
    {
        QFile f(file);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(body) == body.size())
        {
            if (etag.isEmpty())
                settings.remove(SETTINGS_ETAG);
            else
                settings.setValue(SETTINGS_ETAG, QString::fromUtf8(etag));
        }
        else
        {
            settings.remove(SETTINGS_ETAG);
        }
    }
    evaluate(body);
}

void UpdateChecker::evaluate(const QByteArray &json)
{
    const UpdateRelease newest = newestOn(parseReleases(json), channel());
    m_available = UpdateRelease();
    if (newest.isValid() && compareVersions(newest.version, currentVersion()) > 0)
    {
        const QString skip = skippedVersion();
        if (m_manual || skip.isEmpty() || compareVersions(newest.version, skip) != 0)
        {
            m_available = newest;
            finish(Available);
            return;
        }
    }
    finish(UpToDate);
}

void UpdateChecker::finish(Status status, const QString &error)
{
    m_status = status;
    m_errorText = error;
    if (status != Available)
        m_available = UpdateRelease();
    emit statusChanged(status);
    emit checkFinished(status);
}
