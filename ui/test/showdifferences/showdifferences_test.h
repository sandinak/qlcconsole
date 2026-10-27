/*
  Q Light Controller Plus - Unit test
  showdifferences_test.h

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

#ifndef SHOWDIFFERENCES_TEST_H
#define SHOWDIFFERENCES_TEST_H

#include <QObject>

/* The footer's word on what a show carries, and Review…. Every "computer"
   is a temporary HOME; the real one is never read or written. */
class ShowDifferences_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void theFooterSaysWhatCameWithTheShow();
    void reviewSwitchesTheCompatibleAndKeepsTheRest();
    void theAppCarriesThemInAQlccOnly();
    /** RC8 u1: a show drawn from its own copy of a definition this computer
     *  has another of says so on the plot's tooltip and once as it opens,
     *  with Review…; after Use This Computer's, neither. Never switched by
     *  itself. */
    void anOlderEmbeddedDefinitionIsSaidOnThePlot();

private:
    QByteArray m_realHome;
    QByteArray m_realProfile;
};

#endif
