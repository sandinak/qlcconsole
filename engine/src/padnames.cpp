/*
  Q Light Controller Plus
  padnames.cpp

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

#include <QCoreApplication>

#include "padnames.h"

using R = GamepadRole::Role;

namespace
{
/* The tokens, by role: the Xbox names. */
struct Token { const char *token; R role; };
const Token kTokens[] = {
    { "A", R::A }, { "B", R::B }, { "X", R::X }, { "Y", R::Y },
    { "LB", R::LB }, { "RB", R::RB }, { "LT", R::LeftTrigger }, { "RT", R::RightTrigger },
    { "View", R::Back }, { "Menu", R::Start }, { "LS", R::LS }, { "RS", R::RS },
    { "Guide", R::Guide }, { "Touchpad", R::TouchClick },
};

QString tr(const char *text)
{
    return QCoreApplication::translate("PadNames", text);
}

QString xboxName(R role, PadNames::Form form)
{
    switch (role)
    {
        case R::A: return QStringLiteral("A");
        case R::B: return QStringLiteral("B");
        case R::X: return QStringLiteral("X");
        case R::Y: return QStringLiteral("Y");
        case R::LB: return QStringLiteral("LB");
        case R::RB: return QStringLiteral("RB");
        case R::LeftTrigger: return QStringLiteral("LT");
        case R::RightTrigger: return QStringLiteral("RT");
        case R::LS: return QStringLiteral("LS");
        case R::RS: return QStringLiteral("RS");
        case R::Back: return form == PadNames::Glyph ? QString() : tr("View");
        case R::Start: return form == PadNames::Glyph ? QString() : tr("Menu");
        case R::Guide: return form == PadNames::Glyph ? QString() : tr("Xbox button");
        // An Xbox pad has no touchpad: its name, for "not on this controller".
        case R::TouchClick: return form == PadNames::Glyph ? QString() : tr("Touchpad");
        default: return QString();
    }
}

QString playStationName(R role, PadNames::Family family, PadNames::Form form)
{
    const bool glyph = form == PadNames::Glyph;
    switch (role)
    {
        case R::A: return glyph ? QString::fromUtf8("\342\234\225") : tr("Cross");
        case R::B: return glyph ? QString::fromUtf8("\342\227\213") : tr("Circle");
        case R::X: return glyph ? QString::fromUtf8("\342\226\241") : tr("Square");
        case R::Y: return glyph ? QString::fromUtf8("\342\226\263") : tr("Triangle");
        case R::LB: return QStringLiteral("L1");
        case R::RB: return QStringLiteral("R1");
        case R::LeftTrigger: return QStringLiteral("L2");
        case R::RightTrigger: return QStringLiteral("R2");
        case R::LS: return QStringLiteral("L3");
        case R::RS: return QStringLiteral("R3");
        // The DualSense renamed Share to Create.
        case R::Back:
            return glyph ? QString() : family == PadNames::DualSense ? tr("Create") : tr("Share");
        case R::Start: return glyph ? QString() : tr("Options");
        case R::Guide: return glyph ? QString() : tr("PS button");
        case R::TouchClick: return glyph ? QString() : tr("Touchpad");
        default: return QString();
    }
}
}

bool PadNames::isPlayStation(Family family)
{
    return family == DualShock4 || family == DualSense;
}

QString PadNames::name(GamepadRole::Role role, Family family, Form form)
{
    switch (role)
    {
        // The same on every pad.
        case R::DpadUp: return tr("D-pad up");
        case R::DpadDown: return tr("D-pad down");
        case R::DpadLeft: return tr("D-pad left");
        case R::DpadRight: return tr("D-pad right");
        case R::LeftX: case R::LeftY: return tr("Left stick");
        case R::RightX: case R::RightY: return tr("Right stick");
        default: break;
    }
    return isPlayStation(family) ? playStationName(role, family, form) : xboxName(role, form);
}

QString PadNames::expand(const QString &text, Family family, Form form)
{
    if (!text.contains(QLatin1Char('{')))
        return text;
    QString out;
    out.reserve(text.size() + 16);
    int i = 0;
    while (i < text.size())
    {
        const int open = text.indexOf(QLatin1Char('{'), i);
        if (open < 0)
        {
            out += QStringView(text).mid(i);
            break;
        }
        out += QStringView(text).mid(i, open - i);
        const int close = text.indexOf(QLatin1Char('}'), open + 1);
        if (close < 0)
        {
            out += QStringView(text).mid(open);
            break;
        }
        const QStringView token = QStringView(text).mid(open + 1, close - open - 1);
        QString word;
        for (const Token &t : kTokens)
            if (token == QLatin1String(t.token))
            {
                word = name(t.role, family, form);
                // A glyph with nothing written on it: the long name.
                if (word.isEmpty())
                    word = name(t.role, family, Long);
                break;
            }
        if (word.isEmpty())
            out += QStringView(text).mid(open, close - open + 1);    // not ours
        else
            out += word;
        i = close + 1;
    }
    return out;
}

PadNames::Family PadNames::familyFrom(const QString &kindKey)
{
    const QString k = kindKey.trimmed().toLower();
    if (k == QLatin1String("dualshock4"))
        return DualShock4;
    if (k == QLatin1String("dualsense"))
        return DualSense;
    return Xbox;
}

QString PadNames::kindKey(Family family)
{
    switch (family)
    {
        case DualShock4: return QStringLiteral("dualshock4");
        case DualSense: return QStringLiteral("dualsense");
        case Xbox: break;
    }
    return QStringLiteral("xbox");
}

PadNames::Family PadNames::guess(const QString &profile, const QString &device,
                                 const QMap<int, QString> &controls)
{
    const QString a = controls.value(R::A).trimmed();
    const bool ps = profile.contains(QLatin1String("PlayStation"), Qt::CaseInsensitive)
                    || device.contains(QLatin1String("DualShock"), Qt::CaseInsensitive)
                    || device.contains(QLatin1String("DualSense"), Qt::CaseInsensitive)
                    || a.compare(QLatin1String("Cross"), Qt::CaseInsensitive) == 0;
    if (!ps)
        return Xbox;
    return device.contains(QLatin1String("DualSense"), Qt::CaseInsensitive) ? DualSense : DualShock4;
}

QColor PadNames::faceColour(GamepadRole::Role role, Family family)
{
    if (isPlayStation(family))
    {
        switch (role)
        {
            case R::A: return QColor(0x7f, 0xa6, 0xd9);
            case R::B: return QColor(0xe0, 0x56, 0x6b);
            case R::X: return QColor(0xd8, 0x91, 0xbf);
            case R::Y: return QColor(0x3f, 0xb3, 0x9d);
            default: return QColor();
        }
    }
    switch (role)
    {
        case R::A: return QColor(0x3a, 0x9a, 0x4a);
        case R::B: return QColor(0xc8, 0x46, 0x3c);
        case R::X: return QColor(0x3d, 0x7f, 0xc1);
        case R::Y: return QColor(0xd0, 0xa0, 0x1c);
        default: return QColor();
    }
}
