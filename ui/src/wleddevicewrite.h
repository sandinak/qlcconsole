/*
  Q Light Controller Plus
  wleddevicewrite.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDDEVICEWRITE_H
#define WLEDDEVICEWRITE_H

#include <QElapsedTimer>
#include <QJsonObject>
#include <QVector>
#include <QObject>
#include <functional>

#include "qlcui_export.h"
#include "truss.h"

class QNetworkAccessManager;
class QNetworkReply;
class QWidget;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * One write to a WLED's own settings, start to finish: Write Segments (a
 * sprinkle-loop string's spans as the device's segments) or a new LED count
 * (Fix on Device). Each step is one request at a time, with a timeout and no
 * proxy (WLEDs are on the show LAN), and it is read back afterwards: what
 * the device says then is what is reported.
 *
 * While it works, busyText() says what it is waiting on, with the seconds
 * after two (UX-26); progress() fires as that changes, finished() once, with
 * the result in plain words. A write to a device cannot be undone: the
 * confirmations say so.
 */
class QLC_UI_EXPORT WledDeviceWrite final : public QObject
{
    Q_OBJECT

public:
    explicit WledDeviceWrite(QObject *parent = nullptr);
    ~WledDeviceWrite();

    /** Why nothing may be written to the device at @p ip now (a Teaching
     *  show holding real output), in words; empty when it may. A device on
     *  this computer itself (127.x) is not held, as for output. */
    static QString heldReason(const Doc *doc, const QString &ip);

    /** How a device is named to the operator: "WLED (172.18.2.100)". */
    static QString deviceName(const QString &ip);

    /** Asked once the device has been read, before anything is written:
     *  @p segments to write, @p existing it has now. True: write them. */
    typedef std::function<bool(int segments, int existing)> SegmentsConfirm;

    /** Read the device at @p ip (info, then state), refuse what it cannot
     *  keep, ask @p confirm, write @p spans as its segments, read back. */
    void writeSegments(const QString &ip, const QVector<SprinkleSpan> &spans,
                       const SegmentsConfirm &confirm);

    /** Write @p cfgFix (the /json/cfg settings that set the count) to the
     *  device at @p ip, then wait for it to drive @p leds LEDs: WLED 0.14
     *  and later rebuild their LED output from a new LED setting without a
     *  reboot; one that has not taken it after a few seconds is rebooted
     *  ("rb") and waited for. */
    void setLedCount(const QString &ip, const QJsonObject &cfgFix, int leds);

    bool isBusy() const { return !m_step.isEmpty(); }
    /** "Writing 16 segments to WLED (172.18.2.100)… 3 s"; empty when idle. */
    QString busyText() const;
    QString ip() const { return m_ip; }

    /** How long it waits for a new LED count to take before rebooting the
     *  device, and for the reboot (ms; tests make them short). */
    void setWaits(int applyMs, int rebootMs, int pollMs);

    /** The confirmations, as the inspector and Fix on Device show them. */
    static bool confirmSegments(QWidget *parent, const QString &fixtureName, const QString &ip,
                                int segments, int existing);
    static bool confirmLedCount(QWidget *parent, const QString &fixtureName, const QString &ip,
                                int now, int leds);

signals:
    /** busyText() changed (a step, or another second). */
    void progress(const QString &text);
    /** Done: @p ok when it was written and reads back as asked. */
    void finished(bool ok, const QString &message);

private:
    typedef std::function<void(bool ok, const QByteArray &body, const QString &why)> Reply;
    QNetworkReply *get(const QString &path, int timeoutMs, const Reply &then);
    QNetworkReply *post(const QString &path, const QJsonObject &body, const Reply &then);
    void step(const QString &text);
    void finish(bool ok, const QString &message);
    /** Poll /json/info until it drives m_leds, rebooting once if it has to. */
    void pollLeds();

    QNetworkAccessManager *m_nam;
    QTimer *m_tick;
    QElapsedTimer m_since;
    QElapsedTimer m_waiting;
    QString m_ip;
    QString m_step;
    QNetworkReply *m_reply = nullptr;       ///< the request in flight
    int m_leds = 0;
    bool m_rebooted = false;
    int m_applyMs = 8000;
    int m_rebootMs = 45000;
    int m_pollMs = 1000;
};

/** @} */

#endif
