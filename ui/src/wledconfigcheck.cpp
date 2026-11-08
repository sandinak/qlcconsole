/*
  Q Light Controller Plus
  wledconfigcheck.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QDateTime>
#include <QJsonArray>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <memory>
#include <QTimer>
#include <QSet>

#include "wledconfigcheck.h"
#include "monitorproperties.h"
#include "truss.h"
#include "wleddevicewrite.h"
#include "powerdistribution.h"
#include "wleddiscovery.h"
#include "showstatus.h"
#include "qlcfixturemode.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "outputpatch.h"
#include "wledfixture.h"
#include "fixture.h"
#include "doc.h"

WledConfigCheck *WledConfigCheck::s_instance = nullptr;

namespace
{
const int ArtNetPort = 6454;
const int E131Port = 5568;

/* WLED's own DMX modes (WLED const.h), the ones a definition here drives. */
const int DmxSingleRgb = WledFixture::DmxSingleRgb;
const int DmxSingleDrgb = WledFixture::DmxSingleDrgb;
const int DmxEffect = WledFixture::DmxEffect;

/* A realtime timeout (tenths of a second) that holds a stream between the
   show's frames and hands the WLED back to its own effects soon after the
   show stops sending. */
const int TimeoutMin = 10, TimeoutMax = 100, TimeoutWant = 25;

QString dmxModeName(int mode)
{
    switch (mode)
    {
    case 0: return QObject::tr("disabled");
    case DmxSingleRgb: return QObject::tr("Single RGB");
    case DmxSingleDrgb: return QObject::tr("Single DRGB");
    case DmxEffect: return QObject::tr("Effect");
    case 4: return QObject::tr("Multi RGB (pixels)");
    case 5: return QObject::tr("Dimmer + Multi RGB");
    case 6: return QObject::tr("Multi RGBW");
    default: return QObject::tr("mode %1").arg(mode);
    }
}

/** @p from merged into @p into, object by object. */
void mergeJson(QJsonObject &into, const QJsonObject &from)
{
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        if (it.value().isObject() && into.value(it.key()).isObject())
        {
            QJsonObject sub = into.value(it.key()).toObject();
            mergeJson(sub, it.value().toObject());
            into[it.key()] = sub;
        }
        else
            into[it.key()] = it.value();
    }
}

/** What of @p sent @p now does not say, in words ("id.name"). */
QStringList jsonDiffers(const QJsonObject &sent, const QJsonObject &now, const QString &at = QString())
{
    QStringList out;
    for (auto it = sent.begin(); it != sent.end(); ++it)
    {
        const QString path = at.isEmpty() ? it.key() : at + QLatin1Char('.') + it.key();
        if (it.value().isObject())
            out << jsonDiffers(it.value().toObject(), now.value(it.key()).toObject(), path);
        else if (it.value() != now.value(it.key()))
            out << path;
    }
    return out;
}

/** The setting-up findings @p s folded into @p f (one fix, one list). */
void mergeSetUp(WledConfigCheck::Finding &f, const WledConfigCheck::Finding &s)
{
    f.problems << s.problems;
    f.changes << s.changes;
    f.notes << s.notes;
    mergeJson(f.fix, s.fix);
    /* A new LED count sends all of hw.led back as the device said it: with
       the limiter it is about to get, or the count would undo it. */
    const QJsonObject sLed = s.fix.value(QStringLiteral("hw")).toObject().value(QStringLiteral("led")).toObject();
    if (f.ledFix.isEmpty() || !sLed.contains(QStringLiteral("maxpwr")))
        return;
    QJsonObject hw = f.ledFix.value(QStringLiteral("hw")).toObject();
    QJsonObject led = hw.value(QStringLiteral("led")).toObject();
    const QJsonValue ma = sLed.value(QStringLiteral("maxpwr"));
    led[QStringLiteral("maxpwr")] = ma;
    QJsonArray ins = led.value(QStringLiteral("ins")).toArray();
    if (ins.size() == 1)
    {
        QJsonObject out = ins.first().toObject();
        out[QStringLiteral("maxpwr")] = ma;
        ins[0] = out;
        led[QStringLiteral("ins")] = ins;
    }
    hw[QStringLiteral("led")] = led;
    f.ledFix[QStringLiteral("hw")] = hw;
}

QString protocolName(int port)
{
    return port == ArtNetPort ? QStringLiteral("Art-Net")
         : port == E131Port   ? QStringLiteral("E1.31 (sACN)")
                              : QObject::tr("port %1").arg(port);
}
}

WledConfigCheck::WledConfigCheck(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    if (s_instance == nullptr)
        s_instance = this;
    m_nam->setProxy(QNetworkProxy::NoProxy);    // WLEDs are on the show LAN
    m_timer->setInterval(30000);
    connect(m_timer, &QTimer::timeout, this, &WledConfigCheck::checkNow);
    m_timer->start();
}

WledConfigCheck::~WledConfigCheck()
{
    if (s_instance == this)
        s_instance = nullptr;
}

void WledConfigCheck::setInterval(int ms)
{
    m_timer->setInterval(qMax(50, ms));
}

QString WledConfigCheck::baseUrl(const QString &ip)
{
    return ip.contains(QLatin1String("://")) ? ip : QStringLiteral("http://%1").arg(ip);
}

bool WledConfigCheck::isWled(const Fixture *fx)
{
    if (fx == nullptr || fx->connection() != Fixture::Network || fx->ipAddress().isEmpty())
        return false;
    if (fx->transport() == Fixture::WledTransport)
        return true;
    return fx->fixtureDef() != nullptr
           && fx->fixtureDef()->manufacturer().compare(QLatin1String("WLED"), Qt::CaseInsensitive) == 0;
}

int WledConfigCheck::fixtureLedCount(Doc *doc, const Fixture *fx)
{
    if (fx == nullptr)
        return 0;
    if (fx->heads() > 1)
        return fx->heads();
    MonitorProperties *props = doc != nullptr ? doc->monitorProperties() : nullptr;
    if (props != nullptr)
        return qMax(0, props->fixtureRigProps(fx->id()).lightLeds);
    return 0;
}

WledConfigCheck::Finding WledConfigCheck::evaluate(Doc *doc, const Fixture *fx, const QJsonObject &cfg)
{
    Finding f;
    if (fx == nullptr)
        return f;
    f.fixtureId = fx->id();
    f.ip = fx->ipAddress();

    const QJsonObject live = cfg.value(QStringLiteral("if")).toObject()
                                .value(QStringLiteral("live")).toObject();
    const QJsonObject dmx = live.value(QStringLiteral("dmx")).toObject();
    const bool realtime = live.value(QStringLiteral("en")).toBool();
    const int port = live.value(QStringLiteral("port")).toInt();
    const int uni = dmx.value(QStringLiteral("uni")).toInt();
    const int addr = dmx.value(QStringLiteral("addr")).toInt();
    const int mode = dmx.value(QStringLiteral("mode")).toInt();
    const int timeout = live.value(QStringLiteral("timeout")).toInt();

    QJsonObject wantLive;
    QJsonObject wantDmx;

    /* The same device in this show twice, once through WLED's API and once
       as Art-Net/sACN (its LEDs, say): the one wants streams off, the other
       on, and either way one of them is ignored. Said, and the setting left
       alone, so Fix on Device never flips it back and forth. */
    QString both;
    if (doc != nullptr)
    {
        foreach (Fixture *other, doc->fixtures())
            if (other != fx && isWled(other) && other->ipAddress() == fx->ipAddress()
                && (other->transport() == Fixture::WledTransport) != (fx->transport() == Fixture::WledTransport))
            {
                both = other->name();
                break;
            }
    }
    if (!both.isEmpty())
        f.problems << (fx->transport() == Fixture::WledTransport
            ? tr("is also %1 in this show, sent to as Art-Net/sACN: while that streams, the WLED "
                 "ignores these looks. Keep one of the two").arg(both)
            : tr("is also %1 in this show, driven through WLED's API: while this streams, the "
                 "WLED ignores that one's looks. Keep one of the two").arg(both));

    if (fx->transport() == Fixture::WledTransport)
    {
        /* Driven through WLED's own API: a stream arriving on its universe
           would take it over, and the look would be ignored meanwhile. */
        if (realtime && both.isEmpty())
        {
            f.problems << tr("receives %1 streams (universe %2): one would take it over from "
                             "this show for %3 s at a time").arg(protocolName(port)).arg(uni)
                                                            .arg(timeout > 0 ? timeout / 10.0 : 2.5);
            wantLive[QStringLiteral("en")] = false;
            f.changes << tr("Receive realtime streams: on → off");
        }
    }
    else
    {
        // Art-Net or sACN: what its universe sends, and where it starts.
        int wantPort = 0, wantUni = -1;
        if (doc != nullptr && fx->isPatched())
        {
            InputOutputMap *io = doc->inputOutputMap();
            OutputPatch *op = io->outputPatch(fx->universe());
            if (op != nullptr)
            {
                const QMap<QString, QVariant> pp = op->getPluginParameters();
                const QString plugin = op->pluginName();
                if (plugin.compare(QLatin1String("ArtNet"), Qt::CaseInsensitive) == 0)
                {
                    wantPort = ArtNetPort;
                    wantUni = pp.value(QStringLiteral("outputUni"), int(fx->universe())).toInt();
                }
                else if (plugin.startsWith(QLatin1String("E1.31")))
                {
                    wantPort = E131Port;
                    wantUni = pp.value(QStringLiteral("universe"), int(fx->universe()) + 1).toInt();
                }
            }
        }
        const QString modeName = fx->fixtureMode() != nullptr ? fx->fixtureMode()->name() : QString();
        const int wantMode = WledFixture::dmxModeFor(fx);

        if (!realtime && both.isEmpty())
        {
            f.problems << tr("does not listen for Art-Net or sACN at all");
            wantLive[QStringLiteral("en")] = true;
            f.changes << tr("Receive realtime streams: off → on");
        }
        if (wantPort != 0 && port != wantPort)
        {
            f.problems << tr("listens for %1, but its universe goes out as %2")
                              .arg(protocolName(port), protocolName(wantPort));
            wantLive[QStringLiteral("port")] = wantPort;
            f.changes << tr("Protocol: %1 → %2").arg(protocolName(port), protocolName(wantPort));
        }
        if (wantUni >= 0 && uni != wantUni)
        {
            f.problems << tr("listens on universe %1, its patch sends universe %2").arg(uni).arg(wantUni);
            wantDmx[QStringLiteral("uni")] = wantUni;
            f.changes << tr("Universe: %1 → %2").arg(uni).arg(wantUni);
        }
        if (fx->isPatched() && addr != int(fx->address()) + 1)
        {
            f.problems << tr("starts at address %1, it is patched at %2").arg(addr).arg(fx->address() + 1);
            wantDmx[QStringLiteral("addr")] = int(fx->address()) + 1;
            f.changes << tr("Start address: %1 → %2").arg(addr).arg(fx->address() + 1);
        }
        if (wantMode > 0 && mode != wantMode)
        {
            f.problems << tr("is in DMX mode %1, its definition's \"%2\" needs %3")
                              .arg(dmxModeName(mode), modeName, dmxModeName(wantMode));
            wantDmx[QStringLiteral("mode")] = wantMode;
            f.changes << tr("DMX mode: %1 → %2").arg(dmxModeName(mode), dmxModeName(wantMode));
        }
        if (WledFixture::isPixelMode(fx))
        {
            /* Its LEDs from this show, frame by frame: every LED (not its
               main segment's only), at the level sent (not dimmed again by
               its own brightness), and back to its own effects soon after
               the show stops sending -- not before the next frame. */
            if (live.value(QStringLiteral("mso")).toBool())
            {
                f.problems << tr("takes streams on its main segment only: the LEDs of its other "
                                 "segments stay dark");
                wantLive[QStringLiteral("mso")] = false;
                f.changes << tr("Use main segment only: on → off");
            }
            if (live.contains(QStringLiteral("maxbri")) && !live.value(QStringLiteral("maxbri")).toBool())
            {
                f.problems << tr("dims what this show sends by its own brightness");
                wantLive[QStringLiteral("maxbri")] = true;
                f.changes << tr("Force max brightness: off → on");
            }
            if (live.contains(QStringLiteral("timeout")) && (timeout < TimeoutMin || timeout > TimeoutMax))
            {
                f.problems << tr("waits %1 s after the last frame before it plays its own effects "
                                 "again").arg(timeout / 10.0);
                wantLive[QStringLiteral("timeout")] = TimeoutWant;
                f.changes << tr("Realtime timeout: %1 s → %2 s").arg(timeout / 10.0).arg(TimeoutWant / 10.0);
            }
        }
    }

    /* The LED count: the show's against the device's, when the show says
       one. Asked for on its own (ledFix), never folded into the fix. */
    const int want = fixtureLedCount(doc, fx);
    const QJsonObject hwLed = cfg.value(QStringLiteral("hw")).toObject()
                                 .value(QStringLiteral("led")).toObject();
    const QJsonArray ins = hwLed.value(QStringLiteral("ins")).toArray();
    int have = hwLed.value(QStringLiteral("total")).toInt();
    if (have <= 0)
        for (const QJsonValue &out : ins)
            have += out.toObject().value(QStringLiteral("len")).toInt();
    if (want > 0 && have > 0 && want != have)
    {
        f.fixtureLeds = want;
        f.deviceLeds = have;
        f.problems << tr("the WLED drives %1 LEDs; this fixture has %2").arg(have).arg(want);
        if (hwLed.contains(QStringLiteral("matrix")))
            f.ledRefusal = tr("It is set up as a 2D matrix: its size is set in the WLED's own 2D "
                              "settings, not here.");
        else if (ins.size() != 1)
            f.ledRefusal = tr("It drives its LEDs from %1 outputs: how many each has is set in "
                              "the WLED's own LED settings. This show sets the count of a WLED "
                              "with one output only.").arg(ins.size());
        else
        {
            /* WLED builds its outputs from exactly what is sent (an output
               without its pins is dropped, a setting left out goes back to
               its default), so all of hw.led goes back as the device said it,
               only the length and the total changed: never its LED type,
               colour order or pins. */
            QJsonObject out = ins.first().toObject();
            const int start = out.value(QStringLiteral("start")).toInt();
            out[QStringLiteral("len")] = want - start;
            QJsonObject led = hwLed;
            led[QStringLiteral("ins")] = QJsonArray{ out };
            led[QStringLiteral("total")] = want;
            if (want - start > 0)
                f.ledFix = QJsonObject{ { QStringLiteral("hw"), QJsonObject{ { QStringLiteral("led"), led } } } };
        }
    }

    if (!wantDmx.isEmpty())
        wantLive[QStringLiteral("dmx")] = wantDmx;
    if (!wantLive.isEmpty())
    {
        QJsonObject wantIf;
        wantIf[QStringLiteral("live")] = wantLive;
        f.fix[QStringLiteral("if")] = wantIf;
    }
    mergeSetUp(f, setUpFor(doc, fx, cfg, false));
    return f;
}

void WledConfigCheck::checkNow()
{
    QSet<QString> ips;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (isWled(fx))
            ips.insert(fx->ipAddress());
    }

    // Forget what is no longer in the show.
    foreach (const QString &ip, m_cfg.keys())
    {
        if (!ips.contains(ip))
            m_cfg.remove(ip);
    }

    foreach (const QString &ip, ips)
    {
        QNetworkRequest req(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
        req.setTransferTimeout(4000);
        QNetworkReply *r = m_nam->get(req);
        connect(r, &QNetworkReply::finished, this, [this, r, ip]() { replied(ip, r); });
    }
    if (ips.isEmpty() && !m_findings.isEmpty())
    {
        m_findings.clear();
        publish();
        emit changed();
    }
}

void WledConfigCheck::replied(const QString &ip, QNetworkReply *reply)
{
    reply->deleteLater();
    const bool ok = reply->error() == QNetworkReply::NoError;

    /* A failed reply is not read: it may already be closed, and reading it
       logged "QIODevice::read (QNetworkReplyHttpImpl): device not open" for
       every unreachable WLED every 30 s. And said once when a device stops
       answering, and once when it is back -- not on every check. */
    if (!ok && m_answered.value(ip, true))
        qDebug() << "WLED" << ip << "does not answer:" << reply->errorString();
    else if (ok && !m_answered.value(ip, true))
        qDebug() << "WLED" << ip << "answers again";

    receive(ip, ok ? reply->readAll() : QByteArray(), ok);
}

void WledConfigCheck::receive(const QString &ip, const QByteArray &body, bool ok)
{
    m_answered[ip] = ok;
    if (ok)
    {
        const QJsonObject cfg = QJsonDocument::fromJson(body).object();
        if (!cfg.isEmpty())
            m_cfg[ip] = cfg;
    }

    // Every fixture at that address, against what it said (a silent one is
    // the WLED output's to report, not a setting to fix).
    QHash<quint32, Finding> now = m_findings;
    foreach (Fixture *fx, m_doc->fixtures())
    {
        if (!isWled(fx) || fx->ipAddress() != ip)
            continue;
        if (!m_cfg.contains(ip))
        {
            now.remove(fx->id());
            continue;
        }
        const Finding f = evaluate(m_doc, fx, m_cfg.value(ip));
        if (f.problems.isEmpty())
            now.remove(fx->id());
        else
            now.insert(fx->id(), f);
    }
    // Fixtures gone from the show.
    foreach (quint32 fid, now.keys())
    {
        if (m_doc->fixture(fid) == nullptr)
            now.remove(fid);
    }

    const QList<quint32> nowKeys = now.keys(), oldKeys = m_findings.keys();
    bool differs = QSet<quint32>(nowKeys.begin(), nowKeys.end())
                   != QSet<quint32>(oldKeys.begin(), oldKeys.end());
    if (!differs)
    {
        foreach (quint32 fid, now.keys())
            differs = differs || now.value(fid).problems != m_findings.value(fid).problems;
    }
    m_findings = now;
    if (differs)
    {
        publish();
        emit changed();
    }
}

void WledConfigCheck::publish()
{
    ShowStatus *status = ShowStatus::instance();
    QStringList items;
    QVariantList ids;
    foreach (const Finding &f, findings())
    {
        const Fixture *fx = m_doc->fixture(f.fixtureId);
        if (fx == nullptr || f.problems.isEmpty())
            continue;
        items << QStringLiteral("%1 — %2").arg(fx->name(), f.problems.join(QStringLiteral("; ")));
        ids << f.fixtureId;
    }
    if (items.isEmpty())
        status->clearStatus("wled.config");
    else
        status->setStatus("wled.config", ShowStatus::Warning,
            tr("%n WLED(s) not set up as this show drives them", "", items.count()),
            tr("Read from each device's own settings. Fix on Device shows what it would "
               "change, then writes it."),
            items, ids);
}

QList<WledConfigCheck::Finding> WledConfigCheck::findings() const
{
    QList<Finding> out = m_findings.values();
    std::sort(out.begin(), out.end(), [this](const Finding &a, const Finding &b) {
        const Fixture *fa = m_doc->fixture(a.fixtureId), *fb = m_doc->fixture(b.fixtureId);
        return (fa ? fa->name() : QString()) < (fb ? fb->name() : QString());
    });
    return out;
}

WledConfigCheck::Finding WledConfigCheck::finding(quint32 fixtureId) const
{
    return m_findings.value(fixtureId);
}

void WledConfigCheck::fix(const QList<quint32> &fixtureIds)
{
    QSet<QString> written;
    foreach (quint32 fid, fixtureIds)
    {
        const Finding f = m_findings.value(fid);
        if (f.fix.isEmpty() || written.contains(f.ip))
            continue;
        written.insert(f.ip);
        const QString held = WledDeviceWrite::heldReason(m_doc, f.ip);
        if (!held.isEmpty())
        {
            noteWrite(f.ip, tr("Nothing was written to %1: %2").arg(WledDeviceWrite::deviceName(f.ip), held), false);
            continue;
        }
        QNetworkRequest req(QUrl(baseUrl(f.ip) + QStringLiteral("/json/cfg")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setTransferTimeout(4000);
        QNetworkReply *r = m_nam->post(req, QJsonDocument(f.fix).toJson(QJsonDocument::Compact));
        const QString ip = f.ip;
        connect(r, &QNetworkReply::finished, this, [this, r, ip]() {
            r->deleteLater();
            // Read it back: what it says now is what counts.
            QNetworkRequest back(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
            back.setTransferTimeout(4000);
            QNetworkReply *b = m_nam->get(back);
            connect(b, &QNetworkReply::finished, this, [this, b, ip]() { replied(ip, b); });
        });
    }
}

bool WledConfigCheck::confirmFix(QWidget *parent, const QList<quint32> &fixtureIds)
{
    QStringList lines, refusals;
    QList<quint32> doable, counts;
    QSet<QString> countIps;
    foreach (quint32 fid, fixtureIds)
    {
        const Finding f = m_findings.value(fid);
        const Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            continue;
        if (!f.ledFix.isEmpty() && !countIps.contains(f.ip))
        {
            countIps.insert(f.ip);
            counts << fid;
        }
        else if (!f.ledRefusal.isEmpty())
            refusals << QStringLiteral("<b>%1</b> (%2): %3").arg(fx->name().toHtmlEscaped(),
                                                               f.ip.toHtmlEscaped(),
                                                               f.ledRefusal.toHtmlEscaped());
        if (f.fix.isEmpty())
            continue;
        doable << fid;
        QStringList changes;
        foreach (const QString &c, f.changes)
            changes << c.toHtmlEscaped();
        lines << QStringLiteral("<b>%1</b> (%2)<br>&nbsp;&nbsp;%3")
                     .arg(fx->name().toHtmlEscaped(), f.ip.toHtmlEscaped(),
                          changes.join(QStringLiteral("<br>&nbsp;&nbsp;")));
    }
    auto tellRefusals = [&]() {
        if (refusals.isEmpty())
            return;
        QMessageBox box(QMessageBox::Information, tr("Fix on Device"),
                        tr("The LED count can't be set from here:<br><br>%1")
                            .arg(refusals.join(QStringLiteral("<br><br>"))),
                        QMessageBox::Ok, parent);
        box.setObjectName(QStringLiteral("wledLedCountRefused"));
        box.setTextFormat(Qt::RichText);
        box.exec();
    };
    if (doable.isEmpty() && counts.isEmpty())
    {
        tellRefusals();
        return false;
    }

    // A Teaching show holding real output writes nothing to a device either.
    foreach (quint32 fid, doable + counts)
    {
        const QString ip = m_findings.value(fid).ip;
        const QString held = WledDeviceWrite::heldReason(m_doc, ip);
        if (held.isEmpty())
            continue;
        const QString text = tr("Nothing was written to %1: %2").arg(WledDeviceWrite::deviceName(ip), held);
        noteWrite(ip, text, false);
        QMessageBox box(QMessageBox::Information, tr("Fix on Device"), text, QMessageBox::Ok, parent);
        box.setObjectName(QStringLiteral("wledWriteHeld"));
        box.exec();
        return false;
    }

    bool wrote = false;
    if (!doable.isEmpty())
    {
        QMessageBox box(QMessageBox::Question, tr("Fix on Device"),
                        tr("Change these settings on the device%1 itself, so it takes what this show "
                           "sends?<br><br>%2").arg(doable.size() > 1 ? tr("s") : QString(),
                                                   lines.join(QStringLiteral("<br><br>"))),
                        QMessageBox::Yes | QMessageBox::Cancel, parent);
        box.setTextFormat(Qt::RichText);
        box.setDefaultButton(QMessageBox::Yes);
        if (box.exec() == QMessageBox::Yes)
        {
            fix(doable);
            wrote = true;
        }
    }
    // The LED count: asked on its own, with what a wrong one does.
    foreach (quint32 fid, counts)
    {
        const Finding f = m_findings.value(fid);
        const Fixture *fx = m_doc->fixture(fid);
        if (fx != nullptr && WledDeviceWrite::confirmLedCount(parent, fx->name(), f.ip,
                                                              f.deviceLeds, f.fixtureLeds))
            wrote = setLedCount(fid) || wrote;
    }
    tellRefusals();
    return wrote;
}

bool WledConfigCheck::setLedCount(quint32 fixtureId)
{
    const Finding f = m_findings.value(fixtureId);
    if (f.ledFix.isEmpty())
        return false;
    const QString ip = f.ip;
    if (m_writes.value(ip).busy)
        return false;
    const QString held = WledDeviceWrite::heldReason(m_doc, ip);
    if (!held.isEmpty())
    {
        noteWrite(ip, tr("Nothing was written to %1: %2").arg(WledDeviceWrite::deviceName(ip), held), false);
        return false;
    }
    WledDeviceWrite *w = new WledDeviceWrite(this);
    w->setWaits(m_ledWaits[0], m_ledWaits[1], m_ledWaits[2]);
    connect(w, &WledDeviceWrite::progress, this, [this, ip](const QString &text) {
        if (!text.isEmpty())
            noteWrite(ip, text, true);
    });
    connect(w, &WledDeviceWrite::finished, this, [this, w, ip](bool, const QString &message) {
        noteWrite(ip, message, false);
        w->deleteLater();
        // Read its settings back: the finding goes when it matches.
        QNetworkRequest back(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
        back.setTransferTimeout(4000);
        QNetworkReply *b = m_nam->get(back);
        connect(b, &QNetworkReply::finished, this, [this, b, ip]() { replied(ip, b); });
    });
    w->setLedCount(ip, f.ledFix, f.fixtureLeds);
    return true;
}

void WledConfigCheck::noteWrite(const QString &ip, const QString &text, bool busy)
{
    Write w;
    w.text = text;
    w.busy = busy;
    w.at = QDateTime::currentMSecsSinceEpoch();
    m_writes[ip] = w;
    emit writeChanged(ip);
}

QString WledConfigCheck::writeStatus(const QString &ip, bool *busy) const
{
    const Write w = m_writes.value(ip);
    // A result is news for two minutes; then there is nothing to say.
    const bool stale = !w.busy && QDateTime::currentMSecsSinceEpoch() - w.at > 120000;
    if (busy != nullptr)
        *busy = w.busy;
    return stale ? QString() : w.text;
}

void WledConfigCheck::setLedWaits(int applyMs, int rebootMs, int pollMs)
{
    m_ledWaits[0] = applyMs;
    m_ledWaits[1] = rebootMs;
    m_ledWaits[2] = pollMs;
}

/*********************************************************************
 * Set up: what the show keeps on every WLED it drives
 *********************************************************************/

int WledConfigCheck::limiterMilliamps(Doc *doc, const Fixture *fx, double *volts)
{
    if (fx == nullptr || fx->powerProfile().maxAmps <= 0.0 || doc == nullptr)
        return 0;
    /* WLED's limit is the current of its LED supply; a Max draw is read at
       the wall. Without the supply's voltage nothing is worked out. */
    const double supply = doc->monitorProperties() != nullptr
                        ? double(doc->monitorProperties()->fixtureRigProps(fx->id()).ledSupplyVolts) : 0.0;
    if (supply <= 0.0)
        return 0;
    // Its circuit's voltage, else its source's, else 120 V (as the power figure).
    double v = 120.0;
    if (doc->powerDistribution() != nullptr)
    {
        const PowerDistribution *pd = doc->powerDistribution();
        int s = -1, c = -1;
        pd->circuitOf(fx->id(), s, c);
        if (s >= 0 && c >= 0)
        {
            const double cv = pd->sources().at(s).circuits.at(c).effectiveVoltage(pd->sources().at(s).voltage);
            if (cv > 0.0)
                v = cv;
        }
    }
    if (volts != nullptr)
        *volts = v;
    return int(qBound(1.0, fx->powerProfile().maxAmps * v / supply * 1000.0, 65000.0) + 0.5);
}

WledConfigCheck::Finding WledConfigCheck::setUpFor(Doc *doc, const Fixture *fx, const QJsonObject &cfg,
                                                    bool setUp)
{
    Finding f;
    if (fx == nullptr)
        return f;
    f.fixtureId = fx->id();
    f.ip = fx->ipAddress();

    // Its name, and its name on the network (staff-l.local).
    if (cfg.value(QStringLiteral("id")).isObject())
    {
        const QJsonObject id = cfg.value(QStringLiteral("id")).toObject();
        const QString name = id.value(QStringLiteral("name")).toString();
        const QString host = id.value(QStringLiteral("mdns")).toString();
        const QString wantHost = WledDiscovery::hostName(fx->name());
        QJsonObject want;
        if (name != fx->name())
        {
            f.problems << tr("is called \"%1\" on the device, \"%2\" in this show").arg(name, fx->name());
            want[QStringLiteral("name")] = fx->name();
            f.changes << tr("Name: %1").arg(fx->name());
        }
        if (!wantHost.isEmpty() && host != wantHost && (setUp || name != fx->name()))
        {
            want[QStringLiteral("mdns")] = wantHost;
            f.changes << tr("Network name: %1.local").arg(wantHost);
        }
        if (!want.isEmpty())
            f.fix[QStringLiteral("id")] = want;
    }

    /* WLED sync: a WLED takes brightness, colour, effect and palette from
       the others in its sync group, out of the box. On stage one WLED's
       button or app would change the rest; the show drives each itself. */
    const QJsonObject sync = cfg.value(QStringLiteral("if")).toObject().value(QStringLiteral("sync")).toObject();
    QJsonObject wantSync;
    if (sync.value(QStringLiteral("recv")).isObject())
    {
        const QJsonObject recv = sync.value(QStringLiteral("recv")).toObject();
        const QList<QPair<QString, QString> > kinds = {
            { QStringLiteral("bri"), tr("brightness") }, { QStringLiteral("col"), tr("colour") },
            { QStringLiteral("fx"), tr("effect") }, { QStringLiteral("pal"), tr("palette") } };
        QStringList takes;
        QJsonObject wantRecv;
        for (const auto &k : kinds)
            if (recv.value(k.first).toBool())
            {
                takes << k.second;
                wantRecv[k.first] = false;
            }
        if (!takes.isEmpty())
        {
            f.problems << tr("takes its %1 from other WLEDs (sync group %2): one WLED's button or "
                             "app would change it").arg(takes.join(QStringLiteral(", ")))
                                                    .arg(recv.value(QStringLiteral("grp")).toInt());
            wantSync[QStringLiteral("recv")] = wantRecv;
            f.changes << tr("Sync: takes nothing from other WLEDs");
        }
    }
    if (sync.value(QStringLiteral("send")).toObject().value(QStringLiteral("en")).toBool())
    {
        f.problems << tr("sends its own changes to other WLEDs");
        wantSync[QStringLiteral("send")] = QJsonObject{ { QStringLiteral("en"), false } };
        f.changes << tr("Sync: sends nothing to other WLEDs");
    }
    if (!wantSync.isEmpty())
        f.fix[QStringLiteral("if")] = QJsonObject{ { QStringLiteral("sync"), wantSync } };

    // At power-on: dark until the show drives it.
    const QJsonObject def = cfg.value(QStringLiteral("def")).toObject();
    if (def.value(QStringLiteral("on")).toBool())
    {
        const int bri = def.value(QStringLiteral("bri")).toInt();
        f.problems << tr("lights up when it powers on (%1 %), before the show drives it")
                          .arg(qRound(bri * 100.0 / 255.0));
        f.fix[QStringLiteral("def")] = QJsonObject{ { QStringLiteral("on"), false } };
        f.changes << tr("Power-on: dark until the show drives it");
    }

    /* The brightness limit, from the fixture's Max draw and its LED supply.
       All of hw.led goes back as the device said it (WLED rebuilds its
       outputs from what is sent), only the limits changed. */
    const QJsonObject hwLed = cfg.value(QStringLiteral("hw")).toObject().value(QStringLiteral("led")).toObject();
    const int ma = limiterMilliamps(doc, fx);
    if (hwLed.contains(QStringLiteral("maxpwr")) && fx->powerProfile().maxAmps > 0.0 && ma <= 0)
        f.notes << tr("Brightness limit: not set from the show (LED supply voltage unknown); the "
                      "WLED's own is %1 mA").arg(hwLed.value(QStringLiteral("maxpwr")).toInt());
    if (ma > 0 && hwLed.contains(QStringLiteral("maxpwr")))
    {
        const int have = hwLed.value(QStringLiteral("maxpwr")).toInt();
        QJsonArray ins = hwLed.value(QStringLiteral("ins")).toArray();
        const bool perOutput = ins.size() == 1
                               && ins.first().toObject().contains(QStringLiteral("maxpwr"))
                               && ins.first().toObject().value(QStringLiteral("maxpwr")).toInt() != ma;
        if (have != ma || perOutput)
        {
            f.problems << tr("its brightness limit is %1 mA; this fixture's Max draw is %2 mA of LEDs")
                              .arg(have).arg(ma);
            QJsonObject led = hwLed;
            led[QStringLiteral("maxpwr")] = ma;
            if (ins.size() == 1)
            {
                QJsonObject out = ins.first().toObject();
                out[QStringLiteral("maxpwr")] = ma;
                ins[0] = out;
                led[QStringLiteral("ins")] = ins;
            }
            else
                led.remove(QStringLiteral("ins"));     // several: the total only
            f.fix[QStringLiteral("hw")] = QJsonObject{ { QStringLiteral("led"), led } };
            f.changes << tr("Brightness limit: %1 mA").arg(ma);
        }
    }
    return f;
}

bool WledConfigCheck::confirmSetUp(QWidget *parent, const QList<SetUpItem> &items)
{
    if (items.isEmpty())
        return false;
    QString text = items.size() == 1
        ? tr("Set up “%1” (%2) the way this show drives it?")
              .arg(items.first().fixtureName.toHtmlEscaped(),
                   WledDeviceWrite::deviceName(items.first().ip).toHtmlEscaped())
        : tr("Set up these %1 WLEDs the way this show drives them?").arg(items.size());
    foreach (const SetUpItem &item, items)
    {
        text += QStringLiteral("<br><br>");
        if (items.size() > 1)
            text += QStringLiteral("<b>%1</b> (%2)<br>").arg(item.fixtureName.toHtmlEscaped(),
                                                            WledDeviceWrite::deviceName(item.ip).toHtmlEscaped());
        QStringList lines;
        foreach (const QString &c, item.changes)
            lines << QStringLiteral("&nbsp;&nbsp;• ") + c.toHtmlEscaped();
        foreach (const QString &n, item.notes)
            lines << QStringLiteral("&nbsp;&nbsp;<i>%1</i>").arg(n.toHtmlEscaped());
        text += lines.join(QStringLiteral("<br>"));
    }
    text += QStringLiteral("<br><br>")
            + tr("Wi-Fi, LED type, pins and colour order stay as they are. This changes the "
                 "device itself; Undo can't take it back.");
    QMessageBox box(QMessageBox::Question, tr("Set Up WLED"), text,
                    QMessageBox::Yes | QMessageBox::Cancel, parent);
    box.setObjectName(QStringLiteral("wledSetUpConfirm"));
    box.setTextFormat(Qt::RichText);
    box.button(QMessageBox::Yes)->setText(items.size() == 1 ? tr("Set Up Device")
                                                            : tr("Set Up %1 Devices").arg(items.size()));
    box.button(QMessageBox::Cancel)->setText(tr("Not Now"));
    box.setDefaultButton(QMessageBox::Yes);
    return box.exec() == QMessageBox::Yes;
}

void WledConfigCheck::offerSetUp(QWidget *parent, quint32 fixtureId)
{
    offerSetUp(parent, QList<quint32>{ fixtureId });
}

void WledConfigCheck::offerSetUp(QWidget *parent, const QList<quint32> &fixtureIds)
{
    /* Every device read first; then one question for all of them. */
    struct Round
    {
        QPointer<QWidget> over;
        QList<quint32> ids;                 // in order, one per device
        QHash<quint32, QJsonObject> cfg;    // the ones that answered
        int pending = 0;
    };
    auto round = std::make_shared<Round>();
    round->over = parent;
    QSet<QString> ips;
    foreach (quint32 fid, fixtureIds)
    {
        const Fixture *fx = m_doc->fixture(fid);
        if (!isWled(fx) || ips.contains(fx->ipAddress()))
            continue;
        const QString ip = fx->ipAddress();
        ips.insert(ip);
        const QString held = WledDeviceWrite::heldReason(m_doc, ip);
        if (!held.isEmpty())
        {
            noteWrite(ip, tr("Nothing was written to %1: %2").arg(WledDeviceWrite::deviceName(ip), held), false);
            continue;
        }
        round->ids << fid;
    }
    round->pending = round->ids.size();

    auto ask = [this, round]() {
        QList<SetUpItem> items;
        QList<QPair<QString, QJsonObject> > writes;
        foreach (quint32 fid, round->ids)
        {
            const Fixture *fx = m_doc->fixture(fid);
            if (fx == nullptr || !round->cfg.contains(fid))
                continue;
            const QString ip = fx->ipAddress();
            const Finding s = setUpFor(m_doc, fx, round->cfg.value(fid), true);
            if (s.fix.isEmpty())
            {
                noteWrite(ip, tr("%1 is set up the way this show drives it.")
                                  .arg(WledDeviceWrite::deviceName(ip)), false);
                continue;
            }
            // Read, not written: an obvious mismatch is said, never changed here.
            SetUpItem item;
            item.fixtureName = fx->name();
            item.ip = ip;
            item.changes = s.changes;
            item.notes = s.notes;
            const Finding all = finding(fid);
            if (all.fixtureLeds > 0 && all.deviceLeds > 0)
                item.notes << tr("It drives %1 LEDs, this fixture has %2: Fix on Device sets the count.")
                                  .arg(all.deviceLeds).arg(all.fixtureLeds);
            items << item;
            writes << qMakePair(ip, s.fix);
        }
        if (items.isEmpty() || !confirmSetUp(round->over.data(), items))
            return;
        for (const auto &w : std::as_const(writes))
            writeSetUp(w.first, w.second);
    };

    foreach (quint32 fid, round->ids)
    {
        const QString ip = m_doc->fixture(fid)->ipAddress();
        QNetworkRequest req(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
        req.setTransferTimeout(4000);
        QNetworkReply *r = m_nam->get(req);
        connect(r, &QNetworkReply::finished, this, [this, r, ip, fid, round, ask]() {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError)
                noteWrite(ip, tr("%1 did not answer, so it was not set up. Fix on Device offers it "
                                 "once it answers.").arg(WledDeviceWrite::deviceName(ip)), false);
            else
            {
                const QByteArray body = r->readAll();
                receive(ip, body, true);
                const QJsonObject cfg = QJsonDocument::fromJson(body).object();
                if (!cfg.isEmpty())
                    round->cfg.insert(fid, cfg);
            }
            if (--round->pending == 0)
                ask();
        });
    }
}

void WledConfigCheck::writeSetUp(const QString &ip, const QJsonObject &fix)
{
    noteWrite(ip, tr("Setting up %1…").arg(WledDeviceWrite::deviceName(ip)), true);
    QNetworkRequest post(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
    post.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    post.setTransferTimeout(4000);
    QNetworkReply *p = m_nam->post(post, QJsonDocument(fix).toJson(QJsonDocument::Compact));
    connect(p, &QNetworkReply::finished, this, [this, p, ip, fix]() {
        p->deleteLater();
        if (p->error() != QNetworkReply::NoError)
        {
            noteWrite(ip, tr("%1 did not take it: %2").arg(WledDeviceWrite::deviceName(ip), p->errorString()), false);
            return;
        }
        // Read it back: what it says now is what counts.
        QNetworkRequest back(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
        back.setTransferTimeout(4000);
        QNetworkReply *b = m_nam->get(back);
        connect(b, &QNetworkReply::finished, this, [this, b, ip, fix]() {
            b->deleteLater();
            if (b->error() != QNetworkReply::NoError)
            {
                noteWrite(ip, tr("Written to %1, but it could not be read back.")
                                  .arg(WledDeviceWrite::deviceName(ip)), false);
                return;
            }
            const QByteArray now = b->readAll();
            const QStringList differ = jsonDiffers(fix, QJsonDocument::fromJson(now).object());
            noteWrite(ip, differ.isEmpty()
                ? tr("Set up %1 the way this show drives it.").arg(WledDeviceWrite::deviceName(ip))
                : tr("Wrote to %1, but it reads back otherwise: %2")
                      .arg(WledDeviceWrite::deviceName(ip), differ.join(QStringLiteral(", "))), false);
            receive(ip, now, true);
        });
    });
}
