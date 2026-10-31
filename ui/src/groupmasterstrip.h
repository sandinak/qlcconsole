/*
  Q Light Controller Plus
  groupmasterstrip.h

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

#ifndef GROUPMASTERSTRIP_H
#define GROUPMASTERSTRIP_H

#include <QWidget>
#include <QHash>

#include "qlcui_export.h"

class QSlider;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The Run tab's group masters (0.4): a fader for each fixture group with a
 * master (FixtureGroup::hasMaster(), GroupMasters), its name under it and
 * its level over it. Moving one scales the group's intensity at once;
 * double-click a name to put it back to full. Hidden while the show has no
 * group masters, so a show without them looks as it did.
 */
class QLC_UI_EXPORT GroupMasterStrip final : public QWidget
{
    Q_OBJECT

public:
    explicit GroupMasterStrip(Doc *doc, QWidget *parent = nullptr);

    /** The fader of group @a groupId, or null (tests) */
    QSlider *fader(quint32 groupId) const { return m_faders.value(groupId); }
    int faderCount() const { return m_faders.size(); }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    /** The groups with a master changed: the faders again */
    void rebuild();
    void slotLevelChanged(quint32 groupId, uchar level);

private:
    void showLevel(quint32 groupId, uchar level);

    Doc *m_doc;
    QHash<quint32, QSlider *> m_faders;
    QHash<quint32, QLabel *> m_levels;
};

/** @} */

#endif
