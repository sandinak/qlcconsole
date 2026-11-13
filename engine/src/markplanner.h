/*
  Q Light Controller Plus
  markplanner.h

  Automatic move-in-black. When enabled, a coarse timer peeks the next cue
  (CueLookahead) and, if it's far enough away (dark-gap gate), pre-positions every
  fixture that is DARK now but LIT in that cue and whose aim/colour would change —
  by holding the cue's non-intensity values through the MarkEffect. The mark's own
  auto-handoff releases each fixture as the cue reveals it.

  Dark is the master dimmer at or below DARK_LEVEL, or every shutter that can
  close (the definition's close capabilities, ShutterAutoOpen::closingShutters())
  closed; lit is CueOutput::lightsFixture(). Dark by the dimmer, the next cue's
  shutter is pre-set with the rest. Dark by the shutter alone (dimmer up, or
  no dimmer), the shutter is not held: it stays closed until the cue opens it.
  A fixture with neither a dimmer nor such a shutter is never moved.

  The planner only ever touches marks it created itself (m_planned), so manual
  Mark/Unmark coexists untouched.

  Per list and per cue (Q38): isEnabled() is the show's default; the Show
  Tree decides for the next cue (ShowTreeController::moveInBlackForNext) --
  a list set On pre-positions with the show's setting off, a list set Off
  or a cue flagged "Don't move in black into this cue" never does.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef MARKPLANNER_H
#define MARKPLANNER_H

#include <QObject>
#include "qlcengine_export.h"
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QString>

#include "cuelookahead.h"
#include "cueoutput.h"

class Doc;
class MarkEffect;

#define KXMLQLCMoveInBlack QStringLiteral("MoveInBlack")

class QLC_ENGINE_EXPORT MarkPlanner : public QObject
{
    Q_OBJECT

public:
    MarkPlanner(Doc *doc, MarkEffect *mark, QObject *parent = nullptr);

    /** The show's automatic move-in-black on/off (clears the planner's own
     *  marks when turned off; a cue list set On still pre-positions). */
    void setEnabled(bool enable);
    bool isEnabled() const { return m_enabled; }

    /** Minimum lead time (ms) before the next cue for a fixture to be pre-set —
     *  below this there's no time to move in black, so it's skipped. */
    void setDarkGapMs(int ms) { m_darkGapMs = ms; }
    int  darkGapMs() const { return m_darkGapMs; }
    /** The lead time a new show starts with. */
    static int defaultDarkGapMs() { return 800; }

    /** How many fixtures the planner is pre-positioning right now (its own
     *  marks, not manual ones). */
    int plannedCount() const { return m_planned.size(); }

    /** Fixtures that are currently Marked (positioned-but-dark, manual or
     *  auto) but do NOT appear, lit, in any upcoming cue within the
     *  lookahead horizon -- a pre-set that nothing is about to reveal.
     *  Pure query; safe to call regardless of isEnabled(). */
    QList<quint32> dangleFixtures() const;

signals:
    void enabledChanged(bool enable);
    /** Emitted whenever the dangling-fixture set changes (including going
     *  empty again). Checked every tick regardless of isEnabled(), since
     *  manual marks can dangle even with auto move-in-black off. */
    void dangleFixturesChanged(const QList<quint32> &fixtureIds);
    /** The planner's own pre-sets changed: @p count fixtures are being moved
     *  in black now (0: none). For an "it's working" indicator. */
    void plannedChanged(int count);

private slots:
    /** Re-evaluate the plan against the current/next cue. */
    void evaluate();

private:
    void evaluatePlan();
    /** Whether to move in black into @a cue: the Show Tree's per list /
     *  per cue setting, else the show's (isEnabled()). */
    bool wantsMoveInBlack(const CueLookahead::UpcomingCue &cue) const;

private:
    /** Live pre-Grand-Master value at a fixture's absolute channel (0 if off-range). */
    uchar liveValue(const class QList<class Universe*> &unis, int uni, int absAddr) const;
    /** Drop every mark the planner owns, each put back (putBack()). */
    void clearPlanned();
    /** A pre-set let go of before its cue lit the fixture -- the cue was
     *  jumped past, or it is no longer moved in black into: its channels go
     *  back to what they were before the pre-set, so a later cue that sets
     *  only a level does not reveal a look that never played (UX-70). What
     *  plays on them now writes over that the next frame. */
    void putBack(quint32 fid);
    /** @p values without @p fxi's Shutter-group channels. */
    static QHash<quint32, uchar> withoutShutters(const class Fixture *fxi,
                                                 const QHash<quint32, uchar> &values);
    /** Re-run dangleFixtures() and emit dangleFixturesChanged() on change. */
    void updateDangleDetection();

    Doc        *m_doc;
    MarkEffect *m_mark;
    QTimer      m_timer;
    bool        m_enabled = false;
    int         m_darkGapMs = defaultDarkGapMs();   //!< lead time; tune on a rig
    QSet<quint32> m_planned;            //!< fixtures WE marked (vs manual marks)
    /** Of m_planned, those kept dark by a closed shutter (the dimmer is up,
     *  or there is none): their shutter channels are not held, so the
     *  shutter stays closed until the cue opens it. */
    QSet<quint32> m_shutterHeld;
    /** What each pre-set channel was before the planner held it (channel →
     *  value), for putBack(); and the cue each pre-set is for. */
    QHash<quint32, QHash<quint32, uchar>> m_before;
    QHash<quint32, quint32> m_plannedFor;
    QList<quint32> m_lastDangling;      //!< last emitted dangleFixturesChanged() value
    /** The upcoming cues' WLED looks, as they play (their settings come
     *  from the look, not the scene): kept from tick to tick, let go of
     *  when no upcoming cue has them. */
    mutable WledLookCache m_wledLooks;

    static const int DARK_LEVEL = 8;    //!< master intensity ≤ this = dark (or a closed shutter)
    static const int TICK_MS   = 300;   //!< coarse re-plan cadence
    static const int HORIZON_MS = 60000;
};

#endif // MARKPLANNER_H
