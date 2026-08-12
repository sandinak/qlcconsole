/*
  Q Light Controller Plus
  markplanner.h

  Automatic move-in-black. When enabled, a coarse timer peeks the next cue
  (CueLookahead) and, if it's far enough away (dark-gap gate), pre-positions every
  fixture that is DARK now but LIT in that cue and whose aim/colour would change —
  by holding the cue's non-intensity values through the MarkEffect. The mark's own
  auto-handoff releases each fixture as the cue reveals it.

  The planner only ever touches marks it created itself (m_planned), so manual
  Mark/Unmark coexists untouched.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef MARKPLANNER_H
#define MARKPLANNER_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QString>

class Doc;
class MarkEffect;

#define KXMLQLCMoveInBlack QStringLiteral("MoveInBlack")

class MarkPlanner : public QObject
{
    Q_OBJECT

public:
    MarkPlanner(Doc *doc, MarkEffect *mark, QObject *parent = nullptr);

    /** Turn automatic move-in-black on/off (starts/stops the look-ahead timer;
     *  clears the planner's own marks when turned off). */
    void setEnabled(bool enable);
    bool isEnabled() const { return m_enabled; }

    /** Minimum lead time (ms) before the next cue for a fixture to be pre-set —
     *  below this there's no time to move in black, so it's skipped. */
    void setDarkGapMs(int ms) { m_darkGapMs = ms; }
    int  darkGapMs() const { return m_darkGapMs; }

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

private slots:
    /** Re-evaluate the plan against the current/next cue. */
    void evaluate();

private:
    /** Live pre-Grand-Master value at a fixture's absolute channel (0 if off-range). */
    uchar liveValue(const class QList<class Universe*> &unis, int uni, int absAddr) const;
    /** Drop every mark the planner owns. */
    void clearPlanned();
    /** Re-run dangleFixtures() and emit dangleFixturesChanged() on change. */
    void updateDangleDetection();

    Doc        *m_doc;
    MarkEffect *m_mark;
    QTimer      m_timer;
    bool        m_enabled = false;
    int         m_darkGapMs = 800;      //!< default lead time; tune on a rig
    QSet<quint32> m_planned;            //!< fixtures WE marked (vs manual marks)
    QList<quint32> m_lastDangling;      //!< last emitted dangleFixturesChanged() value

    static const int DARK_LEVEL = 8;    //!< master intensity ≤ this = dark
    static const int TICK_MS   = 300;   //!< coarse re-plan cadence
    static const int HORIZON_MS = 60000;
};

#endif // MARKPLANNER_H
