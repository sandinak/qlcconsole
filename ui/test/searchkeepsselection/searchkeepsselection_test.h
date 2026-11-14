/*
  Q Light Controller Plus - Test Unit
  searchkeepsselection_test.h

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

#ifndef SEARCHKEEPSSELECTION_TEST_H
#define SEARCHKEEPSSELECTION_TEST_H

#include <QObject>

class Doc;

/** Clearing (or changing) a tree's search keeps the person on the row they
 *  found: still selected and current, its parents open, its row in view. */
class SearchKeepsSelection_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void fixtureTreeStaysOnTheHitWhenCleared();
    void fixtureTreeStaysWhileTheSearchChanges();
    void fixtureTreeKeepsAMultiSelection();
    void buildGroupsTreeStaysOnTheHitWhenCleared();
    void functionsTreeStaysOnTheHitWhenCleared();
    void outlineStaysOnTheHitWhenCleared();
    void showTreeLibraryStaysOnTheHitWhenCleared();

private:
    Doc *m_doc = nullptr;
};

#endif
