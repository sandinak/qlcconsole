/*
  Q Light Controller Plus
  pmjoverlay.h

  PMJ Black 1 (OpenDeck) device overlay for the control-surface engine. Binds
  the board's real MIDI channels (see resources/inputprofiles/PMJ-Black-1.qxi)
  to ControlSurface::Role, feeds raw input into the engine, and sends LED
  feedback back out. See CONTROL_SURFACE_DESIGN.md, "P1 — PMJ overlay + LED".

  P1 slice 1: full role table registered (so every control lights up
  sensibly and every press/turn is at least received by the engine), but only
  the clearest global static-core actions are wired to real app behaviour so
  far — Master fader to Grand Master, O to Blackout, Set to Blind. Page
  switching, Select/Load/Param, and Transport are logged but not yet wired to
  real selection/navigation — that is the next slice, once the app-side
  selection APIs they need exist. Favorites (proposed Tap) is left unbound —
  the only "tap tempo" in the codebase today is Programming-tab-local, not
  the global static-core action the design doc means.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PMJOVERLAY_H
#define PMJOVERLAY_H

#include <QObject>
#include <QPointer>
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

    /** @p app is used to reach ProgrammingManager (findChild) for
     *  ProgrammingManager::currentPaletteId() — the ground truth for "what
     *  pan/tilt palette is on screen right now," independent of
     *  ProgrammerController's separate focused-scene/palette tracking. */
    PMJOverlay(Doc *doc, ControlSurfaceEngine *engine, App *app, QObject *parent = nullptr);
    ~PMJOverlay() override;

private slots:
    /** Doc::inputOutputMap()'s raw per-line input feed. Filtered to just the
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

private:
    void registerDevice();
    /** Scan every universe's input patch for one whose profile matches
     *  deviceId, so LEDs can light up on connect without waiting for the
     *  user to press something first. Sets m_universe if found. */
    void findKnownUniverse();
    ControlSurface::State stateFor(const ControlSurface::Role &role) const;
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
     *  Not yet paged past 10 (see CONTROL_SURFACE_DESIGN.md P2) — a scene
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

    /** Which page button (Role::Page index) is currently the active page.
     *  -1 = none selected yet. Drives State::Selected on the page buttons;
     *  real per-page behaviour is the next slice. */
    int m_activePage = -1;

    /** The universe real PMJ input has actually been observed arriving on —
     *  learned from the first matching inputValueChanged, since the overlay
     *  doesn't otherwise know which universe the user patched the board to
     *  (and guessing a fixed one, e.g. 0, silently sends LED feedback nowhere
     *  if that guess is wrong). -1 = not yet known, LED sends are skipped. */
    int m_universe = -1;
};

#endif // PMJOVERLAY_H
