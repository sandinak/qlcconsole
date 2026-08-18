/*
  Q Light Controller Plus
  pmjoverlay.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "pmjoverlay.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "universe.h"
#include "programmercontroller.h"
#include "app.h"
#include "programmingmanager.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcchannel.h"

const QString PMJOverlay::deviceId = QStringLiteral("PMJ Black 1");

// Channel numbers straight from resources/inputprofiles/PMJ-Black-1.qxi.
// Kept here rather than parsed from the profile at runtime: the overlay
// needs to know these regardless of whether the user has the PMJ patched at
// all yet, and re-deriving them from whatever profile happens to be loaded
// would let a stale/edited profile silently break the role binding.
namespace {

struct PmjControl
{
    quint32 channel;
    const char *name;
    ControlSurface::Kind kind;
    bool hasFeedback;   // true = the profile gives this control a <Feedback> range
    ControlSurface::Role role;
};

namespace CS = ControlSurface;

// clang-format off
const PmjControl kPmjControls[] = {
    { 32768, "Master", CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, -1) },
    { 32769, "Ch 1",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 1) },
    { 32770, "Ch 2",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 2) },
    { 32771, "Ch 3",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 3) },
    { 32772, "Ch 4",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 4) },
    { 32773, "Ch 5",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 5) },
    { 32774, "Ch 6",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 6) },
    { 32775, "Ch 7",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 7) },
    { 32776, "Ch 8",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 8) },
    { 32777, "Ch 9",   CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 9) },
    { 32778, "Ch 10",  CS::Kind::Fader,  false, CS::Role(CS::RoleType::Level, 10) },

    { 32779, "Enc 1",  CS::Kind::Encoder, false, CS::Role(CS::RoleType::Param, 1) },
    { 32780, "Enc 2",  CS::Kind::Encoder, false, CS::Role(CS::RoleType::Param, 2) },
    { 32781, "Enc 3",  CS::Kind::Encoder, false, CS::Role(CS::RoleType::Param, 3) },
    { 32782, "Enc 4",  CS::Kind::Encoder, false, CS::Role(CS::RoleType::Param, 4) },

    { 32896, "Go",         CS::Kind::Button, true, CS::Role(CS::RoleType::Transport, CS::Go) },
    { 32907, "Back",       CS::Kind::Button, true, CS::Role(CS::RoleType::Transport, CS::Back) },
    { 32932, "Left",       CS::Kind::Button, true, CS::Role(CS::RoleType::Transport, CS::Left) },
    { 32933, "Right",      CS::Kind::Button, true, CS::Role(CS::RoleType::Transport, CS::Right) },
    { 32919, "Pre Page",   CS::Kind::Button, true, CS::Role(CS::RoleType::Transport, CS::Prev) },
    { 32920, "Next Page",  CS::Kind::Button, true, CS::Role(CS::RoleType::Transport, CS::Next) },

    // Static core (design doc proposal, confirmed): O -> Blackout, Set ->
    // Blind. Favorites (proposed Tap) deliberately left unbound this slice
    // — see the header comment.
    { 32918, "O",          CS::Kind::Button, true, CS::Role(CS::RoleType::Static, CS::Blackout) },
    { 32945, "Set",        CS::Kind::Button, true, CS::Role(CS::RoleType::Static, CS::Blind) },
    { 32944, "Favorites",  CS::Kind::Button, true, CS::Role() },

    // The 5 hardware mode buttons ARE the page selector.
    { 32946, "Effects",    CS::Kind::Button, true, CS::Role(CS::RoleType::Page, 0) },
    { 32947, "Groups",     CS::Kind::Button, true, CS::Role(CS::RoleType::Page, 1) },
    { 32948, "Looks",      CS::Kind::Button, true, CS::Role(CS::RoleType::Page, 2) },
    { 32949, "Macros",     CS::Kind::Button, true, CS::Role(CS::RoleType::Page, 3) },
    { 32931, "Fix Cont",   CS::Kind::Button, true, CS::Role(CS::RoleType::Page, 4) },

    // 10 strips: select, load, up/down. Up/Down have no Role yet (see the
    // design doc's open discussion point on what they're actually for).
    { 32934, "1",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 1) },
    { 32935, "2",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 2) },
    { 32936, "3",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 3) },
    { 32937, "4",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 4) },
    { 32938, "5",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 5) },
    { 32939, "6",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 6) },
    { 32940, "7",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 7) },
    { 32941, "8",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 8) },
    { 32942, "9",  CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 9) },
    { 32943, "10", CS::Kind::Button, true, CS::Role(CS::RoleType::Select, 10) },

    { 32921, "1-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 1) },
    { 32922, "2-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 2) },
    { 32923, "3-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 3) },
    { 32924, "4-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 4) },
    { 32925, "5-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 5) },
    { 32926, "6-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 6) },
    { 32927, "7-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 7) },
    { 32928, "8-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 8) },
    { 32929, "9-Load",  CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 9) },
    { 32930, "10-Load", CS::Kind::Button, true, CS::Role(CS::RoleType::Load, 10) },

    { 32897, "1-Down",  CS::Kind::Button, true, CS::Role() },
    { 32898, "2-Down",  CS::Kind::Button, true, CS::Role() },
    { 32899, "3-Down",  CS::Kind::Button, true, CS::Role() },
    { 32900, "4-Down",  CS::Kind::Button, true, CS::Role() },
    { 32901, "5-Down",  CS::Kind::Button, true, CS::Role() },
    { 32902, "6-Down",  CS::Kind::Button, true, CS::Role() },
    { 32903, "7-Down",  CS::Kind::Button, true, CS::Role() },
    { 32904, "8-Down",  CS::Kind::Button, true, CS::Role() },
    { 32905, "9-Down",  CS::Kind::Button, true, CS::Role() },
    { 32906, "10-Down", CS::Kind::Button, true, CS::Role() },

    // "N-Up" resets fader N's currently-mapped value to 0 and doubles as a
    // "this fader is in use" indicator (lit whenever its Level index applies
    // to the focused palette's type — see stateFor()).
    { 32908, "1-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 1) },
    { 32909, "2-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 2) },
    { 32910, "3-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 3) },
    { 32911, "4-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 4) },
    { 32912, "5-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 5) },
    { 32913, "6-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 6) },
    { 32914, "7-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 7) },
    { 32915, "8-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 8) },
    { 32916, "9-Up",  CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 9) },
    { 32917, "10-Up", CS::Kind::Button, true, CS::Role(CS::RoleType::Reset, 10) },
};
// clang-format on

// OpenDeck packs blink speed into the low bits of the same byte that carries
// brightness (see qlcplus-midi-profiler's README, "OpenDeck LED values are
// not just brightness") — only every 4th level is steady. Snap any requested
// brightness to the nearest steady level so a state change never
// accidentally sets a control blinking.
const int kSteadyLevels[] = { 0, 15, 31, 47, 63, 79, 95, 111, 127 };

int snapToSteadyLevel(int level)
{
    int best = kSteadyLevels[0];
    int bestDist = qAbs(level - best);
    for (int candidate : kSteadyLevels)
    {
        int dist = qAbs(level - candidate);
        if (dist < bestDist)
        {
            best = candidate;
            bestDist = dist;
        }
    }
    return best;
}

} // namespace

PMJOverlay::PMJOverlay(Doc *doc, ControlSurfaceEngine *engine, App *app, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_engine(engine)
    , m_app(app)
{
    Q_ASSERT(m_doc != nullptr);
    Q_ASSERT(m_engine != nullptr);
    Q_ASSERT(m_app != nullptr);

    registerDevice();

    connect(m_doc->inputOutputMap(), SIGNAL(inputValueChanged(quint32,quint32,uchar,QString)),
            this, SLOT(slotInputValueChanged(quint32,quint32,uchar,QString)));
    connect(m_engine, &ControlSurfaceEngine::roleActivated,
            this, &PMJOverlay::slotRoleActivated);

    connect(m_doc->inputOutputMap(), &InputOutputMap::blackoutChanged,
            this, &PMJOverlay::slotBlackoutChanged);
    connect(m_doc->inputOutputMap(), &InputOutputMap::outputInhibitedChanged,
            this, &PMJOverlay::slotOutputInhibitedChanged);
    connect(m_doc->inputOutputMap(), &InputOutputMap::grandMasterValueChanged,
            this, &PMJOverlay::slotGrandMasterValueChanged);
    connect(m_doc->programmer(), &ProgrammerController::programmerSelectionChanged,
            this, [this]() { m_engine->refreshDevice(deviceId); });

    findKnownUniverse();
    m_engine->refreshDevice(deviceId);
}

void PMJOverlay::findKnownUniverse()
{
    // So the board lights up on connect/launch instead of staying dark until
    // the user presses something first (slotInputValueChanged() also learns
    // this from real traffic, as a fallback for patching the board AFTER
    // startup, or a re-patch to a different universe mid-session).
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 u = 0; u < iom->universesCount(); u++)
    {
        Universe *universe = iom->universe(u);
        if (universe == nullptr)
            continue;
        InputPatch *patch = universe->inputPatch();
        if (patch != nullptr && patch->profileName().compare(deviceId, Qt::CaseInsensitive) == 0)
        {
            m_universe = int(u);
            return;
        }
    }
}

PMJOverlay::~PMJOverlay()
{
    m_engine->unregisterDevice(deviceId);
}

QList<quint32> PMJOverlay::sceneTargetFixtures() const
{
    QList<quint32> fixtures;
    ProgrammingManager *pm = programmingManager();
    quint32 sceneId = pm ? pm->currentSceneId() : Function::invalidId();
    Scene *scene = sceneId != Function::invalidId()
                       ? qobject_cast<Scene *>(m_doc->function(sceneId)) : nullptr;
    if (!scene)
        return fixtures;

    for (quint32 gid : scene->fixtureGroups())
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (!g)
            continue;
        for (quint32 fid : g->fixtureList())
            if (!fixtures.contains(fid))
                fixtures.append(fid);
    }
    for (quint32 fid : scene->fixtures())
        if (!fixtures.contains(fid))
            fixtures.append(fid);

    return fixtures;
}

bool PMJOverlay::faderInUse(int index) const
{
    ProgrammingManager *pm = programmingManager();
    quint32 pid = pm ? pm->currentPaletteId() : QLCPalette::invalidId();
    QLCPalette *pal = pid != QLCPalette::invalidId() ? m_doc->palette(pid) : nullptr;
    if (!pal)
        return false;
    if (pal->type() == QLCPalette::Color)
        return index >= 1 && index <= 6;
    if (pal->type() == QLCPalette::Dimmer)
        return index == 1;
    return false;
}

ProgrammingManager *PMJOverlay::programmingManager() const
{
    if (!m_programmingManager)
    {
        m_programmingManager = m_app->findChild<ProgrammingManager *>();
        if (m_programmingManager)
        {
            connect(m_programmingManager, &ProgrammingManager::currentPaletteIdChanged,
                    this, [this](quint32) { m_engine->refreshDevice(deviceId); });
        }
    }
    return m_programmingManager;
}

void PMJOverlay::registerDevice()
{
    ControlSurfaceEngine::Device dev;
    dev.id = deviceId;
    dev.maxBrightness = 127;   // 7-bit MIDI velocity, not the engine's 0-15 default

    for (const PmjControl &pc : kPmjControls)
    {
        int index = dev.controls.size();
        ControlSurface::Control ctl;
        ctl.kind = pc.kind;
        ctl.name = QString::fromLatin1(pc.name);
        ctl.ledId = pc.hasFeedback ? int(pc.channel) : -1;
        dev.controls.append(ctl);

        m_channelToIndex.insert(pc.channel, index);
        if (pc.hasFeedback)
            m_ledChannel.insert(index, pc.channel);
        if (pc.role.isValid())
            dev.roleOf.insert(index, pc.role);
    }

    dev.ledSink = [this](const ControlSurface::Control &control, int level) {
        sendLed(control, level);
    };

    m_engine->registerDevice(dev);
    m_engine->setStateProvider([this](const ControlSurface::Role &role) {
        return stateFor(role);
    });
}

void PMJOverlay::sendLed(const ControlSurface::Control &control, int level)
{
    if (!control.hasLed())
        return;
    // MidiPlugin::sendFeedBack() takes a DMX-style 0-255 value and scales it
    // down to 7-bit MIDI on the way out (DMX2MIDI, ~x>>1 with 255->127
    // special-cased) — so a MIDI-space level needs doubling here to survive
    // that halving intact.
    if (m_universe < 0)
        return;   // haven't seen real PMJ input yet, so don't know where to send this
    int snapped = snapToSteadyLevel(level);
    uchar dmxValue = uchar(qMin(255, snapped * 2));
    m_doc->inputOutputMap()->sendFeedBack(quint32(m_universe), quint32(control.ledId), dmxValue, QVariant());
}

void PMJOverlay::slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString &key)
{
    Q_UNUSED(key)
    // Strip any embedded MIDI-channel bits (see midiprotocol.cpp,
    // feedbackToMidi's `channel & 0x0FFF`) before matching against the
    // PMJ's own channel numbers, in case the input line isn't set to "any"
    // yet and something upstream still tagged it.
    quint32 baseChannel = channel & 0x0FFF;
    auto it = m_channelToIndex.constFind(baseChannel);
    if (it == m_channelToIndex.constEnd())
        it = m_channelToIndex.constFind(channel);
    if (it == m_channelToIndex.constEnd())
        return;   // not a PMJ control

    // Learn which universe the board is actually patched to from real
    // traffic, rather than assuming one — LED feedback couldn't go anywhere
    // correct without this (see sendLed()). Re-learn on every event rather
    // than latching once, so re-patching the board to a different universe
    // mid-session self-corrects instead of sending feedback into the void.
    if (int(universe) != m_universe)
    {
        m_universe = int(universe);
        m_engine->refreshDevice(deviceId);   // catch the board up on LEDs now that we can reach it
    }

    m_engine->inputReceived(deviceId, it.value(), value);
}

void PMJOverlay::slotRoleActivated(const QString &fromDeviceId, ControlSurface::Role role, uchar value)
{
    if (fromDeviceId != deviceId)
        return;

    InputOutputMap *iom = m_doc->inputOutputMap();

    switch (role.type)
    {
    case ControlSurface::RoleType::Static:
        if (role.index == ControlSurface::Blackout && value > 0)
            iom->toggleBlackout();
        else if (role.index == ControlSurface::Blind && value > 0)
            iom->setOutputInhibited(!iom->outputInhibited());
        break;

    case ControlSurface::RoleType::Level:
        if (role.index == -1)   // Master fader = Grand Master
        {
            iom->setGrandMasterValue(value);
            break;
        }
        // Per-strip Level(1..10): context-aware on whatever palette is
        // focused in the Look Editor — a Color look maps them to R G B W A
        // U, a Dimmer look maps fader 1 to intensity ("Look-edit mode",
        // CONTROL_SURFACE_DESIGN.md P2). Otherwise, "Selection mode": fader
        // N (1-6) is the intensity of the Nth fixture in the programmer
        // selection (Select/Load below) — a control that does nothing
        // observable stays inert either way, per the "only act on what's
        // real" rule applied elsewhere in this overlay.
        if (role.index >= 1 && role.index <= 10 && faderInUse(role.index) && m_doc->programmer())
        {
            ProgrammingManager *pm = programmingManager();
            quint32 pid = pm->currentPaletteId();
            QLCPalette *pal = m_doc->palette(pid);
            if (pal->type() == QLCPalette::Color)
                m_doc->programmer()->setDesignColorChannel(pid, role.index - 1, value);
            else
                m_doc->programmer()->setDesignDimmerValue(pid, value);
        }
        else if (role.index >= 1 && role.index <= 6 && m_doc->programmer())
        {
            const QList<quint32> sel = m_doc->programmer()->programmerSelection();
            if (role.index - 1 < sel.size())
            {
                quint32 fid = sel.at(role.index - 1);
                Fixture *fxi = m_doc->fixture(fid);
                quint32 ch = fxi ? fxi->channelNumber(QLCChannel::Intensity, QLCChannel::MSB)
                                  : QLCChannel::invalid();
                if (ch != QLCChannel::invalid())
                    m_doc->programmer()->writeChannelLive(fid, ch, value);
            }
        }
        break;

    case ControlSurface::RoleType::Reset:
        // "N-Up": zero out whatever fader N currently controls. Same
        // in-use gate as the Level write path above, so a press on a fader
        // that isn't doing anything right now is a no-op, not a surprise
        // reset of an unrelated channel.
        if (value > 0 && role.index >= 1 && role.index <= 10 && faderInUse(role.index) && m_doc->programmer())
        {
            ProgrammingManager *pm = programmingManager();
            quint32 pid = pm->currentPaletteId();
            QLCPalette *pal = m_doc->palette(pid);
            if (pal->type() == QLCPalette::Color)
                m_doc->programmer()->setDesignColorChannel(pid, role.index - 1, 0);
            else
                m_doc->programmer()->setDesignDimmerValue(pid, 0);
        }
        break;

    case ControlSurface::RoleType::Page:
        if (value > 0)
        {
            m_activePage = role.index;
            m_engine->refreshDevice(deviceId);
        }
        break;

    case ControlSurface::RoleType::Param:
        // Enc 1 = pan, Enc 2 = tilt, direct and unconditional for now — real
        // per-page parameter targeting (colour/beam/etc. on Enc 3/4) needs
        // the same selection/paging model the rest of this slice is missing.
        // The PMJ's encoders send raw twos-complement MIDI (1..63 = +N,
        // 127..65 = -N — confirmed via qlc-midi monitor during the original
        // encoder classification work), BUT by the time it reaches
        // inputValueChanged() here, QLC+'s own MIDI plugin has already
        // scaled it from 7-bit MIDI into its internal 0-255 (DMX-style)
        // space (MIDI2DMX, ~x<<1 with 127->255 special-cased) — confirmed
        // via live debug logging: a raw MIDI 1 arrived here as 2, a raw 127
        // arrived as 255. So the decode threshold is 128/256, not 64/128.
        if (role.index == 1 || role.index == 2)
        {
            int delta = (value < 128) ? int(value) : int(value) - 256;
            const float degreesPerClick = 5.0f;
            // Both encoders' raw MIDI delta is physically inverted relative
            // to their on-screen effect (confirmed live on the rig: enc 1
            // moved the XY pad dot left on clockwise, not right) — negate
            // both consistently so "clockwise increases" holds for pan and
            // tilt alike.
            float dPan  = (role.index == 1) ? -delta * degreesPerClick : 0.0f;
            float dTilt = (role.index == 2) ? -delta * degreesPerClick : 0.0f;
            // The palette actually on screen right now, not
            // ProgrammerController's separate focused-scene/palette
            // tracking — that only updates when a scene is freshly opened
            // in the Programming tab and doesn't survive an app relaunch,
            // confirmed via debug logging returning before ever reaching
            // ProgrammerController's own log line.
            ProgrammingManager *pm = programmingManager();
            quint32 pid = pm ? pm->currentPaletteId() : QLCPalette::invalidId();
            if (pm && m_doc->programmer())
                m_doc->programmer()->nudgeDesignPanTilt(pid, dPan, dTilt);
        }
        break;

    case ControlSurface::RoleType::Select:
        // Toggle strip N's target fixture into/out of the programmer
        // selection — builds a multi-select ("select 1 thru 5"), confirmed
        // with Branson. Not yet paged past 10 targets (see
        // CONTROL_SURFACE_DESIGN.md P2) — a scene with more only exposes the
        // first 10 for now.
        if (value > 0 && role.index >= 1 && role.index <= 10 && m_doc->programmer())
        {
            const QList<quint32> targets = sceneTargetFixtures();
            if (role.index - 1 < targets.size())
            {
                quint32 fid = targets.at(role.index - 1);
                QList<quint32> sel = m_doc->programmer()->programmerSelection();
                if (sel.contains(fid))
                    sel.removeAll(fid);
                else
                    sel.append(fid);
                m_doc->programmer()->setProgrammerSelection(sel);
            }
        }
        break;

    case ControlSurface::RoleType::Load:
        // Jump straight to just strip N's target, replacing the selection —
        // the Role's own doc comment ("load item N into the programmer").
        if (value > 0 && role.index >= 1 && role.index <= 10 && m_doc->programmer())
        {
            const QList<quint32> targets = sceneTargetFixtures();
            if (role.index - 1 < targets.size())
                m_doc->programmer()->setProgrammerSelection(
                    QList<quint32>{ targets.at(role.index - 1) });
        }
        break;

    case ControlSurface::RoleType::Transport:
        // Registered and received, not yet wired to a real app action —
        // needs the paged selection/navigation model from the next slice.
        break;

    default:
        break;
    }
}

void PMJOverlay::slotBlackoutChanged(bool state)
{
    Q_UNUSED(state)
    m_engine->refreshDevice(deviceId);
}

void PMJOverlay::slotOutputInhibitedChanged(bool state)
{
    Q_UNUSED(state)
    m_engine->refreshDevice(deviceId);
}

void PMJOverlay::slotGrandMasterValueChanged(uchar value)
{
    Q_UNUSED(value)
    m_engine->refreshDevice(deviceId);
}

ControlSurface::State PMJOverlay::stateFor(const ControlSurface::Role &role) const
{
    InputOutputMap *iom = m_doc->inputOutputMap();

    switch (role.type)
    {
    case ControlSurface::RoleType::Static:
        if (role.index == ControlSurface::Blackout)
            return iom->blackout() ? ControlSurface::State::Active : ControlSurface::State::Valid;
        if (role.index == ControlSurface::Blind)
            return iom->outputInhibited() ? ControlSurface::State::Active : ControlSurface::State::Valid;
        return ControlSurface::State::Empty;   // Favorites/Tap: unbound this slice

    // Page/Param/Transport are registered and received (turning one does
    // reach the engine — see slotRoleActivated()), but none of them drive
    // anything a user can see or feel yet, which is the whole point of "the
    // LEDs show what's useful right now": a control that does nothing
    // observable isn't useful yet, so it stays dark rather than lighting for
    // a press that goes nowhere. Light these up page-by-page as each one
    // actually gets wired to something real, not before.
    case ControlSurface::RoleType::Page:
    case ControlSurface::RoleType::Param:
    case ControlSurface::RoleType::Transport:
        return ControlSurface::State::Empty;

    // Select/Load(1-10): lit whenever strip N has a real target in the open
    // scene (P2 selection mode), brighter when that target is actually in
    // the programmer selection right now — same "in use" rule Reset's LEDs
    // follow above, just keyed on scene targets instead of the focused
    // palette's type.
    case ControlSurface::RoleType::Select:
    case ControlSurface::RoleType::Load:
    {
        if (role.index < 1 || role.index > 10)
            return ControlSurface::State::Empty;
        const QList<quint32> targets = sceneTargetFixtures();
        if (role.index - 1 >= targets.size())
            return ControlSurface::State::Empty;
        if (!m_doc->programmer())
            return ControlSurface::State::Valid;
        quint32 fid = targets.at(role.index - 1);
        return m_doc->programmer()->programmerSelection().contains(fid)
                   ? ControlSurface::State::Selected
                   : ControlSurface::State::Valid;
    }

    // "N-Up": lit whenever fader N actually controls something on the
    // focused palette right now — the "highlight the faders in use" ask
    // this doubles as, using the exact same rule the Level/Reset write
    // paths gate on (faderInUse()), so the board never lights a fader that
    // pressing/turning it wouldn't actually do anything to.
    case ControlSurface::RoleType::Reset:
        return faderInUse(role.index) ? ControlSurface::State::Valid : ControlSurface::State::Empty;

    default:
        return ControlSurface::State::Empty;
    }
}
