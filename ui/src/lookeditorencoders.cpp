/*
  Q Light Controller Plus
  lookeditorencoders.cpp

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

/* The Look Editor under an encoder bank (EncoderBank): a turn sets the
   control that the look's own page has for the attribute, so it takes the
   same path as a hand on that control -- the same edit, the same undo step
   (merged for a gesture), the same preview. */

#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QColor>
#include <cmath>

#include "virtualconsole/vcxypadarea.h"
#include "encoderattributes.h"
#include "capabilitybar.h"
#include "linespread.h"
#include "paletteops.h"
#include "qlccapability.h"
#include "encoderbank.h"
#include "lookeditor.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "doc.h"

namespace
{
const int kPanDeg = 540;        // as the pan / tilt page maps its pad
const int kTiltDeg = 270;
const qreal kPadMax = 256.0;
const int kBeamStrobe = 4;      // EncoderBank's Beam slots past frost
const int kBeamShutter = 5;

double spreadUnit(bool fine) { return fine ? 0.1 : 0.5; }

int unitOf(int bank, int slot, bool fine, int range)
{
    return qMax(1, int(std::lround(EncoderAttributes::step(bank, slot, fine) * range)));
}
}

QSlider *LookEditor::encoderSlider(int bank, int slot) const
{
    const QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr)
        return nullptr;
    switch (p->type())
    {
    case QLCPalette::Color:
        if (bank != EncoderBank::ColourBank)
            return nullptr;
        switch (slot)
        {
        case 2: return m_redSlider;
        case 3: return m_greenSlider;
        case 4: return m_blueSlider;
        case 5: return m_whiteRow->isHidden() ? nullptr : m_whiteSlider;
        case 6: return m_amberRow->isHidden() ? nullptr : m_amberSlider;
        case 7: return m_uvRow->isHidden() ? nullptr : m_uvSlider;
        }
        return nullptr;
    case QLCPalette::Beam:
        if (bank != EncoderBank::BeamBank)
            return nullptr;
        return slot == 1 ? m_beamFocusSlider : slot == 2 ? m_beamIrisSlider
             : slot == 3 ? m_beamFrostSlider : nullptr;
    case QLCPalette::Gobo:
        return bank == EncoderBank::GoboBank && slot == 0 ? m_singleSlider : nullptr;
    case QLCPalette::Zoom:
        return bank == EncoderBank::BeamBank && slot == 0 ? m_singleSlider : nullptr;
    case QLCPalette::Strobe:
        return bank == EncoderBank::BeamBank && slot == kBeamStrobe ? m_strobeSlider : nullptr;
    case QLCPalette::Shutter:
        return bank == EncoderBank::BeamBank && slot == kBeamShutter ? m_singleSlider : nullptr;
    default:
        break;
    }
    return nullptr;
}

bool LookEditor::encoderHas(int bank, int slot) const
{
    const QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr || slot < 0 || slot >= EncoderBank::slotCount(bank))
        return false;
    switch (p->type())
    {
    case QLCPalette::Dimmer:
        return bank == EncoderBank::IntensityBank;
    case QLCPalette::PanTilt:
        return bank == EncoderBank::PositionBank;
    case QLCPalette::Color:
        return bank == EncoderBank::ColourBank && (slot < 2 || encoderSlider(bank, slot) != nullptr);
    case QLCPalette::Aim:
        return bank == EncoderBank::PositionBank && slot == 0 && !m_lineBox->isHidden();
    default:
        return encoderSlider(bank, slot) != nullptr;
    }
}

bool LookEditor::encoderValue(int bank, int slot, float *norm) const
{
    if (norm == nullptr || !encoderHas(bank, slot))
        return false;
    const QLCPalette *p = m_doc->palette(m_paletteId);
    switch (p->type())
    {
    case QLCPalette::Dimmer:
        *norm = float(m_dimmerBar->value()) / 255.0f;
        return true;
    case QLCPalette::PanTilt:
        *norm = slot == 0 ? float(p->intValue1()) / kPanDeg : float(p->intValue2()) / kTiltDeg;
        return true;
    case QLCPalette::Color:
        if (slot < 2)
        {
            float h, s, v;
            QColor(m_redSlider->value(), m_greenSlider->value(), m_blueSlider->value()).getHsvF(&h, &s, &v);
            *norm = slot == 0 ? qMax(0.0f, h) : s;
            return true;
        }
        break;
    case QLCPalette::Aim:
    {
        const double max = LineSpread::maxSpread();
        *norm = float((m_lineSpreadSpin->value() + max) / (2.0 * max));
        return true;
    }
    default:
        break;
    }
    if (QSlider *sl = encoderSlider(bank, slot))
    {
        *norm = float(sl->value()) / float(qMax(1, sl->maximum()));
        return true;
    }
    return false;
}

bool LookEditor::encoderAdjust(int bank, int slot, int detents, bool fine)
{
    if (detents == 0 || !encoderHas(bank, slot))
        return false;
    QLCPalette *p = m_doc->palette(m_paletteId);

    switch (p->type())
    {
    case QLCPalette::Dimmer:
        // The bar's own change is the Look Editor's edit (slotDimmerChanged).
        m_dimmerBar->setValue(m_dimmerBar->value() + detents * unitOf(bank, slot, fine, 255));
        return true;
    case QLCPalette::PanTilt:
    {
        int pan = p->intValue1();
        int tilt = p->intValue2();
        if (slot == 0)
            pan = qBound(0, pan + detents * unitOf(bank, slot, fine, kPanDeg), kPanDeg);
        else
            tilt = qBound(0, tilt + detents * unitOf(bank, slot, fine, kTiltDeg), kTiltDeg);
        setPanTilt(pan, tilt);
        return true;
    }
    case QLCPalette::Color:
        if (slot < 2)
        {
            float h, s, v;
            QColor(m_redSlider->value(), m_greenSlider->value(), m_blueSlider->value()).getHsvF(&h, &s, &v);
            if (v <= 0.0f)
                v = 1.0f;
            if (slot == 0)
            {
                if (s <= 0.0f)
                    s = 1.0f;
                h = (h < 0.0f ? 0.0f : h) + detents * EncoderAttributes::step(bank, 0, fine);
                h -= std::floor(h);
            }
            else
            {
                s = qBound(0.0f, s + detents * EncoderAttributes::step(bank, 1, fine), 1.0f);
            }
            setColourHsv(h, s, v);
            return true;
        }
        break;
    case QLCPalette::Aim:
        setSpread(m_lineSpreadSpin->value() + detents * spreadUnit(fine));
        return true;
    default:
        break;
    }

    QSlider *sl = encoderSlider(bank, slot);
    if (sl == nullptr)
        return false;
    if ((p->type() == QLCPalette::Gobo || p->type() == QLCPalette::Shutter) && !fine && m_singleChannel != nullptr
        && m_singleChannel->capabilities().size() > 1)
    {
        // A gobo wheel or a shutter steps range to range (a turn is a gobo, or open /
        // closed / strobe), fine is raw.
        const QList<QLCCapability *> caps = m_singleChannel->capabilities();
        const int cur = sl->value();
        int at = 0;
        for (int i = 0; i < caps.size(); ++i)
            if (cur >= caps.at(i)->min())
                at = i;
        if (detents < 0 && cur > caps.at(at)->min())
            ++detents;
        sl->setValue(caps.at(qBound(0, at + detents, int(caps.size()) - 1))->min());
        return true;
    }
    // The slider's own change is the Look Editor's edit.
    sl->setValue(sl->value() + detents * unitOf(bank, slot, fine, sl->maximum()));
    return true;
}

bool LookEditor::encoderHome(int bank, int slot)
{
    if (!encoderHas(bank, slot))
        return false;
    QLCPalette *p = m_doc->palette(m_paletteId);
    switch (p->type())
    {
    case QLCPalette::Dimmer:
        m_dimmerBar->setValue(0);
        return true;
    case QLCPalette::PanTilt:
        setPanTilt(slot == 0 ? kPanDeg / 2 : p->intValue1(), slot == 0 ? p->intValue2() : kTiltDeg / 2);
        return true;
    case QLCPalette::Color:
        if (slot < 2)
        {
            float h, s, v;
            QColor(m_redSlider->value(), m_greenSlider->value(), m_blueSlider->value()).getHsvF(&h, &s, &v);
            if (v <= 0.0f)
                v = 1.0f;
            setColourHsv(slot == 0 ? 0.0f : qMax(0.0f, h), slot == 0 ? (s <= 0.0f ? 1.0f : s) : 1.0f, v);
            return true;
        }
        break;
    case QLCPalette::Aim:
        setSpread(0.0);
        return true;
    default:
        break;
    }
    if (QSlider *sl = encoderSlider(bank, slot))
    {
        sl->setValue(0);
        return true;
    }
    return false;
}

QString LookEditor::encoderLabel(int bank, int slot) const
{
    const QLCPalette *p = m_doc->palette(m_paletteId);
    if (p != nullptr && p->type() == QLCPalette::Aim && bank == EncoderBank::PositionBank && slot == 0)
        return tr("Spread");
    if (p != nullptr && p->type() == QLCPalette::Strobe && bank == EncoderBank::BeamBank && slot == kBeamStrobe)
        return tr("Strobe rate");
    return QString();
}

QString LookEditor::encoderHint() const
{
    const QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr)
        return QString();
    switch (p->type())
    {
    case QLCPalette::Aim:
        return m_lineBox->isHidden() ? tr("an Aim at a single target has no spread to turn") : QString();
    case QLCPalette::Effect:
        return tr("an effect is set on its own page, not with the encoders");
    case QLCPalette::Formation:
        return tr("a Formation is set on its own page, not with the encoders");
    default:
        break;
    }
    return QString();
}

void LookEditor::setSpread(double spread)
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr || p->type() != QLCPalette::Aim || m_lineBox->isHidden())
        return;
    const double max = LineSpread::maxSpread();
    const double to = qBound(-max, spread, max);
    auto set = [this, to]() { m_lineSpreadSpin->setValue(to); };
    // The spread is a param of the look, so it is a param step (merged for a gesture).
    if (PaletteOps *ops = m_doc->paletteOps())
        ops->effectParamStep(tr("Edit %1").arg(p->name()), { p->id() }, set,
                             QStringLiteral("lookspread:%1").arg(p->id()));
    else
        set();
}

void LookEditor::setPanTilt(int pan, int tilt)
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr || p->type() != QLCPalette::PanTilt)
        return;
    if (pan == p->intValue1() && tilt == p->intValue2())
        return;
    editValue([pan, tilt](QLCPalette *pal) { pal->setValue(pan, tilt); });
    m_doc->setModified();
    {
        QSignalBlocker block(m_xyPad);
        m_xyPad->setPosition(QPointF(qreal(pan) / kPanDeg * kPadMax, qreal(tilt) / kTiltDeg * kPadMax));
    }
    m_xyPad->update();
    emit paletteChanged(m_paletteId);
}

void LookEditor::setColourHsv(float h, float s, float v)
{
    const QColor c = QColor::fromHsvF(qBound(0.0f, h, 1.0f), qBound(0.0f, s, 1.0f), qBound(0.0f, v, 1.0f));
    {
        // The three sliders move as one edit, not three.
        const QSignalBlocker r(m_redSlider), g(m_greenSlider), b(m_blueSlider);
        m_redSlider->setValue(c.red());
        m_greenSlider->setValue(c.green());
        m_blueSlider->setValue(c.blue());
    }
    slotRgbSliderChanged();
}
