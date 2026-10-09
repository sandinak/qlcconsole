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
#include <QStringList>
#include <QObject>
#include <QHash>

class QNetworkAccessManager;
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
class WledConfigCheck final : public QObject
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

    /** Read every WLED's settings now (and again every so often). */
    void checkNow();

    /** Write each of @p fixtureIds' fix to its device, then read it back. */
    void fix(const QList<quint32> &fixtureIds);

    /** Show what fix() would change on each of @p fixtureIds, and do it if
     *  the user agrees. True when it was written. */
    bool confirmFix(QWidget *parent, const QList<quint32> &fixtureIds);

    /** How often it reads them again, ms (tests make it quick). */
    void setInterval(int ms);

    /** Where a fixture's device is asked, "http://<ip>" -- a test points
     *  this elsewhere. */
    static QString baseUrl(const QString &ip);

signals:
    /** The findings changed. */
    void changed();

private:
    void receive(const QString &ip, const QByteArray &body, bool ok);
    /** The footer's "wled.config" entry, from the findings. */
    void publish();

    static WledConfigCheck *s_instance;
    Doc *m_doc;
    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    QHash<QString, QJsonObject> m_cfg;          ///< ip -> its last /json/cfg
    QHash<QString, bool> m_answered;            ///< ip -> answered last time
    QHash<quint32, Finding> m_findings;
};

/** @} */

#endif
