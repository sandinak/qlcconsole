/*
  Q Light Controller Plus
  highlighteffect.h

  Forces selected fixtures to full-bright white so the operator can
  visually identify them in the rig. Implemented as a DMXSource that
  writes on every MasterTimer tick with forceLTP, overriding all
  running functions for the duration.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef HIGHLIGHTEFFECT_H
#define HIGHLIGHTEFFECT_H

#include <QMutex>
#include <QObject>
#include <QList>

#include "dmxsource.h"

class Doc;

class HighlightEffect final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    explicit HighlightEffect(Doc *doc);
    ~HighlightEffect() override;

    /** Replace the set of highlighted fixtures. Thread-safe; may be called
     *  from the UI thread at any time. Passing an empty list stops writing. */
    void setFixtures(const QList<quint32>& fixtureIds);

    /** DMXSource override — called every master timer tick. */
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

private:
    struct FixEntry {
        int universeId;
        QList<QPair<int, uchar>> writes; // (universe-local address, value)
    };

    QList<FixEntry> buildEntries(const QList<quint32>& fixtureIds) const;

    Doc *m_doc;
    QList<FixEntry> m_entries;
    QMutex m_mutex;
    bool m_registered = false;
};

#endif // HIGHLIGHTEFFECT_H
