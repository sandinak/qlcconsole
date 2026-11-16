/*
  Q Light Controller Plus
  controllerjobs.h

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

#ifndef CONTROLLERJOBS_H
#define CONTROLLERJOBS_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QList>

#include "qlcengine_export.h"

class QLCIOPlugin;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What each game controller is for (docs/design/CONTROLLER_JOBS.md): its
 * jobs, Run cues, Follow spot and Focus, saved with THIS computer (its
 * settings, never the show), by the controller's identity -- the physical
 * pad, not the input line or universe it lands on:
 *
 *   mac:aa:bb:cc:dd:ee:ff       a Bluetooth address in the device's name
 *   port:<plugin>:<location>    else the plugin's inputDeviceLocation()
 *                               (the HID plugin: the USB port)
 *   name:<plugin>:<name>        else its name (two such pads of one model
 *                               share their jobs; they can't be told apart)
 *
 * A controller with nothing saved has every job, so one pad works as it
 * always has. The emergency stop is no job: every pad has it.
 */
class QLC_ENGINE_EXPORT ControllerJobs final : public QObject
{
    Q_OBJECT

public:
    enum Job
    {
        NoJobs = 0,
        RunCues = 0x1,      ///< Pad Run's map: the arm, the GO family, the hit
        FollowSpot = 0x2,   ///< the follow spot's sticks, fine stick, speed, size
        Focus = 0x4,        ///< venue focus, Focus mode, Refine, the walk
        AllJobs = RunCues | FollowSpot | Focus
    };
    Q_DECLARE_FLAGS(Jobs, Job)
    Q_FLAG(Jobs)

    explicit ControllerJobs(Doc *doc, QObject *parent = nullptr);
    ~ControllerJobs() override;

    /** The three jobs, in the order they are shown. */
    static QList<Job> jobs();
    /** "Run cues", "Follow spot", "Focus". */
    static QString jobName(Job job);
    /** The settings key's word for a job: "run", "followspot", "focus". */
    static QString jobKey(Job job);

    /*********************************************************************
     * Who a controller is
     *********************************************************************/
public:
    /** The Bluetooth address in @p name, as "aa:bb:cc:dd:ee:ff"; empty
     *  when it holds none (six hex pairs split by ':' or '-'). */
    static QString macIn(const QString &name);
    /** The identity of the device called @p name, at @p location (the
     *  plugin's inputDeviceLocation(), may be empty), of @p plugin. */
    static QString identityFor(const QString &plugin, const QString &name,
                               const QString &location);
    /** The identity of @p plugin's input line @p input. */
    static QString identityOf(QLCIOPlugin *plugin, quint32 input);
    /** The identity of the controller on @p universe's input now; empty
     *  when nothing is patched there. */
    QString identityOfUniverse(quint32 universe) const;

    /** How an identity tells the pad apart, for a person: "Bluetooth
     *  DE:4C:09:77:77:93", "USB port 14200000", or that it can't be
     *  (by name only). */
    static QString describe(const QString &identity);
    /** True when @p identity is only a name: two pads of one model share it. */
    static bool isNameOnly(const QString &identity);
    /** A pad's own name without its serial ("DUALSHOCK 4 Wireless
     *  Controller (DE:4C:...)" is "DUALSHOCK 4 Wireless Controller"). */
    static QString modelName(const QString &name);
    /** A short name a person tells pads apart by: the model and, when the
     *  identity has one, the end of its address or its port
     *  ("DUALSHOCK 4 Wireless Controller (…77:93)"). */
    static QString shortName(const QString &name, const QString &identity);
    /** shortName() of the controller on @p universe; empty: none there. */
    QString nameOfUniverse(quint32 universe) const;

    /*********************************************************************
     * The jobs
     *********************************************************************/
public:
    /** @p identity's jobs (AllJobs when nothing is saved for it). */
    Jobs jobsOf(const QString &identity) const;
    /** Save @p identity's jobs on this computer at once; all three leaves
     *  no key. jobsChanged() when they changed. */
    void setJobs(const QString &identity, Jobs jobs);
    /** One job on or off. */
    void setJob(const QString &identity, Job job, bool on);
    /** The controller on @p universe has @p job. A universe with nothing
     *  patched (no controller there to have jobs) has every job. */
    bool has(quint32 universe, Job job) const;
    /** Forget every saved job (all three on every pad again). */
    void resetAll();
    /** The identities with something saved, sorted. */
    QStringList saved() const;

    /** The settings group the jobs are kept in. */
    static QString settingsGroup();

signals:
    /** @p identity's jobs changed (here, or by resetAll()). */
    void jobsChanged(const QString &identity);

private:
    void load();

private:
    Doc *m_doc;
    /** What the settings hold, read once: identity -> jobs (only those not
     *  AllJobs are kept). */
    QHash<QString, Jobs> m_jobs;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ControllerJobs::Jobs)

/** @} */

#endif
