/*
  Q Light Controller Plus
  groupmasters.h

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

#ifndef GROUPMASTERS_H
#define GROUPMASTERS_H

#include <QObject>
#include <QList>
#include <QSet>

#include "qlcengine_export.h"

class FixtureGroup;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Group masters (0.4): a fixture group with a master (FixtureGroup::
 * setMaster()) scales the intensity of its heads by its level, the way the
 * Grand Master scales everything. This keeps each universe's per-channel
 * scales (Universe::setGroupMasters()) in step with the show; the scaling
 * itself is the universe's, in the post-fader stage just before the Grand
 * Master, so it costs a frame one byte test per written channel and
 * nothing new.
 *
 * What a master covers: every channel of each head in the group (the
 * universe scales only the intensity ones -- dimmers, RGB/CMY intensity),
 * and the channels of a fixture that belong to no head (a master dimmer)
 * when the group holds every head of it. A channel in two mastered groups
 * gets both, multiplied (decided, can change).
 */
class QLC_ENGINE_EXPORT GroupMasters final : public QObject
{
    Q_OBJECT

public:
    explicit GroupMasters(Doc *doc);

    /** The groups that have a master, in id order */
    QList<FixtureGroup *> groups() const;

    /** Put every master to full (255) */
    void resetLevels();

    /** Work the scales out again and hand them to the universes now
     *  (normally done for you: at once for a level, coalesced for an edit) */
    void rebuild();

signals:
    /** A group got or lost its master, or was renamed or removed */
    void changed();
    /** A master's level moved */
    void levelChanged(quint32 groupId, uchar level);

private slots:
    void slotGroupAdded(quint32 id);
    void slotMasterChanged(quint32 id);

private:
    void scheduleRebuild();

    Doc *m_doc;
    bool m_pending = false;
    /** The universes handed scales last time (to clear them when a master goes) */
    QSet<quint32> m_scaled;
};

/** @} */

#endif
