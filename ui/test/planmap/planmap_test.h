/*
  Q Light Controller Plus
  planmap_test.h

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

#ifndef PLANMAP_TEST_H
#define PLANMAP_TEST_H

#include <QObject>

/* The plan mini-map for Fixture Properties: labels from CL and the plaster
   line in feet and metres, a drag that snaps (Shift: free), the locked
   state when the fixture hangs on a structure, setters that stay quiet. */
class PlanMap_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void labelsInFeet();
    void labelsInMetres();
    void onTheLinesSaysSo();
    void dragEmitsSnappedPosition();
    void shiftDragIsFree();
    void mountedIsNotEditableAndSaysWhy();
    void settersDoNotEmit();
    void picturesForReview();
};

#endif
