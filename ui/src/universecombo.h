/*
  Q Light Controller Plus
  universecombo.h

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

#ifndef UNIVERSECOMBO_H
#define UNIVERSECOMBO_H

#include <QComboBox>

class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Pick a universe by its name, not its number ("select which U by a drop
 * down vs an id .. sorted by location (which should be by name)").
 *
 * The show's universes in name order -- numbers in names compare as numbers,
 * so "U3-SR" comes before "U12-SL" -- then the new universes a patch can open
 * after them. The item data is the universe index (0-based).
 */
class UniverseCombo final : public QComboBox
{
    Q_OBJECT

public:
    /** @p newOnes: how many not-yet-existing universes to offer after the
     *  show's own ("New universe N"). */
    explicit UniverseCombo(Doc *doc, int newOnes = 1, QWidget *parent = nullptr);

    /** The chosen universe (0-based). */
    quint32 universe() const;

    /** Choose @p universe; one beyond what is offered is added as new. */
    void setUniverse(quint32 universe);

signals:
    void universeChanged(quint32 universe);

private:
    void addNew(quint32 universe);

    quint32 m_existing;
};

/** @} */

#endif
