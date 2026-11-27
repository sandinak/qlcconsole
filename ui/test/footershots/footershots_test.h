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

#ifndef FOOTERSHOTS_TEST_H
#define FOOTERSHOTS_TEST_H

#include <QObject>

class FooterShots_Test : public QObject
{
    Q_OBJECT

private slots:
    /** The footer at 1024x640, 1440x900 and 1920x1080, in Design and in
     *  Operate, saved to QLC_SHOT_DIR when it is set (the UX review's
     *  before/after pictures); the footer is always whole inside the window */
    void theFooterAtThreeSizesInDesignAndOperate();
};

#endif
