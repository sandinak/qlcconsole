/*
  Q Light Controller Plus
  dmxmonitorwindow.cpp

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

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

#include "dmxmonitorwindow.h"
#include "inputoutputmap.h"
#include "monitorfixture.h"
#include "monitorlayout.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

#define SETTINGS_GEOMETRY QStringLiteral("dmxmonitor/geometry")

DmxMonitorWindow::DmxMonitorWindow(Doc *doc, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_doc(doc)
    , m_universe(Universe::invalid())
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("dmxMonitorWindow"));
    setWindowTitle(tr("DMX Monitor"));
    setWindowIcon(QIcon(":/monitor.png"));
    setProperty("helpTopic", QStringLiteral("getting-started.md#the-dmx-monitor"));

    QVBoxLayout *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    QWidget *bar = new QWidget(this);
    QHBoxLayout *row = new QHBoxLayout(bar);
    row->setContentsMargins(6, 4, 6, 4);
    row->addWidget(new QLabel(tr("Universe:"), bar));
    m_universeCombo = new QComboBox(bar);
    m_universeCombo->setObjectName(QStringLiteral("dmxMonitorUniverse"));
    row->addWidget(m_universeCombo);
    row->addSpacing(12);
    row->addWidget(new QLabel(tr("Channels:"), bar));
    m_channelCombo = new QComboBox(bar);
    m_channelCombo->setObjectName(QStringLiteral("dmxMonitorChannels"));
    m_channelCombo->addItem(tr("DMX addresses"), MonitorProperties::DMXChannels);
    m_channelCombo->addItem(tr("Fixture channels"), MonitorProperties::RelativeChannels);
    row->addWidget(m_channelCombo);
    row->addSpacing(12);
    row->addWidget(new QLabel(tr("Values:"), bar));
    m_valueCombo = new QComboBox(bar);
    m_valueCombo->setObjectName(QStringLiteral("dmxMonitorValues"));
    m_valueCombo->addItem(tr("DMX (0-255)"), MonitorProperties::DMXValues);
    m_valueCombo->addItem(tr("Percent"), MonitorProperties::PercentageValues);
    row->addWidget(m_valueCombo);
    row->addStretch(1);
    column->addWidget(bar);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_canvas = new QWidget(m_scroll);
    m_canvas->setBackgroundRole(QPalette::Dark);
    m_layout = new MonitorLayout(m_canvas);
    m_layout->setSpacing(1);
    m_layout->setContentsMargins(1, 1, 1, 1);
    m_scroll->setWidget(m_canvas);
    column->addWidget(m_scroll, 1);

    connect(m_universeCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setUniverse(m_universeCombo->itemData(index).toUInt());
    });
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setChannelStyle(MonitorProperties::ChannelStyle(m_channelCombo->itemData(index).toInt()));
    });
    connect(m_valueCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setValueStyle(MonitorProperties::ValueStyle(m_valueCombo->itemData(index).toInt()));
    });

    connect(m_doc, &Doc::fixtureAdded, this, &DmxMonitorWindow::slotFixturesChanged);
    connect(m_doc, &Doc::fixtureRemoved, this, &DmxMonitorWindow::slotFixturesChanged);
    connect(m_doc, &Doc::fixtureChanged, this, &DmxMonitorWindow::slotFixturesChanged);
    connect(m_doc, &Doc::loaded, this, &DmxMonitorWindow::slotFixturesChanged);
    connect(m_doc, &Doc::cleared, this, &DmxMonitorWindow::slotFixturesChanged);

    QSettings settings;
    const QVariant geometry = settings.value(SETTINGS_GEOMETRY);
    if (geometry.isValid())
        restoreGeometry(geometry.toByteArray());
    else
        resize(800, 500);
}

DmxMonitorWindow::~DmxMonitorWindow()
{
    QSettings().setValue(SETTINGS_GEOMETRY, saveGeometry());
    clearFixtures();
}

QList<quint32> DmxMonitorWindow::shownFixtures() const
{
    QList<quint32> ids;
    for (const MonitorFixture *mof : m_fixtures)
        ids << mof->fixture();
    return ids;
}

void DmxMonitorWindow::setUniverse(quint32 universe)
{
    m_universe = universe;
    const int idx = m_universeCombo->findData(universe);
    if (idx >= 0)
        m_universeCombo->setCurrentIndex(idx);
    m_stale = true;
    if (isVisible())
        rebuild();
}

void DmxMonitorWindow::setChannelStyle(MonitorProperties::ChannelStyle style)
{
    m_channelStyle = style;
    m_channelCombo->setCurrentIndex(qMax(0, m_channelCombo->findData(int(style))));
    for (MonitorFixture *mof : m_fixtures)
        mof->slotChannelStyleChanged(style);
}

void DmxMonitorWindow::setValueStyle(MonitorProperties::ValueStyle style)
{
    m_valueStyle = style;
    m_valueCombo->setCurrentIndex(qMax(0, m_valueCombo->findData(int(style))));
    for (MonitorFixture *mof : m_fixtures)
        mof->slotValueStyleChanged(style);
}

void DmxMonitorWindow::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    fillUniverses();
    if (m_stale)
        rebuild();
}

void DmxMonitorWindow::hideEvent(QHideEvent *ev)
{
    QWidget::hideEvent(ev);
    // Each box listens to its fixture's values: none while nobody looks.
    clearFixtures();
    m_stale = true;
}

void DmxMonitorWindow::slotFixturesChanged()
{
    m_stale = true;
    if (isVisible())
    {
        fillUniverses();
        rebuild();
    }
}

void DmxMonitorWindow::fillUniverses()
{
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    m_universeCombo->blockSignals(true);
    m_universeCombo->clear();
    m_universeCombo->addItem(tr("All universes"), Universe::invalid());
    for (quint32 i = 0; i < ioMap->universesCount(); i++)
    {
        const quint32 id = ioMap->getUniverseID(i);
        m_universeCombo->addItem(ioMap->getUniverseNameByIndex(int(i)), id);
    }
    int idx = m_universeCombo->findData(m_universe);
    if (idx < 0)
    {
        m_universe = Universe::invalid();
        idx = 0;
    }
    m_universeCombo->setCurrentIndex(idx);
    m_universeCombo->blockSignals(false);
}

void DmxMonitorWindow::clearFixtures()
{
    while (!m_fixtures.isEmpty())
        delete m_fixtures.takeFirst();
}

void DmxMonitorWindow::rebuild()
{
    clearFixtures();
    m_stale = false;

    /* The values reach a fixture only from a monitored universe. The
       Lighting Studio watches them all; so does this, for the ones shown. */
    InputOutputMap *ioMap = m_doc->inputOutputMap();
    for (quint32 i = 0; i < ioMap->universesCount(); i++)
    {
        const quint32 id = ioMap->getUniverseID(i);
        if (m_universe == Universe::invalid() || id == m_universe)
            ioMap->setUniverseMonitor(int(i), true);
    }

    foreach (Fixture *fxi, m_doc->fixtures())
    {
        // A staged fixture has no address to show.
        if (fxi == nullptr || !fxi->isPatched())
            continue;
        if (m_universe != Universe::invalid() && fxi->universe() != m_universe)
            continue;
        MonitorFixture *mof = new MonitorFixture(m_canvas, m_doc);
        mof->setFixture(fxi->id());
        mof->slotChannelStyleChanged(m_channelStyle);
        mof->slotValueStyleChanged(m_valueStyle);
        mof->show();
        m_layout->addItem(new MonitorLayoutItem(mof));
        m_fixtures.append(mof);
    }
    m_layout->sort();
    m_canvas->updateGeometry();
}
