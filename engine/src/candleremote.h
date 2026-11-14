/*
  Q Light Controller Plus
  candleremote.h

  Hand-held flameless candles lit by an infrared remote that the show
  drives (docs/design/BLOCKING.md, "Candles"). Every candle in the room
  hears the same remote, so one two-channel fixture -- Generic > IR Candle
  Remote -- stands for all of them: SELECT picks a button (1 On, 2 Off,
  3 Candle, 4 Light, 5 Dim, 6 Brighten; 0 or anything else nothing) and
  WINDOW says how long the blaster keeps resending it.

  What a candle is drawn as comes from what the show last told the remote,
  read where the plot already reads fixture output (Fixture::valuesChanged
  on the UI thread), never on the DMX output path. It is what was SENT: a
  candle in a pocket, out of the beam, did not hear it.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef CANDLEREMOTE_H
#define CANDLEREMOTE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <climits>

#include "qlcengine_export.h"

class StageTarget;
class Fixture;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

class QLC_ENGINE_EXPORT CandleRemote final : public QObject
{
    Q_OBJECT

public:
    /** The buttons, as the SELECT channel's values (DMX-CHART.md). */
    enum Command { Nothing = 0, On = 1, Off = 2, Candle = 3, Light = 4, Dim = 5, Brighten = 6 };

    /** What the candles were last told. Not known until a command is seen
     *  this session: drawn unlit. */
    struct State
    {
        bool known = false;
        bool lit = false;
        bool flicker = false;    ///< Candle mode; Light = steady
        bool dimmed = false;     ///< one step down: the true level is unknowable
        bool operator==(const State &o) const
        { return known == o.known && lit == o.lit && flicker == o.flicker && dimmed == o.dimmed; }
        bool operator!=(const State &o) const { return !(*this == o); }
    };

    /** SELECT's value as a button: 1..6, anything else Nothing. */
    static Command command(int select);
    /** @p s after button @p c. On: lit at full ("it starts full on every
     *  start"); Off: dark; Candle / Light: lit, flickering / steady (a
     *  guess: the mode buttons wake a dark candle); Dim: one step down;
     *  Brighten: back to full. Dim and Brighten never light a dark one. */
    static State apply(State s, Command c);

    /** The bundled definition's manufacturer and model. */
    static QString manufacturer();
    static QString model();
    /** Whether @p fx is a candle remote (that definition). */
    static bool isRemote(const Fixture *fx);

    /** The one for @p doc (made on first use, a child of it). */
    static CandleRemote *of(Doc *doc);
    ~CandleRemote() override;

    /** The candle remotes patched in the show, by id. */
    QList<quint32> remotes() const;
    /** The remote that lights candle @p candle: its litBy() if that is a
     *  remote, else the only remote in the show; UINT_MAX when none. */
    quint32 remoteFor(const StageTarget *candle) const;
    /** What remote @p fid last sent. */
    State state(quint32 fid) const;
    /** What candle @p candle shows (unknown when it has no remote). */
    State stateFor(const StageTarget *candle) const;

    /** Remote @p fid's SELECT and WINDOW as the show now sets them: a change
     *  of either re-sends the button (the blaster restarts its window), so
     *  that is when it counts. What valuesChanged() feeds; tests call it. */
    void feed(quint32 fid, int select, int window);

    /** Forget everything (a new show): every candle unknown. */
    void reset();

signals:
    /** A candle's drawn state changed: views redraw them. */
    void changed();

private:
    explicit CandleRemote(Doc *doc);
    void watch(quint32 fid);
    void readValues(quint32 fid);

    struct Seen
    {
        int select = -1;
        int window = -1;
        State state;
    };
    Doc *m_doc;
    QHash<quint32, Seen> m_seen;
    QHash<quint32, QMetaObject::Connection> m_watching;
};

/** @} */

#endif // CANDLEREMOTE_H
