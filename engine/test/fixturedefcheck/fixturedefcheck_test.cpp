/*
  Q Light Controller Plus
  fixturedefcheck_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>

#include "fixturedefcheck_test.h"
#include "fixturedefcheck.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "../common/resource_paths.h"

using namespace FixtureDefCheck;

namespace
{
    QByteArray readFile(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    const QByteArray goodXml(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE FixtureDefinition>\n"
        "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
        " <Manufacturer>Test</Manufacturer>\n"
        " <Model>Par</Model>\n"
        " <Type>Color Changer</Type>\n"
        " <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n"
        " <Mode Name=\"1ch\">\n"
        "  <Channel Number=\"0\">Dimmer</Channel>\n"
        " </Mode>\n"
        "</FixtureDefinition>\n");

    bool hasIssue(const QList<Issue> &issues, Issue::Level level, const QString &fragment)
    {
        foreach (const Issue &i, issues)
            if (i.level == level && i.message.contains(fragment))
                return true;
        return false;
    }
}

/* The PHS Crew step definition as it sits in the user library: every
   self-closing channel typed "\>". Diagnosed at the first one; one repair
   fixes the lot and the result loads. */
void FixtureDefCheck_Test::aBackslashTypedForASlashIsFoundAndRepaired()
{
    const QByteArray xml = readFile(QString(FIXTUREDEFCHECK_DATA) + "backslash-step.qxf");
    QVERIFY(!xml.isEmpty());

    ParseProblem p;
    QVERIFY(diagnose(xml, &p));
    QCOMPARE(p.line, 12);
    QVERIFY(!p.message.isEmpty());

    const QList<Repair> fixes = repairs(xml);
    QVERIFY(!fixes.isEmpty());
    const Repair &best = fixes.first();
    QVERIFY2(best.description.contains("\\>"), qPrintable(best.description));
    QCOMPARE(best.line, 12);
    QVERIFY(best.before.endsWith("\\>"));
    QVERIFY(best.after.endsWith("/>"));

    // Past that, the next thing wrong with it: 192 channels and no mode.
    QVERIFY(!best.complete);
    QVERIFY(diagnose(best.fixed, &p));
    QVERIFY2(p.message.contains("no modes"), qPrintable(p.message));
    const QList<Repair> next = repairs(best.fixed);
    QVERIFY(!next.isEmpty());
    QVERIFY2(next.first().description.contains("192 channels") && next.first().description.contains("64 heads"),
             qPrintable(next.first().description));
    QVERIFY(next.first().complete);

    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(next.first().fixed));
    QCOMPARE(def.manufacturer(), QString("PHS Crew"));
    QCOMPARE(def.modes().size(), 1);
    QCOMPARE(def.modes().first()->channels().size(), 192);
    QCOMPARE(def.modes().first()->heads().size(), 64);
}

void FixtureDefCheck_Test::aMismatchedClosingTagIsClosedRight()
{
    QByteArray xml = goodXml;
    xml.replace(" </Mode>", " </Mod>");
    ParseProblem p;
    QVERIFY(diagnose(xml, &p));
    const QList<Repair> fixes = repairs(xml);
    QVERIFY(!fixes.isEmpty());
    QVERIFY2(fixes.first().complete, qPrintable(fixes.first().description));
    QVERIFY2(fixes.first().description.contains("<Mode>"), qPrintable(fixes.first().description));
    QCOMPARE(fixes.first().after, QString("</Mode>"));
}

void FixtureDefCheck_Test::aBareAmpersandIsEscaped()
{
    QByteArray xml = goodXml;
    xml.replace("<Model>Par</Model>", "<Model>Par & Wash</Model>");
    QVERIFY(diagnose(xml, nullptr));
    const QList<Repair> fixes = repairs(xml);
    QVERIFY(!fixes.isEmpty());
    QVERIFY(fixes.first().complete);
    QVERIFY(fixes.first().after.contains("&amp;"));
    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(fixes.first().fixed));
    QCOMPARE(def.model(), QString("Par & Wash"));
}

void FixtureDefCheck_Test::aMissingDoctypeIsAdded()
{
    QByteArray xml = goodXml;
    xml.replace("<!DOCTYPE FixtureDefinition>\n", "");
    ParseProblem p;
    QVERIFY(diagnose(xml, &p));
    QVERIFY(p.message.contains("DOCTYPE"));
    const QList<Repair> fixes = repairs(xml);
    QVERIFY(!fixes.isEmpty());
    QVERIFY(fixes.first().complete);
}

void FixtureDefCheck_Test::aGoodFileHasNothingToDiagnose()
{
    QVERIFY(!diagnose(goodXml, nullptr));
    QVERIFY(repairs(goodXml).isEmpty());
    // ...and a real one from the library.
    QVERIFY(!diagnose(readFile(QString("%1/Futurelight/Futurelight-CY-200.qxf").arg(INTERNAL_FIXTUREDIR)), nullptr));
}

void FixtureDefCheck_Test::clearlyWrongDataIsAnError()
{
    QLCFixtureDef def;
    def.setManufacturer("Test");
    def.setModel("Broken");
    QLCChannel *a = new QLCChannel();
    a->setName("Strobe");
    a->setGroup(QLCChannel::Shutter);
    a->addCapability(new QLCCapability(0, 100, "Open"));
    def.addChannel(a);
    QLCChannel *twin = new QLCChannel();
    twin->setName("Spare");
    def.addChannel(twin);
    twin->setName("Strobe");                                  // two with one name
    QLCFixtureMode *mode = new QLCFixtureMode(&def);
    mode->setName("1ch");
    mode->insertChannel(a, 0);
    QLCFixtureHead head;
    head.addChannel(0);
    head.addChannel(5);                                       // not in the mode
    mode->insertHead(-1, head);
    def.addMode(mode);
    QLCFixtureMode *empty = new QLCFixtureMode(&def);
    empty->setName("Empty");
    def.addMode(empty);

    const QList<Issue> issues = check(&def);
    QVERIFY(hasIssue(issues, Issue::Error, "Two channels are called"));
    a->addCapability(new QLCCapability(101, 255, QString()));
    QVERIFY(hasIssue(check(&def), Issue::Warning, "1 of its capabilities have no description"));
    QVERIFY(hasIssue(issues, Issue::Error, "uses channel 6"));
    QVERIFY(hasIssue(issues, Issue::Error, "Mode \"Empty\" has no channels"));
    QCOMPARE(issues.first().level, Issue::Error);             // errors first
}

void FixtureDefCheck_Test::missingDataIsAWarning()
{
    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(goodXml));
    QList<Issue> issues = check(&def);
    QVERIFY(!hasIssue(issues, Issue::Error, QString()));
    QVERIFY(hasIssue(issues, Issue::Warning, "No size"));
    QVERIFY(hasIssue(issues, Issue::Warning, "No light output"));
    QVERIFY(hasIssue(issues, Issue::Warning, "No beam angle"));
    QVERIFY(hasIssue(issues, Issue::Warning, "1 of its 256 values") == false);
    QVERIFY(hasIssue(issues, Issue::Warning, "capabilities have no description") == false);

    // Said, and they go.
    QLCPhysical ph = def.physical();
    ph.setWidth(200); ph.setHeight(200); ph.setDepth(150);
    ph.setBulbLumens(1500);
    ph.setLensDegreesMin(25); ph.setLensDegreesMax(25);
    def.setPhysical(ph);
    issues = check(&def);
    QVERIFY(!hasIssue(issues, Issue::Warning, "No size"));
    QVERIFY(!hasIssue(issues, Issue::Warning, "No light output"));
    QVERIFY(!hasIssue(issues, Issue::Warning, "No beam angle"));
}

/* Each a mistake a person or an old editor makes, each repaired in one go
   by a guess at what was meant -- and the result loads. */
void FixtureDefCheck_Test::educatedGuessesAtTextMistakes_data()
{
    QTest::addColumn<QByteArray>("xml");
    QTest::addColumn<QString>("fix");

    QByteArray curly = goodXml;
    curly.replace("<Mode Name=\"1ch\">", "<Mode Name=\xe2\x80\x9c" "1ch\xe2\x80\x9d>");
    QTest::newRow("curly quotes") << curly << "curly quotes";

    QByteArray latin = goodXml;
    latin.replace("<Model>Par</Model>", "<Model>Par Caf\xe9</Model>");
    QTest::newRow("latin-1") << latin << "Latin-1";

    QTest::newRow("junk before") << (QByteArray("garbage\n") + goodXml) << "before the first tag";
    QTest::newRow("junk after") << (goodXml + "trailing junk\n") << "after </FixtureDefinition>";

    QByteArray lower = goodXml;
    lower.replace("<!DOCTYPE FixtureDefinition>", "<!DOCTYPE fixturedefinition>");
    QTest::newRow("doctype case") << lower << "Spell FixtureDefinition";

    QByteArray lt = goodXml;
    lt.replace("<Model>Par</Model>", "<Model>Par < Wash</Model>");
    QTest::newRow("bare <") << lt << "&lt;";

    QByteArray quote = goodXml;
    quote.replace("<Mode Name=\"1ch\">", "<Mode Name=\"1ch>");
    QTest::newRow("open quote") << quote << "quote left open";
}

void FixtureDefCheck_Test::educatedGuessesAtTextMistakes()
{
    QFETCH(QByteArray, xml);
    QFETCH(QString, fix);
    QVERIFY(diagnose(xml, nullptr));
    const QList<Repair> fixes = repairs(xml);
    QStringList offered;
    const Repair *chosen = nullptr;
    foreach (const Repair &r, fixes)
    {
        offered << r.description;
        if (r.description.contains(fix) && chosen == nullptr)
            chosen = &r;
    }
    QVERIFY2(chosen != nullptr, qPrintable(offered.join(" | ")));
    QVERIFY2(chosen->complete, qPrintable(chosen->description));
    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(chosen->fixed));
}

/* It loads -- but the mode would quietly lose a channel it names wrongly.
   Caught, and the likely meaning offered first. */
void FixtureDefCheck_Test::aModeNamingAMissingChannelIsCaughtAndGuessed()
{
    QByteArray xml = goodXml;
    xml.replace("<Channel Number=\"0\">Dimmer</Channel>", "<Channel Number=\"0\">dimmer </Channel>");
    ParseProblem p;
    QVERIFY(diagnose(xml, &p));
    QVERIFY(p.loads);
    QVERIFY2(p.message.contains("does not have"), qPrintable(p.message));
    QCOMPARE(p.line, 9);

    QList<Repair> fixes = repairs(xml);
    QVERIFY(!fixes.isEmpty());
    QVERIFY2(fixes.first().description.contains("as \"Dimmer\""), qPrintable(fixes.first().description));
    QVERIFY(fixes.first().complete);

    // Nothing like it: add it instead.
    QByteArray other = goodXml;
    other.replace("<Channel Number=\"0\">Dimmer</Channel>",
                  "<Channel Number=\"0\">Dimmer</Channel>\n  <Channel Number=\"1\">Strobe</Channel>");
    fixes = repairs(other);
    QVERIFY(!fixes.isEmpty());
    QVERIFY2(fixes.first().description.contains("Add the missing channel \"Strobe\""),
             qPrintable(fixes.first().description));
    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(fixes.first().fixed));
    QCOMPARE(def.modes().first()->channels().size(), 2);
}

void FixtureDefCheck_Test::unnamedAndDuplicateNamesAreFixed()
{
    QByteArray unnamed = goodXml;
    unnamed.replace("<Mode Name=\"1ch\">", "<Mode>");
    ParseProblem p;
    QVERIFY(diagnose(unnamed, &p));
    QVERIFY(p.message.contains("mode has no name"));
    QList<Repair> fixes = repairs(unnamed);
    QVERIFY(!fixes.isEmpty());
    QVERIFY(fixes.first().description.contains("Name the modes"));
    QVERIFY(fixes.first().complete);

    QByteArray dup = goodXml;
    dup.replace(" <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n",
                " <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n"
                " <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n");
    QVERIFY(diagnose(dup, &p));
    QVERIFY(p.loads);
    fixes = repairs(dup);
    QVERIFY(!fixes.isEmpty());
    QVERIFY(fixes.first().description.contains("Number the channels"));
    QVERIFY(fixes.first().after.contains("Dimmer 2"));
}

void FixtureDefCheck_Test::theBriefSaysItInAFewWords()
{
    ParseProblem p;
    QVERIFY(diagnose(readFile(QString(FIXTUREDEFCHECK_DATA) + "backslash-step.qxf"), &p));
    QCOMPARE(p.brief(), QString("line 12: expected '>' or '/'"));
    QByteArray noMode = readFile(QString(FIXTUREDEFCHECK_DATA) + "backslash-step.qxf");
    noMode.replace("\\>", "/>");
    QVERIFY(diagnose(noMode, &p));
    QCOMPARE(p.brief(), QString("no modes"));
}


/* A make and a model, nothing else (Band-Drummer.qxf in the user library):
   said plainly, and the one honest guess offered. */
void FixtureDefCheck_Test::anEmptyStubBecomesAPlaceholder()
{
    const QByteArray stub(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE FixtureDefinition>\n"
        "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
        " <Manufacturer>Band</Manufacturer>\n"
        " <Model>Drummer</Model>\n"
        " <Type>Other</Type>\n"
        "</FixtureDefinition>\n");
    ParseProblem p;
    QVERIFY(diagnose(stub, &p));
    QCOMPARE(p.brief(), QString("empty: no channels or modes"));
    const QList<Repair> fixes = repairs(stub);
    QVERIFY(!fixes.isEmpty());
    QVERIFY(fixes.first().description.contains("placeholder"));
    QVERIFY(fixes.first().complete);
    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(fixes.first().fixed));
    QCOMPARE(def.model(), QString("Drummer"));
}

/* The step file half fixed by hand, as it was in the repair view: the
   first eight channels ending ">" (left open), the rest "\>". Neither could
   have meant anything but "/>", so both are fixed without asking -- what
   is left, no mode, is a real choice. A channel that does have
   capabilities is followed by them, and keeps its ">". */
void FixtureDefCheck_Test::slashMistakesAreFixedWithoutAsking()
{
    QByteArray xml = readFile(QString(FIXTUREDEFCHECK_DATA) + "backslash-step.qxf");
    for (int i = 0; i < 8; ++i)
    {
        const int at = xml.indexOf("\"\\>");
        QVERIFY(at > 0);
        xml.replace(at, 3, "\">");
    }
    QStringList applied;
    const QByteArray fixed = autoRepair(xml, &applied);
    QCOMPARE(applied.size(), 2);
    QVERIFY2(applied.join(" | ").contains("184"), qPrintable(applied.join(" | ")));
    QVERIFY2(applied.join(" | ").contains("8 channels left open"), qPrintable(applied.join(" | ")));
    ParseProblem p;
    QVERIFY(diagnose(fixed, &p));
    QCOMPARE(p.brief(), QString("no modes"));             // the real choice, left to the user
    QVERIFY(!fixed.contains("\\>"));

    // A channel with capabilities is not "left open".
    QByteArray caps = goodXml;
    caps.replace(" <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n",
                 " <Channel Name=\"Dimmer\">\n  <Group Byte=\"0\">Intensity</Group>\n"
                 "  <Capability Min=\"0\" Max=\"255\">Dim</Capability>\n </Channel>\n");
    QVERIFY(!diagnose(caps, nullptr));
    applied.clear();
    QCOMPARE(autoRepair(caps, &applied), caps);
    QVERIFY(applied.isEmpty());
}

/* The included WLED lantern: loads whole, every mode a lantern lit all
   round, and nothing asks it for a beam angle it does not have. */
void FixtureDefCheck_Test::theWledLanternLoadsAsALantern()
{
    const QByteArray xml = readFile(QString("%1/WLED/WLED-Lantern.qxf").arg(INTERNAL_FIXTUREDIR));
    QVERIFY(!xml.isEmpty());
    QVERIFY(!diagnose(xml, nullptr));
    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(xml));
    QCOMPARE(def.model(), QString("Lantern"));
    QVERIFY(def.modes().size() >= 2);
    foreach (QLCFixtureMode *mode, def.modes())
    {
        QCOMPARE(mode->physical().bodyShape(), QLCPhysical::LanternBody);
        QCOMPARE(mode->physical().emission(), QLCPhysical::OmniEmission);
        QVERIFY(mode->physical().declaresOutput());
    }
    const QList<Issue> issues = check(&def);
    QVERIFY(!hasIssue(issues, Issue::Warning, "No beam angle"));
    QVERIFY(!hasIssue(issues, Issue::Warning, "No light output"));
}

/* Only a light is asked for its light output: the stage curtain (it says
   Emission="None") and a hazer with no intensity channel are not; a hazer
   with LEDs on it, and a light, still are. */
void FixtureDefCheck_Test::onlyALightIsAskedForLightOutput()
{
    const QByteArray curtainXml = readFile(QString("%1/Generic/Generic-Stage-Curtain.qxf").arg(INTERNAL_FIXTUREDIR));
    QVERIFY(!curtainXml.isEmpty());
    QLCFixtureDef curtain;
    QVERIFY(curtain.loadXMLData(curtainXml));
    QVERIFY(!curtain.modes().isEmpty());
    foreach (QLCFixtureMode *mode, curtain.modes())
        QVERIFY2(!expectsLight(mode), qPrintable(mode->name()));
    QList<Issue> issues = check(&curtain);
    QVERIFY(!hasIssue(issues, Issue::Warning, "No light output"));
    QVERIFY(!hasIssue(issues, Issue::Warning, "No beam angle"));

    QByteArray hazerXml = goodXml;
    hazerXml.replace("<Type>Color Changer</Type>", "<Type>Hazer</Type>");
    hazerXml.replace("<Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>",
                     "<Channel Name=\"Dimmer\"><Group Byte=\"0\">Effect</Group></Channel>");
    QLCFixtureDef hazer;
    QVERIFY(hazer.loadXMLData(hazerXml));
    QVERIFY(!expectsLight(hazer.modes().first()));
    QVERIFY(!hasIssue(check(&hazer), Issue::Warning, "No light output"));

    QByteArray ledHazerXml = goodXml;
    ledHazerXml.replace("<Type>Color Changer</Type>", "<Type>Hazer</Type>");
    QLCFixtureDef ledHazer;
    QVERIFY(ledHazer.loadXMLData(ledHazerXml));
    QVERIFY(expectsLight(ledHazer.modes().first()));
    QVERIFY(hasIssue(check(&ledHazer), Issue::Warning, "No light output"));

    QLCFixtureDef light;
    QVERIFY(light.loadXMLData(goodXml));
    QVERIFY(expectsLight(light.modes().first()));
    QVERIFY(hasIssue(check(&light), Issue::Warning, "No light output"));
}

QTEST_APPLESS_MAIN(FixtureDefCheck_Test)
