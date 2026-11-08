/*
  Q Light Controller Plus
  previewkeep.h

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

#ifndef PREVIEWKEEP_H
#define PREVIEWKEEP_H

/** A jump that only LOOKS at something (Show a fixture in the Studio, a
 *  Review line, a Why-is-this-on line, a footer chip): the tab it leaves
 *  hides, and a Show Tree or Workbench preview stops when its tab is left.
 *  Under one of these the leaving page keeps what it is playing, so the
 *  person sees the cue they are building where the jump took them (UX-72).
 *  The page resumes or stops it as usual the next time it is shown. */
class PreviewKeep
{
public:
    PreviewKeep() { ++depth(); }
    ~PreviewKeep() { --depth(); }
    PreviewKeep(const PreviewKeep &) = delete;
    PreviewKeep &operator=(const PreviewKeep &) = delete;
    static bool active() { return depth() > 0; }

private:
    static int &depth() { static int d = 0; return d; }
};

#endif
