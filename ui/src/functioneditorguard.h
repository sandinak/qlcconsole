/*
  Q Light Controller Plus
  functioneditorguard.h

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

#ifndef FUNCTIONEDITORGUARD_H
#define FUNCTIONEDITORGUARD_H

#include <QtGlobal>

class QWidget;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * A function editor (scene, chaser, EFX, RGB matrix, collection, script,
 * audio, video, show) can outlive its function: Build deletes it, an undo
 * takes it back, the Show Tree retires it, File ▸ Open or New clears the
 * show. The host (the Functions window, the Shows tab, the Workbench, ...)
 * may not let the editor go at once, and until it does the Doc's signals
 * still reach it: ChaserEditor answered Doc::modeChanged with
 * m_chaser->stop() on the freed chaser (a hang or a crash going to Operate,
 * 2026-11-13).
 *
 * One mechanism for all of them: the editor holds its function in a
 * QPointer (null once the function is deleted, checked in its destructor
 * and in what its host may call) and calls watch() in its constructor.
 * Doc says functionRemoved() BEFORE it deletes the function: the editor is
 * then cut off -- nothing from the Doc or the Show Tree reaches it or its
 * children, its timers stop, it is disabled and hidden -- and waits, inert,
 * for its host to delete it.
 */
namespace FunctionEditorGuard
{
    /** Cut @a editor off (see cutOff()) when function @a fid leaves
        @a doc. An invalid @a fid watches nothing. */
    void watch(QWidget *editor, Doc *doc, quint32 fid);

    /** No Doc or Show Tree signal reaches @a editor or any of its children
        any more; their timers are stopped; it is disabled and hidden. */
    void cutOff(QWidget *editor, Doc *doc);

    /** cutOff() @a editor and delete it later (a host letting it go). */
    void retire(QWidget *editor, Doc *doc);
}

/** @} */

#endif
