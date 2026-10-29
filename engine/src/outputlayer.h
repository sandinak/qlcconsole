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
     *  anything the layer held before and is not in @p levels is let go. */
    void set(const QList<Level> &levels);

    /** Let go of everything */
    void clear();

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

    /** Guarded: a layer held by one of the Doc's children (a FocusSession)
     *  is destroyed, and lets go, after ~Doc has run. A plain pointer made
     *  that a call on a destroyed Doc (UBSan at quit). */
    QPointer<Doc> m_doc;
    const Universe::FaderPriority m_priority;
    const int m_flags;
    const QString m_name;

    /** Serialises set()/clear(); guards m_slots */
    mutable QMutex m_mutex;
    /** Universe index -> the fader this layer writes there */
    QHash<int, Slot> m_slots;
};

#endif // OUTPUTLAYER_H
