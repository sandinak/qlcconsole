/*
  Q Light Controller Plus
  patchlist.h

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

#ifndef PATCHLIST_H
#define PATCHLIST_H

#include <QStringList>
#include <QVector3D>
#include <QColor>
#include <QString>
#include <QList>

#include "rigowner.h"

class Fixture;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The patch list: the paperwork that goes to the crew hanging and addressing
 * the rig. A row per fixture -- what it is, where it is patched, where that
 * universe goes, what it hangs on -- in universe and address order, the
 * fixtures still in the inventory last. Derived on demand from the patch,
 * the I/O map and the plot; nothing here is stored.
 */
class PatchList
{
public:
    struct Row
    {
        quint32 id = 0;
        QString name;
        QString manufacturer;
        QString model;
        QString mode;
        quint32 channels = 0;

        bool    patched = false;
        bool    clash = false;           //!< asked for channels another fixture holds
        quint32 universe = 0;            //!< zero-based; valid when patched
        QString universeName;
        quint32 address = 0;             //!< zero-based; valid when patched

        bool    network = false;         //!< its own endpoint (Fixture::Network)
        QString ip;                      //!< network fixtures
        QString host;                    //!< network fixtures: the name its IP goes by
                                         //!< (InputOutputMap::targetAlias()), if any
        QString transport;               //!< network fixtures: "Art-Net", "WLED"
        QString output;                  //!< where its universe goes, in words

        QString mountedOn;               //!< MonitorProperties::mountDescription()
        QString owner;                   //!< "House", "Rental"; empty for the tour's (docs/design/VENUES.md)
        RigOwner::Owner ownerKind = RigOwner::Tour;
        QString location;                //!< where it hangs ("Truss FOH"), else its group; empty: neither
        QString group;                   //!< the first fixture group (lowest id) that holds it
        QString type;                    //!< its definition's type: "Moving Head", "LED Bar (Pixels)"
        bool    pixel = false;           //!< a pixel / matrix fixture (isPixelFixture())
        bool    onPlot = false;
        QVector3D position;              //!< metres, when onPlot (fixtureRigPosition())
        QColor  gel;                     //!< its gel (the plot's fixture colour); invalid: none

        /** "#1E50FF", or empty when it has no gel. */
        QString gelText() const { return gel.isValid() ? gel.name().toUpper() : QString(); }

        /** "12" .. "23" as the crew reads it: one-based. */
        quint32 firstChannel() const { return address + 1; }
        quint32 lastChannel() const { return address + qMax(channels, quint32(1)); }
        /** "Chauvet Rogue R2", once when the model repeats the maker
         *  ("Generic"). */
        QString makeModel() const;
        /** "1: Stage left · 12–23", "not patched", "clash · 1 · 12–23". */
        QString patchText() const;
        /** The mode, with its channel count when the name doesn't already
         *  say it: "18 Channel", "165ch", "Open + Fly (2 ch)". */
        QString modeText() const;
    };

    /** How the list is grouped: where the fixtures hang (the default), by
     *  universe, by fixture type, or not at all. */
    enum GroupBy { ByLocation = 0, ByUniverse, ByType, ByNone };

    /** Which fixtures: all, the discrete ones, or the pixel / matrix ones. */
    enum Kind { AllKinds = 0, Discrete, Pixel };

    /** What the list, its exports and the labels include. */
    struct Filter
    {
        Kind kind = AllKinds;
        /** Only these owners; empty: anybody's. */
        QList<RigOwner::Owner> owners;

        bool accepts(const Row &row) const;
        bool isEmpty() const { return kind == AllKinds && owners.isEmpty(); }
    };

    /** A heading of the grouped list and its rows, in patch order. */
    struct Section
    {
        QString title;                   //!< empty when not grouped
        QList<Row> rows;
    };

    /** Every fixture in the show, or only @p ids, in patch order. */
    static PatchList build(Doc *doc, const QList<quint32> &ids = QList<quint32>());

    QList<Row> rows() const { return m_rows; }
    bool isEmpty() const { return m_rows.isEmpty(); }

    /** The rows @p filter accepts; the rest of the list as it is. */
    PatchList filtered(const Filter &filter) const;
    /** The rows grouped by @p by: the sections in order (by name, numbers
     *  in number order, the "Not placed" / inventory section last), each in
     *  patch order (universe, then address). One untitled section for
     *  ByNone; none when there are no rows. */
    QList<Section> sections(GroupBy by) const;
    /** The rows in the order sections(@p by) lists them. */
    QList<Row> arranged(GroupBy by) const;
    /** The heading @p row is listed under. */
    static QString sectionTitle(const Row &row, GroupBy by);
    /** "Location", "Universe", "Type"; empty for ByNone. */
    static QString groupByName(GroupBy by);

    /** The show patches fixtures in more than one universe: a label's
     *  address then says which ("D2.101"). Over the whole show, whatever
     *  build() was asked for, so labels agree however they are picked. */
    bool multiUniverse() const { return m_multiUniverse; }
    /** Some fixture is not the tour's. */
    bool hasOwners() const { return m_owners; }
    /** Some fixture has a gel: the list gets a Gel column. */
    bool hasGels() const { return m_gels; }

    /** A pixel / matrix fixture: an "LED Bar (Pixels)" definition, or one
     *  whose mode has two or more heads with colour (RGB) channels of their
     *  own -- what an RGB Matrix draws across. Everything else is discrete. */
    static bool isPixelFixture(const Fixture *fixture);
    /** Lengths in feet rather than metres (the plot's units). */
    bool feet() const { return m_feet; }

    /** The spreadsheet: a header line, then a line per fixture in the order
     *  arranged(@p by) gives, the section in a first column of its own when
     *  grouped. What the CSV and the XLSX hold. */
    QList<QStringList> table(GroupBy by = ByNone) const;
    /** table() as CSV. */
    QString toCsv(GroupBy by = ByNone) const;
    /** table() as an Excel workbook (.xlsx): one sheet, the header bold,
     *  numbers as numbers. */
    QByteArray toXlsx(GroupBy by = ByNone) const;
    /** Write toXlsx() to @p path. False, with @p error, if it can't. */
    bool writeXlsx(const QString &path, GroupBy by = ByNone, QString *error = nullptr) const;
    /** The printable list, a table per section (what the PDF is made from). */
    QString toHtml(const QString &title, GroupBy by = ByUniverse) const;
    /** Write the PDF to @p path (landscape Letter). False, with @p error, if
     *  the file can't be written. */
    bool writePdf(const QString &path, const QString &title, QString *error = nullptr,
                  GroupBy by = ByUniverse) const;

    /** "Art-Net → CS1 (172.18.2.211) port 1" for every output of
     *  @p universe, or empty when it goes nowhere. */
    static QString universeOutput(Doc *doc, quint32 universe);

private:
    QList<Row> m_rows;
    bool m_feet = false;
    /** Some fixture is not the tour's: the list says whose each one is. */
    bool m_owners = false;
    /** Some fixture has a gel colour: the list shows each one's. */
    bool m_gels = false;
    bool m_multiUniverse = false;
};

/** @} */

#endif
