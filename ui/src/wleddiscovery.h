/*
  Q Light Controller Plus
  wleddiscovery.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDDISCOVERY_H
#define WLEDDISCOVERY_H

#include <QHostAddress>
#include <QObject>
#include <QHash>
#include <QList>
#include <functional>

#include "qlcui_export.h"

class QNetworkAccessManager;
class QUdpSocket;
class Fixture;
class QWidget;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * WLEDs found on the network ("can WLEDs be detected vs having to be
 * discretely configured? .. can they show up with just a right click patch
 * needed to get em in?").
 *
 * Every WLED says it is there over mDNS, as a _wled._tcp service. Every so
 * often, and when asked (the Fixture Manager opening), one mDNS question
 * goes out on each of this computer's networks -- only those on the show's
 * rig networks while the transmit gate holds output elsewhere -- from a port
 * of its own, so each WLED answers it directly. Each one found is then asked
 * its /json/info once (its name, LED count, hardware address, version).
 *
 * A WLED is in the show when a WLED fixture has its hardware address (MAC),
 * or, before that is known, its IP; the MAC is learnt from the IP then.
 * The rest are the Inventory's "found" rows, patched with one right-click
 * (patch(), one undo). A fixture whose WLED answers at another IP is a
 * footer line ("WLED Test is now at .105") with Follow It (follow()).
 * One in the show that does not answer is the WLED output's own footer line
 * (wled.targets): not said twice here.
 *
 * Nothing here needs a library: the question and the answers are a few
 * dozen bytes of DNS, made and read by browseQuery() and parseResponse().
 */
class QLC_UI_EXPORT WledDiscovery final : public QObject
{
    Q_OBJECT

public:
    explicit WledDiscovery(Doc *doc, QObject *parent = nullptr);
    ~WledDiscovery();

    /** The one the app runs (nullptr before it has one). */
    static WledDiscovery *instance() { return s_instance; }

    /** A WLED on the network, as it said itself. */
    struct Device
    {
        QString mac;            ///< "e08cfef992e0" (lower case, no separators); empty: not said
        QString ip;             ///< "172.18.2.100", or "ip:port" when not on 80
        QString host;           ///< its mDNS name ("wled-f992e0")
        QString name;           ///< its own name (Config ▸ User Interface ▸ Server name)
        int leds = 0;
        QString version;
        bool rgbw = false;
        qint64 seenAt = 0;      ///< ms since the epoch it last answered mDNS
    };

    /** One mDNS answer about a _wled._tcp service. */
    struct Answer
    {
        QString instance;       ///< "wled-f992e0"
        QString host;           ///< "wled-f992e0.local"
        quint16 port = 0;
        QString ip;             ///< from its A record; empty when not in the packet
        QString mac;            ///< from its TXT "mac=" (WLED says it there too)
    };

    /** The question: the PTR records of _wled._tcp.local, asking for a
     *  direct (unicast) answer. */
    static QByteArray browseQuery();

    /** The WLED services an mDNS packet tells of (none from a question or
     *  a broken packet). Pure: no network. */
    static QList<Answer> parseResponse(const QByteArray &packet);

    /** "WLED (172.18.2.100) · 50 LEDs" */
    static QString describe(const Device &device);

    /** "wled-f992e0" -> lower-case [a-z0-9-], at most 32: what WLED takes
     *  for its mDNS name ("Staff L" -> "staff-l"). */
    static QString hostName(const QString &name);

    /** Look on the network every so often, from now on. */
    void start();
    /** Ask the network now. */
    void browseNow();
    /** Ask now when it is looking at all (start()) and has not asked in the
     *  last few seconds: the Fixture Manager opening. */
    void browseSoon();
    /** start() has run (or a test's sender is set). */
    bool isRunning() const;
    /** How often it asks again (ms; tests make it quick). */
    void setInterval(int ms);

    /** Where the question goes instead of the network (a test): called with
     *  each query. Answers come back through receive(). */
    typedef std::function<void(const QByteArray &query)> Sender;
    void setSender(const Sender &send);

    /** An mDNS packet that arrived from @p from. */
    void receive(const QByteArray &packet, const QHostAddress &from);

    /** Every WLED found lately, with what it said. */
    QList<Device> devices() const;
    /** Those that no fixture in the show stands for, by name. */
    QList<Device> notInShow() const;
    /** The one with hardware address (or, when it said none, IP) @p key. */
    Device device(const QString &key) const;
    /** A device's key: its MAC, else its IP. */
    static QString keyOf(const Device &device);

    /** The fixture standing for @p device, or nullptr. */
    Fixture *fixtureFor(const Device &device) const;

    struct Move
    {
        quint32 fixtureId = 0;
        QString from;           ///< the IP it has in the show
        QString to;             ///< where its WLED answers now
    };
    /** Fixtures whose WLED (by MAC) answers at another IP. */
    QList<Move> moves() const;

    /** Make a WLED fixture of @p key's device -- WLED Effect Mode, its name,
     *  IP, MAC and LED count -- and give it a universe of its own sent to it
     *  (FixtureCreator::routeWled()), as one undo step; then offer to set
     *  the device up (WledConfigCheck::offerSetUp()) when the app checks
     *  WLEDs. Problems are said over @p parent. 0 when it could not. */
    quint32 patch(QWidget *parent, const QString &key);

    /** Send fixture @p fixtureId to the IP its WLED answers at now: its IP,
     *  and its universe's output aimed there, as one undo step. */
    bool follow(QWidget *parent, quint32 fixtureId);

signals:
    /** What has been found, or what of it is in the show, changed. */
    void changed();

private:
    void fetchInfo(const QString &key);
    void takeInfo(const QString &ip, const QString &host, const QByteArray &body);
    /** Learn MACs, drop what has gone quiet, publish the footer line. */
    void reconcile();
    void publish();
    /** Whether any network may be asked from @p local (the transmit gate). */
    bool mayAskFrom(const QHostAddress &local) const;

    static WledDiscovery *s_instance;
    Doc *m_doc;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    Sender m_sender;
    QList<QUdpSocket *> m_sockets;
    QHash<QString, Device> m_devices;           ///< key -> device (with its info)
    QHash<QString, qint64> m_asked;             ///< ip -> when its info was asked
    QStringList m_lastPublished;
    qint64 m_lastBrowse = 0;
};

/** @} */

#endif
