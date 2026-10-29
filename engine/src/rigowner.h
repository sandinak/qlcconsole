/*
  Q Light Controller Plus
  rigowner.h

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

#ifndef RIGOWNER_H
#define RIGOWNER_H

#include <QString>
#include <QHash>
#include <QList>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Who a fixture belongs to (board Q137, docs/design/VENUES.md section 2): the tour
 * package (Tour, or Rental for the run) travels with the show; the House rig
 * is the room's and lives in that venue's file.
 *
 * A fixture group has an owner and its fixtures follow it; a fixture can
 * override its group. Unset on a fixture means "from its group", on a group
 * "not said" -- and a fixture nobody says anything about is Tour.
 */
namespace RigOwner
{
    enum Owner
    {
        Unset = 0,
        Tour,
        House,
        Rental
    };

    /** "Tour", "House", "Rental"; empty for Unset (the XML words). */
    QLC_ENGINE_EXPORT QString xmlName(Owner owner);
    /** The other way; anything else is Unset. */
    QLC_ENGINE_EXPORT Owner fromXmlName(const QString &text);
    /** The word on screen (translated); "Not set" for Unset. */
    QLC_ENGINE_EXPORT QString displayName(Owner owner);
    /** Tour, House, Rental -- what a fixture can be. */
    QLC_ENGINE_EXPORT QList<Owner> owners();

    /** What @p fid is: its own owner when set; else the owner of the first
     *  group (lowest id) that has one and holds it; else Tour. Never Unset.
     *  @p fromGroup, when given, is that group's id (or UINT_MAX when the
     *  fixture's own owner, or the default, decided). */
    QLC_ENGINE_EXPORT Owner effective(const Doc *doc, quint32 fid, quint32 *fromGroup = nullptr);
    /** effective() for every fixture in @p doc, in one pass over the groups. */
    QLC_ENGINE_EXPORT QHash<quint32, Owner> effectiveAll(const Doc *doc);
}

/** @} */

#endif
