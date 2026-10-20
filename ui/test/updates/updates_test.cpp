/*
  Q Light Controller Plus
  updates_test.cpp

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

#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QPushButton>
#include <QTextBrowser>
#include <QLabel>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSettings>
#include <QProcess>
#include <QtTest>

#include "updatedownloader.h"
#include "updateinstaller.h"
#include "updatemanager.h"
#include "updatechecker.h"
#include "updatedialog.h"
#include "updates_test.h"
#define private public
#include "mastertimer.h"
#undef private
#include "qlcconfig.h"
#include "scene.h"
#include "doc.h"

/*****************************************************************************
 * A stand-in for GitHub: a tiny HTTP/1.1 server on 127.0.0.1
 *****************************************************************************/

class FakeGitHub : public QTcpServer
{
public:
    struct Route
    {
        int code = 200;
        QList<QPair<QByteArray, QByteArray>> headers;
        QByteArray body;
        QByteArray etag;        ///< answer 304 when If-None-Match matches
        bool silent = false;    ///< accept, never answer (a timeout)
    };
    struct Request
    {
        QByteArray path;
        QHash<QByteArray, QByteArray> headers;  ///< lowercase names
    };

    FakeGitHub()
    {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = nextPendingConnection())
            {
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { onRead(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }

    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }
    void route(const QByteArray &path, const Route &r) { m_routes.insert(path, r); }
    void clear() { m_routes.clear(); requests.clear(); }

    QList<Request> requests;

private:
    void onRead(QTcpSocket *s)
    {
        QByteArray &buf = m_buffers[s];
        buf += s->readAll();
        const int end = buf.indexOf("\r\n\r\n");
        if (end < 0)
            return;
        const QList<QByteArray> lines = buf.left(end).split('\n');
        buf.clear();
        Request req;
        const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
        req.path = first.value(1);
        for (int i = 1; i < lines.count(); i++)
        {
            const int colon = lines.at(i).indexOf(':');
            if (colon > 0)
                req.headers.insert(lines.at(i).left(colon).trimmed().toLower(),
                                   lines.at(i).mid(colon + 1).trimmed());
        }
        requests << req;

        Route r;
        if (m_routes.contains(req.path))
            r = m_routes.value(req.path);
        else
            r.code = 404;
        if (r.silent)
            return;
        int code = r.code;
        QByteArray body = r.body;
        if (r.etag.isEmpty() == false && req.headers.value("if-none-match") == r.etag)
        {
            code = 304;
            body.clear();
        }
        QByteArray out = "HTTP/1.1 " + QByteArray::number(code) + " X\r\n";
        if (r.etag.isEmpty() == false)
            out += "ETag: " + r.etag + "\r\n";
        for (const auto &h : r.headers)
            out += h.first + ": " + h.second + "\r\n";
        out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        out += "Connection: close\r\n\r\n";
        out += body;
        s->write(out);
        s->disconnectFromHost();
    }

    QHash<QByteArray, Route> m_routes;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

namespace
{
const QByteArray kDmgName = "qlcconsole-99.0.0-universal-20261020-abc1234.dmg";
const QByteArray kExeName = "qlcconsole-99.0.0-win64-setup.exe";
const QByteArray kDmgBody = "this stands in for a disk image";

QString sha(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QJsonObject release(const QString &tag, bool pre, const QString &base, bool draft = false,
                    const QString &name = QString())
{
    QJsonArray assets;
    for (const QByteArray &n : { kDmgName, kExeName, QByteArray("SHA256SUMS") })
    {
        assets.append(QJsonObject{
            { "name", QString::fromLatin1(n) },
            { "browser_download_url", base + "/dl/" + QString::fromLatin1(n) },
            { "size", n == kDmgName ? double(kDmgBody.size()) : 0.0 } });
    }
    return QJsonObject{
        { "tag_name", tag }, { "name", name.isEmpty() ? "qlcconsole " + tag : name },
        { "body", "## What's new\n\n- **Bold** change\n- Another" },
        { "html_url", base + "/releases/" + tag },
        { "prerelease", pre }, { "draft", draft },
        { "published_at", "2026-10-20T12:00:00Z" },
        { "assets", assets } };
}

/** UpdateManager whose installer launch is only counted. */
class CountingManager : public UpdateManager
{
public:
    using UpdateManager::UpdateManager;
    int launches = 0;
protected:
    bool launchInstaller(bool, const QString &) override { ++launches; return true; }
};

bool run(const QString &program, const QStringList &args, QByteArray *out = nullptr,
         const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment())
{
    QProcess p;
    p.setProcessEnvironment(env);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (p.waitForFinished(120000) == false)
    {
        p.kill();
        p.waitForFinished();
        return false;
    }
    if (out != nullptr)
        *out = p.readAll();
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

/** An ad-hoc signed app bundle: /usr/bin/true as its program, a marker
 *  saying which one it is. */
bool makeApp(const QString &path, const QString &bundleId, const QString &marker)
{
    QDir().mkpath(path + "/Contents/MacOS");
    QDir().mkpath(path + "/Contents/Resources");
    if (QFile::copy("/usr/bin/true", path + "/Contents/MacOS/fake") == false)
        return false;
    QFile plist(path + "/Contents/Info.plist");
    if (plist.open(QIODevice::WriteOnly) == false)
        return false;
    plist.write(QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>"
        "<key>CFBundleIdentifier</key><string>%1</string>"
        "<key>CFBundleExecutable</key><string>fake</string>"
        "<key>CFBundlePackageType</key><string>APPL</string>"
        "</dict></plist>\n").arg(bundleId).toUtf8());
    plist.close();
    QFile m(path + "/Contents/Resources/marker");
    if (m.open(QIODevice::WriteOnly) == false)
        return false;
    m.write(marker.toUtf8());
    m.close();
    return run("/usr/bin/codesign", { "--force", "--sign", "-", path });
}

QString markerOf(const QString &app)
{
    QFile m(app + "/Contents/Resources/marker");
    return m.open(QIODevice::ReadOnly) ? QString::fromUtf8(m.readAll()) : QString();
}

bool makeDmg(const QString &srcFolder, const QString &dmg)
{
    return run("/usr/bin/hdiutil", { "create", "-quiet", "-srcfolder", srcFolder,
                                     "-format", "UDZO", "-volname", "qlcconsole test", dmg });
}
}

/*****************************************************************************
 * Fixture
 *****************************************************************************/

QByteArray Updates_Test::releasesJson(const QString &base) const
{
    QJsonArray list;
    list.append(release("v200.0.0", false, base, true));                 // draft: ignored
    list.append(release("v100.0.0-showtree.1", true, base, false, "Show Tree 1"));
    list.append(release("v99.0.0", false, base));
    list.append(release("nightly", false, base));                        // no version: ignored
    list.append(release("v0.0.1", false, base));
    return QJsonDocument(list).toJson();
}

void Updates_Test::initTestCase()
{
    // testsettings.cpp keeps every UI test off GitHub; this one talks only
    // to its own server, so lifts that for itself.
    qunsetenv("QLCCONSOLE_NO_UPDATES");
    QStandardPaths::setTestModeEnabled(true);
    m_server = new FakeGitHub;
    QVERIFY(m_server->isListening());
}

void Updates_Test::init()
{
    QSettings().remove(QStringLiteral("updates"));
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir(cache + "/updates").removeRecursively();
    m_server->clear();
    FakeGitHub::Route r;
    r.body = releasesJson(m_server->base());
    r.headers << qMakePair(QByteArray("Content-Type"), QByteArray("application/json"));
    m_server->route("/releases", r);
}

void Updates_Test::cleanup()
{
    qunsetenv("QLCCONSOLE_NO_UPDATES");
}

/*****************************************************************************
 * Versions, releases, assets
 *****************************************************************************/

void Updates_Test::compareVersions_data()
{
    QTest::addColumn<QString>("a");
    QTest::addColumn<QString>("b");
    QTest::addColumn<int>("expected");

    QTest::newRow("equal") << "0.1.0" << "0.1.0" << 0;
    QTest::newRow("leading v") << "v0.2.0" << "0.2.0" << 0;
    QTest::newRow("show tree release") << "0.1.0" << "0.2.0" << -1;
    QTest::newRow("patch") << "0.1.1" << "0.1.0" << 1;
    QTest::newRow("numeric not lexical") << "0.10.0" << "0.9.0" << 1;
    QTest::newRow("short form") << "1.0" << "1.0.0" << 0;
    QTest::newRow("pre before release") << "0.2.0-showtree.1" << "0.2.0" << -1;
    QTest::newRow("pre numeric") << "0.2.0-showtree.2" << "0.2.0-showtree.10" << -1;
    QTest::newRow("pre numeric before alpha") << "1.0.0-1" << "1.0.0-alpha" << -1;
    QTest::newRow("pre longer wins") << "1.0.0-alpha.1" << "1.0.0-alpha" << 1;
    QTest::newRow("build tie-break") << "0.2.0+qlcconsole.5000" << "0.2.0+qlcconsole.4990" << 1;
    QTest::newRow("build ignored one-sided") << "0.2.0+qlcconsole.5000" << "0.2.0" << 0;
    QTest::newRow("build never beats version") << "0.1.0+qlcconsole.9999" << "0.2.0" << -1;
    QTest::newRow("invalid below valid") << "nightly" << "0.0.1" << -1;
    QTest::newRow("both invalid") << "x" << "y" << 0;
}

void Updates_Test::compareVersions()
{
    QFETCH(QString, a);
    QFETCH(QString, b);
    QFETCH(int, expected);
    const int got = UpdateChecker::compareVersions(a, b);
    QCOMPARE(got < 0 ? -1 : (got > 0 ? 1 : 0), expected);
    const int back = UpdateChecker::compareVersions(b, a);
    QCOMPARE(back < 0 ? -1 : (back > 0 ? 1 : 0), -expected);
}

void Updates_Test::currentVersion()
{
    const QString v = UpdateChecker::currentVersion();
    QVERIFY(v.startsWith(QStringLiteral(QLCCONSOLE_VERSION)));
    QVERIFY(v.contains(QStringLiteral("+qlcconsole.")));
    QVERIFY(UpdateChecker::isValidVersion(v));
    // The build itself is never "newer than itself", and a release of the
    // same version is not an update.
    QCOMPARE(UpdateChecker::compareVersions(v, QStringLiteral(QLCCONSOLE_VERSION)), 0);
    QCOMPARE(UpdateChecker::defaultChannel(),
             UpdateChecker::isPrereleaseVersion(QStringLiteral(QLCCONSOLE_VERSION))
                 ? UpdateChecker::PreRelease : UpdateChecker::Stable);
}

void Updates_Test::parseReleases()
{
    const QList<UpdateRelease> list = UpdateChecker::parseReleases(releasesJson("http://x"));
    QCOMPARE(list.count(), 3);      // no draft, no "nightly"
    QCOMPARE(list.at(0).version, QString("100.0.0-showtree.1"));
    QCOMPARE(list.at(0).name, QString("Show Tree 1"));
    QVERIFY(list.at(0).prerelease);
    QCOMPARE(list.at(1).tag, QString("v99.0.0"));
    QCOMPARE(list.at(1).assets.count(), 3);
    QCOMPARE(list.at(1).assets.at(0).size, qint64(kDmgBody.size()));
    QVERIFY(list.at(1).body.contains("What's new"));
    QVERIFY(UpdateChecker::parseReleases("not json").isEmpty());
    QVERIFY(UpdateChecker::parseReleases("{\"message\":\"x\"}").isEmpty());
}

void Updates_Test::channels()
{
    const QList<UpdateRelease> list = UpdateChecker::parseReleases(releasesJson("http://x"));
    QCOMPARE(UpdateChecker::newestOn(list, UpdateChecker::Stable).version, QString("99.0.0"));
    QCOMPARE(UpdateChecker::newestOn(list, UpdateChecker::PreRelease).version,
             QString("100.0.0-showtree.1"));

    // A pre-release version published without GitHub's pre-release flag
    // still stays off Stable.
    QJsonArray odd;
    odd.append(release("v0.3.0-showtree.1", false, "http://x"));
    odd.append(release("v0.2.0", false, "http://x"));
    const QList<UpdateRelease> o = UpdateChecker::parseReleases(QJsonDocument(odd).toJson());
    QCOMPARE(UpdateChecker::newestOn(o, UpdateChecker::Stable).version, QString("0.2.0"));

    // The setting: the build's default is not written down.
    UpdateChecker::setChannel(UpdateChecker::defaultChannel());
    QVERIFY(QSettings().contains("updates/channel") == false);
    const UpdateChecker::Channel other = UpdateChecker::defaultChannel() == UpdateChecker::Stable
                                         ? UpdateChecker::PreRelease : UpdateChecker::Stable;
    UpdateChecker::setChannel(other);
    QCOMPARE(UpdateChecker::channel(), other);
}

void Updates_Test::pickAsset_data()
{
    QTest::addColumn<QStringList>("names");
    QTest::addColumn<QString>("os");
    QTest::addColumn<QString>("arch");
    QTest::addColumn<int>("expected");

    const QStringList all = { "SHA256SUMS",
                              "qlcconsole-0.2.0-x86_64.AppImage",
                              "qlcconsole-0.2.0-win64-setup.exe",
                              "qlcconsole-0.2.0-arm64-20261020-abc.dmg",
                              "qlcconsole-0.2.0-universal-20261020-abc.dmg" };
    QTest::newRow("mac universal first") << all << "macos" << "arm64" << 4;
    QTest::newRow("mac intel universal") << all << "macos" << "x86_64" << 4;
    QTest::newRow("windows") << all << "windows" << "x86_64" << 2;
    QTest::newRow("windows on arm") << all << "windows" << "arm64" << 2;
    QTest::newRow("linux pc") << all << "linux" << "x86_64" << 1;
    QTest::newRow("raspberry pi") << all << "linux" << "arm64" << -1;
    const QStringList armOnly = { "SHA256SUMS", "qlcconsole-0.2.0-arm64-20261020-abc.dmg" };
    QTest::newRow("mac arch fallback") << armOnly << "macos" << "arm64" << 1;
    QTest::newRow("mac wrong arch") << armOnly << "macos" << "x86_64" << -1;
    QTest::newRow("nothing") << QStringList{ "SHA256SUMS", "notes.txt" } << "windows" << "x86_64" << -1;
}

void Updates_Test::pickAsset()
{
    QFETCH(QStringList, names);
    QFETCH(QString, os);
    QFETCH(QString, arch);
    QFETCH(int, expected);
    QList<UpdateAsset> assets;
    for (const QString &n : names)
        assets << UpdateAsset{ n, QUrl("https://example.invalid/" + n), 1 };
    QCOMPARE(UpdateChecker::pickAsset(assets, os, arch), expected);
}

void Updates_Test::parseSums()
{
    const QString a(64, QLatin1Char('a')), b(64, QLatin1Char('B'));
    const QByteArray text = (a + "  qlcconsole-0.2.0-universal.dmg\n"
                             + b + " *dist/qlcconsole-0.2.0-win64-setup.exe\n"
                             "garbage line\n"
                             "1234  short-hash.txt\n").toUtf8();
    const QHash<QString, QString> sums = UpdateChecker::parseSha256Sums(text);
    QCOMPARE(sums.count(), 2);
    QCOMPARE(sums.value("qlcconsole-0.2.0-universal.dmg"), a);
    QCOMPARE(sums.value("qlcconsole-0.2.0-win64-setup.exe"), b.toLower());
}

/*****************************************************************************
 * Checking
 *****************************************************************************/

void Updates_Test::checkFindsUpdate()
{
    UpdateChecker::setChannel(UpdateChecker::PreRelease);
    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::Available);
    QCOMPARE(c.available().version, QString("100.0.0-showtree.1"));
    QCOMPARE(c.available().name, QString("Show Tree 1"));

    // What was asked: one GET, no identifying detail.
    QCOMPARE(m_server->requests.count(), 1);
    const FakeGitHub::Request &req = m_server->requests.first();
    QCOMPARE(req.headers.value("user-agent"), QByteArray("qlcconsole"));
    QVERIFY(req.headers.contains("authorization") == false);
    QVERIFY(req.headers.contains("cookie") == false);
    QVERIFY(UpdateChecker::lastCheck().isValid());
}

void Updates_Test::checkStableIgnoresPrerelease()
{
    UpdateChecker::setChannel(UpdateChecker::Stable);
    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::Available);
    QCOMPARE(c.available().version, QString("99.0.0"));
}

void Updates_Test::checkSkipVersion()
{
    UpdateChecker::setChannel(UpdateChecker::Stable);
    UpdateChecker::setSkippedVersion("99.0.0");
    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check(false);
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::UpToDate);

    // Asked for by hand, the skipped one is offered again.
    c.check(true);
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::Available);
    QCOMPARE(c.available().version, QString("99.0.0"));
}

void Updates_Test::checkUpToDate()
{
    QJsonArray list;
    list.append(release("v0.0.1", false, m_server->base()));
    list.append(release(QStringLiteral("v" QLCCONSOLE_VERSION), false, m_server->base()));
    FakeGitHub::Route r;
    r.body = QJsonDocument(list).toJson();
    m_server->route("/releases", r);

    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::UpToDate);
    QVERIFY(c.available().isValid() == false);
}

void Updates_Test::checkEtag()
{
    FakeGitHub::Route r;
    r.body = releasesJson(m_server->base());
    r.etag = "\"abc123\"";
    m_server->route("/releases", r);
    UpdateChecker::setChannel(UpdateChecker::Stable);

    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::Available);
    QVERIFY(m_server->requests.first().headers.contains("if-none-match") == false);

    // Again: If-None-Match goes with it, GitHub's 304 answers from the cache.
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(m_server->requests.count(), 2);
    QCOMPARE(m_server->requests.at(1).headers.value("if-none-match"), QByteArray("\"abc123\""));
    QCOMPARE(c.status(), UpdateChecker::Available);
    QCOMPARE(c.available().version, QString("99.0.0"));
}

void Updates_Test::checkRateLimited()
{
    const qint64 reset = QDateTime::currentSecsSinceEpoch() + 600;
    FakeGitHub::Route r;
    r.code = 403;
    r.body = "{\"message\":\"API rate limit exceeded\"}";
    r.headers << qMakePair(QByteArray("X-RateLimit-Remaining"), QByteArray("0"))
              << qMakePair(QByteArray("X-RateLimit-Reset"), QByteArray::number(reset));
    m_server->route("/releases", r);

    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::RateLimited);
    QCOMPARE(c.retryAfter().toSecsSinceEpoch(), reset);
    QVERIFY(c.isCheckDue() == false);

    // Until then nothing is sent, not even when asked by hand.
    c.check(true);
    QCOMPARE(spy.count(), 2);       // answered at once
    QCOMPARE(c.status(), UpdateChecker::RateLimited);
    QCOMPARE(m_server->requests.count(), 1);
}

void Updates_Test::checkOffline()
{
    // A port nobody listens on: refused, as with no network.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 port = probe.serverPort();
    probe.close();

    UpdateChecker c;
    c.setApiUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/releases").arg(port)));
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::Offline);
    QVERIFY(UpdateChecker::lastCheck().isValid() == false);  // tried again later
}

void Updates_Test::checkTimeout()
{
    FakeGitHub::Route r;
    r.silent = true;
    m_server->route("/releases", r);
    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    c.setTimeout(300);
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    QElapsedTimer t;
    t.start();
    c.check();
    QVERIFY(spy.wait(10000));
    QCOMPARE(c.status(), UpdateChecker::Offline);
    QVERIFY(t.elapsed() < 5000);
}

void Updates_Test::checkDue()
{
    UpdateChecker c;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(c.isCheckDue(now));                         // never checked
    QSettings().setValue("updates/lastCheck", now.addSecs(-3600));
    QVERIFY(c.isCheckDue(now) == false);                // within the day
    QSettings().setValue("updates/lastCheck", now.addSecs(-25 * 3600));
    QVERIFY(c.isCheckDue(now));
    UpdateChecker::setCheckEnabled(false);
    QVERIFY(c.isCheckDue(now) == false);
    UpdateChecker::setCheckEnabled(true);
    QSettings().setValue("updates/retryAfter", now.addSecs(60));
    QVERIFY(c.isCheckDue(now) == false);
}

void Updates_Test::policyDisablesEverything()
{
    qputenv("QLCCONSOLE_NO_UPDATES", "1");
    QVERIFY(UpdateChecker::isDisabledByPolicy());
    QVERIFY(UpdateManager::backgroundChecksAllowed() == false);
    UpdateChecker c;
    c.setApiUrl(QUrl(m_server->base() + "/releases"));
    QVERIFY(c.isCheckDue() == false);
    QSignalSpy spy(&c, &UpdateChecker::checkFinished);
    c.check(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(c.status(), UpdateChecker::Disabled);
    QCOMPARE(m_server->requests.count(), 0);

    qputenv("QLCCONSOLE_NO_UPDATES", "0");
    QVERIFY(UpdateChecker::isDisabledByPolicy() == false);
}

/*****************************************************************************
 * Download and verify
 *****************************************************************************/

void Updates_Test::downloadVerified()
{
    FakeGitHub::Route sums;
    sums.body = (sha(kDmgBody) + "  " + QString::fromLatin1(kDmgName) + "\n").toUtf8();
    m_server->route("/dl/SHA256SUMS", sums);
    // GitHub sends assets through a redirect to its storage.
    FakeGitHub::Route redirect;
    redirect.code = 302;
    redirect.headers << qMakePair(QByteArray("Location"),
                                  (m_server->base() + "/storage/" + kDmgName).toUtf8());
    m_server->route("/dl/" + kDmgName, redirect);
    FakeGitHub::Route file;
    file.body = kDmgBody;
    m_server->route("/storage/" + kDmgName, file);

    const UpdateRelease rel = UpdateChecker::parseReleases(releasesJson(m_server->base())).at(1);
    QTemporaryDir dir;
    UpdateDownloader d;
    QSignalSpy done(&d, &UpdateDownloader::finished);
    QSignalSpy progress(&d, &UpdateDownloader::progress);
    d.start(rel, 0, dir.path() + "/dl");
    QVERIFY(done.wait(10000));
    QCOMPARE(done.first().at(0).toBool(), true);
    QVERIFY(progress.count() > 0);
    QCOMPARE(QFileInfo(d.filePath()).fileName(), QString::fromLatin1(kDmgName));
    QFile f(d.filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), kDmgBody);
}

void Updates_Test::downloadShaMismatchRefused()
{
    FakeGitHub::Route sums;
    sums.body = (sha("something else") + "  " + QString::fromLatin1(kDmgName) + "\n").toUtf8();
    m_server->route("/dl/SHA256SUMS", sums);
    FakeGitHub::Route file;
    file.body = kDmgBody;
    m_server->route("/dl/" + kDmgName, file);

    const UpdateRelease rel = UpdateChecker::parseReleases(releasesJson(m_server->base())).at(1);
    QTemporaryDir dir;
    UpdateDownloader d;
    QSignalSpy done(&d, &UpdateDownloader::finished);
    d.start(rel, 0, dir.path() + "/dl");
    QVERIFY(done.wait(10000));
    QCOMPARE(done.first().at(0).toBool(), false);
    QVERIFY(done.first().at(1).toString().contains("checksum"));
    QVERIFY(d.filePath().isEmpty());
    QVERIFY(QFile::exists(dir.path() + "/dl/" + kDmgName) == false);   // deleted

    // A file SHA256SUMS does not list is refused too.
    sums.body = (sha(kDmgBody) + "  some-other-file.dmg\n").toUtf8();
    m_server->route("/dl/SHA256SUMS", sums);
    d.start(rel, 0, dir.path() + "/dl");
    QVERIFY(done.wait(10000));
    QCOMPARE(done.last().at(0).toBool(), false);
    QVERIFY(QFile::exists(dir.path() + "/dl/" + kDmgName) == false);
}

void Updates_Test::downloadWithoutSumsRefused()
{
    UpdateRelease rel = UpdateChecker::parseReleases(releasesJson(m_server->base())).at(1);
    rel.assets.removeLast();        // no SHA256SUMS
    FakeGitHub::Route file;
    file.body = kDmgBody;
    m_server->route("/dl/" + kDmgName, file);
    QTemporaryDir dir;
    UpdateDownloader d;
    QSignalSpy done(&d, &UpdateDownloader::finished);
    d.start(rel, 0, dir.path() + "/dl");
    if (done.isEmpty())
        QVERIFY(done.wait(5000));
    QCOMPARE(done.first().at(0).toBool(), false);
    QVERIFY(done.first().at(1).toString().contains("SHA256SUMS"));
    QCOMPARE(m_server->requests.count(), 0);    // the file was never fetched
}

/*****************************************************************************
 * Never during a show
 *****************************************************************************/

void Updates_Test::nothingInstallsDuringShow()
{
    Doc doc(this);
    QWidget window;
    CountingManager mgr(&doc, &window);
    mgr.m_release = UpdateChecker::parseReleases(releasesJson(m_server->base())).at(1);
    mgr.m_state = UpdateManager::Ready;
    QSignalSpy restart(&mgr, &UpdateManager::restartRequested);

    Scene *scene = new Scene(&doc);
    QVERIFY(doc.addFunction(scene));
    scene->start(doc.masterTimer(), FunctionParent::master());
    doc.masterTimer()->timerTick();
    QCOMPARE(doc.masterTimer()->runningFunctions(), 1);

    QString why;
    QVERIFY(UpdateInstaller::safeToInstall(&doc, &why) == false);
    QVERIFY(why.contains("running"));
    QVERIFY(mgr.aboutToQuit(true, QString()) == false);
    QVERIFY(mgr.restartNow() == false);
    QCOMPARE(mgr.launches, 0);
    QCOMPARE(restart.count(), 0);

    scene->stop(FunctionParent::master());
    doc.masterTimer()->timerTick();
    doc.masterTimer()->timerTick();
    QCOMPARE(doc.masterTimer()->runningFunctions(), 0);

    doc.setMode(Doc::Operate);
    QVERIFY(UpdateInstaller::safeToInstall(&doc) == false);
    QVERIFY(mgr.aboutToQuit(false, QString()) == false);
    QVERIFY(mgr.restartNow() == false);
    QCOMPARE(mgr.launches, 0);
    doc.setMode(Doc::Design);

    doc.setKiosk(true);
    QVERIFY(mgr.aboutToQuit(false, QString()) == false);
    doc.setKiosk(false);

    // Not staged yet (only found): nothing to install at quit.
    mgr.m_state = UpdateManager::Available;
    QVERIFY(mgr.aboutToQuit(false, QString()) == false);
    QCOMPARE(mgr.launches, 0);

    // Staged, nothing on stage, Design mode: it goes.
    mgr.m_state = UpdateManager::Ready;
    QVERIFY(UpdateInstaller::safeToInstall(&doc));
    QVERIFY(mgr.restartNow());
    QCOMPARE(restart.count(), 1);
    QVERIFY(mgr.aboutToQuit(true, QString()));
    QCOMPARE(mgr.launches, 1);
    QCOMPARE(QSettings().value("updates/pendingVersion").toString(), QString("99.0.0"));
}

void Updates_Test::cleanupAfterUpdate()
{
    QTemporaryDir dir;
    const QString backup = dir.path() + "/.qlcconsole-previous.app";
    QVERIFY(QDir().mkpath(backup + "/Contents"));

    // The new version has started: its backup goes.
    QSettings().setValue("updates/pendingVersion", "0.0.1");
    QSettings().setValue("updates/pendingBackup", backup);
    QCOMPARE(UpdateManager::cleanupAfterUpdate(), QString("0.0.1"));
    QVERIFY(QFileInfo::exists(backup) == false);
    QVERIFY(QSettings().contains("updates/pendingVersion") == false);

    // Never anything but the backup's own name.
    const QString other = dir.path() + "/Important.app";
    QVERIFY(QDir().mkpath(other));
    QSettings().setValue("updates/pendingVersion", "0.0.1");
    QSettings().setValue("updates/pendingBackup", other);
    UpdateManager::cleanupAfterUpdate();
    QVERIFY(QFileInfo::exists(other));

    // Still the old version (the install did not happen): the backup stays.
    QVERIFY(QDir().mkpath(backup));
    QSettings().setValue("updates/pendingVersion", "999.0.0");
    QSettings().setValue("updates/pendingBackup", backup);
    QCOMPARE(UpdateManager::cleanupAfterUpdate(), QString());
    QVERIFY(QFileInfo::exists(backup));
    QVERIFY(QSettings().contains("updates/pendingVersion"));
}

void Updates_Test::windowsScript()
{
    const QString s = UpdateInstaller::windowsScript(
        "C:/Users/me/AppData/Local/Temp/qlcconsole-update-0.2.0/qlcconsole-0.2.0-win64-setup.exe",
        4242, "C:/qlcconsole", "C:/qlcconsole/qlcconsole.exe", true, "C:/Shows/My Show.qxw");
    QVERIFY(s.contains("PID eq 4242"));
    QVERIFY(s.contains("\"C:\\Users\\me\\AppData\\Local\\Temp\\qlcconsole-update-0.2.0\\"
                       "qlcconsole-0.2.0-win64-setup.exe\" /S /D=C:\\qlcconsole"));
    QVERIFY(s.contains("start \"\" \"C:\\qlcconsole\\qlcconsole.exe\" -o \"C:\\Shows\\My Show.qxw\""));
    // The installer runs only after the wait loop.
    QVERIFY(s.indexOf("/S /D=") > s.indexOf(":install"));
    const QString quiet = UpdateInstaller::windowsScript("C:/t/setup.exe", 1, "C:/q", "C:/q/q.exe",
                                                         false, "C:/s.qxw");
    QVERIFY(quiet.contains("start \"\"") == false);
}

void Updates_Test::dialogStates()
{
    Doc doc(this);
    QWidget window;
    CountingManager mgr(&doc, &window);
    UpdateDialog dlg(&mgr);
    auto button = [&dlg](const char *name) { return dlg.findChild<QPushButton *>(name); };

    QVERIFY(button("updateInstall")->isHidden());

    mgr.m_release = UpdateChecker::parseReleases(releasesJson(m_server->base())).at(1);
    mgr.setState(UpdateManager::Available, QString());
    QVERIFY(button("updateInstall")->isHidden() == false);
    QVERIFY(button("updateSkip")->isHidden() == false);
    QVERIFY(button("updateRestart")->isHidden());
    QVERIFY(dlg.findChild<QTextBrowser *>("updateNotes")->toPlainText().contains("Bold change"));

    mgr.setState(UpdateManager::Ready, QString());
    QVERIFY(button("updateRestart")->isHidden() == false);
    QVERIFY(button("updateRestart")->isEnabled());
    doc.setMode(Doc::Operate);
    QMetaObject::invokeMethod(&dlg, "refresh");
    QVERIFY(button("updateRestart")->isEnabled() == false);
    doc.setMode(Doc::Design);

    // The footer chip says what is going on, and is hidden with nothing to say.
    QLabel chip;
    mgr.setFooterLabel(&chip);
    QCOMPARE(chip.text(), QString("Update 99.0.0 installs when you quit"));
    mgr.setState(UpdateManager::Available, QString());
    QCOMPARE(chip.text(), QString("Update available: 99.0.0"));
    mgr.skipVersion();
    QVERIFY(chip.isHidden());
    QCOMPARE(UpdateChecker::skippedVersion(), QString("99.0.0"));
}

/*****************************************************************************
 * The macOS helper, on fake apps
 *****************************************************************************/

void Updates_Test::macHelperPrepare()
{
#if !defined(Q_OS_MACOS)
    QSKIP("macOS only");
#else
    QTemporaryDir dir;
    const QString apps = dir.path() + "/Applications";
    QVERIFY(QDir().mkpath(apps));
    QVERIFY(makeApp(apps + "/qlcconsole.app", "com.test.qlcconsole", "1"));
    QVERIFY(QDir().mkpath(dir.path() + "/src"));
    QVERIFY(makeApp(dir.path() + "/src/qlcconsole.app", "com.test.qlcconsole", "2"));
    const QString dmg = dir.path() + "/new.dmg";
    QVERIFY(makeDmg(dir.path() + "/src", dmg));

    UpdateInstaller inst;
    QSignalSpy spy(&inst, &UpdateInstaller::prepared);
    inst.prepare(dmg, apps + "/qlcconsole.app", true);
    QVERIFY(spy.wait(120000));
    QVERIFY2(spy.first().at(0).toBool(), qPrintable(spy.first().at(1).toString()));
    QCOMPARE(inst.stagedPath(), apps + "/.qlcconsole-update.app");
    QCOMPARE(markerOf(inst.stagedPath()), QString("2"));
    QCOMPARE(markerOf(apps + "/qlcconsole.app"), QString("1"));   // untouched until quit
    QVERIFY(inst.teamId().isEmpty());
#endif
}

void Updates_Test::macHelperRefusals()
{
#if !defined(Q_OS_MACOS)
    QSKIP("macOS only");
#else
    QTemporaryDir dir;
    const QString apps = dir.path() + "/Applications";
    const QString running = apps + "/qlcconsole.app";
    const QString staged = apps + "/.qlcconsole-update.app";
    QVERIFY(QDir().mkpath(apps));
    QVERIFY(makeApp(running, "com.test.qlcconsole", "1"));

    auto prepareFrom = [&](const QString &name, const QString &bundleId, bool tamper,
                           bool allowAdhoc, QString *error) {
        const QString src = dir.path() + "/" + name;
        QDir().mkpath(src);
        if (bundleId.isEmpty() == false)
        {
            makeApp(src + "/qlcconsole.app", bundleId, "2");
            if (tamper)
            {
                QFile m(src + "/qlcconsole.app/Contents/Resources/marker");
                if (m.open(QIODevice::WriteOnly))
                    m.write("evil");
            }
        }
        else
        {
            QFile f(src + "/readme.txt");
            if (f.open(QIODevice::WriteOnly))
                f.write("no app here");
        }
        const QString dmg = dir.path() + "/" + name + ".dmg";
        if (makeDmg(src, dmg) == false)
            return false;
        UpdateInstaller inst;
        QSignalSpy spy(&inst, &UpdateInstaller::prepared);
        inst.prepare(dmg, running, allowAdhoc);
        if (spy.isEmpty() && spy.wait(120000) == false)
            return false;
        *error = spy.first().at(1).toString();
        return spy.first().at(0).toBool();
    };

    QString error;
    // A copy with no Team ID (a dev build) cannot vouch for anything.
    QVERIFY(prepareFrom("unsigned", "com.test.qlcconsole", false, false, &error) == false);
    QVERIFY2(error.contains("not a signed release"), qPrintable(error));
    QVERIFY(QFileInfo::exists(staged) == false);

    QVERIFY(prepareFrom("otherid", "com.test.somethingelse", false, true, &error) == false);
    QVERIFY2(error.contains("bundle identifier"), qPrintable(error));
    QVERIFY(QFileInfo::exists(staged) == false);

    QVERIFY(prepareFrom("tampered", "com.test.qlcconsole", true, true, &error) == false);
    QVERIFY2(error.contains("signature"), qPrintable(error));
    QVERIFY(QFileInfo::exists(staged) == false);

    QVERIFY(prepareFrom("empty", QString(), false, true, &error) == false);
    QVERIFY2(error.contains("no app"), qPrintable(error));

    // No write access to the app's folder: no in-place install at all.
    QFile::setPermissions(apps, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
    QString why;
    QVERIFY(UpdateInstaller::canReplaceBundle(running, &why) == false);
    QVERIFY(why.contains("administrator"));
    QFile::setPermissions(apps, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QVERIFY(UpdateInstaller::canReplaceBundle(running));
    QVERIFY(UpdateInstaller::canReplaceBundle("/private/var/folders/x/AppTranslocation/y/qlcconsole.app") == false);
    QVERIFY(markerOf(running) == "1");
#endif
}

void Updates_Test::macHelperInstall()
{
#if !defined(Q_OS_MACOS)
    QSKIP("macOS only");
#else
    QTemporaryDir dir;
    const QString apps = dir.path() + "/Applications";
    const QString current = apps + "/qlcconsole.app";
    const QString staged = apps + "/.qlcconsole-update.app";
    const QString backup = apps + "/.qlcconsole-previous.app";
    QVERIFY(QDir().mkpath(apps));
    QVERIFY(makeApp(current, "com.test.qlcconsole", "1"));
    QVERIFY(makeApp(staged, "com.test.qlcconsole", "2"));
    QVERIFY(makeApp(backup, "com.test.qlcconsole", "stale"));   // from an earlier update
    const QString show = dir.path() + "/My Show.qxw";
    { QFile f(show); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("<x/>"); }

    // The relaunch is recorded, not done.
    const QString opener = dir.path() + "/fake-open.sh";
    const QString opened = dir.path() + "/opened.txt";
    {
        QFile f(opener);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QStringLiteral("#!/bin/bash\nprintf '%s\\n' \"$@\" > \"%1\"\n")
                    .arg(opened).toUtf8());
    }
    QFile::setPermissions(opener, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    const QString helper = UpdateInstaller::writeMacHelper();     // the copy built into the app
    QVERIFY(helper.isEmpty() == false);

    // A refusal first: the staged copy is not from the expected team.
    QByteArray out;
    QVERIFY(run("/bin/bash", { helper, "install", "--pid", "999999", "--current", current,
                               "--new", staged, "--backup", backup,
                               "--team-id", "ABCDE12345" }, &out) == false);
    QCOMPARE(markerOf(current), QString("1"));
    QVERIFY(QFileInfo::exists(staged) == false);                 // removed, not installed

    // The real thing: waits for "qlcconsole" (a sleep) to end, swaps, reopens.
    QVERIFY(makeApp(staged, "com.test.qlcconsole", "2"));
    QProcess app;
    app.start("/bin/sleep", { "1" });
    QVERIFY(app.waitForStarted());
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QLC_UPDATE_OPEN", opener);
    QElapsedTimer t;
    t.start();
    QVERIFY2(run("/bin/bash", { helper, "install", "--pid", QString::number(app.processId()),
                                "--current", current, "--new", staged, "--backup", backup,
                                "--relaunch", "--show", show }, &out, env),
             out.constData());
    QVERIFY(t.elapsed() >= 700);                  // it waited for the process
    app.waitForFinished();

    QCOMPARE(markerOf(current), QString("2"));
    QCOMPARE(markerOf(backup), QString("1"));
    QVERIFY(QFileInfo::exists(staged) == false);
    QFile f(opened);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts),
             QStringList({ "-n", current, "--args", "-o", show }));
    QFile::remove(helper);
#endif
}

QTEST_MAIN(Updates_Test)
