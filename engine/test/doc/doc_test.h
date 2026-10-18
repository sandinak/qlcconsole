/*
  Q Light Controller Plus - Unit test
  doc_test.h

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#ifndef DOC_TEST_H
#define DOC_TEST_H

#include <QObject>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

class Doc;
class Doc_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void normalizeComponentPath();
    void denormalizeComponentPath();
    void fixtureNamesAreUnique();

    void defaults();
    void mode();

    void createFixtureId();
    void addFixture();
    /** A fixture past the most universes a show can have is refused, said,
     *  not added with no universe under it (pb2: an RGB matrix on one
     *  crashed). Its last channel counts, for one running over the end. */
    void addFixturePastUniverseLimit();
    /** A patched fixture moved past the limit goes back where it was. */
    void moveFixturePastUniverseLimit();
    /** Re-addressing a patched fixture on universe 128+ keeps it there (pb1). */
    void readdressHighUniverseFixture();
    void deleteFixture();
    void movedFixtureChannels();
    void deletedFixtureChannels();
    void renamedFixtureKeepsValues();
    void movedFixtureBeyondUniverse();
    void movedFixtureOverFixtureInBetween();
    void replaceFixtures();
    void fixture();
    void totalPowerConsumption();

    void addFixtureGroup();
    void removeFixtureGroup();

    void channelGroups();
    void palettes();

    void monitorProperties();

    void addFunction();
    void deleteFunction();
    void function();
    void usage();

    void load();
    void loadWrongRoot();
    /** A file cut short loads what came before the cut, and says so:
     *  loadXML() is false and the error log names the line (F2). */
    void loadTruncated();
    /** A fixture on a universe the show can't have (past the limit,
     *  negative, not a number) loads unpatched, said, at once -- not a
     *  hang adding universes up to it, nor an abort (F1). */
    void loadUniverseOutOfRange_data();
    void loadUniverseOutOfRange();
    void save();

private:
    void createFixtureNode(QXmlStreamWriter &doc, quint32 id, quint32 address, quint32 channels,
                           const QString &universe = QStringLiteral("3"));
    void createFixtureGroupNode(QXmlStreamWriter &doc, quint32 id);
    void createCollectionNode(QXmlStreamWriter &doc, quint32 id);
    void createBusNode(QXmlStreamWriter &doc, quint32 id, quint32 value);

private:
    Doc* m_doc;
    int m_currentAddr;
};

#endif
