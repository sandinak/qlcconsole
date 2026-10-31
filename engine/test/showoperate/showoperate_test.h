/*
  Q Light Controller Plus
  showoperate_test.h

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


#ifndef SHOWOPERATE_TEST_H
#define SHOWOPERATE_TEST_H

#include <QObject>

class Doc;

class ShowOperate_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** 4-8 digits; a hash verifies its PIN and no other, salted afresh */
    void aPinIsHashedAndChecked();
    /** A malformed or foreign stored hash never opens the lock */
    void aBadHashOpensNothing();
    /** Set, change and remove; an invalid PIN changes nothing */
    void setAndRemoveThePin();
    /** Saved only when set, read back, and reset with the show */
    void savedWithTheShowOnlyWhenSet();
    void remoteGoIsOffAndSavedOnlyWhenOn();

private:
    Doc *m_doc = nullptr;
};

#endif
