/*
  Q Light Controller Plus
  firespreadperf_test.h

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

#ifndef FIRESPREADPERF_TEST_H
#define FIRESPREADPERF_TEST_H

#include <QObject>
#include <QHash>
#include <QByteArray>
#include <QSet>

class App;
class Doc;
class EffectInstance;

/* Fire Spread over a whole real rig (the stage show rigroomlight_test
 * draws: step rows, the wall washers on the step tops, the pixel bars),
 * as the Rig Overview draws it: what a tick of the effect costs, what
 * delivering its values costs, and what a frame of the 45° view costs
 * while the fire's front is spreading and once everything burns. */
class FireSpreadPerf_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** ms per tick of the effect and per frame of the view, reported */
    void costsWhileTheFireSpreads();

    /** The kept washes on the cyc draw the picture thrown afresh does */
    void theCycCatchesTheSameLight();

private:
    /** Runs the effect at @a ms and puts what it writes on the fixtures;
     *  returns the fixtures whose values changed */
    int tickTo(qint64 ms, double *tickMs = nullptr, double *applyMs = nullptr);

    App *m_app = nullptr;
    EffectInstance *m_fire = nullptr;
    QHash<int, QByteArray> m_universes;
    QSet<quint32> m_uplights;
    QList<quint32> m_lookFixtures;
    bool m_uplightsDark = false;
};

#endif
