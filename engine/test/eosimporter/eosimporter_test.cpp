/*
  Q Light Controller Plus - qlcconsole
  eosimporter_test.cpp

  Licensed under the Apache License, Version 2.0.
*/

#include <QtTest>
#include <QTemporaryDir>

#include "eosfixturemapper.h"
#include "eosasciiparser.h"
#include "eosimporter.h"
#include "eosmagicsheetparser.h"
#include "monitorproperties.h"
#include "qlcfixturedefcache.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "fixturegroup.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qxwimporter.h"
#include "qlcpalette.h"
#include "chaserstep.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

#include "eosimporter_test.h"
#include "../common/resource_paths.h"

namespace
{

const char *kShow =
    "Ident 3:0\nManufacturer ETC\nConsole Eos\n$$Format 3.20\n"
    "$$Title Middle School Base\n"
    "$ParamType 1 1 Intens\n$ParamType 12 3 Red\n$ParamType 13 3 Green\n$ParamType 14 3 Blue\n"
    "$ParamType 2 2 Pan\n$ParamType 3 2 Tilt\n"
    // ETC ColorSource PAR in its 3-channel RGB mode: virtual intensity
    "$Personality 9001\n $$Manuf ETC_Fixtures\n $$Model ColorSource_PAR_RGB\n $$Footprint 3\n"
    " $$PersChan 12 1 1 0 255\n $$PersChan 13 1 2 0 255\n $$PersChan 14 1 3 0 255\n"
    "$Personality 9002\n $$Manuf Generic\n $$Model Dimmer\n $$Footprint 1\n $$PersChan 1 1 1 0 0\n"
    // Nothing in the library: a generic dimmer of 5 channels
    "$Personality 9003\n $$Manuf Acme\n $$Model Zork_Mover\n $$Footprint 5\n"
    " $$PersChan 1 1 1 0 0\n $$PersChan 2 2 2 3 32768\n $$PersChan 3 2 4 5 32768\n"
    "$Patch 1 9002 1 0 1\n Text Relay 3rd Elect\n"
    "$Patch 51 9001 51 0 1\n Text FOH\n"
    "$Patch 52 9001 513 0 1\n Text FOH\n"
    "$Patch 60 9003 100 0 1\n"
    "$Patch 61 9003 0 0 1\n"
    "$Patch 62 9003 600 0 2\n"
    "$Group 1\n Text FOH\n $$ChanList 51 52\n"
    "$Group 2\n Text Missing\n 51 77\n"
    "$IntensPalette 1\n Text 10%\n $$Param 51 1@26\n $$Param 52 1@26\n"
    "$ColorPalette 1\n Text Red\n $$Param 51 12@255 13@0 14@0\n $$Param 52 12@255 13@0 14@0\n"
    "$ColorPalette 2\n Text Mixed\n $$Param 51 12@0 13@0 14@255\n $$Param 52 12@0 13@255 14@0\n"
    "$BeamPalette 1\n Text Strobe\n"
    "$Preset 3\n Text Warm Front\n $$Param 51 1@255 12@CP1 13@CP1 14@CP1\n"
    "$CueList 1\n Text Main\n"
    "Cue 0.5 1\n Text Preset\n Up 0\n"
    "Cue 1 1\n Text House Up\n Up 5\n Down 3\n"
    " $$ChanMove 1@H80\n $$Param 51 1@255 12@CP1 13@CP1 14@CP1\n $$Param 60 1@255 2@40000\n"
    "Cue 2 1\n Up 2 1\n Down 2\n $$ChanMove 1@0\n $$Param 51 1@128\n $$Follow 4\n Link 1\n"
    "Cue 3 1\n Up 1\n $$Hang 2\n Chan 51@FL\n"
    "$CueList 2\n Text Backups\n"
    "Cue 5 2\n Up 1\n $$Param 52 1@255 12@255\n"
    "Sub 1\n Text Worklights\n Up 2\n Chan 1@FL\n"
    "$Effect 901\n Text Circle\n";

} // namespace

void EosImporter_Test::initTestCase()
{
}

void EosImporter_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(dir));
}

void EosImporter_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

static EosShowData parsed()
{
    EosShowData d;
    EosAsciiParser p;
    p.parseText(QString::fromLatin1(kShow), d);
    return d;
}

static EosImportResult built(Doc *doc)
{
    EosImportResult r;
    EosFixtureMapper mapper(doc->fixtureDefCache());
    EosImporter::build(parsed(), doc, mapper, r);
    return r;
}

void EosImporter_Test::mapperWords()
{
    QCOMPARE(EosFixtureMapper::words("ColorSource_SPOT_Jr_RGB"),
             QStringList() << "colorsource" << "spot" << "jr" << "rgb");
    QCOMPARE(EosFixtureMapper::words("3-channel (RGB)"), QStringList() << "3ch" << "rgb");
    QCOMPARE(EosFixtureMapper::words("SlimPAR_Q12_USB_9ch"), QStringList() << "slimpar" << "q12" << "usb" << "9ch");
    QCOMPARE(EosFixtureMapper::manufacturerName("ETC_Fixtures"), QString("ETC"));
    QCOMPARE(EosFixtureMapper::manufacturerName("High_End"), QString("High End"));
    QCOMPARE(EosFixtureMapper::editDistance("kitten", "sitting"), 3);
}

void EosImporter_Test::mapperExactAndSuggested()
{
    EosFixtureMapper mapper(m_doc->fixtureDefCache());
    EosPersonality par;
    par.manufacturer = "ETC_Fixtures";
    par.model = "ColorSource_PAR_RGB";
    par.footprint = 3;
    EosFixtureChoice c = mapper.resolve(par);
    QCOMPARE(c.source, EosFixtureChoice::Exact);
    QCOMPARE(c.manufacturer, QString("ETC"));
    QCOMPARE(c.model, QString("ColorSource PAR"));
    QCOMPARE(c.mode, QString("3-channel (RGB)"));

    // A Spot Jr isn't in the library: a 3-channel ColorSource is offered,
    // never taken as exact.
    EosPersonality jr = par;
    jr.model = "ColorSource_SPOT_Jr_RGB";
    c = mapper.resolve(jr);
    QCOMPARE(c.source, EosFixtureChoice::Suggested);
    QVERIFY(!mapper.candidates(jr).isEmpty());
    for (const EosFixtureChoice &cand : mapper.candidates(jr))
        QVERIFY(!cand.mode.isEmpty());

    EosPersonality none;
    none.manufacturer = "Acme";
    none.model = "Zork_Mover";
    none.footprint = 5;
    QVERIFY(mapper.resolve(none).isGeneric());

    EosPersonality dimmer;
    dimmer.manufacturer = "Generic";
    dimmer.model = "Dimmer";
    dimmer.footprint = 1;
    QVERIFY(mapper.resolve(dimmer).isGeneric());
}

void EosImporter_Test::mapperSavedChoice()
{
    QTemporaryDir tmp;
    const QString path = tmp.filePath("eos-fixture-map.json");
    {
        EosFixtureMapper mapper(m_doc->fixtureDefCache());
        QVERIFY(mapper.loadMap(path));     // missing: empty
        EosFixtureChoice c;
        c.manufacturer = "ETC";
        c.model = "ColorSource PAR";
        c.mode = "3-channel (RGB)";
        mapper.setChoice(EosFixtureMapper::key("ETC_Fixtures", "ColorSource_SPOT_Jr_RGB"), c);
        QVERIFY(mapper.saveMap(path));
    }
    EosFixtureMapper mapper(m_doc->fixtureDefCache());
    QVERIFY(mapper.loadMap(path));
    EosPersonality jr;
    jr.manufacturer = "ETC_Fixtures";
    jr.model = "ColorSource_SPOT_Jr_RGB";
    jr.footprint = 3;
    const EosFixtureChoice c = mapper.resolve(jr);
    QCOMPARE(c.source, EosFixtureChoice::Saved);
    QCOMPARE(c.model, QString("ColorSource PAR"));
}

void EosImporter_Test::fixtures()
{
    const EosImportResult r = built(m_doc);
    QCOMPARE(r.fixturesParsed, 5);              // the part-2 patch is left out
    QCOMPARE(r.fixturesUnpatched, 1);
    QCOMPARE(r.fixturesUnmapped, 2);            // the two Zork movers

    Fixture *dim = m_doc->fixture(1);           // the Eos channel number is the ID
    QVERIFY(dim != nullptr);
    QCOMPARE(dim->name(), QString("Ch 1 Relay 3rd Elect"));
    QCOMPARE(dim->channels(), quint32(1));
    QCOMPARE(dim->address(), quint32(0));

    Fixture *par = m_doc->fixture(51);
    QCOMPARE(par->fixtureDef()->model(), QString("ColorSource PAR"));
    QCOMPARE(par->fixtureMode()->name(), QString("3-channel (RGB)"));
    QCOMPARE(par->universe(), quint32(0));
    QCOMPARE(par->address(), quint32(50));
    Fixture *par2 = m_doc->fixture(52);
    QCOMPARE(par2->universe(), quint32(1));     // 513 = universe 2, address 1
    QCOMPARE(par2->address(), quint32(0));

    Fixture *mover = m_doc->fixture(60);
    QCOMPARE(mover->channels(), quint32(5));    // generic, the footprint kept
    QVERIFY(mover->isPatched());
    QVERIFY(!m_doc->fixture(61)->isPatched());  // staged in the inventory
    QVERIFY(m_doc->fixture(62) == nullptr);
}

void EosImporter_Test::groups()
{
    const EosImportResult r = built(m_doc);
    QCOMPARE(r.fixtureGroupsParsed, 2);
    FixtureGroup *foh = nullptr;
    for (FixtureGroup *g : m_doc->fixtureGroups())
        if (g->name() == "FOH")
            foh = g;
    QVERIFY(foh != nullptr);
    QCOMPARE(foh->path(), QString("Eos"));
    QCOMPARE(foh->fixtureList(), QList<quint32>() << 51 << 52);
    QVERIFY(r.warnings.join("\n").contains("Group Missing: 1 channel"));
}

void EosImporter_Test::palettes()
{
    const EosImportResult r = built(m_doc);
    QCOMPARE(r.palettesParsed, 3);              // IP 1, CP 1, CP 2; BP 1 reported
    QHash<QString, QLCPalette *> byName;
    for (QLCPalette *p : m_doc->palettes())
        byName.insert(p->name(), p);
    QCOMPARE(byName.value("10%")->type(), QLCPalette::Dimmer);
    QCOMPARE(byName.value("10%")->value().toInt(), 26);
    QCOMPARE(byName.value("Red")->type(), QLCPalette::Color);
    QCOMPARE(byName.value("Red")->value().toString(), QString("#ff0000"));
    QVERIFY(byName.contains("Mixed"));
    const QString w = r.warnings.join("\n");
    QVERIFY(w.contains("hold different values on different channels") && w.contains("CP 2 “Mixed”"));
    QVERIFY(w.contains("BP 1 “Strobe”"));
}

static Scene *stepScene(Doc *doc, const Chaser *c, int i)
{
    return qobject_cast<Scene *>(doc->function(c->steps().at(i).fid));
}

void EosImporter_Test::cueLists()
{
    const EosImportResult r = built(m_doc);
    QCOMPARE(r.cueListsParsed, 2);
    QCOMPARE(r.cuesParsed, 5);
    QCOMPARE(r.chasers.count(), 2);

    const Chaser *main = qobject_cast<const Chaser *>(m_doc->function(r.chasers.first()));
    QVERIFY(main != nullptr);
    QCOMPARE(main->name(), QString("Middle School Base — list 1 Main"));
    QCOMPARE(main->path(true), QString("Eos"));
    QCOMPARE(main->stepsCount(), 4);
    QCOMPARE(main->steps().at(0).cueNumber, QString("0.5"));
    QCOMPARE(main->steps().at(1).cueNumber, QString("1"));

    // Cue 1: timing, and the full state.
    const ChaserStep s1 = main->steps().at(1);
    QCOMPARE(s1.fadeIn, 5000u);
    QCOMPARE(s1.fadeOut, 3000u);
    QCOMPARE(s1.hold, Function::infiniteSpeed());          // waits for GO
    Scene *c1 = stepScene(m_doc, main, 1);
    QCOMPARE(c1->name(), QString("1-House Up"));
    QCOMPARE(c1->path(true), QString("Eos/Main"));
    QCOMPARE(c1->value(1, 0), uchar(128));
    QCOMPARE(c1->value(51, 0), uchar(255));     // CP1 through the palette: red
    QCOMPARE(c1->value(51, 1), uchar(0));
    QCOMPARE(c1->value(60, 0), uchar(255));     // generic: by the personality's offsets
    QCOMPARE(c1->value(60, 1), uchar(40000 >> 8));
    QCOMPARE(c1->value(60, 2), uchar(40000 & 0xFF));
    QVERIFY(!c1->fixtures().contains(52));      // never lit: not in the cue

    // Cue 2 tracks cue 1, moves 51 to half (virtual intensity scales the
    // colour) and takes the dimmer out; it follows on 4 s after GO.
    const ChaserStep s2 = main->steps().at(2);
    QCOMPARE(s2.fadeIn, 2000u);
    QCOMPARE(s2.hold, 2000u);
    QCOMPARE(s2.duration, 4000u);
    Scene *c2 = stepScene(m_doc, main, 2);
    QCOMPARE(c2->name(), QString("2"));
    QCOMPARE(c2->value(51, 0), uchar(128));
    QVERIFY(!c2->fixtures().contains(1));       // out: left out
    QCOMPARE(c2->value(60, 0), uchar(255));     // tracked

    // Cue 3: Chan is the whole intensity picture.
    const ChaserStep s3 = main->steps().at(3);
    QCOMPARE(s3.hold, 2000u);                   // hang
    Scene *c3 = stepScene(m_doc, main, 3);
    QCOMPARE(c3->value(51, 0), uchar(255));
    QVERIFY(!c3->fixtures().contains(60));      // not in Chan: out

    const Chaser *backups = qobject_cast<const Chaser *>(m_doc->function(r.chasers.at(1)));
    QCOMPARE(backups->stepsCount(), 1);
    QCOMPARE(stepScene(m_doc, backups, 0)->value(52, 0), uchar(255));
}

void EosImporter_Test::presetsAndSubs()
{
    const EosImportResult r = built(m_doc);
    QCOMPARE(r.presetsParsed, 1);
    QCOMPARE(r.submastersParsed, 1);
    Scene *preset = nullptr, *sub = nullptr;
    for (Function *f : m_doc->functions())
    {
        if (f->name() == "Preset 3 Warm Front")
            preset = qobject_cast<Scene *>(f);
        if (f->name() == "Sub 1 Worklights")
            sub = qobject_cast<Scene *>(f);
    }
    QVERIFY(preset != nullptr && sub != nullptr);
    QCOMPARE(preset->path(true), QString("Eos/Presets"));
    QCOMPARE(preset->value(51, 0), uchar(255));
    QCOMPARE(sub->value(1, 0), uchar(255));
    QCOMPARE(sub->fadeInSpeed(), 2000u);
}

void EosImporter_Test::warnings()
{
    const QString w = built(m_doc).warnings.join("\n");
    QVERIFY(w.contains("Acme Zork_Mover"));
    QVERIFY(w.contains("1 cue(s) have a delay"));
    QVERIFY(w.contains("cue 2 → 1"));
    QVERIFY(w.contains("$Effect × 1"));
    QVERIFY(w.contains("multi-part"));
}

void EosImporter_Test::realExportIntoShowTree()
{
    // The whole path on a real export: parse, build the scratch Doc, merge
    // with QxwImporter, then the Show Tree: one tracking list.
    EosShowData data;
    EosAsciiParser parser;
    QVERIFY(parser.parse(QString(EOS_SAMPLES_DIR) + "otbpa-eos-3.3.6.asc", data));
    EosImportResult r;
    EosFixtureMapper mapper(m_doc->fixtureDefCache());
    QVERIFY(EosImporter::build(data, m_doc, mapper, r));
    QCOMPARE(r.fixturesParsed, 44);
    QCOMPARE(r.cuesParsed, 66);
    QCOMPARE(r.fixtureGroupsParsed, 5);

    Doc *target = new Doc(this);
    QDir dir(INTERNAL_FIXTUREDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    target->fixtureDefCache()->loadMap(dir);

    QList<quint32> fixtures, groups, functions, palettes;
    for (Fixture *f : m_doc->fixtures())
        fixtures << f->id();
    for (FixtureGroup *g : m_doc->fixtureGroups())
        groups << g->id();
    for (Function *f : m_doc->functions())
        functions << f->id();
    for (QLCPalette *p : m_doc->palettes())
        palettes << p->id();
    const QxwImportResult m = QxwImporter::import(m_doc, target, fixtures, groups, functions, palettes);
    QCOMPARE(m.fixturesImported, 44);
    QCOMPARE(m.palettesImported, int(palettes.count()));
    QVERIFY(m.functionMap.contains(r.chasers.first()));

    QStringList lines;
    const QList<quint32> lists = EosImporter::toShowTree(target, QList<quint32>() << m.functionMap.value(r.chasers.first()),
                                                         &lines);
    QCOMPARE(lists.count(), 1);
    const ShowTree::CueList *list = target->showTree()->cueList(lists.first());
    QVERIFY(list != nullptr);
    QCOMPARE(list->cues.count(), 66);
    QCOMPARE(list->name, QString("OTBPA DRESS TUES"));
    QCOMPARE(list->mode, ShowTree::ListMode::Tracking);
    QCOMPARE(list->cues.at(1).number, QString("1"));
    QCOMPARE(list->cues.at(1).name, QString("PRESHOW"));
    qDebug() << lines;
    // One undo step takes the lot back out of the tree.
    QVERIFY(target->showTree()->canUndo());
    QVERIFY(target->showTree()->undo());
    QVERIFY(target->showTree()->cueList(lists.first()) == nullptr);
    delete target;
}

void EosImporter_Test::magicSheet()
{
    // The shape of an Eos magic sheet export (eos-toolkit's ms_gen.py
    // documents it; Eos writes UTF-16): two channel objects, a group
    // button, a CP button, a decoration.
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n<!DOCTYPE ETC>\n"
        "<MAGICSHEET SHOWFILE_VERSION=\"115\"><VIEW><VIEWPORT POSX=\"0\" POSY=\"0\"/></VIEW>"
        "<ETCGRAPHICSSCENE><ITEMLIST>"
        "<ITEM KEY=\"Triangle\" ROT=\"0\" POSX=\"-540\" POSY=\"40\"><ITEMDATA><RECT Y=\"0\" W=\"40\" X=\"0\" H=\"20\"/>"
        "<MAGICSHEET><TARGET TARGETLISTID=\"-1\" TARGETID=\"51\" TARGETTYPE=\"20\"/><TEXT STR=\"51\"/></MAGICSHEET>"
        "</ITEMDATA></ITEM>"
        "<ITEM KEY=\"Triangle\" ROT=\"0\" POSX=\"-500\" POSY=\"40\"><ITEMDATA><RECT Y=\"0\" W=\"40\" X=\"0\" H=\"20\"/>"
        "<MAGICSHEET><TARGET TARGETLISTID=\"-1\" TARGETID=\"52\" TARGETTYPE=\"20\"/></MAGICSHEET></ITEMDATA></ITEM>"
        "<ITEM KEY=\"Button\" POSX=\"0\" POSY=\"0\"><ITEMDATA><MAGICSHEET>"
        "<TARGET TARGETLISTID=\"-1\" TARGETID=\"1\" TARGETTYPE=\"3\"/></MAGICSHEET></ITEMDATA></ITEM>"
        "<ITEM KEY=\"Button\" POSX=\"0\" POSY=\"0\"><ITEMDATA><MAGICSHEET>"
        "<TARGET TARGETLISTID=\"3\" TARGETID=\"2\" TARGETTYPE=\"6\"/></MAGICSHEET></ITEMDATA></ITEM>"
        "<ITEM KEY=\"Square\" POSX=\"0\" POSY=\"0\"><ITEMDATA><RECT Y=\"0\" W=\"1400\" X=\"0\" H=\"140\"/>"
        "<MAGICSHEET><TARGET TARGETTYPE=\"0\" TARGETID=\"0\"/></MAGICSHEET></ITEMDATA></ITEM>"
        "<ITEM KEY=\"Clock\" POSX=\"0\" POSY=\"0\"><ITEMDATA><MAGICSHEET>"
        "<TARGET TARGETTYPE=\"77\" TARGETID=\"4\"/></MAGICSHEET></ITEMDATA></ITEM>"
        "</ITEMLIST></ETCGRAPHICSSCENE></MAGICSHEET>\n");
    QByteArray utf16;
    utf16.append(char(0xFF)).append(char(0xFE));
    utf16.append(reinterpret_cast<const char *>(xml.utf16()), xml.size() * 2);

    QStringList warnings;
    EosMagicSheetParser parser;
    const EosMagicSheet sheet = parser.parseXml(utf16, "Magic Sheet 1", warnings);
    QCOMPARE(sheet.name, QString("Magic Sheet 1"));
    QCOMPARE(sheet.objects.count(), 4);
    QCOMPARE(sheet.channels().count(), 2);
    QCOMPARE(sheet.channels().at(0).targetId, QString("51"));
    QCOMPARE(sheet.channels().at(0).position, QPointF(-520, 50));   // the object's centre
    QCOMPARE(sheet.channels().at(0).label, QString("51"));
    QCOMPARE(sheet.objects.at(3).kind, EosMagicSheetObject::Palette);
    QCOMPARE(sheet.objects.at(3).targetListId, 3);                  // CP
    QCOMPARE(sheet.imagesDropped, 1);
    QCOMPARE(sheet.unknownKindsDropped, 1);

    QVERIFY(parser.parseXml("<NOTASHEET/>", "x", warnings).name.isEmpty());

    EosImportResult r = built(m_doc);
    QCOMPARE(EosImporter::placeMagicSheet(sheet, m_doc, r), 2);
    MonitorProperties *mon = m_doc->monitorProperties();
    QVERIFY(mon->containsFixture(51));
    QCOMPARE(mon->fixturePosition(51, 0, 0), QVector3D(-5200, 500, 0));
    const quint32 layer = mon->fixtureLayer(51);
    QCOMPARE(mon->layer(layer).name, QString("Imported — Magic Sheet 1"));
    QVERIFY(r.warnings.last().contains("2 channel(s) placed"));
}

void EosImporter_Test::showFromSampleDir()
{
    // Real shows that can't live in the repo: point QLC_EOS_SAMPLE_DIR at a
    // folder of Eos exports (*.asc, and magic sheet *.xml) to run them.
    const QString dirPath = qEnvironmentVariable("QLC_EOS_SAMPLE_DIR");
    if (dirPath.isEmpty())
        QSKIP("QLC_EOS_SAMPLE_DIR not set");
    QDir dir(dirPath);
    const QStringList ascs = dir.entryList(QStringList() << "*.asc" << "*.ASC", QDir::Files);
    QVERIFY2(!ascs.isEmpty(), "no .asc in QLC_EOS_SAMPLE_DIR");
    QStringList sheets;
    for (const QString &x : dir.entryList(QStringList() << "*.xml", QDir::Files))
        sheets << dir.absoluteFilePath(x);

    for (const QString &asc : ascs)
    {
        Doc *scratch = new Doc(this);
        QDir fx(INTERNAL_FIXTUREDIR);
        fx.setFilter(QDir::Files);
        fx.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
        scratch->fixtureDefCache()->loadMap(fx);

        EosShowData data;
        EosAsciiParser p;
        QVERIFY(p.parse(dir.absoluteFilePath(asc), data));
        EosFixtureMapper mapper(scratch->fixtureDefCache());
        QHash<int, int> uses;
        for (const EosPatch &pt : data.patch)
            uses[pt.personality]++;
        QStringList types;
        int exact = 0, suggested = 0, generic = 0;
        for (const EosPersonality &pers : data.personalities)
        {
            if (!uses.contains(pers.id))
                continue;
            const EosFixtureChoice c = mapper.resolve(pers);
            const char *src = c.source == EosFixtureChoice::Exact ? "exact"
                              : c.source == EosFixtureChoice::Suggested ? "suggested"
                              : c.source == EosFixtureChoice::Saved ? "saved" : "generic";
            exact += c.source == EosFixtureChoice::Exact;
            suggested += c.source == EosFixtureChoice::Suggested;
            generic += c.isGeneric();
            types << QString("  %1 %2 (%3 ch) x%4 -> %5 %6").arg(pers.manufacturer, pers.model)
                     .arg(pers.footprint).arg(uses.value(pers.id)).arg(src, c.text());
        }
        qInfo().noquote() << asc << "console" << data.console << "format" << data.format;
        qInfo().noquote() << QString("patch %1, types %2 (exact %3, suggested %4, generic %5), groups %6, "
                                     "cue lists %7, cues %8, palettes %9, presets %10, subs %11")
                             .arg(data.patch.count()).arg(types.count()).arg(exact).arg(suggested).arg(generic)
                             .arg(data.groups.count()).arg(data.cueLists.count()).arg(data.cueCount())
                             .arg(data.palettes.count()).arg(data.presets.count()).arg(data.subs.count());
        qInfo().noquote() << types.join("\n");

        EosImportBundle bundle;
        bundle.asciiPath = dir.absoluteFilePath(asc);
        bundle.magicSheetPaths = sheets;
        bundle.fixtureMapPath = QDir::temp().absoluteFilePath("no-such-eos-map.json");
        EosImportResult r;
        QCOMPARE(EosImporter::load(bundle, r, nullptr, scratch), scratch);
        qInfo().noquote() << QString("built: fixtures %1 (generic %2, staged %3), groups %4, palettes %5, "
                                     "cues %6, subs %7, sheets %8, placed %9")
                             .arg(r.fixturesParsed).arg(r.fixturesUnmapped).arg(r.fixturesUnpatched)
                             .arg(r.fixtureGroupsParsed).arg(r.palettesParsed).arg(r.cuesParsed)
                             .arg(r.submastersParsed).arg(r.magicSheetsParsed).arg(r.channelObjectsPlaced);
        qInfo().noquote() << "warnings:\n  " + r.warnings.join("\n  ");

        Doc *target = new Doc(this);
        target->fixtureDefCache()->loadMap(fx);
        QList<quint32> fixtures, groups, functions, palettes;
        for (Fixture *f : scratch->fixtures())
            fixtures << f->id();
        for (FixtureGroup *g : scratch->fixtureGroups())
            groups << g->id();
        for (Function *f : scratch->functions())
            functions << f->id();
        for (QLCPalette *pl : scratch->palettes())
            palettes << pl->id();
        const QxwImportResult m = QxwImporter::import(scratch, target, fixtures, groups, functions, palettes);
        QCOMPARE(m.fixturesImported, r.fixturesParsed);
        QList<quint32> chasers;
        for (quint32 id : r.chasers)
            chasers << m.functionMap.value(id);
        QStringList lines;
        const QList<quint32> lists = EosImporter::toShowTree(target, chasers, &lines);
        QCOMPARE(lists.count(), r.cueListsParsed);
        qInfo().noquote() << "show tree:\n  " + lines.join("\n  ");
        delete target;
        delete scratch;
    }
}

QTEST_MAIN(EosImporter_Test)
