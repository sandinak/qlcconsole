/*
  Q Light Controller Plus - qlcconsole
  eosasciiparser_test.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include <QtTest>

#include "eosasciiparser.h"
#include "eosasciiparser_test.h"

namespace
{

// A small show in the Eos 3.x export dialect: the record shapes are an
// Eos export's own (see test-workspaces/eos-samples/), the content made up.
const char *kShow =
    "Ident 3:0\n"
    "Manufacturer ETC\n"
    "Console Eos\n"
    "$$Format 3.20\n"
    "$$Software Version 3.1.0 Build 94\n"
    "! a comment line\n"
    "$$Title Middle School Base\n"
    "Clear All\n"
    "Set Channels 5000\n"
    "$ParamType            1 1 Intens\n"
    "   $$ShortName            Intens\n"
    "$ParamType           12 3 Red\n"
    "$ParamType           13 3 Green\n"
    "$ParamType           14 3 Blue\n"
    "$ParamType            2 2 Pan\n"
    "$ParamType            3 2 Tilt\n"
    "\n"
    "$Personality 9001\n"
    "   $$Manuf ETC_Fixtures\n"
    "   $$Model ColorSource_SPOT_RGB\n"
    "   $$Dcid 3D0425CB-6E05-427C-A84E-121FACB6E20B\n"
    "   $$Footprint 3\n"
    "   $$PersChan    12     1     1     0   255\n"
    "    $$PersSlot     0   255   255     0.000000   100.000000 %\n"
    "   $$PersChan    13     1     2     0   255\n"
    "   $$PersChan    14     1     3     0   255\n"
    "$Personality 9002\n"
    "   $$Manuf Generic\n"
    "   $$Model Dimmer\n"
    "   $$Footprint 1\n"
    "   $$PersChan     1     1     1     0     0\n"
    "$Personality 9003\n"
    "   $$Manuf Acme\n"
    "   $$Model Mover_16bit\n"
    "   $$Footprint 5\n"
    "   $$PersChan     1     1     1     0     0\n"
    "   $$PersChan     2     2     2     3 32768\n"
    "   $$PersChan     3     2     4     5 32768\n"
    "\n"
    "$Patch 1 9002 1 0 1\n"
    "   $$Pers Dimmer\n"
    "   Text Relay 3rd Elect\n"
    "$Patch 51 9001 51 0 1\n"
    "   $$Pers ColorSource_SPOT_RGB\n"
    "   Text FOH\n"
    "$Patch 52 9001 513 0 1\n"
    "   Text FOH\n"
    "$Patch 60 9003 0 0 1\n"
    "   $$Data Augment3dFlags 4\n"
    "$Patch 61 9003 600 0 2\n"
    "\n"
    "$Group 1\n"
    "   Text FOH\n"
    "   $$ChanList     51    52\n"
    "$Group 2\n"
    "   Text Everything\n"
    "   1 Thru 3\n"
    "   51\n"
    "   51\n"
    "\n"
    "$IntensPalette 1\n"
    "   Text 10%\n"
    "   $$Param  51 1@26\n"
    "$ColorPalette 1\n"
    "   Text Red\n"
    "   $$Param  51 12@255 13@0 14@0\n"
    "   $$Param  52 12@255 13@0 14@0\n"
    "$Preset 3\n"
    "   Text Warm Front\n"
    "   $$Param  51 1@255 12@CP1\n"
    "\n"
    "$CueList 1\n"
    "   Text Main\n"
    "Cue 1 1\n"
    "   Text House Up\n"
    "   Up 5\n"
    "   $$TimeUp 5 0 1 1\n"
    "   Down 3\n"
    "   $$TimeDown 3 0 1 1\n"
    "   $$ChanMove  1@H80 51@FL\n"
    "   Chan  1@H80 51@Hff\n"
    "   $$Param  51 1@255 12@CP1 13@CP1 14@CP1\n"
    "Cue 0.5 1\n"
    "   Text Preset\n"
    "   Up 1:30\n"
    "Cue 2 1\n"
    "   Text Go Dark  ! not a comment in a label? it is: labels keep everything\n"
    "   Up 2 1\n"
    "   $$ChanMove  1@0 51@50\n"
    "   $$Param  51 12@r0\n"
    "   FollowOn 4\n"
    "   $$Follow 4\n"
    "   Link 1\n"
    "$Cue 1.25 1\n"
    "   $$Hang 2\n"
    "$CueList 2\n"
    "   Text Backups\n"
    "Cue 5 2\n"
    "   Up 1\n"
    "   $$Param  52 1@Q3\n"
    "\n"
    "Sub 1\n"
    "   Text Worklights\n"
    "   Up 2\n"
    "   Chan 1@FL\n"
    "EndData\n"
    "Cue 99 1\n";

EosShowData parse(const QString &text)
{
    EosShowData d;
    EosAsciiParser p;
    const bool ok = p.parseText(text, d);
    if (!ok)
        qWarning() << d.warnings;
    return d;
}

} // namespace

void EosAsciiParser_Test::levels()
{
    EosLevel l = EosAsciiParser::parseLevel("H80", true);
    QCOMPARE(l.kind, EosLevel::Value);
    QCOMPARE(l.value, 128);
    QCOMPARE(EosAsciiParser::parseLevel("Hff", true).value, 255);
    QCOMPARE(EosAsciiParser::parseLevel("FL", true).value, 255);
    QCOMPARE(EosAsciiParser::parseLevel("50", true).value, 128);    // a percentage
    QCOMPARE(EosAsciiParser::parseLevel("200", false).value, 200);  // a raw level
    QCOMPARE(EosAsciiParser::parseLevel("40000", false).value, 40000);
    QCOMPARE(EosAsciiParser::parseLevel("b127", false).value, 127); // blocked: still the level

    l = EosAsciiParser::parseLevel("CP10", false);
    QCOMPARE(l.kind, EosLevel::PaletteRef);
    QCOMPARE(l.refKind, QChar('C'));
    QCOMPARE(l.ref, QString("10"));
    l = EosAsciiParser::parseLevel("kPR2", false);
    QCOMPARE(l.kind, EosLevel::PresetRef);
    QCOMPARE(l.ref, QString("2"));
    QCOMPARE(EosAsciiParser::parseLevel("r0", false).kind, EosLevel::Release);
    QCOMPARE(EosAsciiParser::parseLevel("n0", false).kind, EosLevel::Release);
    QCOMPARE(EosAsciiParser::parseLevel("G4#200", false).value, 200);
    QCOMPARE(EosAsciiParser::parseLevel(".GR33#160", false).value, 160);   // a Rosco gel match
    QCOMPARE(EosAsciiParser::parseLevel("Q5", false).kind, EosLevel::Unsupported);
}

void EosAsciiParser_Test::times()
{
    QCOMPARE(EosAsciiParser::parseTime("5"), 5.0);
    QCOMPARE(EosAsciiParser::parseTime("2.5"), 2.5);
    QCOMPARE(EosAsciiParser::parseTime("1:30"), 90.0);
    QCOMPARE(EosAsciiParser::parseTime("M"), -1.0);
    QCOMPARE(EosAsciiParser::parseTime(""), -1.0);
}

void EosAsciiParser_Test::channelLists()
{
    QCOMPARE(EosAsciiParser::parseChannelList(QStringList() << "1" << "Thru" << "3" << "8" << "10>12"),
             QList<int>() << 1 << 2 << 3 << 8 << 10 << 11 << 12);
}

void EosAsciiParser_Test::header()
{
    const EosShowData d = parse(kShow);
    QCOMPARE(d.manufacturer, QString("ETC"));
    QCOMPARE(d.console, QString("Eos"));
    QCOMPARE(d.format, QString("3.20"));
    QCOMPARE(d.title, QString("Middle School Base"));
    QCOMPARE(d.paramTypes.count(), 6);
    QCOMPARE(d.paramName(12), QString("Red"));
    QCOMPARE(d.paramName(999), QString("Param 999"));
}

void EosAsciiParser_Test::personalityAndPatch()
{
    const EosShowData d = parse(kShow);
    QCOMPARE(d.personalities.count(), 3);
    const EosPersonality &cs = d.personalities.value(9001);
    QCOMPARE(cs.manufacturer, QString("ETC_Fixtures"));
    QCOMPARE(cs.model, QString("ColorSource_SPOT_RGB"));
    QCOMPARE(cs.footprint, 3);
    QCOMPARE(cs.channels.count(), 3);
    QVERIFY(cs.channel(1) == nullptr);             // virtual intensity
    QCOMPARE(cs.channel(13)->offset, 2);
    const EosPersonality &mover = d.personalities.value(9003);
    QCOMPARE(mover.channel(2)->size, 2);
    QCOMPARE(mover.channel(2)->lsbOffset, 3);
    QCOMPARE(mover.channel(3)->home, 32768);

    QCOMPARE(d.patch.count(), 5);
    const EosPatch *p1 = d.findPatch(1);
    QCOMPARE(p1->personality, 9002);
    QCOMPARE(p1->address, 1);
    QCOMPARE(p1->label, QString("Relay 3rd Elect"));
    QCOMPARE(p1->personalityName, QString("Dimmer"));
    QCOMPARE(d.findPatch(52)->address, 513);       // universe 2, address 1
    QCOMPARE(d.findPatch(60)->address, 0);         // unpatched
    QCOMPARE(d.findPatch(61)->part, 2);
}

void EosAsciiParser_Test::conventionalPatch()
{
    const EosShowData d = parse("Ident 3:0\nPatch 1 1<1@FL 2<12@FL 10<100\nEndData\n");
    QCOMPARE(d.patch.count(), 3);
    QCOMPARE(d.findPatch(2)->address, 12);
    QCOMPARE(d.findPatch(10)->address, 100);
    QCOMPARE(d.findPatch(10)->personality, 0);
}

void EosAsciiParser_Test::groups()
{
    const EosShowData d = parse(kShow);
    QCOMPARE(d.groups.count(), 2);
    QCOMPARE(d.groups.at(0).label, QString("FOH"));
    QCOMPARE(d.groups.at(0).channels, QList<int>() << 51 << 52);
    QCOMPARE(d.groups.at(1).channels, QList<int>() << 1 << 2 << 3 << 51);   // duplicates go
}

void EosAsciiParser_Test::cues()
{
    const EosShowData d = parse(kShow);
    QCOMPARE(d.cueLists.count(), 2);
    QCOMPARE(d.cueLists.value(1).label, QString("Main"));
    const QList<EosCue> cues = d.cueLists.value(1).cues;
    // In number order, whatever the file's order; EndData ends the file.
    QCOMPARE(cues.count(), 4);
    QCOMPARE(cues.at(0).number, QString("0.5"));
    QCOMPARE(cues.at(1).number, QString("1"));
    QCOMPARE(cues.at(2).number, QString("1.25"));
    QCOMPARE(cues.at(3).number, QString("2"));

    QCOMPARE(cues.at(0).up, 90.0);
    const EosCue &c1 = cues.at(1);
    QCOMPARE(c1.label, QString("House Up"));
    QCOMPARE(c1.up, 5.0);
    QCOMPARE(c1.down, 3.0);
    QCOMPARE(c1.moves.value(1).value(1).value, 128);
    QCOMPARE(c1.moves.value(51).value(1).value, 255);       // $$Param after $$ChanMove
    QCOMPARE(c1.moves.value(51).value(12).kind, EosLevel::PaletteRef);
    QVERIFY(c1.hasTracked);
    QCOMPARE(c1.tracked.value(51), 255);

    QCOMPARE(cues.at(2).hang, 2.0);
    const EosCue &c2 = cues.at(3);
    QVERIFY(c2.label.startsWith("Go Dark"));
    QCOMPARE(c2.up, 2.0);
    QCOMPARE(c2.upDelay, 1.0);
    QCOMPARE(c2.follow, 4.0);
    QCOMPARE(c2.link, QString("1"));
    QCOMPARE(c2.moves.value(51).value(1).value, 128);
    QCOMPARE(c2.moves.value(51).value(12).kind, EosLevel::Release);
    QVERIFY(!c2.hasTracked);

    QCOMPARE(d.cueLists.value(2).cues.count(), 1);
    QCOMPARE(d.cueLists.value(2).cues.at(0).list, 2);
    QVERIFY(d.warnings.join("\n").contains("Q3"));
    QCOMPARE(d.cueCount(), 5);
}

void EosAsciiParser_Test::cueParts()
{
    const EosShowData d = parse("Ident 3:0\n$CueList 1\n"
                                "Cue 5 1\n Text Sunrise\n Up 3\n $$ChanMove 1@FL\n"
                                "Cue 5/2 1\n Text Part two\n Up 8\n $$ChanMove 2@50\n"
                                "Cue 6 1\n Up 1\n Part 2\n $$ChanMove 3@FL\n");
    const QList<EosCue> cues = d.cueLists.value(1).cues;
    QCOMPARE(cues.count(), 2);
    QCOMPARE(cues.at(0).parts, 2);
    QCOMPARE(cues.at(0).label, QString("Sunrise"));
    QCOMPARE(cues.at(0).up, 3.0);                   // the first part's time
    QCOMPARE(cues.at(0).moves.keys(), QList<int>() << 1 << 2);
    QCOMPARE(cues.at(1).parts, 2);
    QCOMPARE(cues.at(1).moves.keys(), QList<int>() << 3);
    QVERIFY(d.warnings.join("\n").contains("parts flattened"));
}

void EosAsciiParser_Test::palettesAndPresets()
{
    const EosShowData d = parse(kShow);
    QCOMPARE(d.palettes.count(), 2);
    const EosPalette *ip = d.findPalette('I', "1");
    QVERIFY(ip != nullptr);
    QCOMPARE(ip->label, QString("10%"));
    QCOMPARE(ip->levels.value(51).value(1).value, 26);
    const EosPalette *cp = d.findPalette('C', "1");
    QCOMPARE(cp->levels.keys(), QList<int>() << 51 << 52);
    QCOMPARE(cp->levels.value(52).value(12).value, 255);
    QVERIFY(d.findPalette('F', "1") == nullptr);
    QCOMPARE(d.presets.count(), 1);
    QCOMPARE(d.findPalette('P', "3")->label, QString("Warm Front"));
}

void EosAsciiParser_Test::subs()
{
    const EosShowData d = parse(kShow);
    QCOMPARE(d.subs.count(), 1);
    QCOMPARE(d.subs.at(0).label, QString("Worklights"));
    QCOMPARE(d.subs.at(0).up, 2.0);
    QCOMPARE(d.subs.at(0).levels.value(1).value(1).value, 255);
}

void EosAsciiParser_Test::unknownRecordsSkipped()
{
    const EosShowData d = parse("Ident 3:0\n$Effect 901\n Text Circle\n $$Type 1\n"
                                "$Effect 902\n $$Time 1.5\n$MagicSheet 1\n Text Plot\n"
                                "$SomethingNew 4\n $$Whatever 1 2 3\n"
                                "$Group 1\n Text Still read\n 1\n");
    QCOMPARE(d.skipped.value("$Effect"), 2);
    QCOMPARE(d.skipped.value("$MagicSheet"), 1);
    QCOMPARE(d.skipped.value("$SomethingNew"), 1);
    QCOMPARE(d.groups.count(), 1);
    QCOMPARE(d.groups.at(0).label, QString("Still read"));
}

void EosAsciiParser_Test::notAscii()
{
    EosShowData d;
    EosAsciiParser p;
    QVERIFY(!p.parseText("PK\x03\x04 garbage\nmore garbage\n", d));
    QVERIFY(!d.warnings.isEmpty());
    EosShowData d2;
    QVERIFY(!p.parse("/nonexistent/show.asc", d2));
}

void EosAsciiParser_Test::realExport()
{
    // A real Eos 3.3.6 export (see test-workspaces/eos-samples/NOTICE).
    EosShowData d;
    EosAsciiParser p;
    QVERIFY(p.parse(QString(EOS_SAMPLES_DIR) + "otbpa-eos-3.3.6.asc", d));
    QCOMPARE(d.title, QString("OTBPA DRESS TUES"));
    QCOMPARE(d.personalities.count(), 5);
    QCOMPARE(d.personalities.value(23759).model, QString("ColorSource_SPOT_Jr_DB_5ch"));
    QCOMPARE(d.patch.count(), 44);
    QCOMPARE(d.findPatch(1)->address, 210);
    QCOMPARE(d.groups.count(), 5);
    QCOMPARE(d.groups.at(0).channels, QList<int>() << 1 << 2 << 3 << 4);
    QCOMPARE(d.cueLists.count(), 1);
    QCOMPARE(d.cueLists.value(1).label, QString("SETUP"));
    QCOMPARE(d.cueLists.value(1).cues.count(), 66);
    QCOMPARE(d.cueLists.value(1).cues.first().number, QString("0.5"));
    QCOMPARE(d.findPalette('C', "10")->label, QString("sky blue"));
    QCOMPARE(d.skipped.value("$Effect"), 18);
    QCOMPARE(d.skipped.value("$Curve"), 11);
    // Every cue has a time; the follows come through.
    int follows = 0;
    for (const EosCue &c : d.cueLists.value(1).cues)
    {
        QVERIFY2(c.up >= 0, qPrintable(c.number));
        if (c.follow >= 0)
            ++follows;
    }
    QCOMPARE(follows, 2);
}

QTEST_APPLESS_MAIN(EosAsciiParser_Test)
