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
#include <QThread>
#include <cstring>

#include "functionparent.h"
#include "chaseraction.h"
#include "inputoutputmap.h"
#include "showcontrol.h"
#include "inputpatch.h"
#include "mastertimer.h"
#include "function.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "chaser.h"
#include "doc.h"

#define KXMLQLCShowControlMainCueList QStringLiteral("MainCueList")
#define KXMLQLCShowControlOut QStringLiteral("Out")

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
        case SubGo:  return QStringLiteral("SUB GO");
        case SubBack: return QStringLiteral("SUB BACK");
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
    emit mainCueListChanged(m_mainCueList);
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

void ShowControl::goSubCue(Chaser *chaser, int step, int subCue)
{
    // A jump to a sub-cue: its parent first, if that isn't the one playing
    // (the runner sees to it); a stopped list starts there.
    chaser->jumpToSubCue(step, subCue);
    if (!chaser->isRunning())
        chaser->start(m_doc->masterTimer(), FunctionParent::master());
    else if (chaser->isPaused())
        chaser->setPause(false);
}

QString ShowControl::cueName(const Chaser *chaser, int step, int subCue)
{
    if (chaser == nullptr)
        return QString();
    return subCue < 0 ? chaser->stepCue(step) : chaser->subCue(step, subCue);
}

int ShowControl::standbyStep(const Chaser *chaser) const
{
    if (chaser == nullptr)
        return -1;
    const int loaded = m_loaded.value(chaser->id()).step;
    if (loaded >= 0 && loaded < chaser->stepsCount())
        return loaded;
    if (chaser->isRunning())
        return chaser->computeNextStep(chaser->currentStepIndex());
    return chaser->firstPlayableStep();
}

int ShowControl::standbySubCue(const Chaser *chaser) const
{
    if (chaser == nullptr)
        return -1;
    const Standby loaded = m_loaded.value(chaser->id());
    if (loaded.step < 0 || loaded.step >= chaser->stepsCount()
            || loaded.subCue < 0 || loaded.subCue >= chaser->subCueCount(loaded.step))
        return -1;
    return loaded.subCue;
}

QString ShowControl::routeName(Route route)
{
    switch (route)
    {
        case RouteKey:        return QStringLiteral("key");
        case RouteRunTab:     return QStringLiteral("Run tab");
        case RouteVcRunner:   return QStringLiteral("VC Cue Runner");
        case RoutePad:        return QStringLiteral("pad");
        case RouteControlMap: return QStringLiteral("Control Map");
        case RouteFocusCall:  return QStringLiteral("focus call");
        case RouteOsc:        return QStringLiteral("OSC");
        case RouteMsc:        return QStringLiteral("MSC");
        case RouteWeb:        return QStringLiteral("web");
        case RouteOther:      break;
    }
    return QStringLiteral("other");
}

bool ShowControl::run(const Request &request, Route route)
{
    /* The one way in (docs/design/RUN_COMMANDS.md). The handler is the UI's
       (notices, the preview hold it reports): only on this object's thread.
       Off it -- a command arriving on a plugin's thread -- execute() alone,
       whose aboutToTakeStage() still holds the previews, queued. */
    if (m_runHandler && QThread::currentThread() == thread())
        return m_runHandler(request, route);
    return execute(request);
}

bool ShowControl::wouldTakeStage(const Request &request) const
{
    if (m_doc == nullptr || !takesStage(request.command))
        return false;
    const Chaser *chaser = request.chaser != UINT_MAX ? qobject_cast<const Chaser *>(m_doc->function(request.chaser))
                                                      : cueList(request.list);
    if (chaser == nullptr)
        return false;
    // As execute(): Sub GO / Sub Back take no cue; a named cue must be there.
    if (request.command == SubGo || request.command == SubBack || request.cue.isEmpty())
        return true;
    int step = -1, sub = -1;
    return chaser->findCueOrSubCue(request.cue, &step, &sub);
}

bool ShowControl::execute(const Request &request)
{
    // Sub GO / Sub Back take no cue: they step through the cue playing.
    const bool subStep = request.command == SubGo || request.command == SubBack;
    const QString cue = subStep ? QString() : request.cue;
    QString what = commandName(request.command);
    if (!cue.isEmpty())
        what += tr(" cue %1").arg(cue);

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

    Chaser *chaser = request.chaser != UINT_MAX ? qobject_cast<Chaser *>(m_doc->function(request.chaser))
                                                : cueList(request.list);
    if (chaser == nullptr)
    {
        if (!request.list.isEmpty())
            return finish(false, tr("no cue list %1").arg(request.list));
        return finish(false, tr("no main cue list (mark one in its chaser)"));
    }
    const QString name = listName(chaser);

    // A cue by number: a main cue -- one numbered "4.2" wins -- else a
    // sub-cue, "4.2" being sub-cue 2 of cue 4 (or one numbered so).
    int step = -1;
    int sub = -1;
    if (!cue.isEmpty())
    {
        if (!chaser->findCueOrSubCue(cue, &step, &sub))
            return finish(false, tr("no such cue in %1").arg(name));
    }

    // What runs takes the stage from the previews first (RC8).
    if (takesStage(request.command))
        emit aboutToTakeStage(m_source);

    int sentStep = -1;
    switch (request.command)
    {
        case Go:
            if (step < 0)
            {
                // The cue on standby; else the next one -- from stopped,
                // the first.
                const Standby loaded = m_loaded.value(chaser->id());
                step = loaded.step;
                sub = standbySubCue(chaser);
                if (step < 0 && !chaser->isRunning())
                {
                    step = chaser->firstPlayableStep();
                    if (step < 0)
                        return finish(false, tr("every cue in %1 is skipped").arg(name));
                }
            }
            // The cue this GO plays, found before it acts (a GO on a running
            // list plays the next).
            sentStep = step >= 0 ? step : standbyStep(chaser);
            m_loaded.remove(chaser->id());
            if (sub >= 0)
                goSubCue(chaser, step, sub);
            else
                go(chaser, step);
            // On to QLab and the like -- only a command from this console:
            // one that came in by OSC or MSC is not echoed back.
            if (m_source == Local && sentStep >= 0)
                sendOut(chaser, sentStep, cueName(chaser, sentStep, sub));
            if (step >= 0 && cue.isEmpty())
                what += tr(" cue %1").arg(cueName(chaser, step, sub));
            // A skipped cue named: played, as asked (GO alone passes over it).
            else if (!cue.isEmpty() && sub < 0 && chaser->isStepSkipped(step))
                what += tr(" (skipped: played as named)");
            return finish(true, name);

        case Load:
            if (step < 0)
                return finish(false, tr("LOAD needs a cue"));
            m_loaded.insert(chaser->id(), Standby{ step, sub });
            return finish(true, name);

        case Stop:
            if (!chaser->isRunning())
                return finish(false, tr("%1 isn't playing").arg(name));
            if (step >= 0 && (step != chaser->currentStepIndex()
                              || (sub >= 0 && sub != chaser->currentSubCueIndex())))
                return finish(false, tr("cue %1 isn't the one playing").arg(cue));
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

        case SubGo:
        case SubBack:
        {
            if (!chaser->isRunning())
                return finish(false, tr("%1 isn't playing").arg(name));
            const int current = chaser->currentStepIndex();
            const QString parent = chaser->stepCue(current);
            if (!chaser->isSubGoStep(current))
                return finish(false, tr("cue %1 has no sub-cues on Sub GO").arg(parent));
            // Not yet started (GO into the cue this very moment): the
            // runner steps it once it has, so there's nothing to check.
            if (chaser->currentSubCueIndex() >= 0)
            {
                const int target = request.command == SubGo ? chaser->nextSubCueIndex()
                                                            : chaser->previousSubCueIndex();
                if (target < 0)
                    return finish(false, request.command == SubGo
                                         ? tr("end of %1").arg(parent)
                                         : tr("%1 is at its first sub-cue").arg(parent));
                what += tr(" cue %1").arg(chaser->subCue(current, target));
            }
            if (request.command == SubGo)
                chaser->subGo();
            else
                chaser->subBack();
            return finish(true, name);
        }

        case AllOff:
            break;
    }
    return false;
}

void ShowControl::slotFunctionRemoved(quint32 id)
{
    m_loaded.remove(id);
    if (id == m_mainCueList)
    {
        m_mainCueList = UINT_MAX;
        emit mainCueListChanged(m_mainCueList);
    }
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

void ShowControl::setMscInput(const QString &plugin, const QString &input)
{
    m_mscInputPlugin = input.isEmpty() ? QString() : plugin;
    m_mscInput = plugin.isEmpty() ? QString() : input;
}

bool ShowControl::acceptsMscFrom(quint32 universe) const
{
    if (m_mscInput.isEmpty())
        return true;
    if (m_doc == nullptr || m_doc->inputOutputMap() == nullptr)
        return false;
    const InputPatch *ip = m_doc->inputOutputMap()->inputPatch(universe);
    return ip != nullptr && ip->pluginName() == m_mscInputPlugin && ip->inputName() == m_mscInput;
}

QList<quint32> ShowControl::mscUniverses() const
{
    QList<quint32> list;
    if (m_doc == nullptr || m_doc->inputOutputMap() == nullptr)
        return list;
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 i = 0; i < iom->universesCount(); i++)
    {
        const quint32 uni = iom->getUniverseID(i);
        if (iom->inputPatch(uni) != nullptr && acceptsMscFrom(uni))
            list << uni;
    }
    return list;
}

void ShowControl::handleSysEx(quint32 universe, const QByteArray &sysEx)
{
    if (!m_mscEnabled || !acceptsMscFrom(universe))
        return;
    Request r;
    if (!parseMsc(sysEx, m_mscDeviceId, &r))
        return;
    m_source = Msc;
    m_sourceUniverse = universe;
    run(r, RouteMsc);
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

/** The same for a 32-bit float, to the digits it holds: 2.2, not the
 *  2.200000048 it is as a double -- which no cue "2.2" would match. */
QString cueFromFloat(float v)
{
    return QString::number(double(v), 'g', 7);
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
                arg = cueFromFloat(f);
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
    else if (first == QLatin1String("subgo"))
        r.command = ShowControl::SubGo;
    else if (first == QLatin1String("subback"))
        r.command = ShowControl::SubBack;
    else
        return;
    if (r.command == ShowControl::AllOff || r.command == ShowControl::Pause
            || r.command == ShowControl::Resume || r.command == ShowControl::Reset
            || r.command == ShowControl::Back || r.command == ShowControl::SubGo
            || r.command == ShowControl::SubBack)
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
        if (run(r, RouteOsc))
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
    m_out = OutSettings();
}

/*****************************************************************************
 * Show control out
 *****************************************************************************/

void ShowControl::setOutSettings(const OutSettings &settings)
{
    m_out = settings;
    m_out.oscPort = qBound(1, m_out.oscPort, 65535);
    m_out.mscDevice = (m_out.mscDevice >= 0 && m_out.mscDevice <= 0x6F) ? m_out.mscDevice : 0x7F;
    if (m_out.oscPath.trimmed().isEmpty())
        m_out.oscPath = OutSettings().oscPath;
}

QByteArray ShowControl::oscMessage(const QString &path)
{
    QByteArray b = path.toUtf8();
    b.append(QByteArray(4 - (b.size() % 4), '\0'));
    b.append(",\0\0\0", 4);
    return b;
}

QByteArray ShowControl::mscGo(int deviceId, const QString &cue)
{
    QByteArray b;
    b.append(char(0xF0));
    b.append(char(0x7F));
    b.append(char(deviceId & 0x7F));
    b.append(char(0x02));       // MSC
    b.append(char(0x01));       // lighting
    b.append(char(0x01));       // GO
    b.append(cue.toLatin1());
    b.append(char(0xF7));
    return b;
}

QList<ShowControl::OutMessage> ShowControl::outMessagesFor(const Chaser *chaser, int step,
                                                           const QString &cue) const
{
    QList<OutMessage> list;
    if (chaser == nullptr || cue.isEmpty())
        return list;
    if (m_out.cues == OutNumberedCues && !chaser->stepHasCueNumber(step))
        return list;

    if (m_out.oscEnabled && !m_out.oscHost.trimmed().isEmpty())
    {
        OutMessage m;
        m.kind = OutMessage::Osc;
        m.line = m_out.oscLine;
        m.host = m_out.oscHost.trimmed();
        m.port = m_out.oscPort;
        QString path = m_out.oscPath;
        path.replace(QStringLiteral("{cue}"), cue);
        m.data = oscMessage(path);
        list << m;
    }
    if (m_out.mscEnabled && !m_out.mscOutput.isEmpty())
    {
        OutMessage m;
        m.kind = OutMessage::Msc;
        m.line = m_out.mscOutput;
        m.data = mscGo(m_out.mscDevice, cue);
        list << m;
    }
    return list;
}

void ShowControl::sendOut(const Chaser *chaser, int step, const QString &cue)
{
    const QList<OutMessage> messages = outMessagesFor(chaser, step, cue);
    for (const OutMessage &m : messages)
    {
        const QString what = m.kind == OutMessage::Osc
            ? tr("sent %1 to %2:%3").arg(QString::fromUtf8(m.data.left(m.data.indexOf('\0'))), m.host)
                  .arg(m.port)
            : tr("sent MSC GO %1 to device %2").arg(cue).arg(m_out.mscDevice);
        // The sockets and MIDI ports belong to the main thread.
        if (QThread::currentThread() == thread())
        {
            const bool ok = m_outSender ? m_outSender(m) : sendViaPlugin(m);
            emit sentOut(what, ok);
        }
        else
        {
            QMetaObject::invokeMethod(this, [this, m, what]() {
                const bool ok = m_outSender ? m_outSender(m) : sendViaPlugin(m);
                emit sentOut(what, ok);
            }, Qt::QueuedConnection);
        }
    }
}

bool ShowControl::sendViaPlugin(const OutMessage &message)
{
    if (m_doc == nullptr || m_doc->ioPluginCache() == nullptr)
        return false;
    const bool osc = message.kind == OutMessage::Osc;
    QLCIOPlugin *plugin = m_doc->ioPluginCache()->plugin(osc ? QStringLiteral("OSC") : QStringLiteral("MIDI"));
    if (plugin == nullptr)
        return false;
    const QStringList outs = plugin->outputs();
    int line = outs.indexOf(message.line);
    if (line < 0 && osc && message.line.isEmpty())
    {
        // The interface on the target's subnet (/24); else the first.
        const QString prefix = message.host.left(message.host.lastIndexOf('.') + 1);
        for (int i = 0; i < outs.count() && line < 0; ++i)
            if (!prefix.isEmpty() && outs.at(i).startsWith(prefix))
                line = i;
        if (line < 0 && !outs.isEmpty())
            line = 0;
    }
    if (line < 0)
        return false;
    return plugin->sendShowControl(quint32(line), message.data, message.host, quint16(message.port));
}

bool ShowControl::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCShowControl)
        return false;
    bool ok = false;
    const quint32 id = root.attributes().value(KXMLQLCShowControlMainCueList).toString().toUInt(&ok);
    m_mainCueList = ok ? id : UINT_MAX;
    OutSettings out;
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCShowControlOut)
        {
            const QXmlStreamAttributes a = root.attributes();
            out.oscEnabled = a.value(QStringLiteral("Osc")) == QStringLiteral("1");
            out.oscHost = a.value(QStringLiteral("Host")).toString();
            out.oscPort = a.value(QStringLiteral("Port")).toInt();
            if (out.oscPort <= 0)
                out.oscPort = OutSettings().oscPort;
            out.oscLine = a.value(QStringLiteral("Line")).toString();
            if (a.hasAttribute(QStringLiteral("Path")))
                out.oscPath = a.value(QStringLiteral("Path")).toString();
            out.mscEnabled = a.value(QStringLiteral("Msc")) == QStringLiteral("1");
            out.mscOutput = a.value(QStringLiteral("MidiOut")).toString();
            if (a.hasAttribute(QStringLiteral("Device")))
                out.mscDevice = a.value(QStringLiteral("Device")).toInt();
            out.cues = a.value(QStringLiteral("Cues")) == QStringLiteral("numbered") ? OutNumberedCues
                                                                                       : OutAllCues;
        }
        root.skipCurrentElement();
    }
    setOutSettings(out);
    return true;
}

bool ShowControl::saveXML(QXmlStreamWriter *doc) const
{
    if (m_mainCueList == UINT_MAX && m_out == OutSettings())
        return true;
    doc->writeStartElement(KXMLQLCShowControl);
    if (m_mainCueList != UINT_MAX)
        doc->writeAttribute(KXMLQLCShowControlMainCueList, QString::number(m_mainCueList));
    if (!(m_out == OutSettings()))
    {
        doc->writeStartElement(KXMLQLCShowControlOut);
        doc->writeAttribute(QStringLiteral("Osc"), m_out.oscEnabled ? QStringLiteral("1") : QStringLiteral("0"));
        doc->writeAttribute(QStringLiteral("Host"), m_out.oscHost);
        doc->writeAttribute(QStringLiteral("Port"), QString::number(m_out.oscPort));
        doc->writeAttribute(QStringLiteral("Line"), m_out.oscLine);
        doc->writeAttribute(QStringLiteral("Path"), m_out.oscPath);
        doc->writeAttribute(QStringLiteral("Msc"), m_out.mscEnabled ? QStringLiteral("1") : QStringLiteral("0"));
        doc->writeAttribute(QStringLiteral("MidiOut"), m_out.mscOutput);
        doc->writeAttribute(QStringLiteral("Device"), QString::number(m_out.mscDevice));
        doc->writeAttribute(QStringLiteral("Cues"), m_out.cues == OutNumberedCues ? QStringLiteral("numbered")
                                                                                   : QStringLiteral("all"));
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}
