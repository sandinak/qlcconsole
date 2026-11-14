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
#include "patchundo.h"
#include "monitor.h"
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

void WledConfigCheck::noteWrite(const QString &ip, const QString &text, bool busy, bool failed)
{
    Write w;
    w.text = text;
    w.busy = busy;
    w.failed = failed;
    w.at = QDateTime::currentMSecsSinceEpoch();
    m_writes[ip] = w;
    emit writeChanged(ip);
    publishWrites();
    if (!busy)      // its line leaves the footer once it is old news
        QTimer::singleShot(121000, this, &WledConfigCheck::publishWrites);
}

void WledConfigCheck::publishWrites()
{
    /* What setting WLEDs up (and writing to them) did lately, in the
       footer: wherever the patch was made -- Connections, Add Fixture,
       Patch, the Inventory -- the result is said in one place too. */
    ShowStatus *status = ShowStatus::instance();
    QStringList items;
    bool failed = false;
    QStringList ips = m_writes.keys();
    ips.sort();
    foreach (const QString &ip, ips)
    {
        bool busy = false;
        const QString said = writeStatus(ip, &busy);
        if (said.isEmpty() || busy)
            continue;
        const bool bad = m_writes.value(ip).failed;
        failed = failed || bad;
        items << (bad ? QStringLiteral("⚠ ") : QString()) + said;
    }
    if (items.isEmpty())
        status->clearStatus("wled.setup");
    else
        status->setStatus("wled.setup", failed ? ShowStatus::Warning : ShowStatus::Ok,
            failed ? tr("A WLED could not be set up") : tr("WLEDs set up for this show"),
            tr("What setting each WLED up did when it was patched (its name, no sync, dark at "
               "power-on, no streams taking it over)."),
            items);
}

bool WledConfigCheck::writeFailed(const QString &ip) const
{
    const Write w = m_writes.value(ip);
    return w.failed && !writeStatus(ip).isEmpty();
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
    /* No fixture (a WLED a universe sends to with none patched to it): the
       settings that need none -- sync, power-on -- and not its name or its
       limit, which come from the fixture. */
    if (fx != nullptr)
    {
        f.fixtureId = fx->id();
        f.ip = fx->ipAddress();
    }

    // Its name, and its name on the network (staff-l.local).
    if (fx != nullptr && cfg.value(QStringLiteral("id")).isObject())
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
    if (fx == nullptr)
        return f;
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

namespace
{
/* @p add laid over @p base, objects merged key by key (what WLED does with a
   partial /json/cfg it is sent). */
QJsonObject mergedJson(QJsonObject base, const QJsonObject &add)
{
    for (auto it = add.constBegin(); it != add.constEnd(); ++it)
    {
        if (it.value().isObject() && base.value(it.key()).isObject())
            base[it.key()] = mergedJson(base.value(it.key()).toObject(), it.value().toObject());
        else
            base[it.key()] = it.value();
    }
    return base;
}
}

QStringList WledConfigCheck::wledIps(Doc *doc, quint32 universe)
{
    QStringList ips;
    InputOutputMap *io = doc != nullptr ? doc->inputOutputMap() : nullptr;
    if (io == nullptr || universe >= io->universesCount())
        return ips;
    for (int i = 0; i < io->outputPatchesCount(universe); i++)
    {
        OutputPatch *op = io->outputPatch(universe, i);
        if (op == nullptr || op->pluginName() != QLatin1String("WLED"))
            continue;
        const QMap<QString, QVariant> pp = op->getPluginParameters();
        for (auto it = pp.constBegin(); it != pp.constEnd(); ++it)
            if (it.key().startsWith(QLatin1String("outputIP")) && !it.value().toString().isEmpty()
                && !ips.contains(it.value().toString()))
                ips << it.value().toString();
    }
    ips.sort();
    return ips;
}

void WledConfigCheck::setUpUniverse(quint32 universe)
{
    m_suggestions.remove(universe);
    const QList<quint32> ids = fixturesSentTo(m_doc, universe);
    QSet<QString> covered;
    foreach (quint32 fid, ids)
        if (const Fixture *fx = m_doc->fixture(fid))
            covered.insert(fx->ipAddress());
    QStringList uncovered;
    foreach (const QString &ip, wledIps(m_doc, universe))
        if (!covered.contains(ip))
            uncovered << ip;
    setUpNow(ids);

    /* A device IP no fixture on the universe has: the same device written
       down twice with two addresses (its fixture keeps an old one), another
       device, or one with no fixture at all. Its MAC says which. */
    foreach (const QString &ip, uncovered)
    {
        const QString held = WledDeviceWrite::heldReason(m_doc, ip);
        if (!held.isEmpty())
        {
            noteWrite(ip, tr("%1 was not set up: %2").arg(WledDeviceWrite::deviceName(ip), held), false, true);
            continue;
        }
        QString mac;
        if (WledDiscovery *disc = WledDiscovery::instance())
        {
            foreach (const WledDiscovery::Device &d, disc->devices())
            {
                if (d.ip == ip && !d.mac.isEmpty())
                    mac = d.mac;
            }
        }
        if (!mac.isEmpty())
        {
            resolveDevice(universe, ip, mac, true, uncovered.size());
            continue;
        }
        noteWrite(ip, tr("Asking %1 which WLED it is…").arg(WledDeviceWrite::deviceName(ip)), true);
        QNetworkRequest req(QUrl(baseUrl(ip) + QStringLiteral("/json/info")));
        req.setTransferTimeout(4000);
        QNetworkReply *r = m_nam->get(req);
        const int count = uncovered.size();
        connect(r, &QNetworkReply::finished, this, [this, r, ip, universe, count]() {
            r->deleteLater();
            const bool ok = r->error() == QNetworkReply::NoError;
            const QString said = ok ? QJsonDocument::fromJson(r->readAll()).object()
                                          .value(QStringLiteral("mac")).toString()
                                    : QString();
            resolveDevice(universe, ip, said, ok, count);
        });
    }
}

namespace
{
QString macKey(const QString &mac)
{
    QString out;
    for (const QChar c : mac)
        if (c.isLetterOrNumber())
            out += c.toLower();
    return out;
}

QString macShown(const QString &mac)
{
    const QString k = macKey(mac);
    if (k.size() != 12)
        return mac;
    QStringList pairs;
    for (int i = 0; i < 12; i += 2)
        pairs << k.mid(i, 2);
    return pairs.join(QLatin1Char(':'));
}
}

void WledConfigCheck::resolveDevice(quint32 universe, const QString &ip, const QString &mac,
                                    bool answered, int deviceIps)
{
    InputOutputMap *io = m_doc->inputOutputMap();
    const QString uniName = io->getUniverseNameByID(universe).isEmpty()
        ? tr("Universe %1").arg(universe + 1) : io->getUniverseNameByID(universe);
    const QStringList ips = wledIps(m_doc, universe);
    // The WLED fixtures on it at none of its device IPs: where a stale one is.
    QList<Fixture *> others;
    foreach (Fixture *fx, m_doc->fixtures())
        if (isWled(fx) && fx->transport() == Fixture::WledTransport && fx->isPatched()
            && fx->universe() == universe && !ips.contains(fx->ipAddress()))
            others << fx;

    // 1. Its fixture, by MAC, at an old address: the same device. Follow it.
    if (!mac.isEmpty())
    {
        foreach (Fixture *fx, others)
        {
            if (macKey(fx->macAddress()) != macKey(mac))
                continue;
            const QString was = fx->ipAddress();
            const quint32 fid = fx->id();
            fx->setIPAddress(ip);
            Doc *doc = m_doc;
            if (Monitor *mon = Monitor::instance())
                mon->pushUndoCallback([doc, fid, was]() {
                    if (Fixture *f = doc->fixture(fid))
                    {
                        f->setIPAddress(was);
                        f->setID(f->id());          // changed: every view of it
                    }
                });
            fx->setID(fx->id());
            m_doc->setModified();
            setUpNow({ fid }, tr("%1 is this device (%2): its address was %3, now %4. ")
                                  .arg(fx->name(), macShown(mac), was, ip));
            return;
        }
    }

    if (!answered)
    {
        /* Not answering. With one fixture and one device IP that differ,
           name both: one of them is out of date -- and when the fixture's
           own address answers as that fixture, offer to send there. */
        if (others.size() == 1 && deviceIps == 1)
        {
            Fixture *fx = others.first();
            const QString fip = fx->ipAddress();
            const QString fname = fx->name();
            const QString fmac = fx->macAddress();
            noteWrite(ip, tr("%1 sends to %2 but its WLED fixture %3 is at %4: one of them is out "
                             "of date.").arg(uniName, ip, fname, fip), false, true);
            QNetworkRequest req(QUrl(baseUrl(fip) + QStringLiteral("/json/info")));
            req.setTransferTimeout(4000);
            QNetworkReply *r = m_nam->get(req);
            connect(r, &QNetworkReply::finished, this, [this, r, ip, fip, fname, fmac, uniName, universe]() {
                r->deleteLater();
                if (r->error() != QNetworkReply::NoError)
                    return;
                const QString there = QJsonDocument::fromJson(r->readAll()).object()
                                          .value(QStringLiteral("mac")).toString();
                if (!fmac.isEmpty() && macKey(there) != macKey(fmac))
                    return;
                m_suggestions[universe] = fip;
                noteWrite(ip, tr("%1 sends to %2, where nothing answers; %3%4 answers at %5. Send "
                                 "there instead?")
                                  .arg(uniName, ip, fname,
                                       there.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(macShown(there)),
                                       fip), false, true);
            });
            return;
        }
        noteWrite(ip, tr("%1 was not set up: it did not answer. It is set up when you set the "
                         "universe's WLED devices again, or with Fix on Device once it answers.")
                          .arg(WledDeviceWrite::deviceName(ip)), false, true);
        return;
    }

    // Answering, but not the WLED its fixture is.
    if (!others.isEmpty())
    {
        Fixture *fx = others.first();
        noteWrite(ip, tr("%1 sends to %2, a WLED%3 that is not its fixture %4 (%5%6): one of them is "
                         "out of date.")
                          .arg(uniName, ip, mac.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(macShown(mac)),
                               fx->name(), fx->ipAddress(),
                               fx->macAddress().isEmpty() ? QString()
                                   : QStringLiteral(", %1").arg(macShown(fx->macAddress()))),
                  false, true);
        return;
    }

    /* 4. No WLED fixture on the universe at all: set up what needs none --
       no sync in or out, dark at power-on, no streams taking it over -- and
       say what was left (its name, its limit). Not trouble: the ordinary
       colour. */
    noteWrite(ip, tr("Setting up %1…").arg(WledDeviceWrite::deviceName(ip)), true);
    QNetworkRequest req(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
    req.setTransferTimeout(4000);
    QNetworkReply *r = m_nam->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, ip]() {
        r->deleteLater();
        const QString name = WledDeviceWrite::deviceName(ip);
        if (r->error() != QNetworkReply::NoError)
        {
            noteWrite(ip, tr("%1 was not set up: it did not answer (%2).").arg(name, r->errorString()),
                      false, true);
            return;
        }
        const QJsonObject cfg = QJsonDocument::fromJson(r->readAll()).object();
        Finding f = setUpFor(m_doc, nullptr, cfg, true);
        const QJsonObject live = cfg.value(QStringLiteral("if")).toObject()
                                    .value(QStringLiteral("live")).toObject();
        if (live.value(QStringLiteral("en")).toBool())
        {
            f.fix = mergedJson(f.fix, QJsonObject{ { QStringLiteral("if"), QJsonObject{
                { QStringLiteral("live"), QJsonObject{ { QStringLiteral("en"), false } } } } } });
            f.changes << tr("Receive realtime streams: on → off");
        }
        const QString without = tr("no WLED fixture is patched to it, so its name and brightness "
                                   "limit are left as they are");
        if (f.fix.isEmpty())
        {
            noteWrite(ip, tr("%1 is set up without a name: %2.").arg(name, without), false);
            return;
        }
        writeSetUp(ip, f.fix, tr("Set up %1 without a name (%2): %3.")
                                  .arg(name, without, f.changes.join(QStringLiteral("; "))));
    });
}

QString WledConfigCheck::suggestedIp(quint32 universe) const
{
    return m_suggestions.value(universe);
}

bool WledConfigCheck::sendThere(quint32 universe)
{
    const QString to = m_suggestions.value(universe);
    InputOutputMap *io = m_doc->inputOutputMap();
    if (to.isEmpty() || io == nullptr || universe >= io->universesCount())
        return false;
    OutputPatch *op = nullptr;
    for (int i = 0; i < io->outputPatchesCount(universe) && op == nullptr; i++)
        if (io->outputPatch(universe, i) != nullptr
            && io->outputPatch(universe, i)->pluginName() == QLatin1String("WLED"))
            op = io->outputPatch(universe, i);
    if (op == nullptr)
        return false;
    if (io->patchUndo() != nullptr)
        io->patchUndo()->capture(QList<quint32>() << universe,
                                 tr("WLED of universe %1 sent to %2").arg(universe + 1).arg(to));
    op->setPluginParameter(QStringLiteral("outputIP"), to);
    m_doc->setModified();
    setUpUniverse(universe);
    return true;
}

QList<quint32> WledConfigCheck::fixturesSentTo(Doc *doc, quint32 universe)
{
    QList<quint32> ids;
    const QStringList list = wledIps(doc, universe);
    const QSet<QString> ips(list.begin(), list.end());
    if (ips.isEmpty())
        return ids;
    foreach (Fixture *fx, doc->fixtures())
        if (isWled(fx) && fx->transport() == Fixture::WledTransport && fx->isPatched()
            && fx->universe() == universe && ips.contains(fx->ipAddress()))
            ids << fx->id();
    return ids;
}

void WledConfigCheck::setUpNow(const QList<quint32> &fixtureIds, const QString &prefix)
{
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
            noteWrite(ip, prefix + tr("%1 was not set up: %2").arg(WledDeviceWrite::deviceName(ip), held), false, true);
            continue;
        }
        noteWrite(ip, prefix + tr("Setting up %1…").arg(WledDeviceWrite::deviceName(ip)), true);
        QNetworkRequest req(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
        req.setTransferTimeout(4000);
        QNetworkReply *r = m_nam->get(req);
        connect(r, &QNetworkReply::finished, this, [this, r, ip, fid, prefix]() {
            r->deleteLater();
            if (r->error() != QNetworkReply::NoError)
            {
                noteWrite(ip, prefix + tr("%1 was not set up: it did not answer (%2). It is set up when "
                                 "you patch it again, or with Fix on Device once it answers.")
                                  .arg(WledDeviceWrite::deviceName(ip), r->errorString()), false, true);
                return;
            }
            const QByteArray body = r->readAll();
            const QJsonObject cfg = QJsonDocument::fromJson(body).object();
            const Fixture *fx = m_doc->fixture(fid);
            if (cfg.isEmpty() || fx == nullptr)
            {
                noteWrite(ip, prefix + tr("%1 was not set up: its answer is not a WLED's settings.")
                                  .arg(WledDeviceWrite::deviceName(ip)), false, true);
                return;
            }
            receive(ip, body, true);
            /* Set Up's settings and the ones Fix on Device would make for
               how this show drives it: everything in one write. */
            const Finding own = setUpFor(m_doc, fx, cfg, true);
            const Finding drive = evaluate(m_doc, fx, cfg);
            const QJsonObject fix = mergedJson(own.fix, drive.fix);
            const QString name = WledDeviceWrite::deviceName(ip);
            if (fix.isEmpty())
            {
                noteWrite(ip, prefix + tr("%1 is set up the way this show drives it.").arg(name), false);
                return;
            }
            /* evaluate() has Set Up's own findings too (its name, power-on):
               what is left is how it was set up to be driven -- taking a
               stream, another protocol, universe or address. */
            QStringList changes = own.changes, was;
            foreach (const QString &c, drive.changes)
                if (!changes.contains(c))
                    changes << c;
            foreach (const QString &p, drive.problems)
                if (!own.problems.contains(p))
                    was << p;
            const bool otherwise = changes.size() > own.changes.size();
            const QString done = otherwise
                ? tr("Reconfigured %1: it %2. Now: %3.")
                      .arg(name, was.join(QStringLiteral("; ")), changes.join(QStringLiteral("; ")))
                : tr("Set up %1 for this show: %2.").arg(name, changes.join(QStringLiteral("; ")));
            writeSetUp(ip, fix, prefix + done, prefix);
        });
    }
}

void WledConfigCheck::writeSetUp(const QString &ip, const QJsonObject &fix, const QString &done,
                                 const QString &prefix)
{
    noteWrite(ip, prefix + tr("Setting up %1…").arg(WledDeviceWrite::deviceName(ip)), true);
    QNetworkRequest post(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
    post.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    post.setTransferTimeout(4000);
    QNetworkReply *p = m_nam->post(post, QJsonDocument(fix).toJson(QJsonDocument::Compact));
    connect(p, &QNetworkReply::finished, this, [this, p, ip, fix, done, prefix]() {
        p->deleteLater();
        if (p->error() != QNetworkReply::NoError)
        {
            noteWrite(ip, prefix + tr("%1 did not take it: %2").arg(WledDeviceWrite::deviceName(ip), p->errorString()),
                      false, true);
            return;
        }
        // Read it back: what it says now is what counts.
        QNetworkRequest back(QUrl(baseUrl(ip) + QStringLiteral("/json/cfg")));
        back.setTransferTimeout(4000);
        QNetworkReply *b = m_nam->get(back);
        connect(b, &QNetworkReply::finished, this, [this, b, ip, fix, done, prefix]() {
            b->deleteLater();
            if (b->error() != QNetworkReply::NoError)
            {
                noteWrite(ip, prefix + tr("Written to %1, but it could not be read back.")
                                  .arg(WledDeviceWrite::deviceName(ip)), false);
                return;
            }
            const QByteArray now = b->readAll();
            const QStringList differ = jsonDiffers(fix, QJsonDocument::fromJson(now).object());
            if (differ.isEmpty())
                noteWrite(ip, done.isEmpty()
                    ? tr("Set up %1 the way this show drives it.").arg(WledDeviceWrite::deviceName(ip))
                    : done, false);
            else
                noteWrite(ip, prefix + tr("Wrote to %1, but it reads back otherwise: %2")
                                  .arg(WledDeviceWrite::deviceName(ip), differ.join(QStringLiteral(", "))),
                          false, true);
            receive(ip, now, true);
        });
    });
}
