/*
  Q Light Controller Plus
  speedmasters.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QMutexLocker>
#include <QDebug>

#include <cmath>

#include "speedmasters.h"
#include "mastertimer.h"
#include "show.h"
#include "doc.h"

#define KXMLMaster  QStringLiteral("Master")
#define KXMLIndex   QStringLiteral("Index")
#define KXMLName    QStringLiteral("Name")
#define KXMLMode    QStringLiteral("Mode")
#define KXMLFollows QStringLiteral("Follows")
#define KXMLRatio   QStringLiteral("Ratio")
#define KXMLBpm     QStringLiteral("BPM")
#define KXMLRate    QStringLiteral("Rate")
#define KXMLPaused  QStringLiteral("Paused")

SpeedMasters::SpeedMasters(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    m_clock.start();
    reset();
}

int SpeedMasters::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_count;
}

void SpeedMasters::setCount(int count)
{
    count = qBound(int(DefaultCount), count, int(MaxCount));
    {
        QMutexLocker locker(&m_mutex);
        if (count == m_count)
            return;
        m_count = count;
    }
    emit countChanged(count);
}

void SpeedMasters::reset()
{
    {
        QMutexLocker locker(&m_mutex);
        m_count = DefaultCount;
        const qint64 now = m_clock.nsecsElapsed();
        for (int i = 0; i < MaxCount; ++i)
        {
            m_masters[i] = Master();
            m_masters[i].anchorNs = now;
        }
    }
    for (int i = 0; i < MaxCount; ++i)
        emit changed(i);
}

/*********************************************************************
 * Assignments
 *********************************************************************/

QString SpeedMasters::assignmentToString(int assignment)
{
    if (assignment == Free)
        return QStringLiteral("Free");
    if (assignment == MainClock)
        return QStringLiteral("Main");
    if (isMaster(assignment))
        return QString::number(assignment);
    return QStringLiteral("Inherit");
}

int SpeedMasters::stringToAssignment(const QString &str)
{
    if (str == QLatin1String("Free"))
        return Free;
    if (str == QLatin1String("Main"))
        return MainClock;
    bool ok = false;
    const int n = str.toInt(&ok);
    if (ok && isMaster(n))
        return n;
    return Inherit;
}

QString SpeedMasters::assignmentName(int assignment) const
{
    if (assignment == Free)
        return tr("Own speed");
    if (assignment == MainClock)
        return tr("Main clock");
    if (isMaster(assignment))
        return name(assignment - Master1);
    return tr("Inherit");
}

/*********************************************************************
 * Settings
 *********************************************************************/

QString SpeedMasters::name(int index) const
{
    if (!valid(index))
        return QString();
    QMutexLocker locker(&m_mutex);
    return m_masters[index].name.isEmpty() ? tr("Speed %1").arg(index + 1) : m_masters[index].name;
}

void SpeedMasters::setName(int index, const QString &name)
{
    if (!valid(index))
        return;
    {
        QMutexLocker locker(&m_mutex);
        // The default reads back as the default, not as a name somebody chose.
        m_masters[index].name = name == tr("Speed %1").arg(index + 1) ? QString() : name;
    }
    emit changed(index);
}

SpeedMasters::Mode SpeedMasters::mode(int index) const
{
    QMutexLocker locker(&m_mutex);
    return valid(index) ? m_masters[index].mode : Own;
}

int SpeedMasters::follows(int index) const
{
    QMutexLocker locker(&m_mutex);
    return valid(index) ? m_masters[index].follows : MainClock;
}

double SpeedMasters::ratio(int index) const
{
    QMutexLocker locker(&m_mutex);
    return valid(index) ? m_masters[index].ratio : 1.0;
}

bool SpeedMasters::setFollow(int index, int source, double ratio)
{
    if (!valid(index) || !(ratio > 0)
        || (source != MainClock && source != ShowTimeline && !valid(source)))
        return false;
    {
        QMutexLocker locker(&m_mutex);
        // Only another master the show uses.
        if (source != MainClock && source != ShowTimeline && source >= m_count)
            return false;
        // Following itself, or something that (eventually) follows it: a loop.
        for (int s = source; s != MainClock && s != ShowTimeline; )
        {
            if (s == index)
                return false;
            s = m_masters[s].mode == Follow ? m_masters[s].follows : int(MainClock);
        }
        Master &m = m_masters[index];
        m.mode = Follow;
        m.follows = source;
        m.ratio = ratio;
        m.taps.clear();
    }
    emit changed(index);
    return true;
}

void SpeedMasters::setOwn(int index)
{
    if (!valid(index))
        return;
    double mainPos, mainBpm;
    mainClock(&mainPos, &mainBpm);
    {
        QMutexLocker locker(&m_mutex);
        Master &m = m_masters[index];
        if (m.mode == Own)
            return;
        const qint64 now = m_clock.nsecsElapsed();
        // Carry on at the tempo and the place it was following.
        const double pos = positionLocked(index, now, mainPos);
        const double bpm = m.paused ? 0 : bpmLocked(index, mainBpm);
        m.mode = Own;
        m.anchorBeats = pos;
        m.anchorNs = now;
        m.rate = 1.0;
        if (bpm > 0)
            m.bpm = bpm;
    }
    emit changed(index);
}

double SpeedMasters::setBpm(int index) const
{
    QMutexLocker locker(&m_mutex);
    return valid(index) ? m_masters[index].bpm : 0;
}

void SpeedMasters::setBpm(int index, double bpm)
{
    if (!valid(index) || !(bpm >= 1.0 && bpm <= 999.0))
        return;
    {
        QMutexLocker locker(&m_mutex);
        reanchorLocked(index, m_clock.nsecsElapsed());
        m_masters[index].bpm = bpm;
    }
    emit changed(index);
}

double SpeedMasters::rate(int index) const
{
    QMutexLocker locker(&m_mutex);
    return valid(index) ? m_masters[index].rate : 1.0;
}

void SpeedMasters::setRate(int index, double rate)
{
    if (!valid(index))
        return;
    {
        QMutexLocker locker(&m_mutex);
        reanchorLocked(index, m_clock.nsecsElapsed());
        m_masters[index].rate = qBound(0.0, rate, 2.0);
    }
    emit changed(index);
}

bool SpeedMasters::isPaused(int index) const
{
    QMutexLocker locker(&m_mutex);
    return valid(index) && m_masters[index].paused;
}

void SpeedMasters::setPaused(int index, bool paused)
{
    if (!valid(index))
        return;
    double mainPos, mainBpm;
    mainClock(&mainPos, &mainBpm);
    {
        QMutexLocker locker(&m_mutex);
        Master &m = m_masters[index];
        if (m.paused == paused)
            return;
        const qint64 now = m_clock.nsecsElapsed();
        if (paused)
            m.anchorBeats = positionLocked(index, now, mainPos);   // freeze here
        // An Own master carries on from where it stopped; a Follow master
        // drops back onto what it follows (positionLocked() ignores the anchor).
        m.anchorNs = now;
        m.paused = paused;
    }
    emit changed(index);
}

bool SpeedMasters::tap(int index)
{
    if (!valid(index))
        return false;
    {
        QMutexLocker locker(&m_mutex);
        Master &m = m_masters[index];
        if (m.mode != Own)
            return false;
        const qint64 now = m_clock.nsecsElapsed();
        if (!m.taps.isEmpty() && now - m.taps.last() > 2000000000LL)
            m.taps.clear();
        m.taps << now;
        while (m.taps.size() > 5)
            m.taps.removeFirst();

        reanchorLocked(index, now);
        if (m.taps.size() >= 2)
        {
            const double avgNs = double(m.taps.last() - m.taps.first()) / (m.taps.size() - 1);
            const double heard = 60e9 / avgNs;
            // The rate fader stays where it is: the tempo HEARD is the tap's.
            m.bpm = qBound(1.0, m.rate > 0 ? heard / m.rate : heard, 999.0);
        }
        if (!m.paused)
            m.anchorBeats = std::round(m.anchorBeats);    // the beat is now
    }
    emit changed(index);
    return true;
}

/*********************************************************************
 * The clocks
 *********************************************************************/

void SpeedMasters::mainClock(double *pos, double *bpm) const
{
    MasterTimer *mt = m_doc != nullptr ? m_doc->masterTimer() : nullptr;
    *pos = mt != nullptr ? mt->beatPosition() : 0.0;
    *bpm = mt != nullptr ? mt->bpm() : 0.0;

    /* The show timeline, read here -- outside m_mutex -- for the masters
       that follow it: its playhead in beats of the show's grid. */
    bool wanted;
    {
        QMutexLocker locker(&m_mutex);
        wanted = anyFollowsTimelineLocked();
    }
    double tp = 0, tb = 0;
    const bool running = wanted && timelineClock(&tp, &tb);
    QMutexLocker locker(&m_mutex);
    const_cast<SpeedMasters *>(this)->m_timelinePos = running ? tp : *pos;
    const_cast<SpeedMasters *>(this)->m_timelineBpm = running ? tb : *bpm;
}

bool SpeedMasters::anyFollowsTimelineLocked() const
{
    for (int i = 0; i < m_count; ++i)
        if (m_masters[i].mode == Follow && m_masters[i].follows == ShowTimeline)
            return true;
    return false;
}

bool SpeedMasters::timelineClock(double *pos, double *bpm) const
{
    if (m_doc == nullptr)
        return false;
    foreach (Function *f, m_doc->functions())
    {
        Show *show = qobject_cast<Show *>(f);
        if (show == nullptr || !show->isRunning() || show->timeDivisionType() == Show::Time
            || show->timeDivisionBPM() <= 0)
            continue;
        *bpm = show->timeDivisionBPM();
        *pos = double(show->runningTime()) / (60000.0 / *bpm);
        return true;
    }
    return false;
}

double SpeedMasters::positionLocked(int index, qint64 now, double mainPos, int depth) const
{
    const Master &m = m_masters[index];
    if (m.paused)
        return m.anchorBeats;
    if (m.mode == Own || depth > MaxCount)
        return m.anchorBeats + double(now - m.anchorNs) / 1e9 * m.bpm * m.rate / 60.0;
    const double src = m.follows == MainClock ? mainPos
                     : m.follows == ShowTimeline ? m_timelinePos
                     : positionLocked(m.follows, now, mainPos, depth + 1);
    return src * m.ratio;
}

double SpeedMasters::bpmLocked(int index, double mainBpm, int depth) const
{
    const Master &m = m_masters[index];
    if (m.paused)
        return 0;
    if (m.mode == Own || depth > MaxCount)
        return m.bpm * m.rate;
    const double src = m.follows == MainClock ? mainBpm
                     : m.follows == ShowTimeline ? m_timelineBpm
                     : bpmLocked(m.follows, mainBpm, depth + 1);
    return src * m.ratio;
}

void SpeedMasters::reanchorLocked(int index, qint64 now)
{
    Master &m = m_masters[index];
    if (m.mode != Own || m.paused)
        return;
    m.anchorBeats = positionLocked(index, now, 0);
    m.anchorNs = now;
}

double SpeedMasters::bpm(int index) const
{
    if (!valid(index))
        return 0;
    double mainPos, mainBpm;
    mainClock(&mainPos, &mainBpm);
    QMutexLocker locker(&m_mutex);
    return bpmLocked(index, mainBpm);
}

double SpeedMasters::beatPosition(int index) const
{
    if (!valid(index))
        return 0;
    double mainPos, mainBpm;
    mainClock(&mainPos, &mainBpm);
    QMutexLocker locker(&m_mutex);
    return positionLocked(index, m_clock.nsecsElapsed(), mainPos);
}

bool SpeedMasters::clockFor(int assignment, double *beatPos, double *bpm) const
{
    double mainPos, mainBpm;
    mainClock(&mainPos, &mainBpm);
    if (assignment == MainClock)
    {
        *beatPos = mainPos;
        *bpm = mainBpm;
        return true;
    }
    if (!isMaster(assignment))
        return false;
    const int index = assignment - Master1;
    QMutexLocker locker(&m_mutex);
    *beatPos = positionLocked(index, m_clock.nsecsElapsed(), mainPos);
    *bpm = bpmLocked(index, mainBpm);
    return true;
}

/*********************************************************************
 * Control surface inputs
 *********************************************************************/

QString SpeedMasters::controlToString(Control control)
{
    switch (control)
    {
        case TapControl:   return QStringLiteral("Tap");
        case RateControl:  return QStringLiteral("Rate");
        case PauseControl: return QStringLiteral("Pause");
        default:           return QString();
    }
}

void SpeedMasters::setInput(int index, Control control, quint32 universe, quint32 channel)
{
    if (!valid(index) || control < 0 || control >= ControlCount)
        return;
    {
        QMutexLocker locker(&m_mutex);
        Master &m = m_masters[index];
        m.inUniverse[control] = universe;
        m.inChannel[control] = channel;
        m.inLast[control] = 0;
    }
    emit changed(index);
}

void SpeedMasters::clearInput(int index, Control control)
{
    setInput(index, control, UINT_MAX, UINT_MAX);
}

bool SpeedMasters::input(int index, Control control, quint32 *universe, quint32 *channel) const
{
    if (!valid(index) || control < 0 || control >= ControlCount)
        return false;
    QMutexLocker locker(&m_mutex);
    const Master &m = m_masters[index];
    if (universe != nullptr) *universe = m.inUniverse[control];
    if (channel != nullptr) *channel = m.inChannel[control];
    return m.inUniverse[control] != UINT_MAX;
}

void SpeedMasters::slotInputValue(quint32 universe, quint32 channel, uchar value)
{
    // Find what this channel drives first, then act outside the lock: tap(),
    // setRate() and setPaused() take it themselves.
    struct Hit { int index; Control control; bool pressed; };
    QList<Hit> hits;
    {
        QMutexLocker locker(&m_mutex);
        for (int i = 0; i < m_count; ++i)
        {
            Master &m = m_masters[i];
            for (int c = 0; c < ControlCount; ++c)
            {
                if (m.inUniverse[c] != universe || m.inChannel[c] != channel)
                    continue;
                hits << Hit{ i, Control(c), m.inLast[c] == 0 && value > 0 };
                m.inLast[c] = value;
            }
        }
    }
    foreach (const Hit &h, hits)
    {
        switch (h.control)
        {
            case TapControl:
                if (h.pressed)
                    tap(h.index);
            break;
            case RateControl:
            {
                // A fader's middle is 100 %: snap there, so a fader parked
                // near the centre detent runs the master at its tempo.
                double rate = value / 127.5;
                if (qAbs(rate - 1.0) < 0.02)
                    rate = 1.0;
                setRate(h.index, rate);
            }
            break;
            case PauseControl:
                if (h.pressed)
                    setPaused(h.index, !isPaused(h.index));
            break;
            default:
            break;
        }
    }
}

/*********************************************************************
 * Load & save
 *********************************************************************/

bool SpeedMasters::isDefault() const
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < MaxCount; ++i)
    {
        const Master &m = m_masters[i];
        if (!m.name.isEmpty() || m.mode != Own || m.bpm != 120.0 || m.rate != 1.0 || m.paused)
            return false;
        for (int c = 0; c < ControlCount; ++c)
            if (m.inUniverse[c] != UINT_MAX)
                return false;
    }
    return m_count == DefaultCount;
}

bool SpeedMasters::saveXML(QXmlStreamWriter *doc) const
{
    if (isDefault())
        return true;
    QMutexLocker locker(&m_mutex);
    doc->writeStartElement(KXMLQLCSpeedMasters);
    if (m_count != DefaultCount)
        doc->writeAttribute(QStringLiteral("Count"), QString::number(m_count));
    for (int i = 0; i < m_count; ++i)
    {
        const Master &m = m_masters[i];
        doc->writeStartElement(KXMLMaster);
        doc->writeAttribute(KXMLIndex, QString::number(i));
        if (!m.name.isEmpty())
            doc->writeAttribute(KXMLName, m.name);
        doc->writeAttribute(KXMLMode, m.mode == Follow ? QStringLiteral("Follow") : QStringLiteral("Own"));
        if (m.mode == Follow)
        {
            doc->writeAttribute(KXMLFollows, m.follows == MainClock ? QStringLiteral("Main")
                                           : m.follows == ShowTimeline ? QStringLiteral("Timeline")
                                           : QString::number(m.follows + 1));
            doc->writeAttribute(KXMLRatio, QString::number(m.ratio));
        }
        doc->writeAttribute(KXMLBpm, QString::number(m.bpm));
        if (m.rate != 1.0)
            doc->writeAttribute(KXMLRate, QString::number(m.rate));
        if (m.paused)
            doc->writeAttribute(KXMLPaused, QStringLiteral("1"));
        for (int c = 0; c < ControlCount; ++c)
        {
            if (m.inUniverse[c] == UINT_MAX)
                continue;
            doc->writeStartElement(QStringLiteral("Input"));
            doc->writeAttribute(QStringLiteral("Control"), controlToString(Control(c)));
            doc->writeAttribute(QStringLiteral("Universe"), QString::number(m.inUniverse[c]));
            doc->writeAttribute(QStringLiteral("Channel"), QString::number(m.inChannel[c]));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}

bool SpeedMasters::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCSpeedMasters)
        return false;
    reset();
    setCount(root.attributes().value(QStringLiteral("Count")).toString().toInt());
    struct Pending { int index; int follows; double ratio; };
    QList<Pending> follows;
    while (root.readNextStartElement())
    {
        if (root.name() != KXMLMaster)
        {
            root.skipCurrentElement();
            continue;
        }
        const QXmlStreamAttributes a = root.attributes();
        const int i = a.value(KXMLIndex).toString().toInt();
        if (valid(i))
        {
            {
                QMutexLocker locker(&m_mutex);
                Master &m = m_masters[i];
                m.name = a.value(KXMLName).toString();
                if (a.hasAttribute(KXMLBpm))
                {
                    const double bpm = a.value(KXMLBpm).toString().toDouble();
                    if (bpm >= 1.0 && bpm <= 999.0)
                        m.bpm = bpm;
                }
                if (a.hasAttribute(KXMLRate))
                    m.rate = qBound(0.0, a.value(KXMLRate).toString().toDouble(), 2.0);
                m.paused = a.value(KXMLPaused).toString() == QLatin1String("1");
            }
            if (a.value(KXMLMode).toString() == QLatin1String("Follow"))
            {
                const QString f = a.value(KXMLFollows).toString();
                const int src = f == QLatin1String("Main") ? int(MainClock)
                              : f == QLatin1String("Timeline") ? int(ShowTimeline)
                              : f.toInt() - 1;
                follows << Pending{ i, src, a.value(KXMLRatio).toString().toDouble() };
            }
        }
        while (root.readNextStartElement())
        {
            if (root.name() == QLatin1String("Input") && valid(i))
            {
                const QXmlStreamAttributes in = root.attributes();
                for (int c = 0; c < ControlCount; ++c)
                    if (in.value(QStringLiteral("Control")) == controlToString(Control(c)))
                        setInput(i, Control(c), in.value(QStringLiteral("Universe")).toString().toUInt(),
                                 in.value(QStringLiteral("Channel")).toString().toUInt());
            }
            root.skipCurrentElement();
        }
    }
    // Follows last, once every master exists -- setFollow() refuses loops a
    // hand-edited file might have.
    foreach (const Pending &p, follows)
        if (!setFollow(p.index, p.follows, p.ratio))
            qWarning() << "[SpeedMasters] master" << p.index + 1 << "can't follow" << p.follows;
    for (int i = 0; i < MaxCount; ++i)
        emit changed(i);
    return true;
}
