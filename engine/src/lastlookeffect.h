/*
  Q Light Controller Plus
  lastlookeffect.h

  Fork "last-look persistence" support. When a Show stops or reaches the end of
  its timeline in Operate mode, its final output would otherwise blackout —
  Universe::processFaders() zeroes HTP intensity every tick, so once the show's
  faders release, intensity falls to 0. This holder snapshots the show's final
  per-channel output and re-asserts it every MasterTimer tick (forceLTP, which
  survives the intensity zero-reset), so the last look SITS on the rig instead
  of going black.

  The hold is "whole-look replace": it is cleared the instant any new function
  starts (the next cue owns the rig), and on blackout/Design-mode/workspace
  clear. It is runtime-only state and is NOT persisted to the workspace.

  Modeled on ParkEffect (the same DMXSource-holds-forceLTP pattern), but the
  values come from the live output at stop time, not user selection.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef LASTLOOKEFFECT_H
#define LASTLOOKEFFECT_H

#include <QMutex>
#include <QObject>
#include <QList>
#include <QPair>

#include "dmxsource.h"

class Doc;

class LastLookEffect final : public QObject, public DMXSource
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

    /** Install a hold of the given channel values (replaces any current hold).
     *  Registers the DMX source so the values are asserted every tick. An empty
     *  list clears the hold. */
    void hold(const QList<ChannelHold> &channels);

    /** Add channels to the current hold (accumulate), keeping existing ones —
     *  for per-track hold-last, where several tracks contribute their looks as
     *  each ends. Any existing entries for the same fixtures are replaced. */
    void addHold(const QList<ChannelHold> &channels);

    /** Drop the held channels belonging to any of @p fixtureIds (the rest keep
     *  holding). Unregisters the source if nothing is left. Used when a new cue
     *  claims those fixtures so its output isn't fought by the hold. */
    void releaseFixtures(const QList<quint32> &fixtureIds);

    /** Release the whole hold (unregisters the DMX source; rig returns to
     *  whatever else is driving those channels — typically nothing). */
    void clear();

    bool isActive() const;

    /** DMXSource override — called every master timer tick. */
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

private:
    /** Reconcile registration to the current entries (serialized; safe vs the
     *  timer thread). Never call while holding m_mutex. */
    void syncRegistration();

    Doc *m_doc;

    /** Guards m_entries (read on the DMX thread, written on stop/clear). */
    mutable QMutex m_mutex;
    QList<ChannelHold> m_entries;
    /** Serializes registration reconciliation; guards m_registered. */
    QMutex m_regMutex;
    bool m_registered = false;
};

#endif // LASTLOOKEFFECT_H
