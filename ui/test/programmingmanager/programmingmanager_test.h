/*
  Q Light Controller Plus - Test Unit
  programmingmanager_test.h

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

#ifndef PROGRAMMINGMANAGER_TEST_H
#define PROGRAMMINGMANAGER_TEST_H

#include <QObject>

class Doc;

class ProgrammingManager_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** "Didn't change colour when colour was changed in program from white
     *  to blue." A palette isn't a Function, so editing one through the real
     *  look editor UI must explicitly tell the scenes that use it -- nothing
     *  does that for free. Drives the actual LookEditor color-change slot
     *  (not a manually-injected signal) and checks the owning scene's
     *  Doc::functionChanged actually fires. */
    void editingALookColourNotifiesItsScene();

private:
    Doc* m_doc;
};

#endif
