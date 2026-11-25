/*
  Q Light Controller Plus
  pmjoverlay.h

  PMJ Black 1 (OpenDeck) device overlay for the control-surface engine. Binds
  the board's real MIDI channels (see resources/inputprofiles/PMJ-Black-1.qxi)
  to ControlSurface::Role, feeds raw input into the engine, and sends LED
  feedback back out. See docs/design/CONTROL_SURFACE_DESIGN.md, "P1 — PMJ overlay + LED".

  P1 slice 1: full role table registered (so every control lights up
  sensibly and every press/turn is at least received by the engine). Wired:
  Master fader to Grand Master, O to Blackout, Set to Blind, Go and Back to
  the Run tab's GO and Back (CueRunner::runTabCommand()), Select/Load, the
  faders and Enc 1/2. The mode buttons only say which page is open (its lamp);
  per-page jobs, Left/Right and the page arrows are the next slice.
  Favorites (proposed Tap) is left unbound —
  the only "tap tempo" in the codebase today is Programming-tab-local, not
  the global static-core action the design doc means.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PMJOVERLAY_H
#define PMJOVERLAY_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QHash>

#include "controlsurface.h"
#include "controlsurfaceengine.h"

class Doc;
class App;
class ProgrammingManager;

class PMJOverlay : public QObject
{
    Q_OBJECT

public:
    /** id this device registers under with the engine. */
    static const QString deviceId;
    /** The name of the PMJ's control on input @p channel ("Ch 3",
     *  "Enc 2"); empty when the PMJ has none there. Who listens to a
     *  control asks it (ControlOwners): the overlay hears these on any
     *  universe patched with the PMJ Black 1 profile. */
    static QString controlName(quint32 channel);
    /** Every input channel the overlay acts on (a control with a job). */
    static QList<quint32> controlChannels();

    /** @p app is used to reach ProgrammingManager (findChild) for
     *  ProgrammingManager::currentPaletteId() — the ground truth for "what
     *  pan/tilt palette is on screen right now," independent of
     *  ProgrammerController's separate focused-scene/palette tracking. */
    PMJOverlay(Doc *doc, ControlSurfaceEngine *engine, App *app, QObject *parent = nullptr);
    ~PMJOverlay() override;

signals:
    /** A fader or an encoder was refused (Operate): the words, as the footer shows them. */
    void refused(const QString &why);

public:
    /** The last refusal's words (empty: none yet). */
    QString lastRefusal() const { return m_lastRefusal; }

private slots:
    /** Doc::inputOutputMap()'s per-line input feed, without this
     *  computer's desk controls (showInputValueChanged()). Filtered to just the
     *  channel numbers this overlay knows (the PMJ's, from its profile) so it
     *  doesn't need to know which universe the board happens to be patched
     *  to. */
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString &key);

    /** A bound PMJ control was actuated. */
    void slotRoleActivated(const QString &deviceId, ControlSurface::Role role, uchar value);

    /** Recompute LEDs when global state this overlay's static-core roles
     *  reflect changes (blackout, blind, grand master). */
    void slotBlackoutChanged(bool state);
    void slotOutputInhibitedChanged(bool state);
    void slotGrandMasterValueChanged(uchar value);
    /** Go's or Back's lamp may have changed (a GO, a stop, a list or mode
     *  change): once per turn of the event loop, the board repainted only
     *  when one of the two did. */
    void scheduleTransportLamps();

private:
    void registerDevice();
    /** Whether @a universe's input is patched with the PMJ Black 1 profile. */
    bool isPmjUniverse(quint32 universe) const;
    /** In Operate a fader or an encoder edits nothing (as the Encoder bank):
     *  true when refused, with the words in the footer. */
    bool refuseInOperate(bool encoder);
    /** Scan every universe's input patch for one whose profile matches
     *  deviceId, so LEDs can light up on connect without waiting for the
     *  user to press something first. Sets m_universe if found. */
    void findKnownUniverse();
    ControlSurface::State stateFor(const ControlSurface::Role &role) const;
    /** Whether the show (a Control Map trigger, a VC widget...) has learned
     *  PMJ control @a channel: then that is its job, not the overlay's. */
    bool showOwns(quint32 channel) const;
    void sendLed(const ControlSurface::Control &control, int level);

    /** ProgrammingManager doesn't exist yet when PMJOverlay is constructed
     *  (it's built later in the same App::init() call), so it can't be
     *  looked up and connected once at construction — this finds it (and
     *  wires ProgrammingManager::currentPaletteIdChanged to a device
     *  refresh, so LEDs reflect a look focus change immediately rather than
     *  only on the next unrelated interaction) the first time anything
     *  needs it, then caches the result. Returns nullptr if still not
     *  constructed yet. const so stateFor() can use it too — the cache and
     *  one-time connect() are the only mutation, via the mutable member
     *  below. */
    ProgrammingManager *programmingManager() const;

    /** Whether Level/Reset fader index @p index (1-10) currently maps to
     *  anything, given the palette focused in the Look Editor — Color maps
     *  1-6 (R G B White Amber UV), Dimmer maps just 1 (intensity), anything
     *  else (including nothing focused) maps none. Single source of truth
     *  shared by the Level write path, the Reset (fader-Up) write path, and
     *  their LED state in stateFor(), so all three always agree on which
     *  faders are "live" right now. */
    bool faderInUse(int index) const;

    /** P2 "selection mode": the fixtures Select(1-10)/Load(1-10) currently
     *  address — the scene shown in the Programming canvas
     *  (ProgrammingManager::currentSceneId())'s fixture-group members
     *  (expanded, group order) followed by its individually-fixed fixtures,
     *  deduped, in that combined order ("canvas order"). Index 0 = strip 1.
     *  Not yet paged past 10 (see docs/design/CONTROL_SURFACE_DESIGN.md P2) — a scene
     *  with more than 10 targets only exposes the first 10 for now. */
    QList<quint32> sceneTargetFixtures() const;

    Doc *m_doc;
    /* QPointer, not a raw pointer: the overlay and the engine are both
       children of App, so on shutdown QObject child deletion can destroy the
       engine first and leave this dangling (see ~PMJOverlay). */
    QPointer<ControlSurfaceEngine> m_engine;
    App *m_app;
    mutable ProgrammingManager *m_programmingManager = nullptr;

    /** PMJ MIDI channel number -> index into the registered control list.
     *  Channel numbers come straight from PMJ-Black-1.qxi (see
     *  registerDevice()); this is how a raw inputValueChanged is turned back
     *  into "which control fired" without re-parsing the profile at runtime. */
    QHash<quint32, int> m_channelToIndex;

    /** Index into the registered control list -> its LED feedback channel
     *  number (only present for controls the profile gives a <Feedback> —
     *  faders and encoder pushes have none). */
    QHash<int, quint32> m_ledChannel;

    /** Which page button (Role::Page index) is the active page: Effects
     *  (0) until another is pressed. Its lamp is lit (State::Active); real
     *  per-page behaviour is the next slice. */
    int m_activePage = 0;
    bool m_transportQueued = false;
    int m_transportLamps = -1;      ///< Go's and Back's states last painted
    /** Go and Back held down (ControlSurface::Go / Back): a press is the
     *  edge from 0, never a repeat of a held key. */
    QSet<int> m_transportDown;

    /** The universe real PMJ input has actually been observed arriving on —
     *  learned from the first matching inputValueChanged, since the overlay
     *  doesn't otherwise know which universe the user patched the board to
     *  (and guessing a fixed one, e.g. 0, silently sends LED feedback nowhere
     *  if that guess is wrong). -1 = not yet known, LED sends are skipped. */
    int m_universe = -1;
    QString m_lastRefusal;
    QSet<int> m_warnedSecond;   // second boards already named
    QElapsedTimer m_refusedAt[2];   // faders, encoders: each said once a second
};

#endif // PMJOVERLAY_H
