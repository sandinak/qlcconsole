/*
  Q Light Controller Plus
  wledstate.cpp

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

#include <QJsonArray>

#include "wledstate.h"

QJsonObject WledState::decode(const QByteArray &universe, int address, int channels,
                              int transitionMs)
{
    auto at = [&](int offset) -> int {
        const int i = address - 1 + offset;
        return (i >= 0 && i < universe.size()) ? int(uchar(universe.at(i))) : 0;
    };
    auto colour = [&](int offset) {
        return QJsonArray{ at(offset), at(offset + 1), at(offset + 2) };
    };

    int bri = 255;
    QJsonObject seg;
    if (channels >= 15)
    {
        bri = at(0);
        seg[QStringLiteral("fx")]  = at(1);
        seg[QStringLiteral("sx")]  = at(2);
        seg[QStringLiteral("ix")]  = at(3);
        seg[QStringLiteral("pal")] = at(4);
        const int option = at(5) / 64;
        seg[QStringLiteral("mi")]  = (option == 1 || option == 3);
        seg[QStringLiteral("rev")] = (option == 2 || option == 3);
        seg[QStringLiteral("col")] = QJsonArray{ colour(6), colour(9), colour(12) };
    }
    else
    {
        const int first = (channels >= 4) ? 1 : 0;
        if (channels >= 4)
            bri = at(0);
        seg[QStringLiteral("fx")]  = 0;                    // solid
        seg[QStringLiteral("col")] = QJsonArray{ colour(first) };
    }

    QJsonObject state;
    // WLED has no brightness 0: off is "on": false.
    state[QStringLiteral("on")] = bri > 0;
    state[QStringLiteral("bri")] = qMax(1, bri);
    // In tenths of a second: a glide between sends instead of a step.
    state[QStringLiteral("transition")] = qMax(0, (transitionMs + 50) / 100);
    state[QStringLiteral("seg")] = QJsonArray{ seg };
    return state;
}
