/*
  Q Light Controller Plus
  dmxmonitorwindow.h

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

#ifndef DMXMONITORWINDOW_H
#define DMXMONITORWINDOW_H

#include <QWidget>
#include <QList>

#include "monitorproperties.h"

class Doc;
class QComboBox;
class QScrollArea;
class MonitorFixture;
class MonitorLayout;

/** @addtogroup ui UI
 * @{
 */

/**
 * QLC+ 4's Monitor in its DMX mode, as a window of its own: a box for each
 * patched fixture with its channels and their live values, every universe
 * or one at a time. The Lighting Studio (the Monitor's 2D view) is a tab
 * here and has no channel grid; this is what View ▸ DMX Monitor and
 * Classic's toolbar open. It builds its boxes only while shown.
 */
class DmxMonitorWindow final : public QWidget
{
    Q_OBJECT

public:
    DmxMonitorWindow(Doc *doc, QWidget *parent = nullptr);
    ~DmxMonitorWindow();

    /** The fixtures shown now, in order (tests). */
    QList<quint32> shownFixtures() const;
    /** The universe shown (Universe::invalid(): all of them). */
    quint32 universe() const { return m_universe; }
    void setUniverse(quint32 universe);
    void setChannelStyle(MonitorProperties::ChannelStyle style);
    void setValueStyle(MonitorProperties::ValueStyle style);

protected:
    void showEvent(QShowEvent *ev) override;
    void hideEvent(QHideEvent *ev) override;

private slots:
    /** The show's fixtures changed: rebuilt (now if shown, else when next shown). */
    void slotFixturesChanged();

private:
    void fillUniverses();
    void rebuild();
    void clearFixtures();

private:
    Doc *m_doc;
    quint32 m_universe;
    MonitorProperties::ChannelStyle m_channelStyle = MonitorProperties::DMXChannels;
    MonitorProperties::ValueStyle m_valueStyle = MonitorProperties::DMXValues;
    QComboBox *m_universeCombo = nullptr;
    QComboBox *m_channelCombo = nullptr;
    QComboBox *m_valueCombo = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_canvas = nullptr;
    MonitorLayout *m_layout = nullptr;
    QList<MonitorFixture *> m_fixtures;
    bool m_stale = true;
};

/** @} */

#endif
