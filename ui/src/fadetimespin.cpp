/*
  Q Light Controller Plus
  fadetimespin.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "fadetimespin.h"
#include "qlcpalette.h"
#include "scene.h"
#include "doc.h"

FadeTimeSpin::FadeTimeSpin(QWidget *parent, const QString &defaultLabel)
    : QDoubleSpinBox(parent)
    , m_defaultLabel(defaultLabel)
{
    setDecimals(2);
    setSingleStep(0.1);
    setSuffix(tr(" s"));
    setKeyboardTracking(false);             // a typed time counts on Enter, once
    if (hasDefault())
    {
        setRange(-0.01, 600.0);             // the lowest value is the default
        setSpecialValueText(m_defaultLabel);
    }
    else
        setRange(0.0, 600.0);
    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emit msChanged(ms()); });
}

int FadeTimeSpin::ms() const
{
    if (hasDefault() && value() < 0.0)
        return -1;
    return int(value() * 1000.0 + 0.5);
}

void FadeTimeSpin::setMs(int ms)
{
    const bool was = blockSignals(true);
    setValue((ms < 0 && hasDefault()) ? minimum() : qMax(0, ms) / 1000.0);
    blockSignals(was);
}

QString FadeTimeSpin::text(int ms, const QString &defaultLabel)
{
    if (ms < 0)
        return defaultLabel;
    return QString::number(ms / 1000.0, 'g', 3) + tr(" s");
}

bool LookFades::hasFadeIn(Doc *doc, const Scene *scene, quint32 paletteId)
{
    QLCPalette *p = (doc != nullptr) ? doc->palette(paletteId) : nullptr;
    if (p == nullptr || scene == nullptr || p->type() == QLCPalette::Effect)
        return false;
    return !QLCPalette::isEffectScoped(doc, scene->palettes(), paletteId);
}

bool LookFades::hasFadeOut(Doc *doc, const Scene *scene, quint32 paletteId)
{
    QLCPalette *p = (doc != nullptr) ? doc->palette(paletteId) : nullptr;
    if (p == nullptr || scene == nullptr)
        return false;
    if (p->type() == QLCPalette::Effect)
        return true;                        // its release
    return !QLCPalette::isEffectScoped(doc, scene->palettes(), paletteId);
}

QString LookFades::defaultLabel(Doc *doc, quint32 paletteId)
{
    QLCPalette *p = (doc != nullptr) ? doc->palette(paletteId) : nullptr;
    // An effect's release left alone is no release: it stops.
    if (p != nullptr && p->type() == QLCPalette::Effect)
        return QObject::tr("snap");
    return QObject::tr("scene");
}
