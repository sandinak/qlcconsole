/*
  Q Light Controller Plus
  lightcontrols.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>

#include "monitorproperties.h"
#include "qlcfixturemode.h"
#include "lightcontrols.h"
#include "qlcphysical.h"
#include "righeads.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

LightControls::LightControls(QWidget *parent)
    : QObject(parent)
{
    m_lightRow = new QWidget(parent);
    QHBoxLayout *lay = new QHBoxLayout(m_lightRow);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    m_kind = new QComboBox(m_lightRow);
    m_kind->addItem(QString(), int(PointSource));
    m_kind->addItem(QString(), int(HangingCluster));
    m_kind->addItem(QString(), int(LedTape));
    m_kind->addItem(QString(), int(FaceArray));
    m_kind->setToolTip(tr("What makes its light. A point source is one glow; a hanging "
                          "LED cluster is a wire string let fall through the cage; LED "
                          "tape is wound tight round a core in the middle. With LEDs, "
                          "an effect running on a WLED shows along them."));
    m_count = new QSpinBox(m_lightRow);
    m_count->setRange(1, 1024);
    m_count->setSuffix(tr(" LEDs"));
    m_count->setKeyboardTracking(false);
    m_count->setToolTip(tr("How many LEDs. Left as defined, a WLED that answers says how "
                           "many it drives."));
    lay->addWidget(m_kind, 1);
    lay->addWidget(m_count);

    m_glass = new QComboBox(parent);
    m_glass->addItem(QString(), 0);
    m_glass->addItem(QString(), 1);
    m_glass->setToolTip(tr("What the light is seen through. Diffusing glass glows, each "
                           "pane in the LEDs nearest it, the LEDs a blur behind; clear "
                           "glass shows every LED."));

    connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const int kind = m_kind->currentData().toInt();
        m_count->setVisible(kind != PointSource && kind >= 0);
        if (m_loading || kind < 0)
            return;
        showMixed(m_kind, false);
        m_kindSet = true;
        emit edited();
    });
    connect(m_count, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_loading || (!m_count->specialValueText().isEmpty() && v == m_count->minimum()))
            return;
        m_count->setSpecialValueText(QString());
        m_count->setMinimum(1);
        m_kindSet = true;
        emit edited();
    });
    connect(m_glass, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_loading || m_glass->currentData().toInt() < 0)
            return;
        showMixed(m_glass, false);
        m_glassSet = true;
        emit edited();
    });
}

bool LightControls::relevant(const Fixture *fx)
{
    const QLCFixtureMode *mode = (fx != nullptr) ? fx->fixtureMode() : nullptr;
    if (mode == nullptr)
        return false;
    const QLCPhysical &ph = mode->physical();
    return ph.leds() > 0 || ph.emission() == QLCPhysical::OmniEmission
           || ph.bodyShape() == QLCPhysical::LanternBody;
}

void LightControls::showMixed(QComboBox *cb, bool mixed)
{
    const int at = cb->findData(-1);
    if (mixed && at < 0)
        cb->addItem(tr("— mixed —"), -1);
    else if (!mixed && at >= 0)
        cb->removeItem(at);
    if (mixed)
        cb->setCurrentIndex(cb->findData(-1));
}

void LightControls::load(Doc *doc, const QList<const Fixture *> &fixtures)
{
    QList<const Fixture *> those;
    foreach (const Fixture *fx, fixtures)
        if (relevant(fx))
            those << fx;
    if (those.isEmpty())
        return;
    load(doc, those.first());
    bool kindDiffers = false, countDiffers = false, glassDiffers = false;
    int firstLayout = QLCPhysical::ClusterLeds;
    const int firstLeds = RigHeads::lightLeds(doc, those.first()->id(), 0, &firstLayout);
    const bool firstClear = RigHeads::clearGlass(doc, those.first()->id());
    for (int i = 1; i < those.size(); ++i)
    {
        int layout = QLCPhysical::ClusterLeds;
        const int leds = RigHeads::lightLeds(doc, those.at(i)->id(), 0, &layout);
        if ((leds > 0) != (firstLeds > 0) || (leds > 0 && layout != firstLayout))
            kindDiffers = true;
        else if (leds != firstLeds)
            countDiffers = true;
        if (RigHeads::clearGlass(doc, those.at(i)->id()) != firstClear)
            glassDiffers = true;
    }
    m_loading = true;
    showMixed(m_kind, kindDiffers);
    if (kindDiffers || countDiffers)
    {
        m_count->setMinimum(0);
        m_count->setSpecialValueText(tr("mixed"));
        m_count->setValue(0);
        m_count->setVisible(!kindDiffers && firstLeds > 0);
    }
    showMixed(m_glass, glassDiffers);
    m_loading = false;
}

void LightControls::load(Doc *doc, const Fixture *fx)
{
    if (!relevant(fx))
        return;
    m_loading = true;
    m_kindSet = m_glassSet = false;
    showMixed(m_kind, false);
    showMixed(m_glass, false);
    m_count->setSpecialValueText(QString());
    m_count->setMinimum(1);
    const QLCPhysical &ph = fx->fixtureMode()->physical();
    auto kindOf = [](int leds, int layout) {
        return leds <= 0 ? PointSource
             : layout == QLCPhysical::TapeLeds ? LedTape
             : layout == QLCPhysical::FaceLeds ? FaceArray : HangingCluster;
    };
    const int defKind = kindOf(ph.leds(), ph.ledLayout());
    const QString defined = tr(" (as defined)");
    m_kind->setItemText(PointSource,    tr("Point source") + (defKind == PointSource ? defined : QString()));
    m_kind->setItemText(HangingCluster, tr("Hanging LED cluster") + (defKind == HangingCluster ? defined : QString()));
    m_kind->setItemText(LedTape,        tr("LED tape") + (defKind == LedTape ? defined : QString()));
    m_kind->setItemText(FaceArray,      tr("LEDs on the lens face") + (defKind == FaceArray ? defined : QString()));
    m_glass->setItemText(0, tr("Diffuse") + (!ph.clearGlass() ? defined : QString()));
    m_glass->setItemText(1, tr("Clear") + (ph.clearGlass() ? defined : QString()));

    int layout = QLCPhysical::ClusterLeds;
    const int leds = RigHeads::lightLeds(doc, fx->id(), 0, &layout);
    m_kind->setCurrentIndex(kindOf(leds, layout));
    m_count->setValue(leds > 0 ? leds : (ph.leds() > 0 ? ph.leds() : 16));
    m_count->setVisible(leds > 0);
    m_glass->setCurrentIndex(RigHeads::clearGlass(doc, fx->id()) ? 1 : 0);
    m_loading = false;
}

void LightControls::store(const Fixture *fx, FixtureRigProps &rp) const
{
    if (!relevant(fx))
        return;
    const QLCPhysical &ph = fx->fixtureMode()->physical();
    const int kind = m_kind->currentData().toInt();
    if (m_kindSet && kind == PointSource)
    {
        rp.lightLeds = (ph.leds() <= 0) ? -1 : 0;
        rp.lightLayout = -1;
    }
    else if (m_kindSet && kind > 0)
    {
        // A count still reading "mixed" keeps each one's own.
        int count = m_count->value();
        if (!m_count->specialValueText().isEmpty() && count == m_count->minimum())
            count = rp.lightLeds > 0 ? rp.lightLeds : (ph.leds() > 0 ? ph.leds() : 16);
        // The definition's count is left to it: a WLED's own count can win.
        rp.lightLeds = (ph.leds() > 0 && count == ph.leds()) ? -1 : count;
        const int layout = (kind == LedTape) ? int(QLCPhysical::TapeLeds)
                         : (kind == FaceArray) ? int(QLCPhysical::FaceLeds)
                                               : int(QLCPhysical::ClusterLeds);
        rp.lightLayout = (ph.leds() > 0 && layout == int(ph.ledLayout())) ? -1 : layout;
    }
    if (!m_glassSet || m_glass->currentData().toInt() < 0)
        return;
    const bool clear = m_glass->currentData().toInt() == 1;
    rp.lightGlass = (clear == ph.clearGlass()) ? -1 : (clear ? 1 : 0);
}
