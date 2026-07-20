/*
  Q Light Controller Plus - Unit test
  resource_paths.h

  Copyright (c) Jano Svitok

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

#ifndef RESOURCE_PATHS_H
#define RESOURCE_PATHS_H

// When the test CMake provides QLC_TEST_RESOURCE_ROOT (an absolute path to the
// source tree's resources/ dir), use it — the old relative paths assume an
// in-tree (qmake) layout and break with an out-of-tree CMake build/ dir, so
// every fixture-dependent test would abort at loadMap(). The relative paths are
// kept as a fallback for builds that don't set the macro.
#ifdef QLC_TEST_RESOURCE_ROOT
#define INTERNAL_FIXTUREDIR QLC_TEST_RESOURCE_ROOT "fixtures/"
#define INTERNAL_PROFILEDIR QLC_TEST_RESOURCE_ROOT "inputprofiles/"
#define INTERNAL_SCRIPTDIR  QLC_TEST_RESOURCE_ROOT "rgbscripts/"
#else
#define INTERNAL_FIXTUREDIR "../../../resources/fixtures/"
#define INTERNAL_PROFILEDIR "../../../resources/inputprofiles/"
#define INTERNAL_SCRIPTDIR "../../../resources/rgbscripts/"
#endif

#endif
