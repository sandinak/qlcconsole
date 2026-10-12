/*
  Q Light Controller Plus
  speedmasters.h

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

#ifndef SPEEDMASTERS_H
#define SPEEDMASTERS_H

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QList>

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCSpeedMasters QStringLiteral("SpeedMasters")

/**
 * The show's speed masters (SPEED_SPREAD_DESIGN.md, S2): four tempo clocks
 * an effect, a look, a chase step, a chase or a collection can be put on, so
 * one tap or one fader moves everything on it.
 *
 * Each master either keeps its OWN tempo (a BPM, a tap, a rate fader) or
 * FOLLOWS the main beat clock (MasterTimer) or another master at a ratio --
 * ×2 and it plays two beats to the other's one, on the other's beats. A
 * master can be paused: its position stops and every effect on it freezes.
 *
 * What runs on a master reads its beatPosition() the way effects read the
 * main clock's. Readable from any thread.
 */
class SpeedMasters final : public QObject
{
    Q_OBJECT

public:
    static const int Count = 4;

    /** What a function, step or effect runs on. Stored as these numbers
     *  (an effect palette's "__speedMaster" param, the "SpeedMaster"
     *  attribute of functions and chase steps). */
    enum Assignment
    {
        Inherit = -1,   //!< whatever started it says (the default)
        Free = 0,       //!< no clock: its own speed, as before masters
        Master1 = 1,    //!< 1..Count: speed master N
        MainClock = 99  //!< the main beat clock (MasterTimer)
    };
    static bool isMaster(int assignment) { return assignment >= Master1 && assignment < Master1 + Count; }
    /** "Inherit", "Free", "Main", "1".."4" (XML) and back. */
    static QString assignmentToString(int assignment);
    static int stringToAssignment(const QString &str);
    /** For people: "Speed master 2", "Main clock", "Own speed". */
    QString assignmentName(int assignment) const;

    enum Mode { Own, Follow };

    explicit SpeedMasters(Doc *doc, QObject *parent = nullptr);

    /** Back to four masters on their own at 120 BPM. */
    void reset();

    QString name(int index) const;
    void setName(int index, const QString &name);

    Mode mode(int index) const;
    /** What a Follow master follows: MainClock or 0..Count-1. */
    int follows(int index) const;
    double ratio(int index) const;
    /** Follow @p source (MainClock or another master's index) at @p ratio.
     *  Refused (false) when it would make a loop or the ratio is not > 0. */
    bool setFollow(int index, int source, double ratio = 1.0);
    /** Back to its own tempo, carrying on from where it is. */
    void setOwn(int index);

    /** An Own master's set tempo (before the rate fader). */
    double setBpm(int index) const;
    void setBpm(int index, double bpm);
    /** An Own master's rate fader, 0..2 (1 = as set). */
    double rate(int index) const;
    void setRate(int index, double rate);
    bool isPaused(int index) const;
    void setPaused(int index, bool paused);

    /** Tap an Own master: tempo from its last four taps, beat on this one.
     *  A Follow master isn't tapped (false). */
    bool tap(int index);

    /** The tempo it is running at now (rate, ratio, what it follows), 0
     *  while paused. */
    double bpm(int index) const;
    /** Its running beat count (see MasterTimer::beatPosition()). */
    double beatPosition(int index) const;

    /** Beats of @p assignment (a master or MainClock) and its tempo; false
     *  for Free/Inherit. */
    bool clockFor(int assignment, double *beatPos, double *bpm) const;

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;
    /** Nothing changed from reset(): not written to the file. */
    bool isDefault() const;

signals:
    void changed(int index);

private:
    struct Master
    {
        QString name;
        Mode mode = Own;
        int follows = MainClock;
        double ratio = 1.0;
        double bpm = 120.0;
        double rate = 1.0;
        bool paused = false;
        qint64 anchorNs = 0;        //!< Own: clock time of the anchor
        double anchorBeats = 0;     //!< Own: position at the anchor; paused: the frozen position
        QList<qint64> taps;
    };

    bool valid(int index) const { return index >= 0 && index < Count; }
    double positionLocked(int index, qint64 now, double mainPos, int depth = 0) const;
    double bpmLocked(int index, double mainBpm, int depth = 0) const;
    /** Re-anchor an Own master at its current position (before a change). */
    void reanchorLocked(int index, qint64 now);
    void mainClock(double *pos, double *bpm) const;

    Doc *m_doc;
    mutable QMutex m_mutex;
    QElapsedTimer m_clock;
    Master m_masters[Count];
};

/** @} */

#endif
