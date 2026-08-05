/*
  Q Light Controller Plus
  effectscriptrunner.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectscriptrunner.h"
#include "effectinstance.h"

#include "doc.h"
#include "scene.h"
#include "qlcpalette.h"
#include "mastertimer.h"
#include "inputoutputmap.h"
#include "universe.h"
#include "fadechannel.h"
#include "audiocapture.h"

#include <QCoreApplication>
#include <QDebug>

static const int TICK_INTERVAL_MS = 20; // ~50 Hz, matches MasterTimer default
// GC one effect instance every N ticks (round-robin). At 5 (~100 ms) a rig of N
// effects has each instance collected every 5*N ticks, bounding the V4 heap
// without a synchronized stop-the-world burst.
static const int GC_EVERY_TICKS = 5;

EffectScriptRunner::EffectScriptRunner(Doc *doc)
    : QObject(doc)
    , m_doc(doc)
{
    m_cache.rescan();
    m_presetCache.rescan();

    // Drive tick() on the main thread. Use a precise timer so the effect
    // actuates promptly and doesn't stutter/lag when the GUI event loop is busy
    // (a coarse timer can drift by tens of ms under load, which reads as the
    // flicker "starting late" or freezing while the window has focus).
    connect(&m_tickTimer, &QTimer::timeout,
            this, &EffectScriptRunner::slotTick);
    m_tickTimer.setTimerType(Qt::PreciseTimer);
    m_tickTimer.setInterval(TICK_INTERVAL_MS);
    m_tickTimer.start();

    // Destroy all JS engines before Qt's object tree tears down — QJSEngine
    // V4 GC teardown is expensive and blocks the main thread for seconds.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &EffectScriptRunner::slotPrepareQuit);

    // Subscribe to all input value changes
    if (m_doc->inputOutputMap())
    {
        connect(m_doc->inputOutputMap(),
                SIGNAL(inputValueChanged(quint32, quint32, uchar, const QString&)),
                this,
                SLOT(slotInputValueChanged(quint32, quint32, uchar, const QString&)));
    }
}

EffectScriptRunner::~EffectScriptRunner()
{
    // slotPrepareQuit() should have already cleaned up via aboutToQuit.
    // This is a safety net for cases where the runner is destroyed without
    // the application quitting (unit tests, etc.).
    slotPrepareQuit();
}

void EffectScriptRunner::slotPrepareQuit()
{
    m_tickTimer.stop();

    if (m_registered && m_doc && m_doc->masterTimer())
    {
        m_doc->masterTimer()->unregisterDMXSource(this);
        m_registered = false;
    }

    m_faders.clear(); // faders dismissed via universe destructor during shutdown

    {
        QMutexLocker locker(&m_instanceMutex);
        qDeleteAll(m_instances);
        m_instances.clear();
    }

    qDeleteAll(m_parked);
    m_parked.clear();

    QMutexLocker locker(&m_writesMutex);
    m_publishedWrites.clear();
}

// ---------------------------------------------------------------------------
// Scene lifecycle
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotFunctionStarted(quint32 fid)
{
    Function *f = m_doc->function(fid);
    if (!f || f->type() != Function::SceneType)
        return;
    createInstancesForScene(fid);
}

void EffectScriptRunner::slotFunctionStopped(quint32 fid)
{
    // Deliberately NOT gated on Doc::function(fid) still resolving. This slot
    // is queued, so by the time it runs the function may already be gone from
    // the Doc (workspace cleared, or the scene deleted while running) — and an
    // early return there would strand the instance forever: it keeps ticking,
    // keeps re-asserting its last frame into an Override-priority fader, and
    // leaks a whole QJSEngine. destroyInstancesForScene() is keyed on the
    // instance's own sceneId, so it needs no Doc lookup and is a no-op for ids
    // that own no instances.
    destroyInstancesForScene(fid);
}

void EffectScriptRunner::slotFunctionRemoved(quint32 fid)
{
    // Doc::deleteFunction() removes the function from the MasterTimer's list
    // WITHOUT emitting functionStopped, so deleting a running scene would
    // otherwise never tear its effect instances down.
    destroyInstancesForScene(fid);
}

void EffectScriptRunner::slotDocCleared()
{
    // File > New / Open: every scene is about to be deleted. Drop all
    // instances now rather than waiting for stop signals that won't arrive.
    QList<quint32> sceneIds;
    {
        QMutexLocker locker(&m_instanceMutex);
        for (const EffectInstance *inst : m_instances)
            if (!sceneIds.contains(inst->sceneId()))
                sceneIds.append(inst->sceneId());
    }
    for (quint32 sid : sceneIds)
        destroyInstancesForScene(sid);

    // Persistence is scoped to the workspace: a new one starts every effect
    // clean, so nothing may be carried over (and the parked engines belong to
    // palettes that are about to be destroyed).
    qDeleteAll(m_parked);
    m_parked.clear();
}

void EffectScriptRunner::slotPaletteRemoved(quint32 paletteId)
{
    // The look is gone — so is any engine parked under it.
    discardParked(paletteId);
}

void EffectScriptRunner::createInstancesForScene(quint32 sceneId)
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (!scene)
        return;

    for (quint32 pid : scene->palettes())
    {
        QLCPalette *pal = m_doc->palette(pid);
        if (!pal || pal->type() != QLCPalette::Effect)
            continue;

        // Already running this look (another scene in the same collection also
        // carries it, or a crossfade has both scenes live)? Leave the single
        // running engine alone — one look means one effect.
        if (instanceForPalette(pid) != nullptr)
            continue;

        EffectInstance *inst = nullptr;

        // Persistent look: adopt the parked engine if we have one, so the script
        // resumes exactly where it left off instead of restarting.
        if (pal->persistent() && m_parked.contains(pid))
        {
            inst = m_parked.take(pid);
            inst->rebindToScene(sceneId);
            qDebug() << "[EffectScriptRunner] resumed persistent effect palette"
                     << pid << "in scene" << sceneId;
        }

        if (inst == nullptr)
            inst = new EffectInstance(m_doc, sceneId, pid);

        if (!inst->isValid())
        {
            qWarning() << "[EffectScriptRunner] instance invalid for palette" << pid;
            delete inst;
            continue;
        }

        {
            QMutexLocker locker(&m_instanceMutex);
            m_instances.append(inst);
        }

        // Register DMXSource on first instance
        if (!m_registered && m_doc->masterTimer())
        {
            m_doc->masterTimer()->registerDMXSource(this);
            m_registered = true;
        }

    }

    updateAudioSubscription();
}

void EffectScriptRunner::syncScene(quint32 sceneId)
{
    // Used when a running scene's palette list changes (no stop/start fired) and
    // when the Programming tab re-syncs a look the user is editing.
    //
    // The parked engines for this scene's looks are DISCARDED, not reused. This
    // is an edit, not a transition: the script, its parameters or the persistence
    // flag itself may have just changed, and adopting the parked engine would
    // silently keep running the pre-edit script. Persistence is meant to carry an
    // effect across a scene CHANGE, never across a change to the effect.
    QList<quint32> paletteIds;
    if (Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId)))
        paletteIds = scene->palettes();

    destroyInstancesForScene(sceneId);

    for (quint32 pid : paletteIds)
        discardParked(pid);

    createInstancesForScene(sceneId);
}

void EffectScriptRunner::destroyInstancesForScene(quint32 sceneId)
{
    // Deadlock prevention: unregisterDMXSource acquires m_dmxSourceListMutex,
    // but the timer thread holds m_dmxSourceListMutex while calling writeDMX.
    // Collect cleanup work under the lock, then release before touching
    // MasterTimer or universes.
    bool shouldUnregister = false;

    {
        QMutexLocker locker(&m_instanceMutex);
        QMutableListIterator<EffectInstance*> it(m_instances);
        while (it.hasNext())
        {
            EffectInstance *inst = it.next();
            if (inst->sceneId() != sceneId)
                continue;

            it.remove();

            // A persistent look is PARKED rather than destroyed: hold the engine
            // (and the script state inside it) so the next scene using this same
            // palette can pick the effect up mid-flight. Its writes stop
            // immediately either way — publishWrites() below only walks
            // m_instances, so a parked effect drives no DMX.
            const QLCPalette *pal = m_doc->palette(inst->effectPaletteId());
            if (pal != nullptr && pal->persistent())
            {
                // Shouldn't happen (one look = one engine), but never leak if it does.
                delete m_parked.value(inst->effectPaletteId(), nullptr);
                m_parked.insert(inst->effectPaletteId(), inst);
            }
            else
            {
                delete inst;
            }
        }

        if (m_instances.isEmpty() && m_registered)
        {
            shouldUnregister = true;
            m_registered = false;
        }
    } // release m_instanceMutex before touching MasterTimer

    // Republish immediately: the destroyed instances' writes are still sitting in
    // m_publishedWrites, and writeDMX() would keep asserting that dead frame until
    // the next tick republished without them.
    publishWrites();

    if (shouldUnregister)
    {
        if (m_doc->masterTimer())
            m_doc->masterTimer()->unregisterDMXSource(this);

        // Dismiss all per-universe faders so no stale values linger.
        QList<Universe*> ua = m_doc->inputOutputMap()->claimUniverses();
        for (auto it2 = m_faders.begin(); it2 != m_faders.end(); ++it2)
        {
            int idx = it2.key();
            if (!it2.value().isNull() && idx >= 0 && idx < ua.size())
                ua.at(idx)->dismissFader(it2.value());
        }
        m_doc->inputOutputMap()->releaseUniverses(false);
        m_faders.clear();
    }

    updateAudioSubscription();
}

void EffectScriptRunner::updateAudioSubscription()
{
    // Does any live instance subscribe to the "audio" data channel?
    bool wantAudio = false;
    {
        QMutexLocker locker(&m_instanceMutex);
        for (EffectInstance *inst : m_instances)
        {
            if (inst->dataChannelKeys().contains(QStringLiteral("audio")))
            {
                wantAudio = true;
                break;
            }
        }
    }

    if (wantAudio == m_audioActive)
        return;

    QSharedPointer<AudioCapture> cap = m_doc->audioInputCapture();
    if (cap.isNull())
        return;
    const int bands = 16;

    if (wantAudio)
    {
        connect(cap.data(), SIGNAL(dataProcessed(double*,int,double,quint32)),
                this, SLOT(slotAudioData(double*,int,double,quint32)));
        cap->registerBandsNumber(bands);   // starts the capture thread on first use
        m_audioActive = true;
    }
    else
    {
        cap->unregisterBandsNumber(bands);
        disconnect(cap.data(), SIGNAL(dataProcessed(double*,int,double,quint32)),
                   this, SLOT(slotAudioData(double*,int,double,quint32)));
        m_audioActive = false;
        // Clear the channel so a re-subscribing effect starts from silence.
        setDataChannel(QStringLiteral("audio"), QVariantMap());
    }
}

void EffectScriptRunner::slotAudioData(double *spectrumBands, int size,
                                       double maxMagnitude, quint32 power)
{
    QVariantList bands;
    bands.reserve(size);
    double peak = 0.0;
    for (int i = 0; i < size; i++)
    {
        double b = (maxMagnitude > 0.0) ? (spectrumBands[i] / maxMagnitude) : 0.0;
        if (b < 0.0) b = 0.0; else if (b > 1.0) b = 1.0;
        bands << b;
        if (b > peak) peak = b;
    }
    // power is a 15-bit-ish signal power (matches AudioTriggerWidget's /0x7FFF).
    double level = double(power) / 32767.0;
    if (level > 1.0) level = 1.0;

    QVariantMap ad;
    ad[QStringLiteral("bands")] = bands;   // [16] low→high, 0..1
    ad[QStringLiteral("level")] = level;   // overall volume 0..1
    ad[QStringLiteral("peak")]  = peak;    // loudest band 0..1
    setDataChannel(QStringLiteral("audio"), ad);
}

// ---------------------------------------------------------------------------
// Main-thread tick
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotTick()
{
    // Effect scripts run in BOTH modes: live in Operate, and on the previewed
    // scene in Edit so the operator can fly the beam (joystick) while building.
    // The previewed scene is started via Function::start() in Design too, so it
    // has live instances here.  The beam position carries continuously across an
    // Edit↔Run switch via m_lastSpotDeg (followMode = "lastPosition").
    // NOTE: no lock is held across runTick(). m_instances is main-thread-only
    // (see the header) and writeDMX() no longer reads it, so the MasterTimer
    // thread cannot be blocked by script evaluation here.
    QMutexLocker locker(&m_instanceMutex);
    bool anyTicked = false;
    for (EffectInstance *inst : m_instances)
    {
        // Wait-for-input: an input-reactive effect (midi/audio/joystick) whose
        // last frame drove nothing, with no new input since, is skipped entirely
        // — no 1024-cell JS rebuild at 50 Hz just to emit black. It keeps its
        // (empty) last writes and wakes the moment fresh input sets m_inputDirty.
        // Time-driven effects (canIdle() == false) always tick.
        if (inst->canIdle() && inst->lastFrameEmpty() && !m_inputDirty)
            continue;

        anyTicked = true;
        inst->setLastSpotPositions(m_lastSpotDeg);
        inst->setDataChannels(m_dataChannels);
        inst->runTick();

        // Capture the pan/tilt the script just produced so a later scene (whose
        // followspot uses followMode = "lastPosition") can resume from where the
        // beam left off — even after this instance is destroyed on scene stop.
        const QHash<quint32, QPointF> &deg = inst->lastIntentDegrees();
        for (auto it = deg.constBegin(); it != deg.constEnd(); ++it)
            m_lastSpotDeg[it.key()] = it.value();
    }
    // Consume the input edge once every instance has had its chance to react.
    m_inputDirty = false;

    // Bound the per-instance QJSEngine heap: runTick() allocates a fresh fixtures
    // array + objects every tick, which V4 only reclaims lazily, so RSS creeps up
    // over a long show. GC one instance every few ticks (round-robin, not all at
    // once) so the cost is spread and no single tick stalls. This is on the main
    // thread; writeDMX() reads m_publishedWrites, so the DMX thread never waits.
    if (!m_instances.isEmpty() && (++m_gcTick % GC_EVERY_TICKS) == 0)
    {
        m_gcCursor = (m_gcCursor + 1) % m_instances.size();
        m_instances.at(m_gcCursor)->collectGarbage();
    }
    locker.unlock();

    // Nothing ticked → the aggregate is identical to last frame; skip the rebuild.
    if (anyTicked)
        publishWrites();
}

EffectInstance *EffectScriptRunner::instanceForPalette(quint32 paletteId) const
{
    QMutexLocker locker(&m_instanceMutex);
    for (EffectInstance *inst : m_instances)
        if (inst->effectPaletteId() == paletteId)
            return inst;
    return nullptr;
}

void EffectScriptRunner::updateEffectParams(quint32 paletteId)
{
    // Live param edit: overlay the palette's new values onto the running instance
    // (main thread — same thread runTick() runs on, so no lock vs. the tick). No
    // instance recreation, so the effect keeps its JS state.
    if (EffectInstance *inst = instanceForPalette(paletteId))
        inst->reloadParamsFromPalette();
}

void EffectScriptRunner::discardParked(quint32 paletteId)
{
    delete m_parked.take(paletteId);
}

void EffectScriptRunner::publishWrites()
{
    QList<EffectInstance::DmxWrite> combined;
    {
        QMutexLocker locker(&m_instanceMutex);
        for (const EffectInstance *inst : m_instances)
            combined += inst->dmxWrites();
    }

    QMutexLocker locker(&m_writesMutex);
    m_publishedWrites = combined;
}

void EffectScriptRunner::setDataChannel(const QString &name, const QVariantMap &data)
{
    m_dataChannels[name] = data;
    // Fresh input (midi/audio/joystick) → wake idle input-reactive effects on the
    // next tick (see slotTick's idle skip).
    m_inputDirty = true;
}

void EffectScriptRunner::publishMidiData()
{
    // Build a per-universe view AND a merged "any-source" view. Effects read the
    // merged data.midi by default, or data.midi.universes[<n>] to lock onto one
    // device (n = the universe number shown on the Inputs/Outputs page).
    int   mergedVel[128];
    bool  mergedHeld[128];
    for (int i = 0; i < 128; i++) { mergedVel[i] = 0; mergedHeld[i] = false; }
    bool    anySustain  = false;
    int     lastNote    = -1;
    int     lastVel     = 0;
    quint32 noteOnCount = 0;
    QVariantMap universes;

    for (auto it = m_midiByUniverse.constBegin(); it != m_midiByUniverse.constEnd(); ++it)
    {
        const MidiNoteState &st = it.value();
        QVariantList vel, held;
        vel.reserve(128);
        held.reserve(128);
        for (int i = 0; i < 128; i++)
        {
            vel << int(st.velocity[i]);
            held << st.held[i];
            if (st.held[i])
            {
                mergedHeld[i] = true;
                if (int(st.velocity[i]) > mergedVel[i]) mergedVel[i] = st.velocity[i];
            }
        }
        if (st.sustain) anySustain = true;
        noteOnCount += st.noteOnCount;
        if (st.lastNote >= 0) { lastNote = st.lastNote; lastVel = st.lastVelocity; }

        QVariantMap um;
        um[QStringLiteral("velocity")]     = vel;
        um[QStringLiteral("held")]         = held;
        um[QStringLiteral("sustain")]      = st.sustain;
        um[QStringLiteral("lastNote")]     = st.lastNote;
        um[QStringLiteral("lastVelocity")] = st.lastVelocity;
        um[QStringLiteral("noteOnCount")]  = int(st.noteOnCount);
        // Key by the 1-based universe number shown on the I/O page.
        universes[QString::number(it.key() + 1)] = um;
    }

    QVariantList velocity, held;
    velocity.reserve(128);
    held.reserve(128);
    for (int i = 0; i < 128; i++) { velocity << mergedVel[i]; held << mergedHeld[i]; }

    QVariantMap md;
    md[QStringLiteral("velocity")]     = velocity;      // merged [128] 0-127
    md[QStringLiteral("held")]         = held;          // merged [128] bool
    md[QStringLiteral("sustain")]      = anySustain;
    md[QStringLiteral("lastNote")]     = lastNote;
    md[QStringLiteral("lastVelocity")] = lastVel;
    md[QStringLiteral("noteOnCount")]  = int(noteOnCount);
    md[QStringLiteral("universes")]    = universes;     // per-source states
    setDataChannel(QStringLiteral("midi"), md);
}

// ---------------------------------------------------------------------------
// Input routing
// ---------------------------------------------------------------------------

void EffectScriptRunner::slotInputValueChanged(quint32 universe, quint32 channel,
                                                uchar value, const QString &key)
{
    Q_UNUSED(key)

    // Any input event wakes idle input-reactive effects for the next tick (covers
    // bound CC/joystick inputs that don't flow through setDataChannel).
    m_inputDirty = true;

    // --- MIDI note/CC → the "midi" data channel (for note-reactive effects) ---
    // The MIDI plugin decodes notes to input channels 128-255 (note = ch-128,
    // value = velocity*2, 0 = note-off) and CCs to 0-127 (CC64 = sustain).
    // NOTE: not scoped by patch type yet — a non-MIDI input in this channel range
    // would also register; MIDI effects are opt-in via dataChannels:["midi"].
    if (channel >= 128 && channel <= 255)
    {
        MidiNoteState &st = m_midiByUniverse[universe];   // per-source state
        const int note = int(channel) - 128;
        const bool held = value > 0;
        if (held)
        {
            const uchar vel = (value == 255) ? 127 : uchar(value >> 1);
            st.velocity[note] = vel;
            if (!st.held[note])
            {
                st.noteOnCount++;
                st.lastNote = note;
                st.lastVelocity = vel;
            }
            st.held[note] = true;
        }
        else
        {
            st.held[note] = false;   // release; keep velocity as decay ref
        }
        publishMidiData();
    }
    else if (channel == 64)   // CC64 sustain pedal
    {
        MidiNoteState &st = m_midiByUniverse[universe];
        const bool sus = value >= 128;        // MIDI2DMX(64) == 128
        if (sus != st.sustain)
        {
            st.sustain = sus;
            publishMidiData();
        }
    }

    const float norm = value / 255.0f;

    QMutexLocker locker(&m_instanceMutex);
    for (EffectInstance *inst : m_instances)
    {
        QLCPalette *pal = m_doc->palette(inst->effectPaletteId());
        if (!pal) continue;

        const QMap<QString, QPair<quint32,quint32>> &binds = pal->effectInputBindings();
        for (auto it = binds.constBegin(); it != binds.constEnd(); ++it)
        {
            if (it.value().first == universe && it.value().second == channel)
                inst->setInputValue(it.key(), norm);
        }
    }
}

// ---------------------------------------------------------------------------
// DMXSource — timer thread
// ---------------------------------------------------------------------------

void EffectScriptRunner::writeDMX(MasterTimer *timer, QList<Universe*> universes)
{
    Q_UNUSED(timer)

    // Effect scripts write in both modes — live in Operate, and on the previewed
    // scene in Edit (so the joystick can fly the beam while building).

    // Clear all faders so channels from the previous tick don't linger when
    // the effect stops writing them (e.g. fixture removed from scene).
    for (auto &fader : m_faders)
        if (!fader.isNull()) fader->removeAll();

    // Take a copy of the last published tick and release immediately. This mutex
    // is NEVER held across script evaluation (that is the whole reason it exists
    // separately from m_instanceMutex), so this can't stall the DMX thread.
    QList<EffectInstance::DmxWrite> writes;
    {
        QMutexLocker locker(&m_writesMutex);
        writes = m_publishedWrites;
    }

    for (const auto &w : writes)
    {
        if (w.universeId < 0 || w.universeId >= universes.size())
            continue;
        Universe *uni = universes.at(w.universeId);
        if (!uni)
            continue;

        // Get or create a GenericFader for this universe. Using a fader
        // instead of uni->write() ensures that processFaders() writes our
        // values AFTER zeroIntensityChannels() has run — without this,
        // the RGB writes are wiped before they reach the DMX output.
        //
        // Priority is Override (not Auto): an effect palette is meant to
        // layer ON TOP of the scene it's attached to.  At equal (Auto)
        // priority the winner is decided by fader insertion order, which is
        // fragile across scene transitions — the scene's Aim/PanTilt palette
        // could win and the effect's pan/tilt would be silently overridden
        // (followspot couldn't move the beam).  Override guarantees the
        // effect fader is written last, so it reliably wins LTP channels
        // (pan/tilt).  HTP channels (intensity) still max regardless of
        // order, so this doesn't change colour/dimmer behaviour.
        QSharedPointer<GenericFader> &fader = m_faders[w.universeId];
        if (fader.isNull())
        {
            fader = uni->requestFader(Universe::Override);
            fader->setName(QStringLiteral("EffectScript"));
        }

        FadeChannel fc(m_doc, w.fixtureId, w.channel);
        fc.setTarget(w.value);
        fc.setCurrent(w.value);
        fc.setFadeTime(0);
        // replace=true → the effect OWNS this channel: write it verbatim
        // (bypassing the HTP merge) so it can drive the value DOWN below the
        // scene's level (e.g. a candle dimmer flicker under a lit base).
        if (w.replace)
            fc.addFlag(FadeChannel::Override);
        // replace(), not add(): removeAll() above has already emptied the
        // fader, so add()'s HTP-merge branch could never fire anyway — but
        // its insert branch logs a qDebug() line per channel. At 50 Hz over
        // a rig's worth of channels that is thousands of formatted stderr
        // writes per second on the MasterTimer thread, which on its own can
        // blow the tick budget. replace() is the same insert without the log.
        fader->replace(fc);
    }
}
