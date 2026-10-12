/*
  Q Light Controller Plus - Test Unit
  universeusagewidget_test.h

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

#ifndef UNIVERSEUSAGEWIDGET_TEST_H
#define UNIVERSEUSAGEWIDGET_TEST_H

#include <QObject>

class Doc;

class UniverseUsageWidget_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** The title label is a one-shot snapshot taken at construction; a
     *  rename after that must still reach it (see the widget's own comment
     *  on why Universe::nameChanged() needed wiring up by hand). */
    void titleFollowsARename();

private:
    Doc* m_doc;
};

#endif
