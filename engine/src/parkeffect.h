/*
  Q Light Controller Plus
  parkeffect.h

  Fork "Park" support. Holds a set of fixture channels at fixed DMX values on
  every MasterTimer tick (forceLTP), overriding whatever cues/functions would
  otherwise drive them — the operator can quarantine a stuck fixture or set one
  aside while programming. Unlike HighlightEffect (always full white) the parked
  values are captured from the live output at park time, so a fixture holds
  exactly where it was. The parked set is owned here and round-trips to the
  workspace <Park> element so a quarantine survives a reload.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PARKEFFECT_H
#define PARKEFFECT_H

#include <QMutex>
#include <QObject>
#include <QHash>
#include <QList>

#include "dmxsource.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

#define KXMLQLCPark        QStringLiteral("Park")
#define KXMLQLCParkFixture QStringLiteral("PFixture")
#define KXMLQLCParkChannel QStringLiteral("PChannel")
#define KXMLQLCParkFixtureID QStringLiteral("ID")
#define KXMLQLCParkChannelNum QStringLiteral("Number")

class ParkEffect final : public QObject, public DMXSource
{
    Q_OBJECT

public:
    explicit ParkEffect(Doc *doc);
    ~ParkEffect() override;

    /** Park @p fixtureId, holding the given channel→value map (relative channel
     *  indices). Replaces any existing park for that fixture. */
    void parkFixture(quint32 fixtureId, const QHash<quint32, uchar> &values);

    /** Release the park on @p fixtureId. */
    void unparkFixture(quint32 fixtureId);

    /** Release every park. */
    void unparkAll();

    bool isParked(quint32 fixtureId) const;
    QList<quint32> parkedFixtures() const;
    bool isEmpty() const;

    /** DMXSource override — called every master timer tick. */
    void writeDMX(MasterTimer *timer, QList<Universe*> universes) override;

    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

private:
    /** Flattened, absolute-address writes for the DMX thread. */
    struct FixEntry {
        int universeId;
        QList<QPair<int, uchar>> writes; // (universe-local address, value)
    };

    /** Rebuild m_entries from m_parked and (un)register the DMX source.
     *  Call with m_mutex NOT held. */
    void rebuild();

    Doc *m_doc;

    /** Semantic park set: fixtureId → (relative channel index → value). */
    QHash<quint32, QHash<quint32, uchar>> m_parked;

    /** Guards m_entries (read on the DMX thread). */
    mutable QMutex m_mutex;
    QList<FixEntry> m_entries;
    bool m_registered = false;
};

#endif // PARKEFFECT_H
