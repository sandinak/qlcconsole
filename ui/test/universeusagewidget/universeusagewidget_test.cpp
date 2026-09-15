/*
  Q Light Controller Plus - Test Unit
  universeusagewidget_test.cpp

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

#include <QtTest>
#include <QLabel>

#include "universeusagewidget.h"
#include "inputoutputmap.h"
#include "doc.h"

#include "universeusagewidget_test.h"

void UniverseUsageWidget_Test::init()
{
    m_doc = new Doc(this);
}

void UniverseUsageWidget_Test::cleanup()
{
    delete m_doc;
}

void UniverseUsageWidget_Test::titleFollowsARename()
{
    InputOutputMap *iomap = m_doc->inputOutputMap();
    QVERIFY(iomap->addUniverse());
    const quint32 uni = 0;
    iomap->setUniverseName(int(uni), "Original Name");

    UniverseUsageWidget widget(m_doc, uni);
    QLabel *title = widget.findChild<QLabel *>("universeUsageTitle");
    QVERIFY2(title != nullptr, "the title label was not found");
    QVERIFY2(title->text().contains("Original Name"),
             "the title did not show the universe's name at construction");

    iomap->setUniverseName(int(uni), "Renamed Universe");

    QVERIFY2(title->text().contains("Renamed Universe"),
             "the title stayed on the old name after the universe was renamed");
    QVERIFY2(title->text().contains("Original Name") == false,
             "the title still carries the old name after the rename");
}

QTEST_MAIN(UniverseUsageWidget_Test)
