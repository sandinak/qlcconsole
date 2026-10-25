/*
  Q Light Controller Plus
  controllerscheme.cpp

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
#include <QList>

#include "controllerscheme.h"

namespace
{
QString tr(const char *text)
{
    return QCoreApplication::translate("ControllerScheme", text);
}
}

ControllerScheme::Verb ControllerScheme::verbFor(GamepadRole::Role role)
{
    switch (role)
    {
        case GamepadRole::LeftTrigger:  return PreviousGroup;
        case GamepadRole::RightTrigger: return NextGroup;
        case GamepadRole::LB:           return PreviousItem;
        case GamepadRole::RB:           return NextItem;
        case GamepadRole::Y:            return CycleScope;
        case GamepadRole::A:            return Keep;
        case GamepadRole::B:            return Revert;
        case GamepadRole::X:            return Reset;
        case GamepadRole::Back:         return Undo;
        case GamepadRole::Start:        return Exit;
        // Branson: zoom reads like a camera (up is in); right tightens the
        // focus (its value up), left loosens it (its value down).
        case GamepadRole::DpadUp:       return ZoomNarrower;
        case GamepadRole::DpadDown:     return ZoomWider;
        case GamepadRole::DpadLeft:     return FocusNearer;
        case GamepadRole::DpadRight:    return FocusFarther;
        case GamepadRole::LS:           return Alternate;
        case GamepadRole::RS:           return WhereAmI;
        default:                        return NoVerb;
    }
}

GamepadRole::Role ControllerScheme::roleFor(Verb verb)
{
    switch (verb)
    {
        case PreviousGroup: return GamepadRole::LeftTrigger;
        case NextGroup:     return GamepadRole::RightTrigger;
        case PreviousItem:  return GamepadRole::LB;
        case NextItem:      return GamepadRole::RB;
        case CycleScope:    return GamepadRole::Y;
        case Keep:          return GamepadRole::A;
        case Revert:        return GamepadRole::B;
        case Reset:         return GamepadRole::X;
        case Undo:          return GamepadRole::Back;
        case Exit:          return GamepadRole::Start;
        case ZoomNarrower:  return GamepadRole::DpadUp;
        case ZoomWider:     return GamepadRole::DpadDown;
        case FocusNearer:   return GamepadRole::DpadLeft;
        case FocusFarther:  return GamepadRole::DpadRight;
        case Alternate:     return GamepadRole::LS;
        case WhereAmI:      return GamepadRole::RS;
        case NoVerb:        break;
    }
    return GamepadRole::None;
}

QList<ControllerScheme::Verb> ControllerScheme::verbs()
{
    return { PreviousGroup, NextGroup, PreviousItem, NextItem, CycleScope,
             Keep, Revert, Reset, Undo, Exit,
             ZoomNarrower, ZoomWider, FocusNearer, FocusFarther,
             Alternate, WhereAmI };
}

QString ControllerScheme::label(Tool tool, Verb verb)
{
    switch (verb)
    {
        case PreviousGroup:
            return tool == RefineTool ? tr("Previous layer")
                 : tool == FocusCallsTool ? tr("Previous focus call") : QString();
        case NextGroup:
            return tool == RefineTool ? tr("Next layer")
                 : tool == FocusCallsTool ? tr("Next focus call") : QString();
        case PreviousItem:
            return tool == RefineTool ? tr("Previous fixture / group") : tr("Previous fixture");
        case NextItem:
            return tool == RefineTool ? tr("Next fixture / group") : tr("Next fixture");
        case CycleScope:
            return tool == RefineTool ? tr("Scope: layer \342\206\222 group \342\206\222 fixture")
                                      : tr("Scope: group \342\206\224 fixture");
        case Keep:
            return tool == RefineTool ? tr("Keep in the cue")
                 : tool == FocusCallsTool ? tr("Save for this venue") : tr("Save");
        case Revert:
            return tool == RefineTool ? tr("Back to the cue") : tr("Revert");
        case Reset:
            return tool == RefineTool ? tr("Clear the exception") : tr("Zero");
        case Undo:
            return tool == RefineTool ? tr("Undo (+A: keep at the source)") : tr("Undo save");
        case Exit:
            return tr("Exit");
        case ZoomWider:
            return tr("Zoom out (wider beam)");
        case ZoomNarrower:
            return tr("Zoom in (narrower beam)");
        case FocusNearer:
            return tr("Focus looser");
        case FocusFarther:
            return tr("Focus tighter");
        case Alternate:
            return tool == RefineTool ? tr("D-pad page: beam / colour / gobo")
                 : tool == FocusCallsTool ? tr("Target page: move the call's target")
                                          : tr("Second reference");
        case WhereAmI:
            return tr("Say the light (hold: feedback)");
        case NoVerb:
            break;
    }
    return QString();
}

QString ControllerScheme::stickLabel(Tool tool, bool fine)
{
    if (tool == RefineTool)
        return fine ? tr("Fine move") : tr("Move");
    return fine ? tr("Fine nudge") : tr("Nudge");
}

QString ControllerScheme::feedbackName(Feedback feedback)
{
    switch (feedback)
    {
        case LightsFeedback: return tr("lights only");
        case VoiceFeedback:  return tr("voice only");
        case BothFeedback:   break;
    }
    return tr("lights and voice");
}

ControllerScheme::Feedback ControllerScheme::nextFeedback(Feedback feedback)
{
    switch (feedback)
    {
        case BothFeedback:   return LightsFeedback;
        case LightsFeedback: return VoiceFeedback;
        case VoiceFeedback:  break;
    }
    return BothFeedback;
}

QString ControllerScheme::feedbackKey(Feedback feedback)
{
    switch (feedback)
    {
        case LightsFeedback: return QStringLiteral("lights");
        case VoiceFeedback:  return QStringLiteral("voice");
        case BothFeedback:   break;
    }
    return QStringLiteral("both");
}

ControllerScheme::Feedback ControllerScheme::feedbackFromKey(const QString &key)
{
    if (key == QLatin1String("lights"))
        return LightsFeedback;
    if (key == QLatin1String("voice"))
        return VoiceFeedback;
    return BothFeedback;
}

bool ControllerScheme::repeats(Verb verb)
{
    return verb == ZoomWider || verb == ZoomNarrower || verb == FocusNearer || verb == FocusFarther;
}

void ControllerScheme::HoldRepeat::press(int key, bool repeats)
{
    m_held = repeats ? key : -1;
    m_wait = delay();
}

void ControllerScheme::HoldRepeat::release(int key)
{
    if (key == m_held)
        m_held = -1;
}

int ControllerScheme::HoldRepeat::due(double dt)
{
    if (m_held < 0)
        return 0;
    m_wait -= dt;
    int n = 0;
    while (m_wait <= 1e-9)      // a hair over: sums of seconds are not exact
    {
        ++n;
        m_wait += interval();
    }
    return n;
}

bool ControllerScheme::TriggerButton::update(float norm)
{
    if (!m_down && norm >= pressAt())
    {
        m_down = true;
        return true;
    }
    if (m_down && norm <= releaseAt())
        m_down = false;
    return false;
}
