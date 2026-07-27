/*
  Q Light Controller Plus - Test Unit
  functionstreewidget_test.h

  Copyright (c) Massimo Callegari

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

#ifndef FUNCTIONSTREEWIDGET_TEST_H
#define FUNCTIONSTREEWIDGET_TEST_H

#include <QObject>

class Doc;

class FunctionsTreeWidget_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** The role-rank table orders Show < Chaser < Collection < RGBMatrix <
     *  EFX < Scene, and all below unknown/other types. */
    void typeSortRankTable();

    /** With type-order-by-role ON, top-level categories follow the role order. */
    void categoryOrderByRole();

    /** With it OFF, top-level categories stay classic A–Z. */
    void categoryOrderAlphabetical();

private:
    Doc *m_doc;
};

#endif // FUNCTIONSTREEWIDGET_TEST_H
