/*
  Q Light Controller Plus
  qlcengine_export.h

  Windows DLL import/export for the engine library's classes.

  Only Windows needs this, and only for classes whose signals are named by
  member-function pointer from another module (connect(), QSignalSpy). MinGW
  exports a DLL's symbols automatically, but in the importing module the
  address of such a function is a local import thunk, not the function: moc's
  signal lookup compares addresses, finds no match, and the connection
  silently never happens. dllimport makes the importer take the real address.

  Code linked into the engine DLL from a static library (qlcplusaudio) is
  built with QLC_ENGINE_BUILD, since CMake only defines qlcplusengine_EXPORTS
  for the DLL's own sources.

  The library is still linked with --export-all-symbols (see its
  CMakeLists.txt), so every other class keeps being exported as before.

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

#ifndef QLCENGINE_EXPORT_H
#define QLCENGINE_EXPORT_H

#include <QtGlobal>

#if defined(Q_OS_WIN)
#  if defined(qlcplusengine_EXPORTS) || defined(QLC_ENGINE_BUILD)
#    define QLC_ENGINE_EXPORT Q_DECL_EXPORT
#  else
#    define QLC_ENGINE_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define QLC_ENGINE_EXPORT
#endif

#endif
