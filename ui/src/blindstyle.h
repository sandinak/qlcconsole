/*
  Q Light Controller Plus
  blindstyle.h

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

#ifndef BLINDSTYLE_H
#define BLINDSTYLE_H

#include <QString>

/**
 * Blind is blue, everywhere (docs/design/LIVE_BLIND.md): the Blind button,
 * the footer, the Show Tree's and the Workbench's "what the rig shows"
 * lines. Live keeps the theme's own colours. One blue, with white words,
 * so it reads the same on every theme.
 */
namespace BlindStyle
{
    /** The blind blue (Material blue 800). */
    inline QString colour() { return QStringLiteral("#1565c0"); }
    /** Its darker edge. */
    inline QString edge() { return QStringLiteral("#0d47a1"); }
    /** A label that says something is blind: white on the blue. */
    inline QString labelSheet()
    {
        return QStringLiteral("QLabel { color: #ffffff; background: %1; padding: 1px 6px;"
                              " border-radius: 3px; }").arg(colour());
    }
}

#endif
