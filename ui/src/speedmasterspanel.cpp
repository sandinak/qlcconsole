/*
  Q Light Controller Plus
  speedmasterspanel.cpp

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

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QSlider>
#include <QLabel>
#include <QTimer>

#include <cmath>

#include "speedmasterspanel.h"
#include "speedmasters.h"
#include "doc.h"

static_assert(SpeedMasters::Count == 4, "SpeedMastersPanel lays out four rows");

QList<double> SpeedMastersPanel::ratios()
{
    return { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 };
}

QList<int> SpeedMastersPanel::choiceValues()
{
    QList<int> v = { int(SpeedMasters::Inherit), int(SpeedMasters::Free), int(SpeedMasters::MainClock) };
    for (int i = 0; i < SpeedMasters::Count; ++i)
        v << SpeedMasters::Master1 + i;
    return v;
}

QStringList SpeedMastersPanel::choiceNames(Doc *doc, const QString &inheritText)
{
    QStringList names;
    foreach (int v, choiceValues())
        names << (v == SpeedMasters::Inherit ? inheritText : doc->speedMasters()->assignmentName(v));
    return names;
}

void SpeedMastersPanel::fillCombo(QComboBox *cb, Doc *doc, const QString &inheritText, int current)
{
    const QList<int> values = choiceValues();
    const QStringList names = choiceNames(doc, inheritText);
    for (int i = 0; i < values.size(); ++i)
        cb->addItem(names.at(i), values.at(i));
    cb->setCurrentIndex(qMax(0, cb->findData(current)));
}

static QString ratioText(double r)
{
    if (qAbs(r - 0.25) < 1e-6) return QStringLiteral("×¼");
    if (qAbs(r - 1.0 / 3.0) < 1e-6) return QStringLiteral("×⅓");
    if (qAbs(r - 0.5) < 1e-6) return QStringLiteral("×½");
    if (qAbs(r - 0.75) < 1e-6) return QStringLiteral("×¾");
    if (qAbs(r - 1.5) < 1e-6) return QStringLiteral("×1½");
    return QStringLiteral("×%1").arg(r);
}

SpeedMastersPanel::SpeedMastersPanel(Doc *doc, QWidget *parent)
    : QWidget(parent, Qt::Tool)
    , m_doc(doc)
{
    setWindowTitle(tr("Speed Masters"));
    setAttribute(Qt::WA_DeleteOnClose);
    setProperty("helpTopic", QStringLiteral("effects.md#speed-masters"));
    SpeedMasters *sm = m_doc->speedMasters();

    QGridLayout *grid = new QGridLayout(this);
    const QStringList heads = { QString(), tr("Name"), tr("Follows"), tr("Ratio"), tr("BPM"),
                                QString(), tr("Rate"), QString(), QString() };
    for (int c = 0; c < heads.size(); ++c)
        if (!heads.at(c).isEmpty())
            grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(heads.at(c)), this), 0, c);

    for (int i = 0; i < SpeedMasters::Count; ++i)
    {
        Row &r = m_rows[i];
        const int g = i + 1;

        r.light = new QLabel(this);
        r.light->setFixedSize(14, 14);
        grid->addWidget(r.light, g, 0);

        r.name = new QLineEdit(this);
        r.name->setPlaceholderText(tr("Speed %1").arg(i + 1));
        r.name->setMinimumWidth(110);
        grid->addWidget(r.name, g, 1);
        connect(r.name, &QLineEdit::editingFinished, this, [this, i]() {
            m_doc->speedMasters()->setName(i, m_rows[i].name->text().trimmed());
            m_doc->setModified();
        });

        r.source = new QComboBox(this);
        r.source->addItem(tr("Its own tempo"), -1);
        r.source->addItem(tr("Main clock"), int(SpeedMasters::MainClock));
        for (int j = 0; j < SpeedMasters::Count; ++j)
            if (j != i)
                r.source->addItem(tr("Speed %1").arg(j + 1), j);
        r.source->setToolTip(tr("Keep its own tempo, or follow the main clock or another master "
                                "at a ratio, on its beats."));
        grid->addWidget(r.source, g, 2);

        r.ratio = new QComboBox(this);
        foreach (double v, ratios())
            r.ratio->addItem(ratioText(v), v);
        grid->addWidget(r.ratio, g, 3);
        connect(r.source, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, i](int) { applySource(i); });
        connect(r.ratio, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, i](int) { applySource(i); });

        r.bpm = new QDoubleSpinBox(this);
        r.bpm->setRange(1, 999);
        r.bpm->setDecimals(1);
        r.bpm->setKeyboardTracking(false);
        grid->addWidget(r.bpm, g, 4);
        connect(r.bpm, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double v) {
            if (m_refreshing) return;
            m_doc->speedMasters()->setBpm(i, v);
            m_doc->setModified();
        });

        r.tap = new QPushButton(tr("Tap"), this);
        r.tap->setToolTip(tr("Tap the beat: the tempo from your last four taps, the beat on the last."));
        grid->addWidget(r.tap, g, 5);
        connect(r.tap, &QPushButton::clicked, this, [this, i]() {
            m_doc->speedMasters()->tap(i);
            m_doc->setModified();
        });

        r.rate = new QSlider(Qt::Horizontal, this);
        r.rate->setRange(0, 200);
        r.rate->setMinimumWidth(100);
        r.rate->setToolTip(tr("Ride the master: 100% is the tempo as set."));
        grid->addWidget(r.rate, g, 6);
        r.rateText = new QLabel(this);
        r.rateText->setFixedWidth(44);
        grid->addWidget(r.rateText, g, 7);
        connect(r.rate, &QSlider::valueChanged, this, [this, i](int v) {
            m_rows[i].rateText->setText(QStringLiteral("%1%").arg(v));
            if (m_refreshing) return;
            m_doc->speedMasters()->setRate(i, v / 100.0);
            m_doc->setModified();
        });

        r.pause = new QPushButton(tr("Pause"), this);
        r.pause->setCheckable(true);
        r.pause->setToolTip(tr("Stop the master: everything on it freezes where it is."));
        grid->addWidget(r.pause, g, 8);
        connect(r.pause, &QPushButton::toggled, this, [this, i](bool on) {
            if (m_refreshing) return;
            m_doc->speedMasters()->setPaused(i, on);
            m_doc->setModified();
        });
    }
    // Only once every row exists: a row's refresh renames it in the others'
    // "Follows" lists.
    for (int i = 0; i < SpeedMasters::Count; ++i)
        refresh(i);
    grid->setColumnStretch(6, 1);

    connect(sm, &SpeedMasters::changed, this, &SpeedMastersPanel::refresh);
    m_lightTimer = new QTimer(this);
    m_lightTimer->setInterval(30);
    connect(m_lightTimer, &QTimer::timeout, this, &SpeedMastersPanel::tickLights);
    m_lightTimer->start();
}

SpeedMastersPanel *SpeedMastersPanel::open(Doc *doc, QWidget *parent)
{
    static QPointer<SpeedMastersPanel> panel;
    if (panel.isNull())
        panel = new SpeedMastersPanel(doc, parent);
    panel->show();
    panel->raise();
    panel->activateWindow();
    return panel;
}

void SpeedMastersPanel::applySource(int index)
{
    if (m_refreshing)
        return;
    SpeedMasters *sm = m_doc->speedMasters();
    Row &r = m_rows[index];
    const int source = r.source->currentData().toInt();
    if (source == -1)
        sm->setOwn(index);
    else if (!sm->setFollow(index, source, r.ratio->currentData().toDouble()))
        refresh(index);                 // a loop: put the row back as it is
    m_doc->setModified();
}

void SpeedMastersPanel::refresh(int index)
{
    if (index < 0 || index >= SpeedMasters::Count)
        return;
    SpeedMasters *sm = m_doc->speedMasters();
    Row &r = m_rows[index];
    m_refreshing = true;
    const QString name = sm->name(index);
    r.name->setText(name == tr("Speed %1").arg(index + 1) ? QString() : name);
    const bool own = sm->mode(index) == SpeedMasters::Own;
    r.source->setCurrentIndex(qMax(0, r.source->findData(own ? -1 : sm->follows(index))));
    int ri = 0;
    for (int k = 0; k < r.ratio->count(); ++k)
        if (qAbs(r.ratio->itemData(k).toDouble() - sm->ratio(index)) < 1e-6)
            ri = k;
    r.ratio->setCurrentIndex(ri);
    r.ratio->setEnabled(!own);
    r.bpm->setValue(own ? sm->setBpm(index) : sm->bpm(index));
    r.bpm->setEnabled(own);
    r.tap->setEnabled(own);
    r.rate->setValue(qRound(sm->rate(index) * 100));
    r.rateText->setText(QStringLiteral("%1%").arg(r.rate->value()));
    r.rate->setEnabled(own);
    r.pause->setChecked(sm->isPaused(index));
    // Other masters' "Follows" lists name them; keep the names current.
    for (int j = 0; j < SpeedMasters::Count; ++j)
    {
        const int k = m_rows[j].source->findData(index);
        if (j != index && k >= 0)
            m_rows[j].source->setItemText(k, name);
    }
    m_refreshing = false;
}

void SpeedMastersPanel::tickLights()
{
    SpeedMasters *sm = m_doc->speedMasters();
    for (int i = 0; i < SpeedMasters::Count; ++i)
    {
        const double pos = sm->beatPosition(i);
        const bool on = !sm->isPaused(i) && pos - std::floor(pos) < 0.15;
        if (m_rows[i].light->property("lit").toBool() != on
            || !m_rows[i].light->property("lit").isValid())
        {
            // Only on a change: a stylesheet re-polishes the widget.
            m_rows[i].light->setProperty("lit", on);
            m_rows[i].light->setStyleSheet(on ? QStringLiteral("background:#f5a623;border-radius:7px;")
                                              : QStringLiteral("background:#444;border-radius:7px;"));
        }
        // A follower's tempo moves with what it follows.
        if (sm->mode(i) == SpeedMasters::Follow && !m_rows[i].bpm->hasFocus())
        {
            m_refreshing = true;
            m_rows[i].bpm->setValue(qMax(1.0, sm->bpm(i)));
            m_refreshing = false;
        }
    }
}
