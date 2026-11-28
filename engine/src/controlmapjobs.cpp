/*
  Q Light Controller Plus
  controlmapjobs.cpp

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

/* The board jobs (board Q355, docs/features/control-surfaces.md "What a
   control can do"): what a control of a board can be given besides a
   show's scene, cue or hit -- Sub GO, Fade and stop, Blackout, Blind, Flash
   a group, Next / Previous song, Edit Live's Store, Clear the selection,
   Tap, and on a fader a group's level, the effects' speed or their size.
   Each acts through the one place that owns it (ShowControl for a run
   command, FixtureRest for Fade and stop, the InputOutputMap for Blackout,
   the UI's Blind button, the group masters, the speed masters, Edit Live),
   never a copy of it here. The dangerous ones take two controls (the
   operator rules: Shift held, as Shift+STOP). */

#include <climits>

#include "outputrulekeeper.h"
#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "programmercontroller.h"
#include "inputoutputmap.h"
#include "surfaceboardlog.h"
#include "surfacelayout.h"
#include "surfacesetup.h"
#include "speedmasters.h"
#include "fixturegroup.h"
#include "fixturerest.h"
#include "outputlayer.h"
#include "showcontrol.h"
#include "mastertimer.h"
#include "fadechannel.h"
#include "inputpatch.h"
#include "controlmap.h"
#include "editlive.h"
#include "qlcchannel.h"
#include "qlcfixturemode.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

namespace
{
const quint32 kNone = 0xFFFFFFFF;
}

bool ControlMap::blindIsOn() const
{
    if (m_blindOn)
        return m_blindOn();
    const InputOutputMap *iom = m_doc->inputOutputMap();
    return iom != nullptr && iom->outputInhibited();
}

quint32 ControlMap::shiftChannelOf(quint32 universe) const
{
    InputOutputMap *iom = m_doc->inputOutputMap();
    InputPatch *ip = iom != nullptr ? iom->inputPatch(universe) : nullptr;
    if (ip == nullptr)
        return kNone;
    return SurfaceLayout::forProfile(ip->profileName()).shiftChannel();
}

void ControlMap::noteShift(quint32 universe, quint32 channel, uchar value)
{
    const quint32 shift = shiftChannelOf(universe);
    if (shift == kNone || shift != channel)
        return;
    const bool was = m_shiftDown.contains(universe);
    if (value > 0)
        m_shiftDown.insert(universe);
    else
        m_shiftDown.remove(universe);
    if (was != (value > 0))
        slotLampsMayHaveChanged();
}

const ControlMap::Binding *ControlMap::faderJobOn(quint64 k) const
{
    for (int i : m_byControl.value(k))
        if (isFaderJob(m_bindings.at(i).target.type))
            return &m_bindings.at(i);
    return nullptr;
}

int ControlMap::songStep(int dir) const
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    if (st == nullptr || sc == nullptr)
        return -1;
    const quint32 lid = runList();
    const ShowTree::CueList *list = st->cueList(lid);
    if (list == nullptr)
        return -1;
    Chaser *chaser = targetChaser(Binding(0, 0, cueList(lid)));
    if (chaser == nullptr || chaser->stepsCount() == 0)
        return -1;
    const QList<ShowTree::Section> sections = st->sections(lid);
    if (sections.isEmpty())
        return -1;
    /* Each step's song, in the order the list plays them (a running order
       moves the steps, never the cues: Q190 ro). A cue with sub-cues plays
       their chaser as its step. */
    // What each step plays -> its cue's song: one walk of each, not one per pair.
    QHash<quint32, int> songOfFunction;
    for (int i = 0; i < list->cues.size(); ++i)
    {
        const ShowTree::Cue &c = list->cues.at(i);
        const int sec = ShowTree::sectionOfIndex(sections, i);
        songOfFunction.insert(c.collection, sec);
        if (const ShowTree::CueList *subs = st->cueList(st->subCueList(c.id)))
            songOfFunction.insert(subs->chaser, sec);
    }
    QVector<int> song(chaser->stepsCount(), -1);
    for (int s = 0; s < chaser->stepsCount(); ++s)
        if (const ChaserStep *step = chaser->stepAt(s))
            song[s] = songOfFunction.value(step->fid, -1);
    // From the cue playing, else the one GO would play.
    int from = chaser->isRunning() ? chaser->currentStepIndex() : sc->standbyStep(chaser);
    if (from < 0 || from >= song.size())
        from = dir > 0 ? -1 : song.size();
    const int cur = from >= 0 && from < song.size() ? song.at(from) : -2;
    if (dir > 0)
    {
        for (int s = from + 1; s < song.size(); ++s)
            if (song.at(s) >= 0 && song.at(s) != cur && (s == 0 || song.at(s - 1) != song.at(s)))
                return s;
        return -1;
    }
    // The start of this song, then the start of the one before it.
    int r = qMin(from, int(song.size()));
    while (r > 0 && r < song.size() && song.at(r - 1) == cur)
        --r;
    int p = r - 1;
    while (p >= 0 && song.at(p) < 0)
        --p;
    if (p < 0)
        return -1;
    const int prev = song.at(p);
    while (p > 0 && song.at(p - 1) == prev)
        --p;
    return p;
}

void ControlMap::flashGroup(quint32 groupId, bool on)
{
    if (on)
        m_flashHeld.insert(groupId);
    else if (!m_flashHeld.remove(groupId))
        return;
    if (m_flashLayer == nullptr)
    {
        if (m_flashHeld.isEmpty())
            return;
        // Over the cues (Flashing), under the Simple Desk; a press is not
        // the show, so a Show-only fixture stays safe.
        m_flashLayer = new OutputLayer(m_doc, Universe::Flashing, 0, QStringLiteral("Flash"));
        m_flashLayer->setHoldsShowOnly(true);
    }
    /* The emergency stop lets go of a Flash held through it (E7, as the
       Simple Desk and the VC sliders): its key still down is no new press.
       The keeper is made after this map, so connected here. */
    if (on)
        if (OutputRuleKeeper *rules = m_doc->outputRuleKeeper())
            connect(rules, &OutputRuleKeeper::emergencyStopStruck, this, &ControlMap::dropFlashes,
                    Qt::UniqueConnection);
    /* Every held group's intensity at full: the dimmer and the red, green,
       blue and white of a head with none. Cyan, magenta and yellow are left
       (full is dark for them). */
    QList<OutputLayer::Level> levels;
    QSet<quint64> seen;
    for (quint32 gid : std::as_const(m_flashHeld))
    {
        const FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g == nullptr)
            continue;
        for (quint32 fid : g->fixtureList())
        {
            const Fixture *fxi = m_doc->fixture(fid);
            if (fxi == nullptr || fxi->fixtureMode() == nullptr)
                continue;
            /* A press is not a run: stage machinery, a one-shot (a kabuki
               drop), a switch (a relay, the house lights) and a hazard move
               only when a run plays them (FixtureRest::freezesOnStop()). */
            if (FixtureRest::freezesOnStop(fxi) || FixtureRest::isHazard(fxi))
                continue;
            for (quint32 c = 0; c < fxi->channels(); ++c)
            {
                const QLCChannel *ch = fxi->channel(c);
                if (ch == nullptr || ch->group() != QLCChannel::Intensity)
                    continue;
                if (ch->colour() == QLCChannel::Cyan || ch->colour() == QLCChannel::Magenta
                    || ch->colour() == QLCChannel::Yellow)
                    continue;
                const quint64 k = (quint64(fid) << 32) | c;
                if (seen.contains(k))
                    continue;
                seen.insert(k);
                levels.append({ fid, c, uchar(255) });
            }
        }
    }
    if (levels.isEmpty())
        m_flashLayer->clear();
    else
        m_flashLayer->set(levels);
    slotLampsMayHaveChanged();
}

void ControlMap::dropFlashes()
{
    if (m_flashHeld.isEmpty())
        return;
    m_flashHeld.clear();
    if (m_flashLayer != nullptr)
        m_flashLayer->clear();
    slotLampsMayHaveChanged();
}

bool ControlMap::pressJob(const Binding &b)
{
    const quint64 k = key(b.universe, b.channel);
    const TargetType type = b.target.type;
    const QString what = targetName(b.target);
    SurfaceBoardLog *log = !isKey(b) && m_doc->surfaceSetup() != nullptr ? m_doc->surfaceSetup()->boardLog()
                                                                         : nullptr;
    /* Two controls for a dangerous job (the operator rules): the board's
       Shift held, as Shift+STOP. Alone it does nothing, and says so. */
    if (isGuardedJob(type))
    {
        /* On the Shift key itself (a show file's, a Learn's: the map never
           offers it) its own press would be both controls: never. */
        const bool onShift = !isKey(b) && b.channel == shiftChannelOf(b.universe);
        const bool shift = isKey(b) ? (b.channel & quint32(Qt::ShiftModifier)) != 0
                                    : isShiftHeld(b.universe) && !onShift;
        if (!shift)
        {
            const bool hasShift = isKey(b) || shiftChannelOf(b.universe) != kNone;
            if (log != nullptr)
                log->note(b.universe, tr("%1 refused: Shift is not held").arg(what));
            if (onShift)
                say(k, tr("%1 is on the Shift key itself, so it never fires: give it another key "
                          "(Connections ▸ Control surfaces).").arg(what));
            else
                say(k, hasShift ? tr("%1 takes two controls: hold Shift and press %2. Alone it does nothing.")
                                      .arg(what, controlName(b.universe, b.channel))
                                : tr("%1 takes two controls, and this board has no Shift key: give it to a "
                                     "board that has one (Connections ▸ Control surfaces).").arg(what));
            return false;
        }
    }
    if (log != nullptr)
        log->note(b.universe, tr("%1: pressed").arg(what));

    InputOutputMap *iom = m_doc->inputOutputMap();
    ShowControl *sc = m_doc->showControl();
    bool acted = false;
    switch (type)
    {
        case SubGoTarget:
        {
            Chaser *chaser = targetChaser(Binding(0, 0, cueList(runList())));
            if (chaser == nullptr)
            {
                say(k, tr("Sub GO: the Run tab has no cue list to play."));
                break;
            }
            ShowControl::Request r;
            r.command = ShowControl::SubGo;
            r.chaser = chaser->id();
            // The one run path, and its double-GO guard (ShowControl::run()).
            if (sc != nullptr && sc->run(r, ShowControl::RouteControlMap))
            {
                emit fired(tr("Sub GO"));
                acted = true;
            }
            break;
        }
        case FadeStopTarget:
        {
            // The toolbar's Fade and stop, the same code (FixtureRest::stopAll()).
            const quint32 seconds = qBound(quint32(1), b.target.id, quint32(600));
            FixtureRest::stopAll(m_doc, int(seconds * 1000));
            emit fired(tr("Fade and stop: everything fades out over %1 s").arg(seconds));
            acted = true;
            break;
        }
        case BlackoutTarget:
            if (iom != nullptr)
            {
                const bool on = iom->toggleBlackout();
                emit fired(on ? tr("Blackout on") : tr("Blackout off"));
                acted = true;
            }
            break;
        case BlindTarget:
            // The console's Blind button: a mute in Design, the Blind view in Operate.
            if (m_blindToggle)
                m_blindToggle();
            else if (iom != nullptr)
                iom->setOutputInhibited(!iom->outputInhibited());
            emit fired(blindIsOn() ? tr("Blind on") : tr("Blind off"));
            acted = true;
            break;
        case FlashGroupTarget:
            if (m_doc->fixtureGroup(b.target.id) == nullptr)
                break;
            flashGroup(b.target.id, true);
            emit fired(what);
            return true;        // its release lets go
        case NextSongTarget:
        case PreviousSongTarget:
        {
            const int dir = type == NextSongTarget ? 1 : -1;
            const int step = songStep(dir);
            Chaser *chaser = targetChaser(Binding(0, 0, cueList(runList())));
            if (step < 0 || chaser == nullptr || sc == nullptr)
            {
                say(k, dir > 0 ? tr("Next song: there is no song after this one in the Run tab's list.")
                               : tr("Previous song: there is no song before this one in the Run tab's list."));
                break;
            }
            /* On standby, never played: GO plays it (a cue list is never
               changed from a board, only where it stands). */
            ShowControl::Request r;
            r.command = ShowControl::Load;
            r.chaser = chaser->id();
            r.cue = ShowControl::cueName(chaser, step, -1);
            if (sc->run(r, ShowControl::RouteControlMap))
            {
                emit fired(tr("%1: cue %2 is next").arg(what, r.cue));
                acted = true;
            }
            break;
        }
        case EditLiveStoreTarget:
        {
            QString why;
            if (m_editLive->store(&why))
            {
                emit fired(tr("Edit Live: stored"));
                acted = true;
            }
            else
            {
                say(k, why.isEmpty() ? tr("Edit Live: nothing to store.") : tr("Edit Live: %1").arg(why));
            }
            break;
        }
        case ClearSelectionTarget:
            if (ProgrammerController *pc = m_doc->programmer())
            {
                pc->clearProgrammerSelection();
                emit fired(tr("Selection cleared"));
                acted = true;
            }
            break;
        case TapTarget:
            if (SpeedMasters *sm = m_doc->speedMasters())
            {
                if (int(b.target.id) < sm->count() && sm->tap(int(b.target.id)))
                {
                    emit fired(what);
                    acted = true;
                }
                else
                {
                    say(k, tr("%1: it follows another clock, so it is not tapped.").arg(what));
                }
            }
            break;
        default:
            break;      // a fader's job: faderJob()
    }
    slotLampsMayHaveChanged();
    return acted;
}

void ControlMap::releaseJob(const Binding &b)
{
    if (b.target.type == FlashGroupTarget)
        flashGroup(b.target.id, false);
}

void ControlMap::faderJob(const Binding &b, uchar value)
{
    const quint64 k = key(b.universe, b.channel);
    /* A board just back sends where its faders are: its dump, not a hand
       (as the Grand Master's pick-up, Q316). Nothing moves. */
    auto back = m_backAt.constFind(b.universe);
    if (back != m_backAt.constEnd() && back->elapsed() < m_dumpWindowMs)
        return;
    FaderPickup &pick = m_pickups[k];
    pick.setSource(b.universe, b.channel);
    // The control given another job is another fader: picked up again.
    const quint64 job = (quint64(b.target.type) << 32) | b.target.id;
    if (m_pickupJob.value(k, ~quint64(0)) != job)
    {
        pick.reset();
        m_pickupJob.insert(k, job);
    }
    int current = -1;
    switch (b.target.type)
    {
        case GroupLevelTarget:
        {
            FixtureGroup *g = m_doc->fixtureGroup(b.target.id);
            if (g == nullptr)
                return;
            /* A fader parked at the bottom while the group is up must not
               black it out on the first brush (Q316's rule for the Grand
               Master): it is picked up first. All the way down is the safe
               way, and is taken at once. */
            current = g->masterLevel();
            if (value == 0 || pick.accept(value, current))
            {
                if (value == 0)
                    pick.accept(value, value);  // where it is now: followed from here
                g->setMasterLevel(value);
            }
            break;
        }
        case EffectSpeedTarget:
        {
            SpeedMasters *sm = m_doc->speedMasters();
            if (sm == nullptr || int(b.target.id) >= sm->count())
                return;
            // The middle is the tempo as set, the top twice it (the rate's 0..2).
            current = qBound(0, qRound(sm->rate(int(b.target.id)) * 127.5), 255);
            if (pick.accept(value, current))
                sm->setRate(int(b.target.id), double(value) / 127.5);
            break;
        }
        case EffectSizeTarget:
            current = m_effectSize;
            if (pick.accept(value, current))
            {
                m_effectSize = value;
                if (EffectScriptRunner *runner = m_doc->effectScriptRunner())
                    runner->setSizeMaster(float(value) / 255.0f);
            }
            break;
        default:
            return;
    }
    // Waiting to be picked up: where to move it, once per hold.
    if (current >= 0 && pick.waiting(current))
        say(k, FaderPickup::hintText(targetName(b.target), qRound(current * 100.0 / 255.0)));
}

ControlMap::Lamp ControlMap::jobLamp(const Binding &b) const
{
    // A job that takes Shift on a board with none never fires: dark.
    if (isGuardedJob(b.target.type) && !isKey(b)
        && (shiftChannelOf(b.universe) == kNone || shiftChannelOf(b.universe) == b.channel))
        return LampOff;
    switch (b.target.type)
    {
        case SubGoTarget:
        {
            Chaser *chaser = targetChaser(Binding(0, 0, cueList(runList())));
            return chaser != nullptr && chaser->isRunning() && chaser->isSubGoStep(chaser->currentStepIndex())
                       ? LampStandby : LampOff;
        }
        case FadeStopTarget:
            return m_doc->masterTimer() != nullptr && m_doc->masterTimer()->runningFunctions() > 0
                       ? LampStandby : LampOff;
        case BlackoutTarget:
            return m_doc->inputOutputMap() != nullptr && m_doc->inputOutputMap()->blackout()
                       ? LampRunning : LampStandby;
        case BlindTarget:
            return blindIsOn() ? LampRunning : LampStandby;
        case FlashGroupTarget:
            return isFlashing(b.target.id) ? LampRunning : LampStandby;
        case NextSongTarget:
            return songStep(1) >= 0 ? LampStandby : LampOff;
        case PreviousSongTarget:
            return songStep(-1) >= 0 ? LampStandby : LampOff;
        case EditLiveStoreTarget:
            return m_editLive->isOn() && m_editLive->hasTrims() ? LampStandby : LampOff;
        case ClearSelectionTarget:
            return m_doc->programmer() != nullptr && !m_doc->programmer()->programmerSelection().isEmpty()
                       ? LampStandby : LampOff;
        case TapTarget:
            return LampStandby;
        default:
            break;      // a fader has no light
    }
    return LampOff;
}
