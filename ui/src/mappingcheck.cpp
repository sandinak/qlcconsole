/*
  Q Light Controller Plus
  mappingcheck.cpp

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

#include "mappingcheck.h"

using R = GamepadRole::Role;

namespace
{
QString tr(const char *text)
{
    return QCoreApplication::translate("MappingCheck", text);
}

const int kPressed = 128;     // a button's value is 255 down, 0 up
const int kTriggerFull = 230; // "pull fully"
const int kStickPush = 64;    // off the middle (128) by half the travel
}

QString MappingCheck::roleName(R role, PadNames::Family family)
{
    switch (role)
    {
        case R::LeftY: return tr("Left stick up");
        case R::LeftX: return tr("Left stick right");
        case R::RightY: return tr("Right stick up");
        case R::RightX: return tr("Right stick right");
        default: break;
    }
    QString name = PadNames::name(role, family);
    if (name.isEmpty())
        return GamepadRole::key(role);
    const bool face = role == R::A || role == R::B || role == R::X || role == R::Y;
    if (face && PadNames::isPlayStation(family))
        name += QStringLiteral(" (%1)").arg(PadNames::name(role, family, PadNames::Glyph));
    return name;
}

MappingCheck::MappingCheck(PadNames::Family family)
    : m_family(family)
{
    const bool ps = PadNames::isPlayStation(family);
    auto add = [this](R role, const QString &action, const QString &hint) {
        m_steps.append({ role, roleName(role, m_family), hint, action });
    };
    const QString press = tr("Press");
    const QString pull = tr("Pull");
    const QString push = tr("Push");

    // The face buttons, by colour on an Xbox pad and by place on both.
    add(R::A, press, ps ? tr("the bottom face button") : tr("the green bottom face button"));
    add(R::B, press, ps ? tr("the right face button") : tr("the red right face button"));
    add(R::X, press, ps ? tr("the left face button") : tr("the blue left face button"));
    add(R::Y, press, ps ? tr("the top face button") : tr("the yellow top face button"));
    add(R::LB, press, tr("the left bumper, on the top edge"));
    add(R::RB, press, tr("the right bumper, on the top edge"));
    add(R::LeftTrigger, pull, tr("the left trigger, under the bumper: all the way"));
    add(R::RightTrigger, pull, tr("the right trigger, under the bumper: all the way"));
    add(R::Back, press, ps ? tr("the small button left of the touchpad")
                           : tr("the small button left of centre, with two squares"));
    add(R::Start, press, ps ? tr("the small button right of the touchpad")
                            : tr("the small button right of centre, with three lines"));
    if (ps)
        add(R::TouchClick, press, tr("the touchpad: press it in until it clicks"));
    add(R::LS, press, tr("the left stick: push it straight down"));
    add(R::RS, press, tr("the right stick: push it straight down"));
    add(R::DpadUp, press, tr("the D-pad, top"));
    add(R::DpadDown, press, tr("the D-pad, bottom"));
    add(R::DpadLeft, press, tr("the D-pad, left"));
    add(R::DpadRight, press, tr("the D-pad, right"));
    add(R::LeftY, push, tr("the left stick, forward and hold"));
    add(R::LeftX, push, tr("the left stick, to the right and hold"));
    add(R::RightY, push, tr("the right stick, forward and hold"));
    add(R::RightX, push, tr("the right stick, to the right and hold"));

    m_states.fill(Pending, m_steps.size());
    m_read.fill(QString(), m_steps.size());
}

bool MappingCheck::active(R role, int value)
{
    switch (role)
    {
        case R::None:
        case R::TouchX: case R::TouchY: case R::TouchDown:
        case R::Touch2X: case R::Touch2Y: case R::Touch2Down:
            return false;
        case R::LeftTrigger: case R::RightTrigger:
            return value >= kTriggerFull;
        case R::LeftX: case R::LeftY: case R::RightX: case R::RightY:
            return value >= 128 + kStickPush || value <= 128 - kStickPush;
        default:
            return value >= kPressed;
    }
}

QString MappingCheck::prompt() const
{
    if (finished())
        return summary();
    const Step &s = m_steps.at(m_current);
    return QStringLiteral("%1 %2 — %3").arg(s.action, s.name, s.hint);
}

MappingCheck::Result MappingCheck::feed(R role, int value)
{
    if (finished())
        return Ignored;
    const bool down = active(role, value);
    // Let go (or back to rest): it may count again.
    if (!down)
    {
        m_held.remove(int(role));
        return Ignored;
    }
    // Still down from the last time it counted (an axis chattering at full).
    if (m_held.contains(int(role)))
        return Ignored;

    const Step &s = m_steps.at(m_current);
    if (role == s.role)
    {
        m_held.insert(int(role));
        m_states[m_current] = Passed;
        m_read[m_current].clear();
        advance();
        return Matched;
    }
    // A control already marked wrong waits for its retry; it does not pile up.
    if (m_states.at(m_current) == Wrong)
        return Ignored;
    m_held.insert(int(role));
    m_states[m_current] = Wrong;
    m_read[m_current] = roleName(role, m_family);
    return Mismatch;
}

void MappingCheck::advance()
{
    ++m_current;
    if (m_current >= m_steps.size())
        m_current = -1;
}

void MappingCheck::retry()
{
    if (finished() || m_states.at(m_current) != Wrong)
        return;
    m_states[m_current] = Pending;
    m_read[m_current].clear();
}

void MappingCheck::skip()
{
    if (finished())
        return;
    if (m_states.at(m_current) == Pending)
        m_states[m_current] = Skipped;
    advance();
}

void MappingCheck::stop()
{
    while (!finished())
        skip();
}

int MappingCheck::passedCount() const
{
    return m_states.count(Passed);
}

int MappingCheck::wrongCount() const
{
    return m_states.count(Wrong);
}

int MappingCheck::skippedCount() const
{
    return m_states.count(Skipped);
}

QString MappingCheck::summary() const
{
    const int judged = m_steps.size() - skippedCount();
    QString out = tr("%1 of %2 match").arg(passedCount()).arg(judged);
    if (wrongCount() > 0)
    {
        QStringList wrong;
        for (int i = 0; i < m_steps.size(); ++i)
            if (m_states.at(i) == Wrong)
                wrong << tr("%1 read as %2").arg(m_steps.at(i).name, m_read.at(i));
        out += tr("; %1 wrong: %2").arg(wrongCount()).arg(wrong.join(QStringLiteral(", ")));
    }
    if (skippedCount() > 0)
        out += tr("; %1 skipped").arg(skippedCount());
    return out;
}

QString MappingCheck::mismatchText(int i) const
{
    return tr("Expected %1, read %2").arg(m_steps.at(i).name, m_read.at(i));
}

QString MappingCheck::line(int i) const
{
    const QString &name = m_steps.at(i).name;
    switch (m_states.at(i))
    {
        case Passed: return QStringLiteral("✓ %1").arg(name);
        case Wrong: return QStringLiteral("\u2717 %1 \u2014 %2").arg(name, mismatchText(i));
        case Skipped: return tr("- %1 (skipped)").arg(name);
        case Pending: break;
    }
    return QStringLiteral("… %1").arg(name);
}

QString MappingCheck::report() const
{
    QStringList out;
    for (int i = 0; i < m_steps.size(); ++i)
        out << QStringLiteral("  ") + line(i);
    out << QStringLiteral("  ") + summary();
    return out.join(QLatin1Char('\n'));
}
