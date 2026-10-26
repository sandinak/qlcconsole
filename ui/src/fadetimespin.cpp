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

SecondsSpinBox::SecondsSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
{
}

QString SecondsSpinBox::withDecimalPoint(const QString &text, const QLocale &locale)
{
    const QString point = locale.decimalPoint();
    QString out;
    out.reserve(text.size());
    for (const QChar &c : text)
    {
        if (c == QLatin1Char('.') || c == QLatin1Char(','))
            out += point;
        else
            out += c;
    }
    return out;
}

QString SecondsSpinBox::normalised(const QString &text) const
{
    // The words around the number are left as they are ("auto", " s").
    if (!specialValueText().isEmpty() && text == specialValueText())
        return text;
    QString body = text;
    QString pre;
    QString suf;
    if (!prefix().isEmpty() && body.startsWith(prefix()))
    {
        pre = prefix();
        body.remove(0, pre.size());
    }
    if (!suffix().isEmpty() && body.endsWith(suffix()))
    {
        suf = suffix();
        body.chop(suf.size());
    }
    return pre + withDecimalPoint(body, locale()) + suf;
}

QValidator::State SecondsSpinBox::validate(QString &input, int &pos) const
{
    /* The same length, so the cursor stays where it was; the field then
       shows the mark this language writes. */
    const QString fixed = normalised(input);
    if (fixed.size() == input.size())
        input = fixed;
    return QDoubleSpinBox::validate(input, pos);
}

double SecondsSpinBox::valueFromText(const QString &text) const
{
    return QDoubleSpinBox::valueFromText(normalised(text));
}

void SecondsSpinBox::fixup(QString &input) const
{
    input = normalised(input);
    QDoubleSpinBox::fixup(input);
}

FadeTimeSpin::FadeTimeSpin(QWidget *parent, const QString &defaultLabel)
    : SecondsSpinBox(parent)
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
