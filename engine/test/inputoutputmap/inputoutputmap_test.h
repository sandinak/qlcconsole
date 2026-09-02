/*
  Q Light Controller Plus - Unit test
  inputoutputmap_test.h

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

#ifndef INPUTOUTPUTMAP_TEST_H
#define INPUTOUTPUTMAP_TEST_H

#include <QObject>

class Doc;
class InputOutputMap_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void initial();
    void pluginNames();
    void pluginInputs();
    void pluginOutputs();
    void configurePlugin();
    void inputPluginStatus();
    void outputPluginStatus();
    /** A patch naming a plugin line this machine doesn't have is reported,
        so the UI can say the rig won't output instead of failing silently. */
    void danglingOutputPatchDetected();
    /** A patch inside the plugin's real line count is not reported. */
    void validOutputPatchNotFlagged();
    /** A patch naming an interface identity (an ArtNet IP, say) that the
     *  plugin does not currently offer must not silently trust the numeric
     *  index alongside it -- that index may still be in range on THIS
     *  machine and point at a completely different real interface. It
     *  should go pending instead: reported, inert, mapping preserved. */
    void unresolvedInterfaceIdentityGoesPendingNotWrongIndex();
    /** "None" is OutputPatch::outputName()'s own sentinel for "no line was
     *  ever resolved", not a real interface identity -- it must not be
     *  mistaken for one and sent pending. */
    void noneSentinelDoesNotGoPending();

    void universeNames();
    void addUniverse();
    void removeUniverse();
    void universe();
    void profiles();
    void setInputPatch();
    void setOutputPatch();
    void setMultipleOutputPatches();
    void slotValueChanged();
    void slotConfigurationChanged();
    void loadInputProfiles();
    void inputSourceNames();
    void profileDirectories();
    void claimReleaseDumpReset();
    void blackout();
    void grandMaster();

    /* Fork: operator-supplied labels, hand-declared targets, and the input
       conflict query the connections tree warns from. */
    void targetAliases();
    void lineAliases();
    void manualTargets();
    void manualTargetPorts();
    void universesWithInputOn();
    void aliasesAndTargetsSurviveSaveLoad();
    void universeIdAlwaysEqualsItsArrayIndex();

private:
    Doc* m_doc;
};

#endif


