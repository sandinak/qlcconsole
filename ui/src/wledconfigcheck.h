/*
  Q Light Controller Plus
  wledconfigcheck.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDCONFIGCHECK_H
#define WLEDCONFIGCHECK_H

#include <QJsonObject>
#include "qlcui_export.h"
#include <QStringList>
#include <QObject>
#include <QHash>

class QNetworkAccessManager;
class QNetworkReply;
class Fixture;
class QWidget;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Whether each WLED is set up the way this show drives it ("does this tool
 * check the wled host for proper config .. wled control .. or artnet").
 *
 * Every so often it reads each WLED fixture's own settings (/json/cfg) and
 * compares them with the fixture: driven through WLED's API, nothing should
 * be able to take it over with a stream (realtime receive off); driven by
 * Art-Net or sACN, the device has to listen on that protocol, on the universe
 * its output sends, at its address, in the DMX mode of its definition's mode.
 * What does not match is a Finding -- in the readiness footer and on the
 * fixture -- with the settings that would fix it, which fix() writes to the
 * device (after confirmFix() has shown exactly what changes).
 */
class QLC_UI_EXPORT WledConfigCheck final : public QObject
{
    Q_OBJECT

public:
    explicit WledConfigCheck(Doc *doc, QObject *parent = nullptr);
    ~WledConfigCheck();

    /** The one the app runs (nullptr before it has one). */
    static WledConfigCheck *instance() { return s_instance; }

    struct Finding
    {
        quint32 fixtureId = 0;
        QString ip;
        /** What does not match, one line each ("listens for E1.31, but its
         *  universe goes out as Art-Net"). Empty: set up right. */
        QStringList problems;
        /** The /json/cfg settings that would put it right (empty when
         *  nothing can). */
        QJsonObject fix;
        /** In words, what fix() would change ("Protocol: E1.31 → Art-Net"). */
        QStringList changes;

        /** Its LED count against the device's, when both are known and
         *  differ (0 otherwise): set apart from @a fix, since a wrong count
         *  leaves LEDs dark and it is asked for on its own. */
        int fixtureLeds = 0;
        int deviceLeds = 0;
        /** The /json/cfg settings that make the device drive fixtureLeds:
         *  its whole hw.led as it is, the one output's length and the total
         *  changed (WLED rebuilds the outputs from what is sent). Empty when
         *  it has several outputs. */
        QJsonObject ledFix;
        /** Why the count can't be set from here (several outputs). */
        QString ledRefusal;
    };

    /** The fixtures whose WLED is not set up as the show drives it. */
    QList<Finding> findings() const;
    /** One fixture's (problems empty when it is fine or not known yet). */
    Finding finding(quint32 fixtureId) const;

    /** Whether @p fx is a WLED this checks: a WLED definition, on the
     *  network, with an IP. */
    static bool isWled(const Fixture *fx);

    /** The rules: @p fx as the show drives it against its device's own
     *  settings @p cfg (the /json/cfg document). Pure -- no network. */
    static Finding evaluate(Doc *doc, const Fixture *fx, const QJsonObject &cfg);

    /** How many LEDs the show says @p fx has: its heads (a pixel mode), or
     *  the LEDs set on it in Fixture Properties. 0 = not said (the
     *  definition's own guess, which the device's count overrides). */
    static int fixtureLedCount(Doc *doc, const Fixture *fx);

    /** Read every WLED's settings now (and again every so often). */
    void checkNow();

    /** Write each of @p fixtureIds' fix to its device, then read it back. */
    void fix(const QList<quint32> &fixtureIds);

    /** Show what fix() would change on each of @p fixtureIds, and do it if
     *  the user agrees. True when it was written. */
    bool confirmFix(QWidget *parent, const QList<quint32> &fixtureIds);

    /** Set the LED count of @p fixtureId's device to the fixture's (its
     *  finding's ledFix), wait for it to take, then read it back. False when
     *  there is nothing to set or it is refused (writeStatus() says why). */
    bool setLedCount(quint32 fixtureId);

    /** The device write at @p ip in progress or just done, in words, and
     *  whether it is still working; empty when there has been none lately. */
    QString writeStatus(const QString &ip, bool *busy = nullptr) const;

    /** How long a new LED count is waited for (tests make it quick). */
    void setLedWaits(int applyMs, int rebootMs, int pollMs);

    /** How often it reads them again, ms (tests make it quick). */
    void setInterval(int ms);

    /** Where a fixture's device is asked, "http://<ip>" -- a test points
     *  this elsewhere. */
    static QString baseUrl(const QString &ip);

signals:
    /** The findings changed. */
    void changed();
    /** writeStatus(@p ip) changed. */
    void writeChanged(const QString &ip);

private:
    friend class WledConfigCheck_Test;
    /** A /json/cfg reply has finished: read it if it succeeded. */
    void replied(const QString &ip, QNetworkReply *reply);
    void receive(const QString &ip, const QByteArray &body, bool ok);
    /** Keep @p text as what the write at @p ip says now. */
    void noteWrite(const QString &ip, const QString &text, bool busy);
    /** The footer's "wled.config" entry, from the findings. */
    void publish();

    static WledConfigCheck *s_instance;
    Doc *m_doc;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QHash<QString, QJsonObject> m_cfg;          ///< ip -> its last /json/cfg
    QHash<QString, bool> m_answered;            ///< ip -> answered last time
    QHash<quint32, Finding> m_findings;
    struct Write { QString text; bool busy = false; qint64 at = 0; };
    QHash<QString, Write> m_writes;             ///< ip -> its last device write
    int m_ledWaits[3] = { 8000, 45000, 1000 };
};

/** @} */

#endif
