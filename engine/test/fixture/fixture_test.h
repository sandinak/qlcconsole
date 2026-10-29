/*
  Q Light Controller - Unit test
  fixture_test.h

  Copyright (c) Heikki Junnila

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

#ifndef FIXTURE_TEST_H
#define FIXTURE_TEST_H

#include <QObject>

class Doc;

class Fixture_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** A definition not on this computer: the fixture stands in as a dimmer
     *  but saves as what it is; and the show cache keeps definitions here. */
    void aMissingDefinitionIsKeptNotLostOnSave();
    void theShowCacheKeepsDefinitionsOnThisComputer();

    void id();
    void name();
    void address();
    /** setAddress() keeps every bit of the universe: re-addressing a
     *  fixture on universe 128+ doesn't move it 128 down (pb1). */
    void addressKeepsHighUniverses();
    void lessThan();
    void type();
    void dimmer();
    void rgbPanel();
    void rgbPanel16bit();
    void fixtureDef();
    void channels();
    void degrees();
    void indexedPosition();
    void heads();
    void loadWrongRoot();
    void loadFixtureDef();
    void loadFixtureDefWrongChannels();
    void loadDimmer();
    void loadWrongAddress();
    void loadWrongUniverse();
    void loadMoreChannelsThanAUniverse();
    void loadWrongID();
    void loader();
    void save();
    void stageLookDefaultAndSet();
    void stageLookKeys();
    void stageLookSaveLoad();
    void stageLookCurtainSaveLoad();
    void stageLookUnknownKey();
    void stageLookAuto();
    void stageLookIcon();
    //void status();

private:
    Doc* m_doc;
};

#endif
