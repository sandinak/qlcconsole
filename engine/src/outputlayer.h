/*
  Q Light Controller Plus
  outputlayer.h

  Licensed under the Apache License, Version 2.0 (the "License");

  A fixed set of channel levels laid on the output through one GenericFader
  per universe, at a chosen fader priority: how Park, Highlight and the held
  last look reach the rig. They used to write the universes directly from
  the MasterTimer tick, before the universe thread's processFaders(), which
  zeroes every intensity channel and lays the running faders on top: the
  dimmers they held never got out and a running cue beat them (bug B1). A
  fader is written inside processFaders(), in priority order, so where it
  sits in Universe::FaderPriority is the whole precedence (see the comment
  on that enum).

  Thread-safe: set()/clear() may be called from any thread (the UI, or the
  MasterTimer thread mid-tick -- a show captures its last look in postRun()).
  They never claim the universes (the tick holds that lock) and take only
  the universe's fader-list lock and the fader's channel lock, which
  processFaders() takes in that order too.
*/

#ifndef OUTPUTLAYER_H
#define OUTPUTLAYER_H

#include <QSharedPointer>
#include <functional>
#include <QMutex>
#include <QString>
#include <QHash>
#include <QList>
#include <QPointer>

#include "universe.h"

class GenericFader;
class Doc;

class OutputLayer
{
public:
    /** One channel's level: a fixture and its channel, or no fixture
     *  (Fixture::invalidId()) and an absolute address
     *  (universe * UNIVERSE_SIZE + address). */
    struct Level
    {
        quint32 fixtureId;
        quint32 channel;
        uchar value;
    };

    /** @p flags are FadeChannel flags added to every channel (Override,
     *  PastGM); none merges the layer like any running function. */
    OutputLayer(Doc *doc, Universe::FaderPriority priority, int flags, const QString &name);
    ~OutputLayer();

    /** Lay exactly @p levels on the output, from the next frame on;
     *  anything the layer held before and is not in @p levels is let go.
     *
     *  Show only: a layer that is the show (Park, Mark, a held last look)
     *  stamps each level with the Operate session it was set in
     *  (GenericFader::liveEpoch()): one whose value has not changed keeps
     *  its stamp, so parking another fixture in Operate does not make a
     *  Park left from Design the show. @p renewed picks the levels set
     *  anew now even at the same value (the fixture just parked again, a
     *  look just captured); none: only the changed ones. With @p renewed,
     *  a new level it does not pick (a Park loaded with the show, a parked
     *  fixture patched later) is stamped by no session. A level stamped
     *  before this session goes out on a fader that holds Show-only
     *  channels. */
    void set(const QList<Level> &levels,
             const std::function<bool(const Level &)> &renewed = std::function<bool(const Level &)>());

    /** Let go of everything */
    void clear();
    /** Let go over @p ms: each level travels to what is under it
     *  (GenericFader::glideOut()); 0 is clear(). */
    void glideOut(uint ms);
    /** Drop what glideOut() left travelling, at once (its faders deleted at
     *  the next frame): a GO after a stop takes the channels from where the
     *  stop froze them (board 053p review). */
    void dropGlides();
    /** How many faders glideOut() left still travelling (for tests) */
    int glidingCount() const;

    /** Show only (Fixture::ShowOnly): this layer is not the show -- focus,
     *  Refine, Highlight -- and never drives a Show-only fixture
     *  (GenericFader::setHoldsShowOnly()). Park, a held last look and Mark
     *  are the show's or the operator's own hand, and drive it. */
    void setHoldsShowOnly(bool hold);
    bool holdsShowOnly() const { return m_holdsShowOnly; }

    /** The channels the layer writes, per universe index (for tests) */
    QHash<int, int> channelCounts() const;

    /** The Doc the layer writes through; null once it has gone (for tests) */
    Doc *doc() const;

private:
    struct Slot
    {
        Universe *universe = nullptr;
        QSharedPointer<GenericFader> fader;
    };
    /** m_slots' key bit: the universe's fader for levels set before this
     *  Operate session (it holds Show-only channels) */
    static constexpr int kStaleSlot = 0x10000;
    /** Each level's value and the session it was set in */
    struct Stamp
    {
        uchar value = 0;
        quint32 epoch = 0;
    };
    QHash<quint64, Stamp> m_stamps;

    /** Guarded: a layer held by one of the Doc's children (a FocusSession)
     *  is destroyed, and lets go, after ~Doc has run. A plain pointer made
     *  that a call on a destroyed Doc (UBSan at quit). */
    QPointer<Doc> m_doc;
    const Universe::FaderPriority m_priority;
    const int m_flags;
    const QString m_name;
    bool m_holdsShowOnly = false;

    /** Serialises set()/clear(); guards m_slots */
    mutable QMutex m_mutex;
    /** Universe index -> the fader this layer writes there */
    QHash<int, Slot> m_slots;
    /** What glideOut() let go of, still travelling (guarded by m_mutex) */
    QList<QWeakPointer<GenericFader> > m_gliding;
};

#endif // OUTPUTLAYER_H
