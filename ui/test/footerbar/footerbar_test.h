/*
  Q Light Controller Plus - qlcconsole
  footerbar_test.h

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

#ifndef FOOTERBAR_TEST_H
#define FOOTERBAR_TEST_H

#include <QObject>

/* The footer's message (bench 2026-11-27: two messages drawn on top of
   each other, a message under a chip's words). */
class FooterBar_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Two messages one after the other: one label, the second's text. */
    void twoMessagesShowOneAtATime();
    /** The message stops short of the permanent chips and is drawn under none. */
    void theMessageIsNeverUnderAPermanentChip();
    /** A left-hand chip shown again while a message stands stays under the
     *  message's own label (it is not drawn over the words). */
    void aChipShownDuringAMessageIsNotDrawnOverIt();
    /** A long message is cut short with an ellipsis, the whole in its tooltip. */
    void aLongMessageIsElided();
};

#endif
