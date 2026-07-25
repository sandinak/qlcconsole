/*
  Q Light Controller Plus
  studiotemplate.h

  Copyright (c) Massimo Callegari

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

#ifndef STUDIOTEMPLATE_H
#define STUDIOTEMPLATE_H

#include <QString>
#include <QList>

class Doc;

/** Fixture Studio — component template (FIXTURESTUDIO_DESIGN.md Phase 4).
 *
 *  QLC+ fixtures are unique instances, so a reusable "component" (a step, a
 *  pod, …) cannot reference specific fixtures. A template therefore captures the
 *  arrangement by ROLE/ORDER — each role holds a group-local offset (metres) —
 *  plus the anchor kind and seed pitch. Stamping binds the roles, in order, to a
 *  set of real fixtures and drops one placed studio group.
 *
 *  Stored as a small self-describing JSON file. Static helpers only; no state.
 */
namespace StudioTemplate
{
    /** Serialize studio group @p groupId (its members' group-local offsets,
     *  anchor kind and pitch) to @p filePath as a JSON template.
     *  @return true on success. */
    bool saveGroup(Doc *doc, quint32 groupId, const QString &filePath, QString *error = nullptr);

    /** Number of roles a template file declares (0 on failure). Lets a caller
     *  check a selection against the template before stamping. */
    int roleCount(const QString &filePath);

    /** Instantiate the template at @p filePath onto @p fixtureIds (bound to roles
     *  in the given order; extra fixtures/roles are ignored). Creates a new studio
     *  group centred on the members' current centroid and lays them out from the
     *  template's local offsets.
     *  @return the new studio group id, or 0 on failure. */
    quint32 stamp(Doc *doc, const QString &filePath, const QList<quint32> &fixtureIds,
                  QString *error = nullptr);
}

#endif // STUDIOTEMPLATE_H
