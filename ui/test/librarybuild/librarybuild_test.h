/*
  Q Light Controller Plus - Test Unit
  librarybuild_test.h

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

#ifndef LIBRARYBUILD_TEST_H
#define LIBRARYBUILD_TEST_H

#include <QObject>
#include <QColor>
#include <QList>

class Doc;

/* W6 (docs/design/WORKFLOW_DESIGN.md): the Show Tree's Library lent to Build
   (one widget, one model), and the command bar in Build (the Operation
   Registry with a Build context). */
class LibraryBuild_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** The Library in Build is the Show Tree's own instance and model; it
     *  goes home when the Show Tree is shown; Recent is shared. */
    void theLibraryIsOneInstance();
    /** A Library palette and group dropped on the Workbench's layer row
     *  (and Return in the Library) apply to the scene; Undo takes it back. */
    void aDropInBuildAppliesAndUndoes();
    /** Towers colour Blue, dimmer 50, palette …: in the scene loaded, one
     *  undo step per line. */
    void commandWordsInBuild();
    /** cue, list, record, GO… answer "Show Tree only" and change nothing. */
    void showTreeOnlyWordsAreRefused();
    /** ? and help; the registry's questions; a line with nothing loaded. */
    void helpAndQuestions();
    /** Without a Show Tree view the bar and the Groups & fixtures pane
     *  still work. */
    void withoutAShowTreeView();

private:
    quint32 colour(const QColor &c, const QString &name);
    quint32 dimmer(int level, const QString &name);

    Doc *m_doc = nullptr;
    QList<quint32> m_fixtures;
    quint32 m_blue = 0, m_red = 0, m_half = 0;
    quint32 m_towers = 0, m_usts = 0;
};

#endif
