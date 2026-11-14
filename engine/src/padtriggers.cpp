/*
  Q Light Controller Plus
  padtriggers.cpp

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

#include <cstring>

#include "padtriggers.h"

PadTriggers::State PadTriggers::forState(bool runRole, bool armed, bool walking)
{
    State s;
    if (!runRole)
        return s;
    if (walking)
        s.right = s.left = Click;
    /* Armed, the triggers stay loose: in Run they do nothing (0.4.5, no
       list switching from the pad), so a firm R2 would promise a GO. */
    (void)armed;
    return s;
}

const char *PadTriggers::name(Feel feel)
{
    switch (feel)
    {
        case Firm:  return "firm";
        case Click: return "click";
        default:    return "off";
    }
}

PadTriggers::Feel PadTriggers::fromName(const char *n)
{
    if (n != nullptr && std::strcmp(n, "firm") == 0)
        return Firm;
    if (n != nullptr && std::strcmp(n, "click") == 0)
        return Click;
    return Off;
}
