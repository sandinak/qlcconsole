/*
  Q Light Controller Plus - Unit test
  showembedding_test.cpp

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

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "showembedding_test.h"
#include "qlcfixturedefcache.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "rgbscriptscache.h"
#include "showembedding.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "rgbscriptv4.h"
#include "qlcpalette.h"
#include "monitorproperties.h"
#include "headlayout.h"
#include "rgbmatrix.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"

#include "../common/resource_paths.h"

namespace
{
    typedef QList<QPair<QString, QStringList> > Modes;

    const QString kMake = QStringLiteral("Test Maker");
    const QString kModel = QStringLiteral("Step Row");
    const QString kKey = QStringLiteral("fixture:Test Maker|Step Row");

    Modes standardModes()
    {
        return Modes() << qMakePair(QStringLiteral("4ch"), QStringList() << "Dimmer" << "Red" << "Green" << "Blue")
                       << qMakePair(QStringLiteral("6ch"), QStringList() << "Dimmer" << "Red" << "Green"
                                                                         << "Blue" << "Pan" << "Tilt");
    }

    QByteArray qxf(const Modes &modes, const QString &note = QString())
    {
        static const QMap<QString, QString> presets = {
            { "Dimmer", "IntensityMasterDimmer" }, { "Red", "IntensityRed" },
            { "Green", "IntensityGreen" }, { "Blue", "IntensityBlue" },
            { "White", "IntensityWhite" }, { "Pan", "PositionPan" }, { "Tilt", "PositionTilt" } };
        QString x = "<?xml version='1.0' encoding='UTF-8'?>\n<!DOCTYPE FixtureDefinition>\n"
                    "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
                    " <Creator>\n  <Name>Q Light Controller Plus</Name>\n  <Version>4</Version>\n"
                    "  <Author>test</Author>\n </Creator>\n";
        if (!note.isEmpty())
            x += " <!-- " + note + " -->\n";
        x += " <Manufacturer>" + kMake + "</Manufacturer>\n <Model>" + kModel + "</Model>\n"
             " <Type>Color Changer</Type>\n";
        QStringList chans;
        for (const auto &m : modes)
            for (const QString &c : m.second)
                if (!chans.contains(c))
                    chans << c;
        for (const QString &c : chans)
            x += QString(" <Channel Name=\"%1\" Preset=\"%2\"/>\n")
                     .arg(c, presets.value(c, "IntensityMasterDimmer"));
        for (const auto &m : modes)
        {
            x += QString(" <Mode Name=\"%1\">\n").arg(m.first);
            for (int i = 0; i < m.second.size(); ++i)
                x += QString("  <Channel Number=\"%1\">%2</Channel>\n").arg(i).arg(m.second.at(i));
            x += " </Mode>\n";
        }
        x += "</FixtureDefinition>\n";
        return x.toUtf8();
    }

    /* CR LF line ends: what has to travel as base64 to come back exact. */
    QByteArray effectJs(bool withSpeed, const QString &note = QString())
    {
        QString s = "(function() {\n    var effect = new Object;\n    effect.apiVersion = 1;\n"
                    "    effect.name = \"My Chase\";\n    effect.fixtureTypes = [\"rgb\"];\n"
                    "    effect.parameters = [\n";
        if (withSpeed)
            s += "        { name: \"speed\", description: \"Speed\", min: 0.1, max: 3, defaultValue: 1 },\n";
        s += "        { name: \"mode\", description: \"Mode\", min: 0, max: 1, defaultValue: 0, values: [\"A\", \"B\"] }\n"
             "    ];\n";
        s += "    // " + note + "\n";
        s += "    effect.tick = function(fixtures, inputs, palettes, params, state) {\n"
             "        return fixtures.map(function(f) { return { dimmer: 1 }; });\n    };\n"
             "    return effect;\n})()\n";
        s.replace("\n", "\r\n");
        return s.toUtf8();
    }

    QByteArray rgbJs(const QString &note = QString())
    {
        QString s = "(function() {\n  var algo = new Object;\n  algo.apiVersion = 2;\n"
                    "  algo.name = \"My RGB\";\n  algo.author = \"test\";\n  algo.acceptColors = 1;\n"
                    "  algo.properties = new Array();\n  algo.size = 1;\n"
                    "  algo.properties.push(\"name:size|type:range|display:Size|values:1,10|write:setSize|read:getSize\");\n"
                    "  algo.setSize = function(s) { algo.size = s; };\n"
                    "  algo.getSize = function() { return algo.size; };\n"
                    "  algo.rgbMapStepCount = function(width, height) { return 2; };\n"
                    "  algo.rgbMap = function(width, height, rgb, step) {\n"
                    "    var map = new Array(height);\n"
                    "    for (var y = 0; y < height; y++) { map[y] = new Array(width);\n"
                    "      for (var x = 0; x < width; x++) map[y][x] = step ? rgb : 0; }\n"
                    "    return map;\n  };\n";
        s += "  // " + note + "\n  return algo;\n})()\n";
        return s.toUtf8();
    }

    bool write(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
    }

    QByteArray read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    QString userFixture()
    {
        return QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath("Test-Maker-Step-Row.qxf");
    }
    QString userEffect()
    {
        return EffectScriptCache::userScriptsDirectory().absoluteFilePath("my-chase.js");
    }
    QString userRgb()
    {
        return RGBScriptsCache::userScriptsDirectory().absoluteFilePath("my-rgb.js");
    }

    Fixture *addFixture(Doc *doc, const QString &name, quint32 address,
                        const QString &make, const QString &model, const QString &mode)
    {
        QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(make, model);
        if (def == nullptr || def->mode(mode) == nullptr)
            return nullptr;
        Fixture *fx = new Fixture(doc);
        fx->setName(name);
        fx->setUniverse(0);
        fx->setAddress(address);
        fx->setFixtureDefinition(def, def->mode(mode));
        doc->addFixture(fx);
        return fx;
    }

    Fixture *fixtureNamed(Doc *doc, const QString &name)
    {
        foreach (Fixture *fx, doc->fixtures())
            if (fx->name() == name)
                return fx;
        return nullptr;
    }

    QLCPalette *lookNamed(Doc *doc, const QString &name)
    {
        foreach (QLCPalette *p, doc->palettes())
            if (p->name() == name)
                return p;
        return nullptr;
    }

    RGBMatrix *theMatrix(Doc *doc)
    {
        const QList<Function *> fs = doc->functionsByType(Function::RGBMatrixType);
        return fs.isEmpty() ? nullptr : qobject_cast<RGBMatrix *>(fs.first());
    }

    QString matrixScriptFile(Doc *doc)
    {
        RGBMatrix *m = theMatrix(doc);
        if (m == nullptr || m->algorithm() == nullptr || m->algorithm()->type() != RGBAlgorithm::Script)
            return QString();
        return static_cast<RGBScript *>(m->algorithm())->fileName();
    }

    bool under(const QString &path, const QString &dir)
    {
        const QString d = QFileInfo(dir).canonicalFilePath();
        const QString p = QFileInfo(path).canonicalFilePath();
        return !d.isEmpty() && !p.isEmpty() && p.startsWith(d + "/");
    }
}

/*****************************************************************************
 * Set up: computers and shows
 *****************************************************************************/

void ShowEmbedding_Test::initTestCase()
{
    m_realHome = qgetenv("HOME");
    ShowEmbedding::setDeleteInsteadOfTrash(true);
}

void ShowEmbedding_Test::init()
{
    m_root.reset(new QTemporaryDir());
    QVERIFY(m_root->isValid());
    m_computers = 0;
    // The app's own scripts: one effect, in a folder of their own.
    m_shipped = m_root->filePath("app");
    QVERIFY(write(m_shipped + "/EffectScripts/shipped.js",
                  effectJs(false, "shipped").replace("My Chase", "Shipped Chase")));
    QVERIFY(QDir().mkpath(m_shipped + "/RGBScripts"));
    EffectScriptCache::setSystemDirectoriesOverride(m_shipped + "/EffectScripts", m_shipped + "/RGBScripts");
}

void ShowEmbedding_Test::cleanup()
{
    qputenv("HOME", m_realHome);
    EffectScriptCache::setSystemDirectoriesOverride(QString(), QString());
    m_root.reset();
}

void ShowEmbedding_Test::cleanupTestCase()
{
    qputenv("HOME", m_realHome);
    ShowEmbedding::setDeleteInsteadOfTrash(false);
}

QString ShowEmbedding_Test::newComputer()
{
    const QString home = m_root->filePath(QString("computer%1").arg(++m_computers));
    QDir().mkpath(home);
    qputenv("HOME", home.toUtf8());
    return home;
}

Doc *ShowEmbedding_Test::startDoc()
{
    Doc *doc = new Doc(this);
    doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
    doc->fixtureDefCache()->loadMap(QDir(INTERNAL_FIXTUREDIR));
    doc->rgbScriptsCache()->load(RGBScriptsCache::userScriptsDirectory());
    doc->effectScriptRunner()->cache()->rescan();
    return doc;
}

void ShowEmbedding_Test::save(Doc *doc, const QString &path, bool embed)
{
    if (embed)
        doc->showEmbedding()->collect();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QXmlStreamWriter w(&f);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeDTD(QStringLiteral("<!DOCTYPE Workspace>"));
    w.writeStartElement(QStringLiteral("Workspace"));
    if (embed)
        doc->showEmbedding()->saveXML(&w);
    doc->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
}

/* What App::loadXML does: the show's copies first, then its contents. */
void ShowEmbedding_Test::open(Doc *doc, const QString &path)
{
    doc->clearContents();
    doc->setWorkspacePath(QFileInfo(path).absolutePath());
    QVERIFY(doc->showEmbedding()->readWorkspace(path));
    doc->showEmbedding()->apply(QFileInfo(path).absolutePath());
    QXmlStreamReader *xml = QLCFile::getXMLReader(path);
    QVERIFY(xml != nullptr);
    while (!xml->atEnd())
        if (xml->readNext() == QXmlStreamReader::DTD)
            break;
    QVERIFY(xml->readNextStartElement());
    while (xml->readNextStartElement())
    {
        if (xml->name() == QLatin1String("Engine"))
            doc->loadXML(*xml, false);
        else
            xml->skipCurrentElement();
    }
    QLCFile::releaseXMLReader(xml);
}

/* Computer A: one of its own definitions (two fixtures of it), a shipped
   one, its own effect script and RGB script, and a shipped script. */
QString ShowEmbedding_Test::buildShow()
{
    newComputer();
    write(userFixture(), qxf(standardModes()));
    write(userEffect(), effectJs(true));
    write(userRgb(), rgbJs());
    Doc *doc = startDoc();

    addFixture(doc, "Row 1", 0, kMake, kModel, "4ch");
    addFixture(doc, "Row 2", 10, kMake, kModel, "4ch");
    addFixture(doc, "Scanner", 100, "Futurelight", "DJScan250", "Mode 1");

    QLCPalette *look = new QLCPalette(QLCPalette::Effect, doc);
    look->setName("Chase look");
    look->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath("my-chase"));
    look->setEffectParamValue("speed", 2.0);
    look->setEffectParamValue("mode", 1.0);
    doc->addPalette(look);

    QLCPalette *shipped = new QLCPalette(QLCPalette::Effect, doc);
    shipped->setName("Shipped look");
    shipped->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath("shipped"));
    doc->addPalette(shipped);

    RGBMatrix *m = new RGBMatrix(doc);
    m->setName("Matrix");
    m->setAlgorithm(doc->rgbScriptsCache()->script("My RGB"));
    m->setProperty("size", "3");
    doc->addFunction(m);

    const QString path = m_root->filePath("shows/A/show.qlcc");
    save(doc, path, true);
    delete doc;
    return path;
}

/*****************************************************************************
 * Tests
 *****************************************************************************/

void ShowEmbedding_Test::onlyAQlccEmbeds()
{
    QVERIFY(ShowEmbedding::embedsIn("/x/show.qlcc"));
    QVERIFY(ShowEmbedding::embedsIn("/x/SHOW.QLCC"));
    QVERIFY(!ShowEmbedding::embedsIn("/x/show.qxw"));
    QVERIFY(!ShowEmbedding::embedsIn("/x/show.qlcc.autosave"));
}

void ShowEmbedding_Test::savesExactBytesOfWhatIsNotShipped()
{
    const QString path = buildShow();
    const QByteArray def = qxf(standardModes()), fx = effectJs(true), rgb = rgbJs();

    const QString text = QString::fromUtf8(read(path));
    QVERIFY(text.indexOf("<Embedded>") >= 0);
    QVERIFY2(text.indexOf("<Embedded>") < text.indexOf("<Engine"), "embedded comes first");
    QVERIFY(text.contains(ShowEmbedding::sha256(def)));
    QVERIFY(text.contains(ShowEmbedding::sha256(fx)));
    QVERIFY(text.contains(ShowEmbedding::sha256(rgb)));
    QVERIFY(text.contains("Encoding=\"base64\""));          // the CR LF script

    newComputer();
    Doc *doc = startDoc();
    QVERIFY(doc->showEmbedding()->readWorkspace(path));
    const QList<ShowEmbedding::Item> items = doc->showEmbedding()->items();
    QCOMPARE(items.size(), 3);                               // not the shipped ones
    for (const ShowEmbedding::Item &it : items)
    {
        if (it.kind == ShowEmbedding::FixtureDefinition)
        {
            QCOMPARE(it.bytes, def);
            QCOMPARE(it.manufacturer, kMake);
            QCOMPARE(it.from, QString("user"));
            QVERIFY(it.fromModified.isValid() && it.embedded.isValid());
        }
        else if (it.kind == ShowEmbedding::EffectScript)
        {
            QCOMPARE(it.bytes, fx);
            QCOMPARE(it.file, QString("my-chase.js"));
        }
        else
        {
            QCOMPARE(it.bytes, rgb);
            QCOMPARE(it.name, QString("My RGB"));
        }
        QCOMPARE(it.sha256, ShowEmbedding::sha256(it.bytes));
    }
    QVERIFY(!text.contains("Futurelight\" Model"));
    QVERIFY(!text.contains("File=\"shipped.js\""));

    // A .qxw is written as before: nothing embedded.
    const QString qxw = m_root->filePath("shows/A/show.qxw");
    open(doc, path);
    save(doc, qxw, false);
    QVERIFY(!QString::fromUtf8(read(qxw)).contains("<Embedded>"));
    delete doc;
}

void ShowEmbedding_Test::aCleanComputerOpensTheShowComplete()
{
    const QString path = buildShow();
    newComputer();                          // nothing of A's
    Doc *doc = startDoc();
    QVERIFY(doc->fixtureDefCache()->fixtureDef(kMake, kModel) == nullptr);
    open(doc, path);

    ShowEmbedding *e = doc->showEmbedding();
    const QString work = e->workingDirectory();
    QVERIFY(!work.isEmpty());
    Fixture *row = fixtureNamed(doc, "Row 1");
    QVERIFY(row != nullptr);
    QVERIFY(!row->definitionUnavailable());
    QCOMPARE(row->fixtureDef()->model(), kModel);
    QCOMPARE(row->channels(), quint32(4));
    QVERIFY(under(row->fixtureDef()->definitionSourceFile(), work));

    QLCPalette *look = lookNamed(doc, "Chase look");
    QVERIFY(look != nullptr);
    QVERIFY(under(look->scriptPath(), work));
    QVERIFY(under(matrixScriptFile(doc), work));
    QCOMPARE(theMatrix(doc)->algorithm()->name(), QString("My RGB"));

    QCOMPARE(e->showOnly().size(), 3);
    QVERIFY(e->differences().isEmpty());
    delete doc;
}

void ShowEmbedding_Test::installPutsTheShowsCopiesOnThisComputer()
{
    const QString path = buildShow();
    newComputer();
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    foreach (const ShowEmbedding::Item &it, e->showOnly())
    {
        QString error;
        QVERIFY2(e->installShows(it.key(), &error), qPrintable(error));
    }
    QVERIFY(e->showOnly().isEmpty());
    QCOMPARE(read(userFixture()), qxf(standardModes()));
    QCOMPARE(read(userEffect()), effectJs(true));
    QCOMPARE(read(userRgb()), rgbJs());
    // In use now from where it was installed.
    QCOMPARE(QFileInfo(fixtureNamed(doc, "Row 1")->fixtureDef()->definitionSourceFile()).fileName(),
             QFileInfo(userFixture()).fileName());
    QVERIFY(under(lookNamed(doc, "Chase look")->scriptPath(), EffectScriptCache::userScriptsDirectory().absolutePath()));
    QVERIFY(under(matrixScriptFile(doc), RGBScriptsCache::userScriptsDirectory().absolutePath()));
    delete doc;

    // Next time: this computer has them, the same.
    doc = startDoc();
    open(doc, path);
    QVERIFY(doc->showEmbedding()->showOnly().isEmpty());
    QVERIFY(doc->showEmbedding()->differences().isEmpty());
    foreach (const ShowEmbedding::Item &it, doc->showEmbedding()->items())
        QCOMPARE(int(it.state), int(ShowEmbedding::Identical));
    delete doc;
}

void ShowEmbedding_Test::identicalCopiesSayNothing()
{
    const QString path = buildShow();         // computer A has them all, the same
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    QVERIFY(e->differences().isEmpty());
    QVERIFY(e->showOnly().isEmpty());
    // This computer's own files are the ones in use.
    QCOMPARE(fixtureNamed(doc, "Row 1")->fixtureDef()->definitionSourceFile(), userFixture());
    QCOMPARE(lookNamed(doc, "Chase look")->scriptPath(), userEffect());
    delete doc;
}

void ShowEmbedding_Test::aDifferentCopyKeepsTheShows()
{
    const QString path = buildShow();
    newComputer();
    write(userFixture(), qxf(standardModes(), "edited here"));
    write(userEffect(), effectJs(true, "edited here"));
    write(userRgb(), rgbJs("edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    QCOMPARE(e->differences().size(), 3);
    QVERIFY(e->showOnly().isEmpty());
    const QString work = e->workingDirectory();
    QVERIFY(under(fixtureNamed(doc, "Row 1")->fixtureDef()->definitionSourceFile(), work));
    QVERIFY(under(lookNamed(doc, "Chase look")->scriptPath(), work));
    QVERIFY(under(matrixScriptFile(doc), work));
    // The computer's own files untouched.
    QCOMPARE(read(userFixture()), qxf(standardModes(), "edited here"));
    delete doc;
}

void ShowEmbedding_Test::whichSideIsNewer()
{
    const QString path = buildShow();
    newComputer();
    write(userFixture(), qxf(standardModes(), "edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    const ShowEmbedding::Item it = e->item(kKey);
    QVERIFY(it.fromModified.isValid());

    QFile f(userFixture());
    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(it.fromModified.addDays(1), QFileDevice::FileModificationTime));
    f.close();
    QCOMPARE(int(e->newer(it)), int(ShowEmbedding::ComputerNewer));
    QCOMPARE(ShowEmbedding::newerText(ShowEmbedding::ComputerNewer),
             QString("Updated on this computer since the show was saved"));

    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(it.fromModified.addDays(-1), QFileDevice::FileModificationTime));
    f.close();
    QCOMPARE(int(e->newer(it)), int(ShowEmbedding::ShowNewer));

    QVERIFY(f.open(QIODevice::ReadWrite));
    QVERIFY(f.setFileTime(it.fromModified, QFileDevice::FileModificationTime));
    f.close();
    QCOMPARE(int(e->newer(it)), int(ShowEmbedding::NewerUnknown));
    delete doc;
}

void ShowEmbedding_Test::definitionCompatibility()
{
    const QString path = buildShow();
    struct Case { const char *what; Modes modes; bool compatible; const char *says; };
    const QList<Case> cases = {
        { "a comment changed", standardModes(), true, "" },
        { "mode removed",
          Modes() << qMakePair(QString("6ch"), QStringList() << "Dimmer" << "Red" << "Green" << "Blue" << "Pan" << "Tilt"),
          false, "is not in this computer's copy" },
        { "channel count changed",
          Modes() << qMakePair(QString("4ch"), QStringList() << "Dimmer" << "Red" << "Green" << "Blue" << "White"),
          false, "is 5 channels here, 4 in the show" },
        { "channel kind changed",
          Modes() << qMakePair(QString("4ch"), QStringList() << "Dimmer" << "Red" << "Green" << "White"),
          false, "channel 4 is" },
    };
    for (const Case &c : cases)
    {
        newComputer();
        write(userFixture(), qxf(c.modes, "edited here"));
        Doc *doc = startDoc();
        open(doc, path);
        const ShowEmbedding::Compatibility comp = doc->showEmbedding()->compatibility(kKey);
        QVERIFY2(comp.compatible == c.compatible, c.what);
        if (!c.compatible)
        {
            QCOMPARE(comp.problems.size(), 1);
            QVERIFY2(comp.problems.first().contains(c.says), qPrintable(comp.problems.first()));
            // The fixtures it affects, by name.
            QVERIFY2(comp.problems.first().contains("Row 1") && comp.problems.first().contains("Row 2"),
                     qPrintable(comp.problems.first()));
        }
        delete doc;
    }
}

void ShowEmbedding_Test::scriptCompatibility()
{
    const QString path = buildShow();
    newComputer();
    write(userEffect(), effectJs(false, "no speed here"));
    write(userRgb(), rgbJs("edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();

    const ShowEmbedding::Compatibility fx = e->compatibility("effect:my-chase.js");
    QVERIFY(!fx.compatible);
    QCOMPARE(fx.problems.size(), 1);
    QVERIFY2(fx.problems.first().contains("Chase look") && fx.problems.first().contains("speed"),
             qPrintable(fx.problems.first()));

    // Only a comment differs: the matrix's setting is still there.
    QVERIFY(e->compatibility("rgb:my-rgb.js").compatible);
    delete doc;
}

void ShowEmbedding_Test::useThisComputersSwitchesAndIsEmbedded()
{
    const QString path = buildShow();
    newComputer();
    const QByteArray mine = qxf(standardModes(), "edited here");
    const QByteArray myScript = effectJs(true, "edited here");
    const QByteArray myRgb = rgbJs("edited here");
    write(userFixture(), mine);
    write(userEffect(), myScript);
    write(userRgb(), myRgb);
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();

    QString error;
    QVERIFY2(e->useComputers(kKey, &error), qPrintable(error));
    QVERIFY2(e->useComputers("effect:my-chase.js", &error), qPrintable(error));
    QVERIFY2(e->useComputers("rgb:my-rgb.js", &error), qPrintable(error));
    QVERIFY(e->differences().isEmpty());
    QCOMPARE(fixtureNamed(doc, "Row 1")->fixtureDef()->definitionSourceFile(), userFixture());
    QCOMPARE(fixtureNamed(doc, "Row 2")->fixtureDef()->definitionSourceFile(), userFixture());
    QCOMPARE(lookNamed(doc, "Chase look")->scriptPath(), userEffect());
    QCOMPARE(matrixScriptFile(doc), userRgb());
    QCOMPARE(theMatrix(doc)->property("size"), QString("3"));
    QVERIFY(doc->isModified());

    // Saved: this computer's copies are what the show carries now.
    const QString again = m_root->filePath("shows/B/show.qlcc");
    save(doc, again, true);
    QCOMPARE(e->item(kKey).bytes, mine);
    QCOMPARE(e->item("effect:my-chase.js").bytes, myScript);
    QCOMPARE(e->item("rgb:my-rgb.js").bytes, myRgb);
    delete doc;

    doc = startDoc();
    open(doc, again);
    QVERIFY(doc->showEmbedding()->differences().isEmpty());
    delete doc;
}

void ShowEmbedding_Test::useThisComputersRefusesWhatWouldOverlap()
{
    const QString path = buildShow();          // Row 1 at 1, Row 2 at 11, 4 channels each
    newComputer();
    write(userFixture(), qxf(Modes() << qMakePair(QString("4ch"), QStringList() << "Dimmer" << "Red" << "Green"
                                                  << "Blue" << "White" << "Pan" << "Tilt" << "Extra 1"
                                                  << "Extra 2" << "Extra 3" << "Extra 4" << "Extra 5")));
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    QVERIFY(!e->compatibility(kKey).compatible);
    QString error;
    QVERIFY(!e->useComputers(kKey, &error));
    QVERIFY2(error.contains("Row 1") && error.contains("Row 2"), qPrintable(error));
    // Nothing changed.
    QCOMPARE(fixtureNamed(doc, "Row 1")->channels(), quint32(4));
    QCOMPARE(e->differences().size(), 1);
    delete doc;

    // Room to grow: switched (asked first, in the app), and re-sized.
    newComputer();
    write(userFixture(), qxf(Modes() << qMakePair(QString("4ch"), QStringList() << "Dimmer" << "Red" << "Green"
                                                  << "Blue" << "White")));
    doc = startDoc();
    open(doc, path);
    QVERIFY2(doc->showEmbedding()->useComputers(kKey, &error), qPrintable(error));
    QCOMPARE(fixtureNamed(doc, "Row 1")->channels(), quint32(5));
    delete doc;
}

void ShowEmbedding_Test::keepTheShowsIsRemembered()
{
    const QString path = buildShow();
    newComputer();
    write(userFixture(), qxf(standardModes(), "edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    QCOMPARE(e->differences().size(), 1);
    e->keepShows(kKey);
    QVERIFY(e->differences().isEmpty());
    QVERIFY(under(fixtureNamed(doc, "Row 1")->fixtureDef()->definitionSourceFile(), e->workingDirectory()));
    const QString again = m_root->filePath("shows/B/show.qlcc");
    save(doc, again, true);
    QVERIFY(QString::fromUtf8(read(again)).contains("KeptAgainst"));
    delete doc;

    doc = startDoc();
    open(doc, again);
    QVERIFY(doc->showEmbedding()->differences().isEmpty());
    delete doc;

    // This computer's copy changed again: it is worth a word again.
    write(userFixture(), qxf(standardModes(), "edited again"));
    doc = startDoc();
    open(doc, again);
    QCOMPARE(doc->showEmbedding()->differences().size(), 1);
    delete doc;
}

void ShowEmbedding_Test::installTheShowsReplacesThisComputersFile()
{
    const QString path = buildShow();
    newComputer();
    write(userFixture(), qxf(standardModes(), "edited here"));
    write(userEffect(), effectJs(true, "edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    ShowEmbedding *e = doc->showEmbedding();
    QString error;
    QVERIFY2(e->installShows(kKey, &error), qPrintable(error));
    QVERIFY2(e->installShows("effect:my-chase.js", &error), qPrintable(error));
    QCOMPARE(read(userFixture()), qxf(standardModes()));
    QCOMPARE(read(userEffect()), effectJs(true));
    QVERIFY(e->differences().isEmpty());
    QCOMPARE(e->showOnly().size(), 1);                       // the RGB script: only the show's
    QCOMPARE(fixtureNamed(doc, "Row 1")->fixtureDef()->definitionSourceFile(), userFixture());
    QCOMPARE(lookNamed(doc, "Chase look")->scriptPath(), userEffect());
    delete doc;
}

void ShowEmbedding_Test::saveNeverSwapsVersions()
{
    const QString path = buildShow();
    newComputer();
    write(userFixture(), qxf(standardModes(), "edited here"));
    write(userEffect(), effectJs(true, "edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    const QString again = m_root->filePath("shows/B/show.qlcc");
    save(doc, again, true);
    ShowEmbedding *e = doc->showEmbedding();
    QCOMPARE(e->item(kKey).bytes, qxf(standardModes()));
    QCOMPARE(e->item("effect:my-chase.js").bytes, effectJs(true));
    QCOMPARE(e->item(kKey).from, QString("user"));          // where it first came from
    QCOMPARE(e->differences().size(), 2);                    // still to be reviewed
    delete doc;
}

void ShowEmbedding_Test::closingPutsThisComputersBack()
{
    const QString path = buildShow();
    newComputer();
    write(userFixture(), qxf(standardModes(), "edited here"));
    write(userEffect(), effectJs(true, "edited here"));
    write(userRgb(), rgbJs("edited here"));
    Doc *doc = startDoc();
    open(doc, path);
    const QString work = doc->showEmbedding()->workingDirectory();
    QVERIFY(QDir(work).exists());
    doc->clearContents();
    QCOMPARE(doc->fixtureDefCache()->fixtureDef(kMake, kModel)->definitionSourceFile(), userFixture());
    QCOMPARE(doc->effectScriptRunner()->cache()->scriptPath("my-chase"), userEffect());
    QCOMPARE(doc->rgbScriptsCache()->scriptFile("My RGB"), userRgb());
    QVERIFY(doc->showEmbedding()->items().isEmpty());
    QVERIFY(!QDir(work).exists());
    delete doc;

    // A computer without them: gone with the show.
    newComputer();
    doc = startDoc();
    open(doc, path);
    doc->clearContents();
    QVERIFY(doc->fixtureDefCache()->fixtureDef(kMake, kModel) == nullptr);
    QVERIFY(doc->rgbScriptsCache()->scriptFile("My RGB").isEmpty());
    QVERIFY(doc->effectScriptRunner()->cache()->scriptPath("my-chase").isEmpty());
    delete doc;
}

void ShowEmbedding_Test::aFileThatIsNotAShowLeavesTheOpenOneAlone()
{
    const QString path = buildShow();
    newComputer();
    Doc *doc = startDoc();
    open(doc, path);
    QCOMPARE(doc->showEmbedding()->showOnly().size(), 3);
    QVERIFY(!doc->showEmbedding()->readWorkspace(m_root->filePath("not-there.qlcc")));
    QVERIFY(write(m_root->filePath("notes.qlcc"), "<?xml version='1.0'?><!DOCTYPE Notes><Notes/>"));
    QVERIFY(!doc->showEmbedding()->readWorkspace(m_root->filePath("notes.qlcc")));
    QCOMPARE(doc->showEmbedding()->showOnly().size(), 3);
    delete doc;
}

void ShowEmbedding_Test::aCollectedFolderOpensCompleteAsQxw()
{
    const QString path = buildShow();
    // On A: what it carries, collected into a folder beside a .qxw of it.
    Doc *doc = startDoc();
    open(doc, path);
    HeadLayout map;
    map.key = "Test Maker|Step Row|4ch";
    map.size = QSize(2, 2);
    map.cells << QPoint(0, 0) << QPoint(1, 0) << QPoint(0, 1) << QPoint(1, 1);
    doc->monitorProperties()->setHeadLayout(map);
    doc->showEmbedding()->collect();
    const QString folder = m_root->filePath("transfer/Show");
    QStringList written;
    QVERIFY(doc->showEmbedding()->writeTransferFolder(folder, &written));
    QCOMPARE(written.size(), 4);
    QVERIFY(QFile::exists(folder + "/HeadLayouts/" + HeadLayoutLibrary::fileNameFor(map.key)));
    QVERIFY(read(folder + "/HeadLayouts/" + HeadLayoutLibrary::fileNameFor(map.key)).contains("Test Maker|Step Row|4ch"));
    QVERIFY(QFile::exists(folder + "/Fixtures/Test-Maker-Step-Row.qxf"));
    QVERIFY(QFile::exists(folder + "/EffectScripts/my-chase.js"));
    QVERIFY(QFile::exists(folder + "/RGBScripts/my-rgb.js"));
    QCOMPARE(read(folder + "/EffectScripts/my-chase.js"), effectJs(true));
    save(doc, folder + "/Show.qxw", false);
    delete doc;

    // A clean computer: complete, from the folder.
    newComputer();
    doc = startDoc();
    open(doc, folder + "/Show.qxw");
    Fixture *row = fixtureNamed(doc, "Row 1");
    QVERIFY(row != nullptr && !row->definitionUnavailable());
    QCOMPARE(row->channels(), quint32(4));
    QVERIFY(under(row->fixtureDef()->definitionSourceFile(), folder));
    QVERIFY(under(lookNamed(doc, "Chase look")->scriptPath(), folder));
    QVERIFY(under(matrixScriptFile(doc), folder));
    QCOMPARE(doc->showEmbedding()->showOnly().size(), 3);
    foreach (const ShowEmbedding::Item &it, doc->showEmbedding()->showOnly())
    {
        QVERIFY(it.fromShowFolder);
        QCOMPARE(it.from, QString("showfolder"));
    }
    // Saved as a .qlcc from there: it carries them.
    save(doc, m_root->filePath("transfer/again.qlcc"), true);
    QCOMPARE(doc->showEmbedding()->items().size(), 3);
    delete doc;
    // ...and the folder's files were never touched by closing it.
    QVERIFY(QFile::exists(folder + "/Fixtures/Test-Maker-Step-Row.qxf"));
}

QTEST_MAIN(ShowEmbedding_Test)
