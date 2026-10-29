/*
  Q Light Controller Plus - Unit test
  stockqlccheck_test.cpp

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

#include <QBuffer>
#include <QtTest>

#include "stockqlccheck_test.h"
#include "stockqlccheck.h"

static QStringList lossesOf(const QByteArray &xml)
{
    QByteArray data = xml;
    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    return StockQlcCheck::losses(&buf);
}

/* What QLC+ 4.14 reads (Doc::loadXML, MonitorProperties, VCFrame): nothing
   to say. */
void StockQlcCheck_Test::aStockShowLosesNothing()
{
    const QByteArray xml =
        "<?xml version='1.0'?><!DOCTYPE Workspace><Workspace View='qlcconsole'>"
        "<Creator><Name>qlcconsole</Name></Creator>"
        "<Engine><InputOutputMap/><Fixture/><FixtureGroup/><ChannelsGroup/><Palette/>"
        "<Function Type='Scene'/><Bus/>"
        "<Monitor><Font/><ChannelStyle/><ValueStyle/><Grid/><FxItem/><StageItem/><MeshItem/></Monitor>"
        "</Engine>"
        "<VirtualConsole><Frame><Button/><Frame><Slider/><CueList/></Frame><SoloFrame><XYPad/></SoloFrame></Frame>"
        "<Properties/></VirtualConsole><SimpleDesk/><AppState/></Workspace>";
    QVERIFY(lossesOf(xml).isEmpty());
}

/* Each block QLC+ 4 reads past, said once, in the order met; anything new
   it doesn't know is "other qlcconsole settings". */
void StockQlcCheck_Test::eachForkBlockIsNamedOnce()
{
    const QByteArray xml =
        "<Workspace><Engine>"
        "<ShowControl MainCueList='3'/><ShowTree><Production/></ShowTree>"
        "<SpeedMasters/><PowerDistribution/><NetworkTopology/><Timecode Source='0'/>"
        "<Park/><Mark/><FocusCalls/><ControlMap><Trigger/></ControlMap><OutputRuleSettings/><Frobnicate/><Frobnicate/>"
        "<Monitor><Grid/><Truss/><Tower/><Scenery/></Monitor>"
        "</Engine>"
        "<VirtualConsole><Frame><Frame><CueRunner/></Frame><ShowControl/></Frame></VirtualConsole>"
        "</Workspace>";
    const QStringList got = lossesOf(xml);
    const QStringList want = {
        "the Show Tree (its cue lists, cues and hits)",
        "the speed masters",
        "the power plan",
        "the network map",
        "the timecode source",
        "parked and marked channels",
        "the focus calls",
        "the Control Map (MIDI and OSC triggers)",
        "when \"on when not in use\" lights come on",
        "other qlcconsole settings",
        "the Lighting Studio's rigging (trusses, towers, scenery)",
        "Cue Runner and Show Control widgets on the Virtual Console" };
    QCOMPARE(got, want);
}

/* Settings every show writes that QLC+ has no use for: not worth a word. */
void StockQlcCheck_Test::settingsQlcPlusDoesNotNeedAreQuiet()
{
    QVERIFY(lossesOf("<Workspace><Engine><ShutterAutoOpen Enabled='1'/>"
                     "<MoveInBlack Auto='1' DarkGapMs='800'/></Engine></Workspace>").isEmpty());
}

void StockQlcCheck_Test::notAWorkspaceIsNothing()
{
    QVERIFY(lossesOf("<FixtureList><ShowTree/></FixtureList>").isEmpty());
    QVERIFY(lossesOf("not xml at all").isEmpty());
    QVERIFY(StockQlcCheck::lossesInFile("/no/such/file.qxw").isEmpty());
    QVERIFY(StockQlcCheck::losses(nullptr).isEmpty());
}

QTEST_GUILESS_MAIN(StockQlcCheck_Test)
