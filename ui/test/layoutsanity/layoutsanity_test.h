/*
  Q Light Controller Plus - Test Unit
  layoutsanity_test.h

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

#ifndef LAYOUTSANITY_TEST_H
#define LAYOUTSANITY_TEST_H

#include <QObject>
#include <QStringList>

class App;
class QWidget;

/* Every screen the app builds -- the tabs, the editors, the dialogs, the
   panels -- opened at its minimum, a laptop (1280x800) and a wide
   (1920x1080) size, offscreen, and checked for widgets laid over one
   another, cut off by their window, text cut short and controls squeezed
   to nothing (board Q207). One row per screen in screens(); see
   docs/dev/TEST_MAP.md ("Adding a screen to layoutsanity_test"). */
class LayoutSanity_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** The checker itself: a hand-made overlap, a cut-off button and a
     *  squeezed label are found; a stacked page, a scroll area's content
     *  and a marked overlay are not. */
    void theCheckerFindsWhatItShould();

    /** Every screen at every size: no overlaps, nothing clipped. */
    void everyScreenIsLaidOutCleanly_data();
    void everyScreenIsLaidOutCleanly();

private:
    App *m_app = nullptr;
    QStringList m_seenKnown;
};

#endif
