/*
  Q Light Controller Plus
  parkeffect.h

  Fork "Park" support. Holds a set of fixture channels at fixed DMX values,
  overriding whatever cues/functions would otherwise drive them — the operator
  can quarantine a stuck fixture or set one aside while programming. Unlike
  HighlightEffect (always full white) the parked values are captured from the
  live output at park time, so a fixture holds exactly where it was. The
  parked set is owned here and round-trips to the workspace <Park> element so
  a quarantine survives a reload.

  The values go out through an OutputLayer at Universe::Park, the top fader
  priority, with Override + PastGM: a parked channel sends its value over
  every running function and Highlight, and past the Grand Master. Blackout
  still takes it out (see Universe::FaderPriority for the whole model).

*/

#ifndef PARKEFFECT_H
#define PARKEFFECT_H

#include <QObject>
#include <QHash>
#include <QList>

#include "outputlayer.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class Doc;

#define KXMLQLCPark        QStringLiteral("Park")
#define KXMLQLCParkFixture QStringLiteral("PFixture")
#define KXMLQLCParkChannel QStringLiteral("PChannel")
#define KXMLQLCParkFixtureID QStringLiteral("ID")
#define KXMLQLCParkChannelNum QStringLiteral("Number")

class ParkEffect final : public QObject
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

    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

private:
    /** What a parked fixture writes, where (patched fixtures only) */
    struct FixEntry {
        int universeId;
        QList<QPair<int, uchar>> writes; // (universe-local address, value)
    };

    /** Rebuild m_entries from m_parked and lay them on the output. Call
     *  again when a parked fixture is patched elsewhere or unpatched. */
    void rebuild();

    Doc *m_doc;

    /** Semantic park set: fixtureId → (relative channel index → value). */
    QHash<quint32, QHash<quint32, uchar>> m_parked;

    /** The patched part of m_parked, as last laid on the output */
    QList<FixEntry> m_entries;

    /** The Park fader on each universe */
    OutputLayer m_layer;
};

#endif // PARKEFFECT_H
