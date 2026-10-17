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
#include <QMessageBox>
#include <QTimer>
#include <QSet>

#include "wledconfigcheck.h"
#include "showstatus.h"
#include "qlcfixturemode.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "outputpatch.h"
#include "fixture.h"
#include "doc.h"

WledConfigCheck *WledConfigCheck::s_instance = nullptr;

namespace
{
const int ArtNetPort = 6454;
const int E131Port = 5568;

/* WLED's own DMX modes (WLED const.h), the ones a definition here drives. */
const int DmxSingleRgb = 1;
const int DmxSingleDrgb = 2;
const int DmxEffect = 3;

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

    if (fx->transport() == Fixture::WledTransport)
    {
        /* Driven through WLED's own API: a stream arriving on its universe
           would take it over, and the look would be ignored meanwhile. */
        if (realtime)
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
        const int wantMode = modeName.startsWith(QLatin1String("Effect")) ? DmxEffect
                           : modeName.startsWith(QLatin1String("Single DRGB")) ? DmxSingleDrgb
                           : modeName.startsWith(QLatin1String("Single RGB")) ? DmxSingleRgb : -1;

        if (!realtime)
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
    }

    if (!wantDmx.isEmpty())
        wantLive[QStringLiteral("dmx")] = wantDmx;
    if (!wantLive.isEmpty())
    {
        QJsonObject wantIf;
        wantIf[QStringLiteral("live")] = wantLive;
        f.fix[QStringLiteral("if")] = wantIf;
    }
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
    QStringList lines;
    QList<quint32> doable;
    foreach (quint32 fid, fixtureIds)
    {
        const Finding f = m_findings.value(fid);
        const Fixture *fx = m_doc->fixture(fid);
        if (f.fix.isEmpty() || fx == nullptr)
            continue;
        doable << fid;
        QStringList changes;
        foreach (const QString &c, f.changes)
            changes << c.toHtmlEscaped();
        lines << QStringLiteral("<b>%1</b> (%2)<br>&nbsp;&nbsp;%3")
                     .arg(fx->name().toHtmlEscaped(), f.ip.toHtmlEscaped(),
                          changes.join(QStringLiteral("<br>&nbsp;&nbsp;")));
    }
    if (doable.isEmpty())
        return false;
    QMessageBox box(QMessageBox::Question, tr("Fix on Device"),
                    tr("Change these settings on the device%1 itself, so it takes what this show "
                       "sends?<br><br>%2").arg(doable.size() > 1 ? tr("s") : QString(),
                                               lines.join(QStringLiteral("<br><br>"))),
                    QMessageBox::Yes | QMessageBox::Cancel, parent);
    box.setTextFormat(Qt::RichText);
    box.setDefaultButton(QMessageBox::Yes);
    if (box.exec() != QMessageBox::Yes)
        return false;
    fix(doable);
    return true;
}
