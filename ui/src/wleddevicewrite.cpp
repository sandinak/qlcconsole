/*
  Q Light Controller Plus
  wleddevicewrite.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostAddress>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QUrl>

#include "wleddevicewrite.h"
#include "wledconfigcheck.h"
#include "inputoutputmap.h"
#include "pixelpath.h"
#include "doc.h"

namespace
{
const int kTimeoutMs = 4000;

/** The host part of "ip", "ip:port" or "http://ip:port". */
QString hostOf(const QString &ip)
{
    return QUrl(WledConfigCheck::baseUrl(ip)).host();
}

QJsonObject leds(const QByteArray &info)
{
    QJsonObject root = QJsonDocument::fromJson(info).object();
    if (root.value(QStringLiteral("info")).isObject())       // /json carries it under "info"
        root = root.value(QStringLiteral("info")).toObject();
    return root.value(QStringLiteral("leds")).toObject();
}
}

WledDeviceWrite::WledDeviceWrite(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_tick(new QTimer(this))
{
    m_nam->setProxy(QNetworkProxy::NoProxy);    // WLEDs are on the show LAN
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, [this]() { emit progress(busyText()); });
}

WledDeviceWrite::~WledDeviceWrite()
{
    if (m_reply != nullptr)
    {
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

QString WledDeviceWrite::heldReason(const Doc *doc, const QString &ip)
{
    if (doc == nullptr || doc->inputOutputMap() == nullptr || !doc->inputOutputMap()->teachingHold())
        return QString();
    const QHostAddress host(hostOf(ip));
    if (host.isLoopback() || hostOf(ip) == QLatin1String("localhost"))
        return QString();
    return tr("Teaching mode holds real output (File \342\226\270 Show Settings allows it)");
}

QString WledDeviceWrite::deviceName(const QString &ip)
{
    return tr("WLED (%1)").arg(ip);
}

void WledDeviceWrite::setWaits(int applyMs, int rebootMs, int pollMs)
{
    m_applyMs = applyMs;
    m_rebootMs = rebootMs;
    m_pollMs = qMax(10, pollMs);
}

QString WledDeviceWrite::busyText() const
{
    if (m_step.isEmpty())
        return QString();
    const qint64 ms = m_since.isValid() ? m_since.elapsed() : 0;
    return ms < 2000 ? m_step + QStringLiteral("…")
                     : tr("%1… %2 s").arg(m_step).arg(ms / 1000);
}

void WledDeviceWrite::step(const QString &text)
{
    if (m_step.isEmpty())
    {
        m_since.start();
        m_tick->start();
    }
    m_step = text;
    emit progress(busyText());
}

void WledDeviceWrite::finish(bool ok, const QString &message)
{
    m_step.clear();
    m_tick->stop();
    qInfo().noquote() << "[WLED write]" << m_ip << (ok ? "ok:" : "not done:") << message;
    emit finished(ok, message);
}

QNetworkReply *WledDeviceWrite::get(const QString &path, int timeoutMs, const Reply &then)
{
    QNetworkRequest req(QUrl(WledConfigCheck::baseUrl(m_ip) + path));
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *r = m_nam->get(req);
    m_reply = r;
    connect(r, &QNetworkReply::finished, this, [this, r, then]() {
        r->deleteLater();
        if (m_reply == r)
            m_reply = nullptr;
        const bool ok = r->error() == QNetworkReply::NoError;
        QString why;
        if (!ok)
            why = r->error() == QNetworkReply::OperationCanceledError || r->error() == QNetworkReply::TimeoutError
                ? tr("%1 did not answer in time.").arg(deviceName(m_ip))
                : tr("%1 did not answer: %2").arg(deviceName(m_ip), r->errorString());
        then(ok, ok ? r->readAll() : QByteArray(), why);
    });
    return r;
}

QNetworkReply *WledDeviceWrite::post(const QString &path, const QJsonObject &body, const Reply &then)
{
    QNetworkRequest req(QUrl(WledConfigCheck::baseUrl(m_ip) + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(kTimeoutMs);
    QNetworkReply *r = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = r;
    connect(r, &QNetworkReply::finished, this, [this, r, then]() {
        r->deleteLater();
        if (m_reply == r)
            m_reply = nullptr;
        const bool ok = r->error() == QNetworkReply::NoError;
        const QString why = ok ? QString()
                               : tr("%1 did not take it: %2").arg(deviceName(m_ip), r->errorString());
        then(ok, ok ? r->readAll() : QByteArray(), why);
    });
    return r;
}

/*********************************************************************
 * Write Segments
 *********************************************************************/

void WledDeviceWrite::writeSegments(const QString &ip, const QVector<SprinkleSpan> &spans,
                                    const SegmentsConfirm &confirm)
{
    if (isBusy())
        return;
    m_ip = ip;
    step(tr("Reading %1").arg(deviceName(ip)));
    // What it can keep, then what it has.
    get(QStringLiteral("/json/info"), kTimeoutMs, [this, spans, confirm](bool ok, const QByteArray &info, const QString &why) {
        if (!ok)
            return finish(false, why);
        const QJsonObject l = leds(info);
        const int maxSeg = l.value(QStringLiteral("maxseg")).toInt();
        const int count = l.value(QStringLiteral("count")).toInt();
        get(QStringLiteral("/json/state"), kTimeoutMs,
            [this, spans, confirm, maxSeg, count](bool ok, const QByteArray &state, const QString &why) {
            if (!ok)
                return finish(false, why);
            QJsonObject request;
            QString error;
            if (!PixelPath::sprinkleSpansToWled(spans, state, maxSeg, count, request, &error))
                return finish(false, error);
            int existing = 0;
            const QJsonArray had = QJsonDocument::fromJson(state).object()
                                       .value(QStringLiteral("seg")).toArray();
            for (const QJsonValue &v : had)
                existing += v.toObject().value(QStringLiteral("stop")).toInt() > 0 ? 1 : 0;

            // Asked with the device's own numbers in hand; no busy line meanwhile.
            m_tick->stop();
            m_step.clear();
            emit progress(QString());
            if (confirm && !confirm(int(spans.size()), existing))
                return finish(false, tr("Nothing was written."));
            step(spans.size() == 1 ? tr("Writing 1 segment to %1").arg(deviceName(m_ip))
                                    : tr("Writing %1 segments to %2").arg(spans.size()).arg(deviceName(m_ip)));
            post(QStringLiteral("/json/state"), request, [this, spans](bool ok, const QByteArray &, const QString &why) {
                if (!ok)
                    return finish(false, why);
                // Read back: what it says now is what counts.
                step(tr("Checking %1").arg(deviceName(m_ip)));
                get(QStringLiteral("/json/state"), kTimeoutMs, [this, spans](bool ok, const QByteArray &now, const QString &why) {
                    if (!ok)
                        return finish(false, tr("Written, but it could not be read back: %1").arg(why));
                    const QStringList differ = PixelPath::wledSegmentsDiffer(spans, now);
                    if (differ.isEmpty())
                        return finish(true, spans.size() == 1
                            ? tr("Wrote 1 segment to %1.").arg(deviceName(m_ip))
                            : tr("Wrote %1 segments to %2.").arg(spans.size()).arg(deviceName(m_ip)));
                    finish(false, tr("Wrote to %1, but it reads back otherwise: %2")
                                      .arg(deviceName(m_ip), differ.join(QLatin1Char(' '))));
                });
            });
        });
    });
}

/*********************************************************************
 * LED count
 *********************************************************************/

void WledDeviceWrite::setLedCount(const QString &ip, const QJsonObject &cfgFix, int ledCount)
{
    if (isBusy())
        return;
    m_ip = ip;
    m_leds = ledCount;
    m_rebooted = false;
    step(tr("Setting %1 to %2 LEDs").arg(deviceName(ip)).arg(ledCount));
    post(QStringLiteral("/json/cfg"), cfgFix, [this](bool ok, const QByteArray &, const QString &why) {
        if (!ok)
            return finish(false, why);
        step(tr("Waiting for %1 to restart its LEDs").arg(deviceName(m_ip)));
        m_waiting.start();
        QTimer::singleShot(m_pollMs, this, &WledDeviceWrite::pollLeds);
    });
}

void WledDeviceWrite::pollLeds()
{
    /* While it rebuilds its output (or reboots) it may not answer at all:
       that is waited through, up to the limit. */
    get(QStringLiteral("/json/info"), qMin(kTimeoutMs, qMax(500, m_pollMs * 3)),
        [this](bool ok, const QByteArray &info, const QString &) {
        const int count = ok ? leds(info).value(QStringLiteral("count")).toInt() : 0;
        if (ok && count == m_leds)
            return finish(true, tr("%1 now drives %2 LEDs.").arg(deviceName(m_ip)).arg(m_leds));
        const qint64 waited = m_waiting.elapsed();
        if (!m_rebooted && ok && waited >= m_applyMs)
        {
            // Not taken by itself: it applies LED settings on a reboot.
            m_rebooted = true;
            step(tr("Rebooting %1 to apply it").arg(deviceName(m_ip)));
            post(QStringLiteral("/json/state"), QJsonObject{ { QStringLiteral("rb"), true } },
                 [this](bool, const QByteArray &, const QString &) {
                m_waiting.start();
                QTimer::singleShot(m_pollMs, this, &WledDeviceWrite::pollLeds);
            });
            return;
        }
        const int limit = m_rebooted ? m_rebootMs : m_applyMs + m_rebootMs;
        if (waited >= limit)
        {
            if (ok)
                return finish(false, tr("%1 still drives %2 LEDs, not %3: it did not take the new count.")
                                         .arg(deviceName(m_ip)).arg(count).arg(m_leds));
            return finish(false, tr("%1 did not come back in %2 s. Check it is powered and on the "
                                    "network, then try Fix on Device… again.")
                                     .arg(deviceName(m_ip)).arg(limit / 1000));
        }
        QTimer::singleShot(m_pollMs, this, &WledDeviceWrite::pollLeds);
    });
}

/*********************************************************************
 * Confirmations
 *********************************************************************/

bool WledDeviceWrite::confirmSegments(QWidget *parent, const QString &fixtureName, const QString &ip,
                                      int segments, int existing)
{
    QMessageBox box(QMessageBox::Question, tr("Write Segments"),
        (segments == 1 ? tr("Write 1 segment to the WLED at %1?").arg(ip.toHtmlEscaped())
                       : tr("Write %1 segments to the WLED at %2?").arg(segments).arg(ip.toHtmlEscaped()))
        + QStringLiteral("<br><br>")
        + tr("One segment for each span of %1, every second one reversed so each span "
             "runs from the inner wall to the outer. LEDs between spans are left out, dark.")
              .arg(fixtureName.toHtmlEscaped())
        + QStringLiteral(" ")
        + (existing == 1 ? tr("They replace the 1 segment it has now.")
                         : tr("They replace the %1 segments it has now.").arg(existing))
        + QStringLiteral("<br><br>")
        + tr("This changes the device itself, and Undo can't take it back. Read Segments "
             "shows what it has."),
        QMessageBox::Yes | QMessageBox::Cancel, parent);
    box.setObjectName(QStringLiteral("wledWriteSegmentsConfirm"));
    box.setTextFormat(Qt::RichText);
    box.button(QMessageBox::Yes)->setText(segments == 1 ? tr("Write 1 Segment")
                                                        : tr("Write %1 Segments").arg(segments));
    box.setDefaultButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Yes;
}

bool WledDeviceWrite::confirmLedCount(QWidget *parent, const QString &fixtureName, const QString &ip,
                                      int now, int ledCount)
{
    QMessageBox box(QMessageBox::Warning, tr("Set LED Count"),
        tr("Set the WLED at %1 to drive %2 LEDs? It drives %3 now; %4 has %2.")
            .arg(ip.toHtmlEscaped()).arg(ledCount).arg(now).arg(fixtureName.toHtmlEscaped())
        + QStringLiteral("<br><br>")
        + tr("<b>A wrong count can leave LEDs dark:</b> too few and the rest of the string "
             "stays off. Only the count changes, not its LED type, colour order, pins or Wi-Fi.")
        + QStringLiteral("<br><br>")
        + tr("The WLED restarts its LED output to apply it, and is rebooted if it has to be: "
             "dark for a few seconds. This changes the device itself, and Undo can't take it back."),
        QMessageBox::Yes | QMessageBox::Cancel, parent);
    box.setObjectName(QStringLiteral("wledLedCountConfirm"));
    box.setTextFormat(Qt::RichText);
    box.button(QMessageBox::Yes)->setText(tr("Set %1 LEDs").arg(ledCount));
    box.setDefaultButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Yes;
}
