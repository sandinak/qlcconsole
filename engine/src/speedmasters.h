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
#include <climits>

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
    /** Sixteen are always there; a show uses -- lists, saves, offers --
     *  the first count() of them (4 to 16, 4 by default). */
    static const int MaxCount = 16;
    static const int DefaultCount = 4;
    int count() const;
    void setCount(int count);

    /** What a function, step or effect runs on. Stored as these numbers
     *  (an effect palette's "__speedMaster" param, the "SpeedMaster"
     *  attribute of functions and chase steps). */
    enum Assignment
    {
        Inherit = -1,   //!< whatever started it says (the default)
        Free = 0,       //!< no clock: its own speed, as before masters
        Master1 = 1,    //!< 1..Count: speed master N
        MainClock = 99, //!< the main beat clock (MasterTimer)
        /** A Follow source only: the running show's timeline -- its beat grid
         *  (the show's BPM) at its playhead, so a show following timecode
         *  takes everything on the master with it. No show running: the
         *  main clock. */
        ShowTimeline = 98
    };
    static bool isMaster(int assignment) { return assignment >= Master1 && assignment < Master1 + MaxCount; }
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
    /** What a Follow master follows: MainClock, ShowTimeline or 0..MaxCount-1. */
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

    /** Which masters in use (bit N = master N+1) the show actually uses:
     *  an effect, look, chase, chase step or collection is on it, a control
     *  surface input drives it, or a used master follows it. One nothing
     *  uses has no business lighting up or taking footer room. */
    quint32 usedMask() const;
    bool isUsed(int index) const { return (usedMask() >> index) & 1u; }

    /** What a control surface can drive on a master: Tap (a button),
     *  Rate (a fader, 0..200 %), Pause (a button: press to toggle). */
    enum Control { TapControl, RateControl, PauseControl, ControlCount };
    static QString controlToString(Control control);
    /** The input channel driving @p control of master @p index;
     *  universe UINT_MAX when none. */
    void setInput(int index, Control control, quint32 universe, quint32 channel);
    void clearInput(int index, Control control);
    bool input(int index, Control control, quint32 *universe, quint32 *channel) const;

public slots:
    /** A value from an input universe (InputOutputMap::inputValueChanged). */
    void slotInputValue(quint32 universe, quint32 channel, uchar value);

public:
    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;
    /** Nothing changed from reset(): not written to the file. */
    bool isDefault() const;

signals:
    void changed(int index);
    void countChanged(int count);

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
        quint32 inUniverse[ControlCount] = { UINT_MAX, UINT_MAX, UINT_MAX };
        quint32 inChannel[ControlCount] = { UINT_MAX, UINT_MAX, UINT_MAX };
        uchar inLast[ControlCount] = { 0, 0, 0 };   //!< for a button's press
    };

    bool valid(int index) const { return index >= 0 && index < MaxCount; }
    double positionLocked(int index, qint64 now, double mainPos, int depth = 0) const;
    double bpmLocked(int index, double mainBpm, int depth = 0) const;
    /** Re-anchor an Own master at its current position (before a change). */
    void reanchorLocked(int index, qint64 now);
    void mainClock(double *pos, double *bpm) const;
    /** The running show's beat position and tempo; false (main clock values
     *  left alone) when no show with a BPM grid is running. */
    bool timelineClock(double *pos, double *bpm) const;
    /** Whether any master follows the timeline (only then is it read). */
    bool anyFollowsTimelineLocked() const;
    double m_timelinePos = 0, m_timelineBpm = 0;   //!< read before taking m_mutex

    Doc *m_doc;
    mutable QMutex m_mutex;
    QElapsedTimer m_clock;
    Master m_masters[MaxCount];
    int m_count = DefaultCount;
};

/** @} */

#endif
