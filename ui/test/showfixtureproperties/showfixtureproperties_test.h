/*
  Q Light Controller Plus
  showfixtureproperties_test.h

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

#ifndef SHOWFIXTUREPROPERTIES_TEST_H
#define SHOWFIXTUREPROPERTIES_TEST_H

#include <QObject>

class App;

class ShowFixtureProperties_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /** Fixture Properties opens for every fixture in every course show and
     *  test workspace, from a double-click in the Fixture Manager and from
     *  its Fixture Properties… entry: on the plot or not, staged, a house
     *  light, a generic dimmer, a definition that is missing. A fixture not
     *  on the plot has its position rows greyed. */
    void fixturePropertiesOpensForEveryFixture_data();
    void fixturePropertiesOpensForEveryFixture();

private:
    App *m_app = nullptr;
};

#endif
