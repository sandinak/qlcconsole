/*
  Q Light Controller
  eosfixturemapper.h

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

#ifndef EOSFIXTUREMAPPER_H
#define EOSFIXTUREMAPPER_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>

class QLCFixtureDefCache;
class EosPersonality;

/** @addtogroup engine Engine
 * @{
 */

/** What an Eos personality resolves to. */
class EosFixtureChoice
{
public:
    enum Source
    {
        Saved,      //!< the per-user map file said so
        Exact,      //!< the library has this manufacturer + model, a mode of the right size
        Suggested,  //!< a close match; the wizard asks before trusting it
        Generic     //!< nothing matched: a generic dimmer of the same footprint
    };

    Source source = Generic;
    QString manufacturer;   //!< empty for Generic
    QString model;
    QString mode;
    int score = 0;          //!< Suggested: higher is closer

    bool isGeneric() const { return manufacturer.isEmpty(); }
    QString text() const;   //!< "ETC ColorSource PAR (5-channel)" / "Generic dimmer, 5 channels"
};

/**
 * Resolves an Eos personality ("ETC_Fixtures" / "ColorSource_PAR_RGB",
 * footprint 3) to a QLC fixture definition + mode:
 *
 *   1. the per-user map file (~/.qlcconsole/eos-fixture-map.json or the
 *      platform's user folder), keyed "Manufacturer|Model";
 *   2. an exact match in the definition cache: the same manufacturer and
 *      model (Eos writes '_' for ' ' and adds the mode to the model), with
 *      a mode whose channel count is the footprint;
 *   3. a close match, scored on shared words, the footprint and edit
 *      distance -- offered, never trusted silently by the wizard;
 *   4. a generic dimmer of the footprint, so the patch and its addresses
 *      still come across.
 *
 * Engine-side and pure: the wizard asks, this only proposes and remembers.
 */
class EosFixtureMapper
{
public:
    explicit EosFixtureMapper(const QLCFixtureDefCache *cache);

    /** The per-user map file's default path. */
    static QString defaultMapPath();

    /** Load / save the map file. A missing file is an empty map (true). */
    bool loadMap(const QString &path);
    bool saveMap(const QString &path) const;

    /** "ETC_Fixtures|ColorSource_PAR_RGB" */
    static QString key(const EosPersonality &p);
    static QString key(const QString &manufacturer, const QString &model);

    /** The best choice for @a p (see the class comment). */
    EosFixtureChoice resolve(const EosPersonality &p) const;
    /** Close matches, best first (at most @a max). */
    QList<EosFixtureChoice> candidates(const EosPersonality &p, int max = 8) const;

    /** Remember a choice (the wizard's); a Generic choice forgets. */
    void setChoice(const QString &key, const EosFixtureChoice &choice);
    bool hasSaved(const QString &key) const { return m_map.contains(key); }

    /** Words of a name, lower case, '_' and '-' as spaces ("ColorSource_SPOT_Jr_RGB"
     *  -> colorsource spot jr rgb). */
    static QStringList words(const QString &name);
    /** The Eos manufacturer as the library names it ("ETC_Fixtures" -> "ETC"). */
    static QString manufacturerName(const QString &eosManufacturer);
    static int editDistance(const QString &a, const QString &b);

private:
    const QLCFixtureDefCache *m_cache;
    QHash<QString, EosFixtureChoice> m_map;
};

/** @} */

#endif
