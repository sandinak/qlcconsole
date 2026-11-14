/*
  Q Light Controller Plus - qlcconsole
  webaccessrun.cpp

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

#include <QJsonDocument>
#include <QJsonObject>

#include "webaccessrun.h"
#include "showtreecontroller.h"
#include "teachingsettings.h"
#include "cuesheet.h"
#include "showcontrol.h"
#include "showoperate.h"
#include "groupmasters.h"
#include "fixturegroup.h"
#include "effectscriptrunner.h"
#include "inputoutputmap.h"
#include "functionparent.h"
#include "mastertimer.h"
#include "function.h"
#include "chaser.h"
#include "doc.h"
#include "padrun.h"
#include "padnames.h"
#include "showstatus.h"
#include "focussession.h"
#include "programmercontroller.h"
#include "webaccessgamepad.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <climits>

using namespace ShowTree;

WebAccessRun::WebAccessRun(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &WebAccessRun::poll);
    // A command lands at once; the poll catches a list that steps on its own.
    connect(m_doc->showControl(), &ShowControl::commandHandled, this, &WebAccessRun::poll);
    connect(m_doc->operateSettings(), &ShowOperate::settingsChanged, this, &WebAccessRun::poll);
    // A master moved from the desk, a VC slider or a controller shows on the phone.
    connect(m_doc->groupMasters(), &GroupMasters::changed, this, &WebAccessRun::poll);
    connect(m_doc->groupMasters(), &GroupMasters::levelChanged, this, &WebAccessRun::poll);
    connect(m_doc->inputOutputMap(), &InputOutputMap::blackoutChanged, this, &WebAccessRun::poll);
    connect(m_doc->inputOutputMap(), &InputOutputMap::grandMasterValueChanged, this, &WebAccessRun::poll);
    connect(m_doc->inputOutputMap(), &InputOutputMap::outputInhibitedChanged, this, &WebAccessRun::poll);
    // A Teaching show's held show control out, as the console's footer says it.
    connect(m_doc->showControl(), &ShowControl::outHeld, this, [this](const QString &what) {
        m_lastHeld = what;
        poll();
    });
    connect(m_doc->showControl(), &ShowControl::sentOut, this, [this]() {
        m_lastHeld.clear();
        poll();
    });
    if (TeachingSettings *t = m_doc->teachingSettings())
        connect(t, &TeachingSettings::teachingChanged, this, [this]() {
            m_lastHeld.clear();
            poll();
        });
}

void WebAccessRun::setClients(int count)
{
    m_clients = count;
    if (m_clients > 0 && !m_timer.isActive())
    {
        m_last.clear();
        m_timer.start();
    }
    else if (m_clients <= 0)
    {
        m_timer.stop();
    }
}

void WebAccessRun::poll()
{
    if (m_clients <= 0)
        return;
    const QByteArray now = stateJson();
    if (now == m_last)
        return;
    m_last = now;
    emit stateChanged(now);
}

WebAccessRun::Result WebAccessRun::gate(bool unlocked) const
{
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    return Done;
}

bool WebAccessRun::remoteAllowed() const
{
    return m_doc->operateSettings()->remoteGo();
}

bool WebAccessRun::pinRequired() const
{
    return m_doc->operateSettings()->hasPin();
}

WebAccessRun::PinResult WebAccessRun::checkPin(const QString &pin)
{
    if (m_locked)
    {
        if (m_lockout.elapsed() < kLockoutMs)
            return PinLocked;
        m_locked = false;
        m_failures = 0;
    }
    if (m_doc->operateSettings()->checkPin(pin))
    {
        m_failures = 0;
        return PinOk;
    }
    if (++m_failures >= kMaxTries)
    {
        m_locked = true;
        m_lockout.start();
        return PinLocked;
    }
    return PinBad;
}

WebAccessRun::Result WebAccessRun::command(const QString &command, bool unlocked, const QString &cue,
                                           const QString &list)
{
    ShowControl::Request r;
    if (command == QLatin1String("go"))
        r.command = ShowControl::Go;
    else if (command == QLatin1String("back"))
        r.command = ShowControl::Back;
    else if (command == QLatin1String("stop"))
        r.command = ShowControl::Stop;
    else if (command == QLatin1String("subgo"))
        r.command = ShowControl::SubGo;
    else if (command == QLatin1String("subback"))
        r.command = ShowControl::SubBack;
    else if (command == QLatin1String("pause"))
        r.command = ShowControl::Pause;
    else if (command == QLatin1String("resume"))
        r.command = ShowControl::Resume;
    else
        return Unknown;

    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;

    // The main cue list, as the Run tab's default follow does; or the list
    // by its number, and a cue by its number, as OSC names them.
    Chaser *ch = m_doc->showControl()->cueList(list);
    if (ch == nullptr)
        return NothingToDo;
    r.chaser = ch->id();
    r.cue = cue;
    return m_doc->showControl()->run(r, ShowControl::RouteWeb) ? Done : NothingToDo;
}

static uchar percentToByte(int percent)
{
    return uchar(qRound(qBound(0, percent, 100) * double(UCHAR_MAX) / 100.0));
}

static int byteToPercent(uchar value)
{
    return qRound(value * 100.0 / UCHAR_MAX);
}

WebAccessRun::Result WebAccessRun::setMaster(quint32 groupId, int percent, bool unlocked)
{
    // The same rules as GO: the show's switch, then the PIN.
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    FixtureGroup *g = m_doc->fixtureGroup(groupId);
    if (g == nullptr || !g->hasMaster())
        return NothingToDo;
    g->setMasterLevel(percentToByte(percent));
    return Done;
}

WebAccessRun::Result WebAccessRun::hit(quint32 hitId, bool unlocked)
{
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    ShowTreeController *st = m_doc->showTree();
    // Like a VC button: a hit fires in Operate only.
    if (m_doc->mode() != Doc::Operate)
        return WrongMode;
    const QList<quint32> tier = st->armedTier(st->allHits());
    if (tier.isEmpty())
        return NothingToDo;
    if (hitId != UINT_MAX && !tier.contains(hitId))
        return NothingToDo;

    // One running (a toggle on, or a one-shot still going): this lets it go.
    for (quint32 h : tier)
    {
        if (hitId != UINT_MAX && h != hitId)
            continue;
        Function *f = m_doc->function(st->lookFunction(h));
        if (f == nullptr || !f->isRunning())
            continue;
        EffectScriptRunner *esr = m_doc->effectScriptRunner();
        if (esr != nullptr && esr->oneShotState(f->id()) == 1 && esr->restartOneShots(f->id()))
            return Done;
        f->stop(FunctionParent::master());
        return Done;
    }
    const quint32 pick = hitId != UINT_MAX ? hitId : tier.at(m_hitStep++ % tier.count());
    Function *f = m_doc->function(st->lookFunction(pick));
    if (f == nullptr)
        return NothingToDo;
    f->start(m_doc->masterTimer(), FunctionParent::master());
    return Done;
}

WebAccessRun::Result WebAccessRun::blackout(const QString &what, bool unlocked)
{
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    InputOutputMap *io = m_doc->inputOutputMap();
    if (what == QLatin1String("toggle"))
        io->toggleBlackout();
    else if (what == QLatin1String("on"))
        io->setBlackout(true);
    else if (what == QLatin1String("off"))
        io->setBlackout(false);
    else
        return Unknown;
    return Done;
}

WebAccessRun::Result WebAccessRun::setGrand(int percent, bool unlocked)
{
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    m_doc->inputOutputMap()->setGrandMasterValue(percentToByte(percent));
    return Done;
}

WebAccessRun::Result WebAccessRun::nudgeGrand(int delta, bool unlocked)
{
    return setGrand(byteToPercent(m_doc->inputOutputMap()->grandMasterValue()) + delta, unlocked);
}

WebAccessRun::Result WebAccessRun::nudgeMaster(quint32 groupId, int delta, bool unlocked)
{
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    FixtureGroup *g = m_doc->fixtureGroup(groupId);
    if (g == nullptr || !g->hasMaster())
        return NothingToDo;
    return setMaster(groupId, byteToPercent(g->masterLevel()) + delta, unlocked);
}

WebAccessRun::Result WebAccessRun::standby(int direction, bool unlocked)
{
    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;
    ShowControl *sc = m_doc->showControl();
    Chaser *ch = sc->cueList(QString());
    if (ch == nullptr)
        return NothingToDo;
    const int cur = sc->standbyStep(ch);
    const int target = (cur < 0 ? (direction > 0 ? 0 : ch->stepsCount() - 1) : cur + (direction > 0 ? 1 : -1));
    if (target < 0 || target >= ch->stepsCount())
        return NothingToDo;
    ShowControl::Request r;
    r.command = ShowControl::Load;
    r.chaser = ch->id();
    r.cue = ch->stepCue(target);
    return sc->run(r, ShowControl::RouteWeb) ? Done : NothingToDo;
}

/*********************************************************************
 * Blind and Solo (Q229)
 *********************************************************************/

WebAccessRun::Result WebAccessRun::blind(const QString &what, bool unlocked)
{
    InputOutputMap *io = m_doc->inputOutputMap();
    bool on;
    if (what == QLatin1String("toggle"))
        on = !io->outputInhibited();
    else if (what == QLatin1String("on"))
        on = true;
    else if (what == QLatin1String("off"))
        on = false;
    else
        return Unknown;
    const Result g = gate(unlocked);
    if (g != Done)
        return g;
    // Operate: the console's Blind is its own screens' view, never a mute.
    if (m_doc->mode() != Doc::Design)
        return NotInDesign;
    /* What the console's Blind button does in Design (App::slotControlBlind());
       the App's button and footer follow the engine (outputInhibitedChanged).
       Nothing of Teaching locks it on the console, so nothing here: its
       instructor flag (hold show control in Blind) acts on the next GO,
       wherever that comes from (ShowControl::sendOut()). */
    io->setOutputInhibited(on);
    return Done;
}

QList<WebAccessRun::SoloLayer> WebAccessRun::soloLayers() const
{
    QList<SoloLayer> out;
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr || m_doc->mode() != Doc::Design)
        return out;
    const ShowTreeController::PreviewState state = st->previewState();
    if (state != ShowTreeController::PreviewState::Playing && state != ShowTreeController::PreviewState::Dark)
        return out;
    ResolvedCue rc;
    if (!st->resolvedCue(st->previewedCue(), &rc))
        return out;
    // The top layers, as setPreviewSolo() takes them (a child plays with its parent).
    for (const ResolvedLayer &rl : rc.layers)
    {
        if (rl.depth != 0 || rl.layerId == invalidId())
            continue;
        bool seen = false;
        for (const SoloLayer &l : out)
            seen = seen || l.id == rl.layerId;
        if (!seen)
            out << SoloLayer{ rl.layerId, rl.name.isEmpty() ? st->targetLabel(rl.target) : rl.name };
    }
    return out;
}

WebAccessRun::Result WebAccessRun::solo(const QString &what, bool unlocked)
{
    bool byId = false;
    const quint32 id = what.toUInt(&byId);
    if (!byId && what != QLatin1String("off") && what != QLatin1String("toggle")
        && what != QLatin1String("next") && what != QLatin1String("prev"))
        return Unknown;
    const Result g = gate(unlocked);
    if (g != Done)
        return g;
    // As the Show Tree's Solo: Design only (in Operate the tree is blind).
    if (m_doc->mode() != Doc::Design)
        return NotInDesign;
    ShowTreeController *st = m_doc->showTree();
    if (what == QLatin1String("off"))
    {
        st->setPreviewSolo(invalidId(), invalidId());
        return Done;
    }
    const QList<SoloLayer> layers = soloLayers();
    if (layers.isEmpty())
        return NothingToDo;
    const int n = int(layers.size());
    int at = -1;
    for (int i = 0; i < n; ++i)
        if (layers.at(i).id == st->previewSoloLayer())
            at = i;
    quint32 want = invalidId();
    if (byId)
    {
        for (const SoloLayer &l : layers)
            if (l.id == id)
                want = id;
        if (want == invalidId())
            return NothingToDo;
    }
    else if (what == QLatin1String("toggle"))
        want = at >= 0 ? invalidId() : layers.first().id;
    else if (what == QLatin1String("next"))
        want = layers.at(at < 0 ? 0 : (at + 1) % n).id;
    else
        want = layers.at(at < 0 ? n - 1 : (at - 1 + n) % n).id;
    st->setPreviewSolo(want == invalidId() ? invalidId() : st->previewedCue(), want);
    return Done;
}

/*********************************************************************
 * State
 *********************************************************************/

static QJsonObject cueObject(Doc *doc, Chaser *ch, quint32 listId, int step, int sub)
{
    ShowTreeController *st = doc->showTree();
    QJsonObject o;
    if (step < 0)
        return o;
    const quint32 cueId = st->cueOfStep(listId, step, sub);
    if (const CueList *l = st->listOfCue(cueId))
    {
        const int idx = l->indexOf(cueId);
        o[QStringLiteral("num")] = st->cueNumber(*l, idx);
        o[QStringLiteral("name")] = idx >= 0 && idx < l->cues.count() ? l->cues.at(idx).name : QString();
        // Every entry of the Notes pane for this cue, each with its kind.
        QJsonArray notes;
        for (const CueSheet::NoteLine &n : CueSheet::noteLinesOf(st, cueId))
        {
            QJsonObject no;
            no[QStringLiteral("kind")] = n.kind;
            no[QStringLiteral("text")] = n.text;
            no[QStringLiteral("done")] = n.done;
            notes << no;
        }
        if (!notes.isEmpty())
            o[QStringLiteral("notes")] = notes;
        return o;
    }
    if (sub >= 0)
    {
        o[QStringLiteral("num")] = ch->subCue(step, sub);
        o[QStringLiteral("name")] = QString();
        return o;
    }
    const QList<ChaserStep> steps = ch->steps();
    const Function *f = step < steps.count() ? doc->function(steps.at(step).fid) : nullptr;
    o[QStringLiteral("num")] = ch->stepCue(step);
    o[QStringLiteral("name")] = f == nullptr ? QString() : f->name();
    return o;
}

QJsonObject WebAccessRun::padMap()
{
    /* The pad's Run role (PadRun), as far as the page can do it: the arm,
       GO, Back, Sub GO, Stop and the armed hit. The rest (the
       walk, the voice) are the console's. */
    QJsonObject map;
    for (GamepadRole::Role role : WebAccessGamepad::standardButtons())
    {
        const PadRun::Action a = PadRun::actionFor(role);
        QString what;
        switch (a)
        {
            case PadRun::Arm:     what = QStringLiteral("arm"); break;
            case PadRun::Go:      what = QStringLiteral("go"); break;
            case PadRun::Back:    what = QStringLiteral("back"); break;
            case PadRun::SubGo:   what = QStringLiteral("subgo"); break;
            case PadRun::Stop:    what = QStringLiteral("stop"); break;
            case PadRun::FireHit: what = QStringLiteral("hit"); break;
            default: continue;
        }
        QJsonObject e;
        e[QStringLiteral("do")] = what;
        e[QStringLiteral("label")] = PadNames::expand(PadRun::label(a), PadNames::Xbox);
        // Stop sits beside GO: held, as on the console's pad.
        if (a == PadRun::Stop)
            e[QStringLiteral("hold")] = PadRun::stopHoldMs();
        map[GamepadRole::key(role)] = e;
    }

    /* The remote's own (Q229), on controls the Run role leaves free, under
       the same arm (LB held): Blind, and Solo with the D-pad walking the
       layers -- View solos as it does on the console's walk (PadRun). */
    struct Extra
    {
        GamepadRole::Role role;
        const char *what;
        const char *command;
        QString label;
    };
    const Extra extras[] = {
        { GamepadRole::Back, "solo", "solo|toggle",
          tr("Solo a layer of the cue in the preview, or the whole cue again ({LB} held; Design)") },
        { GamepadRole::DpadRight, "solonext", "solo|next", tr("Solo the next layer ({LB} held)") },
        { GamepadRole::DpadLeft, "soloprev", "solo|prev", tr("Solo the previous layer ({LB} held)") },
        { GamepadRole::LS, "blind", "blind|toggle",
          tr("Blind: the rig muted, the screens live ({LB} held; Design)") },
    };
    for (const Extra &x : extras)
    {
        // Never over a control of the console's Run role.
        if (PadRun::actionFor(x.role) != PadRun::NoAction)
            continue;
        QJsonObject e;
        e[QStringLiteral("do")] = QLatin1String(x.what);
        e[QStringLiteral("cmd")] = QLatin1String(x.command);
        e[QStringLiteral("remote")] = true;
        e[QStringLiteral("label")] = PadNames::expand(x.label, PadNames::Xbox);
        map[GamepadRole::key(x.role)] = e;
    }
    return map;
}

QByteArray WebAccessRun::stateJson() const
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    Chaser *ch = sc->cueList(QString());

    QJsonObject o;
    o[QStringLiteral("remote")] = remoteAllowed();
    o[QStringLiteral("pin")] = pinRequired();
    o[QStringLiteral("mode")] = m_doc->mode() == Doc::Operate ? QStringLiteral("LIVE") : QStringLiteral("DESIGN");
    const TeachingSettings *teach = m_doc->teachingSettings();
    o[QStringLiteral("teaching")] = teach != nullptr && teach->enabled();
    const bool blind = m_doc->inputOutputMap()->outputInhibited();
    o[QStringLiteral("blind")] = blind;
    if (teach != nullptr && teach->enabled())
    {
        // What the instructor's flags hold now, as the console's footer says it.
        QJsonObject held;
        held[QStringLiteral("output")] = teach->holdsOutput();
        held[QStringLiteral("showControl")] = teach->showControlHeldBecause(blind);
        held[QStringLiteral("last")] = m_lastHeld;
        o[QStringLiteral("held")] = held;
    }
    const QList<SoloLayer> layers = soloLayers();
    if (!layers.isEmpty())
    {
        const quint32 cueId = st->previewedCue();
        QJsonObject pv;
        if (const ShowTree::Cue *c = st->cue(cueId))
            pv[QStringLiteral("name")] = c->name;
        if (const CueList *l = st->listOfCue(cueId))
            pv[QStringLiteral("num")] = st->cueNumber(*l, l->indexOf(cueId));
        QJsonArray la;
        int solo = -1;
        for (const SoloLayer &l : layers)
        {
            QJsonObject lo;
            lo[QStringLiteral("id")] = int(l.id);
            lo[QStringLiteral("name")] = l.name;
            la.append(lo);
            if (l.id == st->previewSoloLayer())
                solo = int(l.id);
        }
        pv[QStringLiteral("layers")] = la;
        pv[QStringLiteral("solo")] = solo;
        o[QStringLiteral("preview")] = pv;
    }
    o[QStringLiteral("hasList")] = ch != nullptr;
    o[QStringLiteral("api")] = 1;
    o[QStringLiteral("blackout")] = m_doc->inputOutputMap()->blackout();
    o[QStringLiteral("grand")] = byteToPercent(m_doc->inputOutputMap()->grandMasterValue());
    o[QStringLiteral("pad")] = padMap();
    // Focus running on the console: the page offers the focus remote.
    o[QStringLiteral("focus")] = m_doc->programmer() != nullptr && m_doc->programmer()->focusSession()->isActive();
    // Show Status (the footer chip): one line each, never the detail or items.
    QJsonArray status;
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
    {
        if (e.severity == ShowStatus::Ok)
            continue;
        QJsonObject so;
        so[QStringLiteral("level")] = e.severity == ShowStatus::Error ? QStringLiteral("error") : QStringLiteral("warning");
        so[QStringLiteral("text")] = e.summary;
        status.append(so);
    }
    o[QStringLiteral("status")] = status;
    const QList<quint32> hitIds = st->allHits();
    if (!hitIds.isEmpty())
    {
        const QList<quint32> tier = st->armedTier(hitIds);
        QJsonArray hits;
        for (quint32 h : hitIds)
        {
            const ShowTree::Cue *c = st->cue(h);
            const Function *f = m_doc->function(st->lookFunction(h));
            QJsonObject ho;
            ho[QStringLiteral("id")] = int(h);
            ho[QStringLiteral("name")] = c != nullptr ? c->name : QString();
            ho[QStringLiteral("on")] = f != nullptr && f->isRunning();
            ho[QStringLiteral("armed")] = tier.contains(h);
            hits.append(ho);
        }
        o[QStringLiteral("hits")] = hits;
    }
    QJsonArray masters;
    for (const FixtureGroup *g : m_doc->groupMasters()->groups())
    {
        QJsonObject m;
        m[QStringLiteral("id")] = int(g->id());
        m[QStringLiteral("name")] = g->name();
        m[QStringLiteral("level")] = byteToPercent(g->masterLevel());
        masters.append(m);
    }
    if (!masters.isEmpty())
        o[QStringLiteral("masters")] = masters;
    if (ch == nullptr)
        return QJsonDocument(o).toJson(QJsonDocument::Compact);

    quint32 lid = 0xFFFFFFFF;
    const quint32 pid = st->productionOfChaser(ch->id(), &lid);
    const Production *p = st->production(pid);
    const CueList *l = st->cueList(lid);
    o[QStringLiteral("production")] = p != nullptr ? p->name : QString();
    o[QStringLiteral("list")] = l != nullptr ? l->name : ch->name();

    const bool running = ch->isRunning();
    const int cur = running ? ch->currentStepIndex() : -1;
    const int curSub = running ? ch->currentSubCueIndex() : -1;
    const int standby = sc->standbyStep(ch);
    const bool loaded = sc->loadedStep(ch->id()) >= 0;
    const int loadedSub = loaded ? sc->loadedSubCue(ch->id()) : -1;
    const int subNext = running && ch->isSubGoStep(cur) ? ch->nextSubCueIndex() : -1;

    bool hasSub = false;
    for (int i = 0; i < ch->stepsCount() && !hasSub; ++i)
        hasSub = ch->isSubGoStep(i);

    o[QStringLiteral("running")] = running;
    o[QStringLiteral("paused")] = running && ch->isPaused();
    o[QStringLiteral("hasSub")] = hasSub;
    o[QStringLiteral("loaded")] = loaded;

    if (running)
    {
        // The sub-cue is the cue on now when there is one: its number is the full one.
        QJsonObject now = cueObject(m_doc, ch, lid, cur, -1);
        if (curSub >= 0)
            now[QStringLiteral("sub")] = cueObject(m_doc, ch, lid, cur, curSub);
        o[QStringLiteral("now")] = now;
    }

    if (standby >= 0)
    {
        int toSub = loaded ? loadedSub : -1;
        if (toSub < 0 && ch->isSubGoStep(standby))
            toSub = 0;
        o[QStringLiteral("next")] = cueObject(m_doc, ch, lid, standby, loaded ? toSub : -1);
    }
    if (subNext >= 0)
        o[QStringLiteral("subNext")] = cueObject(m_doc, ch, lid, cur, subNext);

    // The song section of the cue on now, else of the one next.
    quint32 sectionCue = running ? st->cueOfStep(lid, cur, -1) : st->cueOfStep(lid, standby, -1);
    Section sec;
    if (sectionCue != 0xFFFFFFFF && st->sectionOfCue(sectionCue, &sec))
        o[QStringLiteral("section")] = sectionTitle(sec);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
