/*
  Q Light Controller Plus - Test Unit
  editorguard_test.h

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

#ifndef EDITORGUARD_TEST_H
#define EDITORGUARD_TEST_H

#include <QObject>

class Doc;
class Function;

/** M998: every function editor, and what else holds a Function*, outlives
    its function (deleted, or the show cleared) without calling into it. */
class EditorGuard_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void eachEditorLetsGoOfItsFunction_data();
    void eachEditorLetsGoOfItsFunction();
    void theFunctionsWindowClosesTheEditor();
    void aSequenceEditorLetsGoOfItsBoundScene();
    void theLookEditorForgetsItsScene();
    void theTimecodeDialogForgetsItsShow();

private:
    Function *make(int type);
    Doc *m_doc = nullptr;
};

#endif
