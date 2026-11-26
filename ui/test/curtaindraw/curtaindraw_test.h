/*
  Q Light Controller Plus
  curtaindraw_test.h

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

#ifndef CURTAINDRAW_TEST_H
#define CURTAINDRAW_TEST_H

#include <QObject>
#include <QString>

class App;
class Chaser;

class CurtainDraw_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void aPickedCueDrawsTheCurtainAndHoldsTheStage();
    void hisCurtain();

private:
    /** Where the plot draws scenery @p sceneryId open (0 closed .. 1), -1
     *  when it draws none; @p what and the engine's own values logged. */
    double drawnOpen(quint32 sceneryId, quint32 fixtureId, const char *what);
    /** The Run's click on cue @p number: Load it, GO. */
    void loadAndGo(Chaser *c, const QString &number);
    void jump(Chaser *c, int step);
    void stopAll();
    int onWire(quint32 fixtureId, quint32 ch);
    bool markShown(quint32 sceneryId);
    /** Channel @p ch of the fixture goes to @p to with no jump (see .cpp); empty when it does */
    QString travels(quint32 fixtureId, quint32 ch, int to, int notBefore, int within);

    App *m_app = nullptr;
};

#endif
