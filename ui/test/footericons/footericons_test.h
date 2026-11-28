/*
  Q Light Controller Plus
  footericons_test.h

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

#ifndef FOOTERICONS_TEST_H
#define FOOTERICONS_TEST_H

#include <QObject>

class App;

class FooterIcons_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /** Every chip has an icon (drawn, not blank) and an accessible name */
    void everyChipHasAnIconAndAName();
    /** View ▸ Footer lists every chip with its icon; a chip hides and shows,
     *  and the choice is saved for this computer */
    void viewFooterHidesAndShowsAChip();
    /** Reset Footer puts every chip back */
    void resetFooterShowsEverything();
    /** A safety chip, and the Student level, can't be hidden */
    void safetyChipsCantBeHidden();
    /** The Design/Operate, Advanced and Desk-when-fine chips are gone; the
     *  Student level and the not-patched desk warning stay */
    void theRemovedChipsAreGone();
    /** A squeezed chip shrinks to its icon before it goes; a chip that is
     *  gone takes its icon with it */
    void aSqueezedChipKeepsItsIconBeforeItGoes();

private:
    App *m_app = nullptr;
};

#endif
