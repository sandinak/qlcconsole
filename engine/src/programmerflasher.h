/*
  Q Light Controller Plus
  programmerflasher.h

  Briefly stomps a fixture's intensity channel(s) so the user can
  visually identify which physical fixture they just toggled in the
  programmer's pad-grid sub-selection.

  Behavior: at flash start, samples the fixture's current intensity
  (post-running-functions). Picks an inverted target — 255 if the
  fixture is currently dim (<128), 0 if currently bright. Holds that
  target for `durationMs`, then stops asserting. The next master timer
  tick lets the natural source (running scene/programmer) take back
  over. Net effect: a brief, visible blip on the chosen fixture.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMERFLASHER_H
#define PROGRAMMERFLASHER_H

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QList>

#include "dmxsource.h"

class Doc;

class ProgrammerFlasher final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    explicit ProgrammerFlasher(Doc *doc);
    ~ProgrammerFlasher() override;

    /**
     * Schedule a flash on @p fixtureId. Intensity is inverted at
     * start (sampled from the universe so the flash is always visible
     * regardless of current state) and held for @p durationMs.
     * Multiple concurrent flashes on different fixtures are fine.
     */
    void flashFixture(quint32 fixtureId, int durationMs = 180);

    /** DMXSource override — called every master timer tick. */
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

private:
    struct Entry {
        quint32 fixtureId;
        int durationMs;
        QElapsedTimer elapsed;
        bool sampled = false;          // true once we've read current value
        QList<int> dmxAddresses;       // intensity channels (absolute DMX addresses)
        int universeId = -1;
        uchar target = 0;              // value to assert during flash
    };

    Doc *m_doc;
    QList<Entry> m_entries;
    QMutex m_mutex;
    bool m_registered = false;
};

#endif
