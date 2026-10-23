/*
  Q Light Controller Plus
  lastlookeffect.h

  Fork "last-look persistence" support. When a Show stops or reaches the end of
  its timeline in Operate mode, its final output would otherwise blackout —
  Universe::processFaders() zeroes HTP intensity every frame, so once the
  show's faders release, intensity falls to 0. This holder snapshots the
  show's final per-channel output and keeps it on the rig, so the last look
  SITS there instead of going black.

  The held values go out through an OutputLayer at Universe::Hold, the
  lowest fader priority, merged like any running function: a base under
  everything -- intensity HTP with whatever else runs, any function's write
  takes an LTP channel. A newly started cue also releases the held channels
  of the fixtures it drives (releaseFixtures()), so it takes them outright;
  the rest of the look keeps holding. Cleared on blackout/Design-mode/
  workspace clear. Runtime-only state, NOT persisted to the workspace.

*/

#ifndef LASTLOOKEFFECT_H
#define LASTLOOKEFFECT_H

#include <QMutex>
#include <QObject>
#include <QList>
#include <QPair>

#include "outputlayer.h"

class Doc;

class LastLookEffect final : public QObject
{
    Q_OBJECT

public:
    explicit LastLookEffect(Doc *doc);
    ~LastLookEffect() override;

    /** One held channel: owning fixture + universe id + universe-local address
     *  + value. The fixture tag lets a newly started cue yield only the
     *  channels it actually drives (per-channel/per-fixture yield). */
    struct ChannelHold {
        quint32 fixtureId;
        int     universeId;
        int     address;
        uchar   value;
    };

    /** Install a hold of the given channel values (replaces any current hold),
     *  on the output from the next frame. An empty list clears the hold. */
    void hold(const QList<ChannelHold> &channels);

    /** Add channels to the current hold (accumulate), keeping existing ones —
     *  for per-track hold-last, where several tracks contribute their looks as
     *  each ends. Any existing entries for the same fixtures are replaced. */
    void addHold(const QList<ChannelHold> &channels);

    /** Drop the held channels belonging to any of @p fixtureIds (the rest keep
     *  holding). Used when a new cue
     *  claims those fixtures so its output isn't fought by the hold. */
    void releaseFixtures(const QList<quint32> &fixtureIds);

    /** Release the whole hold (the rig returns to whatever else is driving
     *  those channels — typically nothing). */
    void clear();

    bool isActive() const;

private:
    /** Lay m_entries on the output. Call with m_mutex held, so two threads
     *  changing the hold can't leave the output on the older one. */
    void apply();

    Doc *m_doc;

    /** Guards m_entries and orders the writes to m_layer (hold/clear come
     *  from the UI thread and, mid-tick, from the MasterTimer thread). */
    mutable QMutex m_mutex;
    QList<ChannelHold> m_entries;

    /** The Hold fader on each universe */
    OutputLayer m_layer;
};

#endif // LASTLOOKEFFECT_H
