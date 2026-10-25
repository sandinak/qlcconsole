/*
  Q Light Controller Plus
  venuerig.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef VENUERIG_H
#define VENUERIG_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>

#include "qlcengine_export.h"
#include "universe.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QTimer;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What a venue owns (board Q137/Q138, VENUES.md): its house rig -- the
 * room's own fixtures, their place on the plot, and which of them fill the
 * show's groups -- the output patch of every slot in this room, and the
 * universes or address ranges the room keeps for itself ("not ours").
 *
 * The data is kept per venue (Data, in VenueCorrections::Venue) and written
 * in the venue sidecar. This class puts one venue's on the rig and takes it
 * off again, follows owners while a venue is applied (a fixture set to
 * House moves into the venue, one set back to Tour moves into the show), and
 * runs the clash check. VenueCorrections owns it and calls it as it applies
 * and un-applies a venue.
 */
class QLC_ENGINE_EXPORT VenueRig final : public QObject
{
    Q_OBJECT

public:
    explicit VenueRig(Doc *doc, QObject *parent = nullptr);
    ~VenueRig() override;

    /** One house fixture as the venue keeps it: the fixture exactly as the
     *  show file would write it, and what the clash check and the venue
     *  page read without loading it. */
    struct HouseFixture
    {
        quint32 id = UINT_MAX;
        QString name;
        quint32 universe = 0;       //!< zero-based
        quint32 address = 0;        //!< zero-based within the universe
        quint32 channels = 0;
        bool patched = true;
        QByteArray xml;             //!< the <Fixture> element
    };

    /** A house head in one of the show's groups, at its cell. */
    struct FillHead
    {
        quint32 fixture = UINT_MAX;
        int head = 0;
        int x = 0;
        int y = 0;
    };

    /** Which house heads fill one of the show's groups in this venue. */
    struct Fill
    {
        quint32 group = UINT_MAX;
        QString groupName;          //!< found again by name if the id is gone
        QList<FillHead> heads;
    };

    /** A universe (whole) or addresses in one that the room keeps. */
    struct NotOurs
    {
        quint32 universe = 0;       //!< zero-based
        int from = -1;              //!< zero-based first channel; -1 = the whole universe
        int to = -1;                //!< zero-based last channel, inclusive
        bool whole() const { return from < 0; }
        bool operator==(const NotOurs &o) const
        { return universe == o.universe && from == o.from && to == o.to; }
    };

    /** Everything one venue owns. Empty: the venue behaves as before. */
    struct Data
    {
        QList<HouseFixture> fixtures;
        /** The house fixtures' plot items and rig props, as a <Monitor>. */
        QByteArray monitorXml;
        QList<Fill> fills;
        /** universe -> its output patch in this room (every slot, once
         *  recorded); empty: the show's patch. */
        QMap<quint32, QList<Universe::OfflinePatch>> patch;
        QList<NotOurs> notOurs;
        /** Elements inside <House> this build doesn't know (a newer build's),
         *  kept whole and written back. */
        QList<QByteArray> houseUnknown;

        bool hasHouse() const { return !fixtures.isEmpty(); }
        bool isEmpty() const { return fixtures.isEmpty() && fills.isEmpty() && patch.isEmpty()
                                      && notOurs.isEmpty(); }
        QSet<quint32> fixtureIds() const;
    };

    /*********************************************************************
     * The sidecar
     *********************************************************************/
public:
    /** Write @p d inside the current <Venue> element; nothing for empty. */
    static void writeXml(QXmlStreamWriter &w, const Data &d);
    /** Read one of writeXml()'s elements (the reader is at its start:
     *  <House>, <Patch> or <NotOurs>) into @p d. False: not one of them
     *  (the reader is not moved). */
    static bool readXml(QXmlStreamReader &r, Data &d);

    /** The element the reader is at, whole, as bytes; the reader is left
     *  at its end. For keeping what a newer build wrote. */
    static QByteArray keepElement(QXmlStreamReader &r);
    /** keepElement()'s bytes, each written into @p w where it is. */
    static void writeKept(QXmlStreamWriter &w, const QList<QByteArray> &kept);

    /*********************************************************************
     * On and off the rig
     *********************************************************************/
public:
    /** Put @p d on the rig: the house fixtures (renumbered when a show
     *  fixture took an id since -- @p d is updated to match), their plot
     *  places, their heads in the groups; the venue's patch over the show's
     *  (which is held for the show file), "not ours" on the universes.
     *  Never marks the show modified. */
    void apply(Data &d);
    /** Take it off: the house fixtures out of the rig, the show's patch
     *  back, nothing not ours. Capture first to keep changes. */
    void unapply();
    /** The rig as it is now into @p d: the house fixtures, their plot data,
     *  the fills (a fill for a group not in the show is kept). True when
     *  anything differs from what @p d had. */
    bool capture(Data &d) const;
    /** The doc was cleared: forget, without touching the rig. */
    void forget();

    /** A venue is applied (its house rig is on, owners are followed). */
    bool isApplied() const { return m_applied; }

    /** @p id is a house fixture of the venue applied now. */
    bool isVenueFixture(quint32 id) const { return m_ids.contains(id); }
    QSet<quint32> venueFixtures() const { return m_ids; }

    /** The ids every venue's house rig uses: new show fixtures skip them. */
    void setReservedIds(const QSet<quint32> &ids) { m_reserved = ids; }
    bool isReservedId(quint32 id) const { return m_reserved.contains(id); }

    /** Fixtures in the show (not the venue's) that are House: with a
     *  venue applied, moveShowHouseIn() takes them into it. */
    QList<quint32> showHouseFixtures() const;
    /** Take them into the venue applied; how many moved. */
    int moveShowHouseIn();

    /*********************************************************************
     * The venue's patch
     *********************************************************************/
public:
    /** Every universe's output patch as it is now. */
    QMap<quint32, QList<Universe::OfflinePatch>> currentPatch() const;
    /** The show's own patch, as the show file has it (the held base where
     *  one is held). */
    QMap<quint32, QList<Universe::OfflinePatch>> showPatch() const;
    /** With @p d applied: is the patch on the rig different from what @p d
     *  says (or, with none in @p d, from the show's)? */
    bool patchChanged(const Data &d) const;
    /** Put the show's own patch back on every universe (a venue's patch
     *  forgotten), holding it again as the base. */
    void restoreShowPatch();
    static bool samePatch(const QList<Universe::OfflinePatch> &a, const QList<Universe::OfflinePatch> &b);

    /** "Not ours" of @p d onto the universes (none for empty). */
    void applyNotOurs(const QList<NotOurs> &notOurs);
    /** "U1 401–512", "U6" -- one line of the list, 1-based. */
    static QString notOursText(const NotOurs &n);
    /** "401-512" / "" -> a range; false when it does not read. */
    static bool parseRange(const QString &text, int &from, int &to);

    /*********************************************************************
     * The clash check (VENUES.md section 6)
     *********************************************************************/
public:
    struct Clash
    {
        enum Kind { Overlap, NotOursKind, Slot };
        Kind kind = Overlap;
        quint32 universe = 0;       //!< zero-based
        quint32 address = 0;        //!< zero-based, where it starts
        quint32 channels = 0;
        QString text;               //!< one line, for people
    };
    /** What going live with @p d would clash on. @p applied: @p d is the
     *  venue on the rig now (its house fixtures are the doc's); else a dry
     *  run of @p d's against the show's tour package. */
    QList<Clash> clashes(const Data &d, bool applied) const;
    static QString kindName(Clash::Kind kind);

signals:
    /** While a venue is applied the house rig changed (a fixture moved in
     *  or out, a fill, a house fixture's place): VenueCorrections captures
     *  and writes the sidecar. @p soon: a burst (a drag) -- wait a little. */
    void rigChanged(bool soon);
    /** The output patch changed while a venue is applied. */
    void patchEdited();

private:
    void sync();
    void watchUniverses();
    quint32 freeId(const QSet<quint32> &taken) const;
    void putFixture(HouseFixture &hf, QMap<quint32, quint32> &remap, const Data &d);
    /** Whether a house fixture at its address clashes with what the venue
        keeps (not ours) or with the show's slots (a Tour slot, when the show
        has House slots at all). Overlaps are Doc::addFixture()'s to refuse. */
    bool clashesOnPut(const Fixture *fx, const Data &d) const;
    void keepUnmodified(bool was);

private:
    Doc *m_doc;
    bool m_applied = false;
    bool m_writing = false;
    QSet<quint32> m_ids;
    /** House fixtures that could not be put in (a definition missing on
     *  this computer): kept in the venue as they were, never dropped. */
    QSet<quint32> m_unloaded;
    QSet<quint32> m_reserved;
    QSet<Universe *> m_watched;
};

/** @} */

#endif
