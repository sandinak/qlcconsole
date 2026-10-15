/*
  Q Light Controller Plus
  showcontrol.cpp

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
#include <QtEndian>
#include <cstring>

#include "functionparent.h"
#include "chaseraction.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "function.h"
#include "chaser.h"
#include "doc.h"

#define KXMLQLCShowControlMainCueList QStringLiteral("MainCueList")

ShowControl::ShowControl(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_mainCueList(UINT_MAX)
    , m_mscDeviceId(1)
    , m_mscEnabled(true)
    , m_source(Local)
    , m_sourceUniverse(UINT_MAX)
{
    if (m_doc != nullptr)
        connect(m_doc, &Doc::functionRemoved, this, &ShowControl::slotFunctionRemoved);
}

QString ShowControl::commandName(Command command)
{
    switch (command)
    {
        case Go:     return QStringLiteral("GO");
        case Stop:   return QStringLiteral("STOP");
        case Pause:  return QStringLiteral("PAUSE");
        case Resume: return QStringLiteral("RESUME");
        case Load:   return QStringLiteral("LOAD");
        case AllOff: return QStringLiteral("ALL OFF");
        case Reset:  return QStringLiteral("RESET");
        case Back:   return QStringLiteral("BACK");
    }
    return QString();
}

/*****************************************************************************
 * Cue lists
 *****************************************************************************/

void ShowControl::setMainCueList(quint32 chaserId)
{
    if (chaserId == m_mainCueList)
        return;
    m_mainCueList = chaserId;
    if (m_doc != nullptr)
        m_doc->setModified();
}

Chaser *ShowControl::cueList(const QString &list) const
{
    if (m_doc == nullptr)
        return nullptr;

    QList<Chaser *> chasers;
    for (Function *f : m_doc->functionsByType(Function::ChaserType))
        if (Chaser *c = qobject_cast<Chaser *>(f))
            chasers << c;

    if (!list.trimmed().isEmpty())
    {
        for (Chaser *c : chasers)
            if (Chaser::cuesMatch(c->cueListNumber(), list))
                return c;
        return nullptr;
    }
    if (Chaser *c = qobject_cast<Chaser *>(m_doc->function(m_mainCueList)))
        return c;
    return chasers.count() == 1 ? chasers.first() : nullptr;
}

QString ShowControl::listName(const Chaser *chaser) const
{
    if (chaser == nullptr)
        return QString();
    if (chaser->cueListNumber().isEmpty())
        return chaser->name();
    return tr("list %1 (%2)").arg(chaser->cueListNumber(), chaser->name());
}

/*****************************************************************************
 * Commands
 *****************************************************************************/

void ShowControl::go(Chaser *chaser, int step)
{
    // As the VC cue list drives a chaser: the action first, then start it
    // if it isn't running (a stopped chaser starts at the action's step).
    ChaserAction action;
    action.m_action = step < 0 ? ChaserNextStep : ChaserSetStepIndex;
    action.m_stepIndex = qMax(step, 0);
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    chaser->setAction(action);

    if (!chaser->isRunning())
        chaser->start(m_doc->masterTimer(), FunctionParent::master());
    else if (chaser->isPaused())
        chaser->setPause(false);
}

int ShowControl::standbyStep(const Chaser *chaser) const
{
    if (chaser == nullptr)
        return -1;
    const int loaded = m_loaded.value(chaser->id(), -1);
    if (loaded >= 0 && loaded < chaser->stepsCount())
        return loaded;
    if (chaser->isRunning())
        return chaser->computeNextStep(chaser->currentStepIndex());
    return chaser->firstPlayableStep();
}

bool ShowControl::execute(const Request &request)
{
    QString what = commandName(request.command);
    if (!request.cue.isEmpty())
        what += tr(" cue %1").arg(request.cue);

    auto finish = [&](bool done, const QString &how) {
        const QString line = how.isEmpty() ? what : what + QStringLiteral(" — ") + how;
        emit commandHandled(line, done);
        emit commandFrom(m_source, m_sourceUniverse, line, done);
        return done;
    };

    if (m_doc == nullptr)
        return false;

    if (request.command == AllOff)
    {
        // Every running function, stopped on the next tick -- not
        // MasterTimer::stopAllFunctions(), which waits for the timer
        // thread, here on the thread a command arrives on.
        m_loaded.clear();
        for (Function *f : m_doc->functions())
            if (f->isRunning())
                f->stop(FunctionParent::master());
        return finish(true, tr("everything stopped"));
    }

    Chaser *chaser = cueList(request.list);
    if (chaser == nullptr)
    {
        if (!request.list.isEmpty())
            return finish(false, tr("no cue list %1").arg(request.list));
        return finish(false, tr("no main cue list (mark one in its chaser)"));
    }
    const QString name = listName(chaser);

    int step = -1;
    if (!request.cue.isEmpty())
    {
        step = chaser->findCue(request.cue);
        if (step < 0)
            return finish(false, tr("no such cue in %1").arg(name));
    }

    switch (request.command)
    {
        case Go:
            if (step < 0)
            {
                // The cue on standby; else the next one -- from stopped,
                // the first.
                step = m_loaded.value(chaser->id(), -1);
                if (step < 0 && !chaser->isRunning())
                {
                    step = chaser->firstPlayableStep();
                    if (step < 0)
                        return finish(false, tr("every cue in %1 is skipped").arg(name));
                }
            }
            m_loaded.remove(chaser->id());
            go(chaser, step);
            if (step >= 0 && request.cue.isEmpty())
                what += tr(" cue %1").arg(chaser->stepCue(step));
            return finish(true, name);

        case Load:
            if (step < 0)
                return finish(false, tr("LOAD needs a cue"));
            m_loaded.insert(chaser->id(), step);
            return finish(true, name);

        case Stop:
            if (!chaser->isRunning())
                return finish(false, tr("%1 isn't playing").arg(name));
            if (step >= 0 && step != chaser->currentStepIndex())
                return finish(false, tr("cue %1 isn't the one playing").arg(request.cue));
            chaser->stop(FunctionParent::master());
            return finish(true, name);

        case Pause:
            if (!chaser->isRunning())
                return finish(false, tr("%1 isn't playing").arg(name));
            chaser->setPause(true);
            return finish(true, name);

        case Resume:
            if (!chaser->isRunning() || !chaser->isPaused())
                return finish(false, tr("%1 isn't paused").arg(name));
            chaser->setPause(false);
            return finish(true, name);

        case Back:
        {
            if (!chaser->isRunning())
                return finish(false, tr("%1 isn't playing").arg(name));
            ChaserAction action;
            action.m_action = ChaserPreviousStep;
            action.m_stepIndex = 0;
            action.m_masterIntensity = 1.0;
            action.m_stepIntensity = 1.0;
            action.m_fadeMode = Chaser::FromFunction;
            chaser->setAction(action);
            m_loaded.remove(chaser->id());
            return finish(true, name);
        }

        case Reset:
            m_loaded.remove(chaser->id());
            if (chaser->isRunning())
                chaser->stop(FunctionParent::master());
            return finish(true, name);

        case AllOff:
            break;
    }
    return false;
}

void ShowControl::slotFunctionRemoved(quint32 id)
{
    m_loaded.remove(id);
    if (id == m_mainCueList)
        m_mainCueList = UINT_MAX;
}

/*****************************************************************************
 * MIDI Show Control
 *****************************************************************************/

void ShowControl::setMscDeviceId(int id)
{
    m_mscDeviceId = qBound(0, id, 0x6F);
}

bool ShowControl::parseMsc(const QByteArray &sysEx, int deviceId, Request *out)
{
    QByteArray b = sysEx;
    if (b.startsWith(char(0xF0)))
        b.remove(0, 1);
    if (b.endsWith(char(0xF7)))
        b.chop(1);

    // 7F <device> 02 <format> <command> <data...>
    if (b.size() < 4 || uchar(b.at(0)) != 0x7F || uchar(b.at(2)) != 0x02)
        return false;

    const int device = uchar(b.at(1));
    if (device != deviceId && device < 0x70)       // 70..7E groups, 7F all-call
        return false;

    const int format = uchar(b.at(3));
    if (format != 0x01 && format != 0x02 && format != 0x7F)
        return false;                               // lighting, moving lights, all
    if (b.size() < 5)
        return false;

    Request r;
    QByteArray data = b.mid(5);
    switch (uchar(b.at(4)))
    {
        case 0x01: r.command = Go; break;
        case 0x02: r.command = Pause; break;        // MSC STOP halts; RESUME continues
        case 0x03: r.command = Resume; break;
        case 0x04: r.command = Go; data = data.mid(5); break;   // TIMED_GO: time first
        case 0x05: r.command = Load; break;
        case 0x08: r.command = AllOff; break;
        case 0x0A: r.command = Reset; break;
        case 0x0B: r.command = Stop; break;         // GO_OFF
        default: return false;
    }

    // Cue, list and path, ASCII, each ended by 00 (the last may not be).
    const QList<QByteArray> fields = data.split('\0');
    auto field = [&fields](int i) {
        return i < fields.count() ? QString::fromLatin1(fields.at(i)).trimmed() : QString();
    };
    r.cue = field(0);
    r.list = field(1);
    if (out != nullptr)
        *out = r;
    return true;
}

void ShowControl::handleSysEx(quint32 universe, const QByteArray &sysEx)
{
    if (!m_mscEnabled)
        return;
    Request r;
    if (!parseMsc(sysEx, m_mscDeviceId, &r))
        return;
    m_source = Msc;
    m_sourceUniverse = universe;
    execute(r);
    m_source = Local;
    m_sourceUniverse = UINT_MAX;
}

/*****************************************************************************
 * OSC
 *****************************************************************************/

namespace
{
/** Reads an OSC string (NUL-ended, padded to 4) at @a pos; false past the end. */
bool oscString(const QByteArray &d, int &pos, QString *out)
{
    const int end = d.indexOf('\0', pos);
    if (end < 0)
        return false;
    if (out != nullptr)
        *out = QString::fromUtf8(d.constData() + pos, end - pos);
    pos = (end + 4) & ~3;
    return pos <= d.size();
}

bool oscInt32(const QByteArray &d, int &pos, qint32 *out)
{
    if (pos + 4 > d.size())
        return false;
    *out = qFromBigEndian<qint32>(reinterpret_cast<const uchar *>(d.constData() + pos));
    pos += 4;
    return true;
}

/** A float cue the way it's written: 12.5, not 12.500000. */
QString cueFromNumber(double v)
{
    return QString::number(v, 'g', 10);
}

void parseOscMessage(const QByteArray &d, QList<ShowControl::Request> *out)
{
    int pos = 0;
    QString address;
    if (!oscString(d, pos, &address) || !address.startsWith(QLatin1Char('/')))
        return;

    // The first argument, as a cue: int, float or string.
    QString arg;
    QString tags;
    if (pos < d.size() && d.at(pos) == ',' && oscString(d, pos, &tags) && tags.size() > 1)
    {
        const QChar t = tags.at(1);
        if (t == QLatin1Char('i'))
        {
            qint32 v;
            if (oscInt32(d, pos, &v))
                arg = QString::number(v);
        }
        else if (t == QLatin1Char('f'))
        {
            qint32 v;
            if (oscInt32(d, pos, &v))
            {
                float f;
                std::memcpy(&f, &v, sizeof f);
                arg = cueFromNumber(f);
            }
        }
        else if (t == QLatin1Char('d') && pos + 8 <= d.size())
        {
            const quint64 v = qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(d.constData() + pos));
            double f;
            std::memcpy(&f, &v, sizeof f);
            arg = cueFromNumber(f);
        }
        else if (t == QLatin1Char('s'))
            oscString(d, pos, &arg);
    }

    // [/qlcconsole][/list/<n>]/<command>...
    auto verb = [](const QString &s) { return s.toLower(); };
    QStringList parts = address.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!parts.isEmpty() && verb(parts.first()) == QLatin1String("qlcconsole"))
        parts.removeFirst();

    ShowControl::Request r;
    if (parts.size() >= 2 && verb(parts.first()) == QLatin1String("list"))
    {
        r.list = parts.at(1);
        parts = parts.mid(2);
    }
    if (parts.isEmpty())
        return;

    const QString first = verb(parts.first());

    if (first == QLatin1String("cue") && parts.size() == 3)
    {
        // /cue/<cue>/<what>
        r.cue = parts.at(1);
        const QString what = verb(parts.at(2));
        if (what == QLatin1String("go") || what == QLatin1String("start"))
            r.command = ShowControl::Go;
        else if (what == QLatin1String("load"))
            r.command = ShowControl::Load;
        else if (what == QLatin1String("stop"))
            r.command = ShowControl::Stop;
        else
            return;
        out->append(r);
        return;
    }

    if (parts.size() > 2)
        return;
    r.cue = parts.size() == 2 ? parts.at(1) : arg;

    if (first == QLatin1String("go") || first == QLatin1String("start"))
        r.command = ShowControl::Go;
    else if (first == QLatin1String("load"))
        r.command = ShowControl::Load;
    else if (first == QLatin1String("stop"))
        r.command = ShowControl::Stop;
    else if (first == QLatin1String("pause"))
        r.command = ShowControl::Pause;
    else if (first == QLatin1String("resume"))
        r.command = ShowControl::Resume;
    else if (first == QLatin1String("reset"))
        r.command = ShowControl::Reset;
    else if (first == QLatin1String("back"))
        r.command = ShowControl::Back;
    else if (first == QLatin1String("panic") || first == QLatin1String("alloff"))
        r.command = ShowControl::AllOff;
    else
        return;
    if (r.command == ShowControl::AllOff || r.command == ShowControl::Pause
            || r.command == ShowControl::Resume || r.command == ShowControl::Reset
            || r.command == ShowControl::Back)
        r.cue.clear();                  // these take no cue
    out->append(r);
}

void parseOscPacket(const QByteArray &d, QList<ShowControl::Request> *out, int depth)
{
    if (depth > 8)
        return;
    if (d.startsWith(QByteArrayLiteral("#bundle\0")))
    {
        int pos = 16;                   // "#bundle\0" + timetag
        while (pos + 4 <= d.size())
        {
            qint32 size;
            if (!oscInt32(d, pos, &size) || size <= 0 || pos + size > d.size())
                return;
            parseOscPacket(d.mid(pos, size), out, depth + 1);
            pos += size;
        }
        return;
    }
    parseOscMessage(d, out);
}
}

QList<ShowControl::Request> ShowControl::parseOsc(const QByteArray &datagram)
{
    QList<Request> out;
    parseOscPacket(datagram, &out, 0);
    return out;
}

int ShowControl::handleOsc(const QByteArray &datagram)
{
    int done = 0;
    m_source = Osc;
    for (const Request &r : parseOsc(datagram))
        if (execute(r))
            ++done;
    m_source = Local;
    return done;
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

void ShowControl::reset()
{
    m_mainCueList = UINT_MAX;
    m_loaded.clear();
}

bool ShowControl::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCShowControl)
        return false;
    bool ok = false;
    const quint32 id = root.attributes().value(KXMLQLCShowControlMainCueList).toString().toUInt(&ok);
    m_mainCueList = ok ? id : UINT_MAX;
    root.skipCurrentElement();
    return true;
}

bool ShowControl::saveXML(QXmlStreamWriter *doc) const
{
    if (m_mainCueList == UINT_MAX)
        return true;
    doc->writeStartElement(KXMLQLCShowControl);
    doc->writeAttribute(KXMLQLCShowControlMainCueList, QString::number(m_mainCueList));
    doc->writeEndElement();
    return true;
}
