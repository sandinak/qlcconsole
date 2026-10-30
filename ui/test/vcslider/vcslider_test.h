/*
  Q Light Controller Plus - Unit test
  vcslider_test.h

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

#ifndef VCSLIDER_TEST_H
#define VCSLIDER_TEST_H

#include <QObject>
#include <QTemporaryDir>

class Doc;
class VCSlider_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /* Speed master mode */
    void speedMasterRate();
    void speedMasterTempo();
    void speedMasterFollowsTheMaster();
    void speedMasterSavesAndLoads();
    void speedMasterProperties();
    void speedDialSetsAMaster();

    /* Effect mode */
    void effectControlFollowsTheCurrentEffect();
    void effectControlTargets();
    void effectControlSavesAndLoads();
    void effectControlProperties();
    void effectControlOnAnEfx();

private:
    /** An effect palette playing @p script (in m_scripts). */
    quint32 effectPalette(const QString &script, const QString &name);
    /** A scene of fixture @p fixture playing palette @p palette, started. */
    quint32 play(quint32 palette, quint32 fixture);
    void tick(int times = 4);
    int dimmerAt(int address);

    Doc *m_doc = nullptr;
    QTemporaryDir m_scripts;
};

#endif
