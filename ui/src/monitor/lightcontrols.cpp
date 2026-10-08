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
        m_count->setVisible(m_kind->currentData().toInt() != PointSource);
        if (!m_loading)
            emit edited();
    });
    connect(m_count, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_loading)
            emit edited();
    });
    connect(m_glass, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_loading)
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

void LightControls::load(Doc *doc, const Fixture *fx)
{
    if (!relevant(fx))
        return;
    m_loading = true;
    const QLCPhysical &ph = fx->fixtureMode()->physical();
    const int defKind = ph.leds() <= 0 ? PointSource
                      : ph.ledLayout() == QLCPhysical::TapeLeds ? LedTape : HangingCluster;
    const QString defined = tr(" (as defined)");
    m_kind->setItemText(PointSource,    tr("Point source") + (defKind == PointSource ? defined : QString()));
    m_kind->setItemText(HangingCluster, tr("Hanging LED cluster") + (defKind == HangingCluster ? defined : QString()));
    m_kind->setItemText(LedTape,        tr("LED tape") + (defKind == LedTape ? defined : QString()));
    m_glass->setItemText(0, tr("Diffuse") + (!ph.clearGlass() ? defined : QString()));
    m_glass->setItemText(1, tr("Clear") + (ph.clearGlass() ? defined : QString()));

    int layout = QLCPhysical::ClusterLeds;
    const int leds = RigHeads::lightLeds(doc, fx->id(), 0, &layout);
    m_kind->setCurrentIndex(leds <= 0 ? PointSource
                            : layout == QLCPhysical::TapeLeds ? LedTape : HangingCluster);
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
    if (kind == PointSource)
    {
        rp.lightLeds = (ph.leds() <= 0) ? -1 : 0;
        rp.lightLayout = -1;
    }
    else
    {
        // The definition's count is left to it: a WLED's own count can win.
        rp.lightLeds = (ph.leds() > 0 && m_count->value() == ph.leds()) ? -1 : m_count->value();
        const int layout = (kind == LedTape) ? int(QLCPhysical::TapeLeds) : int(QLCPhysical::ClusterLeds);
        rp.lightLayout = (ph.leds() > 0 && layout == int(ph.ledLayout())) ? -1 : layout;
    }
    const bool clear = m_glass->currentData().toInt() == 1;
    rp.lightGlass = (clear == ph.clearGlass()) ? -1 : (clear ? 1 : 0);
}
