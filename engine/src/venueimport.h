/*
  Q Light Controller Plus
  venueimport.h

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

#ifndef VENUEIMPORT_H
#define VENUEIMPORT_H

#include <climits>
#include <QCoreApplication>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>

#include "qlcengine_export.h"
#include "venuerig.h"
#include "universe.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * New Venue from a Show (docs/design/VENUE_FROM_SHOW.md): another show
 * file's fixtures become a venue's house rig -- House, at their plot
 * places, on the one universe the show sets aside for the room -- and its
 * groups are matched to the show's by name. Reads the other file only;
 * builds the venue's VenueRig::Data, which VenueCorrections::
 * addVenueFromShow() keeps. The show is never changed.
 */
class QLC_ENGINE_EXPORT VenueImport final
{
    Q_DECLARE_TR_FUNCTIONS(VenueImport)

public:
    /** One of the other show's groups: its name and its heads (the other
     *  show's fixture ids), at their cells. */
    struct Group
    {
        QString name;
        QList<VenueRig::FillHead> heads;
    };

    /** What was read from the other show. */
    struct Source
    {
        QString file;                   //!< as given
        /** Its fixtures, each as that show wrote it (its own ids). */
        QList<VenueRig::HouseFixture> fixtures;
        QList<Group> groups;
        /** Its <Monitor>, whole (the plot and its structure). */
        QByteArray monitorXml;
        /** universe (zero-based) -> its outputs in that show. */
        QMap<quint32, QList<Universe::OfflinePatch>> outputs;

        /** The file's name ("PMS Stage.qlcc"). */
        QString fileName() const;
        /** The universes its fixtures are on, in order. */
        QList<quint32> universes() const;
        /** What it sent its fixtures' universes to. */
        QList<Universe::OfflinePatch> fixtureOutputs() const;
    };

    /** Read @p path. False, with @p why, when it is not a show that can be
     *  read or has no fixtures. */
    static bool read(const QString &path, Source &out, QString *why = nullptr);
    static bool readXml(const QByteArray &xml, Source &out, QString *why = nullptr);

    /*********************************************************************
     * Groups
     *********************************************************************/
public:
    enum MatchKind { NoMatch, SameName, SimilarName };
    /** A suggestion (or his choice) for one of the other show's groups. */
    struct Match
    {
        quint32 group = UINT_MAX;   //!< the show's group; UINT_MAX: not matched
        MatchKind kind = NoMatch;   //!< what suggested it
        bool confirmed = false;     //!< he ticked it (or chose it himself)
    };

    /** A name with case, spaces, punctuation and "&"/"and" taken out. */
    static QString simplified(const QString &name);
    /** The show's group for @p name: the same name first, then a similar one. */
    static Match suggest(const QString &name, const Doc *doc);
    /** One suggestion per group of @p src, in order. */
    static QList<Match> suggest(const Source &src, const Doc *doc);
    /** Rows with a group chosen and not confirmed. */
    static int unconfirmed(const QList<Match> &matches);

    /*********************************************************************
     * Building the venue
     *********************************************************************/
public:
    /** The universe to suggest for the room: the first House slot no tour
     *  fixture is on, else the first universe with no fixture at all, else
     *  UINT_MAX. @p venueFixtures (the house rig applied now) don't count. */
    static quint32 suggestUniverse(const Doc *doc, const QSet<quint32> &venueFixtures = QSet<quint32>());

    struct Options
    {
        quint32 universe = 0;       //!< zero-based: where the fixtures go
        /** One per Source::groups; only confirmed ones fill. */
        QList<Match> matches;
        /** The room's output for that universe (empty: none yet). */
        QList<Universe::OfflinePatch> output;
    };

    struct Result
    {
        VenueRig::Data data;
        int fixtures = 0;
        int filled = 0;             //!< groups matched (fills)
        int held = 0;               //!< groups held for a later match
        quint32 firstAddress = 0;   //!< zero-based
        quint32 lastAddress = 0;    //!< zero-based, inclusive
        QStringList warnings;
    };

    /** The venue's rig from @p src: new ids after every one in @p doc and
     *  in @p reserved; Owner House; universe @p opt.universe, each address as
     *  it was; at its plot place (free-placed where it hung on the other
     *  show's structure); the confirmed matches as fills, the rest held. */
    static Result build(const Source &src, const Doc *doc, const Options &opt,
                        const QSet<quint32> &reserved);

    /** "69 fixtures on U4 1–315". */
    static QString summary(const Result &r, quint32 universe);
};

/** @} */

#endif
