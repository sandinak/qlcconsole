/*
  Q Light Controller Plus
  markeffect.h

  Fork "Mark" / move-in-black support. Holds a set of a fixture's NON-intensity
  channels (pan/tilt, colour, gobo, beam…) at fixed DMX values on every
  MasterTimer tick (forceLTP), so a dark mover can be pre-positioned to an
  upcoming cue and reveal with no visible sweep. It deliberately does NOT hold
  the master intensity, so the fixture stays as dark as the rest of the show
  leaves it. When a real cue lights the fixture (its master intensity rises past
  a small threshold) the mark for that fixture AUTO-RELEASES, handing the fixture
  over seamlessly. Marks round-trip to the workspace <Mark> element.

  Mirrors ParkEffect (a captured-value DMXSource); the two are independent.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef MARKEFFECT_H
#define MARKEFFECT_H

#include <QMutex>
#include <QObject>
#include <QHash>
#include <QSet>
#include <QList>

#include "dmxsource.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

#define KXMLQLCMark        QStringLiteral("Mark")
#define KXMLQLCMarkFixture QStringLiteral("MFixture")
#define KXMLQLCMarkChannel QStringLiteral("MChannel")
#define KXMLQLCMarkFixtureID QStringLiteral("ID")
#define KXMLQLCMarkChannelNum QStringLiteral("Number")

class MarkEffect final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    explicit MarkEffect(Doc *doc);
    ~MarkEffect() override;

    /** Mark @p fixtureId, holding the given channel→value map (relative channel
     *  indices, intensity excluded by the caller). Replaces any existing mark. */
    void markFixture(quint32 fixtureId, const QHash<quint32, uchar> &values);

    /** Release the mark on @p fixtureId. */
    void unmarkFixture(quint32 fixtureId);

    /** Release every mark. */
    void unmarkAll();

    bool isMarked(quint32 fixtureId) const;
    QList<quint32> markedFixtures() const;
    bool isEmpty() const;

    /** DMXSource override — called every master timer tick. */
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

signals:
    /** A marked fixture was lit by a real cue → it should be unmarked (queued to
     *  the GUI thread; writeDMX runs on the MasterTimer thread). */
    void fixtureRevealed(quint32 fixtureId);

private slots:
    /** GUI thread: auto-release a fixture the cue just revealed. */
    void slotFixtureRevealed(quint32 fixtureId);

private:
    /** Master-intensity DMX value above which a fixture counts as "lit" by a real
     *  cue → its mark hands off. Small, to ignore idle/no-drive (0). */
    static const int HANDOFF_INTENSITY = 8;

    /** Flattened, absolute-address writes for the DMX thread. */
    struct FixEntry {
        quint32 fixtureId;
        int universeId;
        int intensityAddr;               // absolute addr of master intensity, or -1
        QList<QPair<int, uchar>> writes; // (universe-local address, value)
    };

    /** Rebuild m_entries from m_marked and (un)register the DMX source.
     *  Call with m_mutex NOT held. */
    void rebuild();

    Doc *m_doc;

    /** Semantic mark set: fixtureId → (relative channel index → value). */
    QHash<quint32, QHash<quint32, uchar>> m_marked;

    /** Guards m_entries + m_revealed (read/written on the DMX thread). */
    mutable QMutex m_mutex;
    QList<FixEntry> m_entries;
    QSet<quint32> m_revealed;   //!< fixtures we've already emitted a reveal for
    bool m_registered = false;
};

#endif // MARKEFFECT_H
