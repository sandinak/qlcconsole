/*
  Q Light Controller Plus
  gamepadrole.cpp

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

#include <QHash>

#include "gamepadrole.h"

namespace
{
struct Entry { GamepadRole::Role role; const char *key; };

const Entry kKeys[] = {
    { GamepadRole::A, "a" },           { GamepadRole::B, "b" },
    { GamepadRole::X, "x" },           { GamepadRole::Y, "y" },
    { GamepadRole::LB, "lb" },         { GamepadRole::RB, "rb" },
    { GamepadRole::Back, "back" },     { GamepadRole::Start, "start" },
    { GamepadRole::LS, "ls" },         { GamepadRole::RS, "rs" },
    { GamepadRole::Guide, "guide" },
    { GamepadRole::DpadUp, "dpad_up" },     { GamepadRole::DpadDown, "dpad_down" },
    { GamepadRole::DpadLeft, "dpad_left" }, { GamepadRole::DpadRight, "dpad_right" },
    { GamepadRole::LeftX, "lx" },      { GamepadRole::LeftY, "ly" },
    { GamepadRole::RightX, "rx" },     { GamepadRole::RightY, "ry" },
    { GamepadRole::LeftTrigger, "lt" }, { GamepadRole::RightTrigger, "rt" },
    { GamepadRole::TouchX, "touch_x" },   { GamepadRole::TouchY, "touch_y" },
    { GamepadRole::TouchDown, "touch_down" },
    { GamepadRole::Touch2X, "touch2_x" }, { GamepadRole::Touch2Y, "touch2_y" },
    { GamepadRole::Touch2Down, "touch2_down" },
    { GamepadRole::TouchClick, "touchpad" },
};

/* Names, squashed to lower-case letters and digits only, that controllers'
   profiles use for the same controls. */
const QHash<QString, GamepadRole::Role> &nameTable()
{
    static const QHash<QString, GamepadRole::Role> t = {
        { "a", GamepadRole::A }, { "buttona", GamepadRole::A }, { "cross", GamepadRole::A },
        { "b", GamepadRole::B }, { "buttonb", GamepadRole::B }, { "circle", GamepadRole::B },
        { "x", GamepadRole::X }, { "buttonx", GamepadRole::X }, { "square", GamepadRole::X },
        { "y", GamepadRole::Y }, { "buttony", GamepadRole::Y }, { "triangle", GamepadRole::Y },
        { "lb", GamepadRole::LB }, { "leftbumper", GamepadRole::LB }, { "l1", GamepadRole::LB },
        { "leftshoulder", GamepadRole::LB },
        { "rb", GamepadRole::RB }, { "rightbumper", GamepadRole::RB }, { "r1", GamepadRole::RB },
        { "rightshoulder", GamepadRole::RB },
        { "back", GamepadRole::Back }, { "view", GamepadRole::Back }, { "select", GamepadRole::Back },
        { "share", GamepadRole::Back },
        { "start", GamepadRole::Start }, { "menu", GamepadRole::Start }, { "options", GamepadRole::Start },
        { "ls", GamepadRole::LS }, { "leftstick", GamepadRole::LS }, { "l3", GamepadRole::LS },
        { "rs", GamepadRole::RS }, { "rightstick", GamepadRole::RS }, { "r3", GamepadRole::RS },
        { "guide", GamepadRole::Guide }, { "xbox", GamepadRole::Guide }, { "home", GamepadRole::Guide },
        { "ps", GamepadRole::Guide },
        { "dpadup", GamepadRole::DpadUp }, { "up", GamepadRole::DpadUp },
        { "dpaddown", GamepadRole::DpadDown }, { "down", GamepadRole::DpadDown },
        { "dpadleft", GamepadRole::DpadLeft }, { "left", GamepadRole::DpadLeft },
        { "dpadright", GamepadRole::DpadRight }, { "right", GamepadRole::DpadRight },
        { "leftstickx", GamepadRole::LeftX }, { "lx", GamepadRole::LeftX },
        { "leftsticky", GamepadRole::LeftY }, { "ly", GamepadRole::LeftY },
        { "rightstickx", GamepadRole::RightX }, { "rx", GamepadRole::RightX },
        { "rightsticky", GamepadRole::RightY }, { "ry", GamepadRole::RightY },
        { "lefttrigger", GamepadRole::LeftTrigger }, { "lt", GamepadRole::LeftTrigger },
        { "l2", GamepadRole::LeftTrigger },
        { "righttrigger", GamepadRole::RightTrigger }, { "rt", GamepadRole::RightTrigger },
        { "r2", GamepadRole::RightTrigger },
        { "touchpad", GamepadRole::TouchClick }, { "touchpadbutton", GamepadRole::TouchClick },
        { "touchpadclick", GamepadRole::TouchClick },
    };
    return t;
}

QString squash(const QString &s)
{
    QString out;
    for (const QChar c : s)
        if (c.isLetterOrNumber())
            out += c.toLower();
    return out;
}
}

GamepadRole::Role GamepadRole::fromProfile(const QString &role, const QString &name)
{
    const QString r = role.trimmed().toLower();
    if (!r.isEmpty())
    {
        for (const Entry &e : kKeys)
            if (r == QLatin1String(e.key))
                return e.role;
        // An unknown key is a profile saying "not one of these": do not
        // second-guess it from the name.
        return None;
    }
    return nameTable().value(squash(name), None);
}

QString GamepadRole::key(Role role)
{
    for (const Entry &e : kKeys)
        if (e.role == role)
            return QLatin1String(e.key);
    return QString();
}

bool GamepadRole::isAxis(Role role)
{
    return role >= LeftX && role <= RightTrigger;
}

bool GamepadRole::isTouch(Role role)
{
    return role >= TouchX && role <= Touch2Down;
}
