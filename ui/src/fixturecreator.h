/*
  Q Light Controller Plus
  fixturecreator.h

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

#ifndef FIXTURECREATOR_H
#define FIXTURECREATOR_H

#include <QCoreApplication>
#include <QList>

class QWidget;
class FixtureGroup;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The "add fixtures" flow shared by every place that can create them: pick a
 * definition in the AddFixture dialog, then create one fixture or a numbered run
 * of them, patched at once or staged in the inventory. Lives here rather than in
 * FixtureManager so the Studio's Inventory panel makes fixtures exactly the way
 * the Fixture Manager does -- same naming, same connection handling.
 */
class FixtureCreator
{
    Q_DECLARE_TR_FUNCTIONS(FixtureCreator)

public:
    /**
     * Run the dialog and create what it describes.
     *
     * @param inventory          stage the fixtures (no universe/address) instead of patching
     * @param preselectUniverse  universe to default the dialog to; ignored for inventory
     * @param addToGroup         group to put the new fixtures in, or NULL
     * @return the ids created; empty if the dialog was cancelled or refused
     */
    static QList<quint32> addFromDialog(QWidget *parent, Doc *doc, bool inventory,
                                        quint32 preselectUniverse, FixtureGroup *addToGroup);
};

/** @} */

#endif
