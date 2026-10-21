/*
  Q Light Controller Plus - Unit test
  showdifferences_test.cpp

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
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "showdifferencesdialog.h"
#include "qlcfixturedefcache.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "showembedding.h"
#include "qlcfixturedef.h"
#include "showstatus.h"
#include "qlcpalette.h"
#include "apputil.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private

#include "showdifferences_test.h"

namespace
{
    /* The user folder is under HOME on macOS and Linux and USERPROFILE on
       Windows (QLCFile::userDirectory): point both, or on Windows the test
       reads the real user's definitions. */
    void setHome(const QString &home)
    {
        qputenv("HOME", home.toUtf8());
        qputenv("USERPROFILE", home.toUtf8());
    }

    void restoreHome(const QByteArray &home, const QByteArray &profile)
    {
        qputenv("HOME", home);
        if (profile.isEmpty())
            qunsetenv("USERPROFILE");
        else
            qputenv("USERPROFILE", profile);
    }
    QByteArray definition(const QString &note)
    {
        return QString("<?xml version='1.0' encoding='UTF-8'?>\n<!DOCTYPE FixtureDefinition>\n"
                       "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
                       " <!-- %1 -->\n"
                       " <Manufacturer>Test Maker</Manufacturer>\n <Model>Bar</Model>\n"
                       " <Type>Color Changer</Type>\n"
                       " <Channel Name=\"Dimmer\" Preset=\"IntensityMasterDimmer\"/>\n"
                       " <Channel Name=\"Red\" Preset=\"IntensityRed\"/>\n"
                       " <Mode Name=\"2ch\">\n  <Channel Number=\"0\">Dimmer</Channel>\n"
                       "  <Channel Number=\"1\">Red</Channel>\n </Mode>\n"
                       "</FixtureDefinition>\n").arg(note).toUtf8();
    }

    QByteArray script(bool withSpeed)
    {
        return QString("(function() {\n var effect = new Object;\n effect.apiVersion = 1;\n"
                       " effect.name = \"Bar Chase\";\n effect.parameters = [\n%1"
                       "  { name: \"mode\", description: \"Mode\", min: 0, max: 1, defaultValue: 0 }\n ];\n"
                       " effect.tick = function(fixtures) { return fixtures.map(function() { return {}; }); };\n"
                       " return effect;\n})()\n")
            .arg(withSpeed ? "  { name: \"speed\", description: \"Speed\", min: 0, max: 3, defaultValue: 1 },\n" : "")
            .toUtf8();
    }

    void write(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(bytes);
    }

    /* A computer with these of its own, and a show open in it. */
    Doc *computer(QObject *parent, const QString &home, const QByteArray &def, const QByteArray &js)
    {
        setHome(home);
        if (!def.isEmpty())
            write(QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath("Test-Maker-Bar.qxf"), def);
        if (!js.isEmpty())
            write(EffectScriptCache::userScriptsDirectory().absoluteFilePath("bar-chase.js"), js);
        Doc *doc = new Doc(parent);
        doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
        doc->effectScriptRunner()->cache()->rescan();
        return doc;
    }

    void save(Doc *doc, const QString &path)
    {
        doc->showEmbedding()->collect();
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return;
        QXmlStreamWriter w(&f);
        w.writeStartDocument();
        w.writeDTD(QStringLiteral("<!DOCTYPE Workspace>"));
        w.writeStartElement(QStringLiteral("Workspace"));
        doc->showEmbedding()->saveXML(&w);
        doc->saveXML(&w);
        w.writeEndElement();
        w.writeEndDocument();
    }

    void open(Doc *doc, const QString &path)
    {
        doc->clearContents();
        doc->setWorkspacePath(QFileInfo(path).absolutePath());
        doc->showEmbedding()->readWorkspace(path);
        doc->showEmbedding()->apply(QFileInfo(path).absolutePath());
        QXmlStreamReader *xml = QLCFile::getXMLReader(path);
        while (!xml->atEnd())
            if (xml->readNext() == QXmlStreamReader::DTD)
                break;
        xml->readNextStartElement();
        while (xml->readNextStartElement())
        {
            if (xml->name() == QLatin1String("Engine"))
                doc->loadXML(*xml, false);
            else
                xml->skipCurrentElement();
        }
        QLCFile::releaseXMLReader(xml);
    }

    /* Computer A's show: two fixtures of its own definition, a look on its
       own script. */
    QString buildShow(QObject *parent, const QString &root)
    {
        Doc *doc = computer(parent, root + "/A", definition("A"), script(true));
        QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Test Maker", "Bar");
        for (int i = 0; i < 2; ++i)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("Bar %1").arg(i + 1));
            fx->setUniverse(0);
            fx->setAddress(quint32(i * 10));
            fx->setFixtureDefinition(def, def->mode("2ch"));
            doc->addFixture(fx);
        }
        QLCPalette *look = new QLCPalette(QLCPalette::Effect, doc);
        look->setName("Bar look");
        look->setScriptPath(doc->effectScriptRunner()->cache()->scriptPath("bar-chase"));
        look->setEffectParamValue("speed", 2.0);
        doc->addPalette(look);
        const QString path = root + "/show.qlcc";
        save(doc, path);
        delete doc;
        return path;
    }

    ShowStatus::Entry entry(const QString &key)
    {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    }
}

void ShowDifferences_Test::initTestCase()
{
    m_realHome = qgetenv("HOME");
    m_realProfile = qgetenv("USERPROFILE");
    ShowEmbedding::setDeleteInsteadOfTrash(true);
}

void ShowDifferences_Test::cleanupTestCase()
{
    restoreHome(m_realHome, m_realProfile);
    ShowEmbedding::setDeleteInsteadOfTrash(false);
}

void ShowDifferences_Test::theFooterSaysWhatCameWithTheShow()
{
    QTemporaryDir root;
    const QString path = buildShow(this, root.path());

    // A computer without them: information, not a problem.
    Doc *doc = computer(this, root.filePath("C"), QByteArray(), QByteArray());
    open(doc, path);
    AppUtil::updateShowEmbeddingStatus(doc);
    const ShowStatus::Entry info = entry("embedded.showonly");
    QCOMPARE(int(info.severity), int(ShowStatus::Ok));
    QVERIFY2(info.summary.contains("2 definition"), qPrintable(info.summary));
    QCOMPARE(info.items.size(), 2);
    QVERIFY(entry("embedded.differ").key.isEmpty());

    // Installed: nothing more to say.
    foreach (const QVariant &key, info.itemData)
        QVERIFY(doc->showEmbedding()->installShows(key.toString()));
    AppUtil::updateShowEmbeddingStatus(doc);
    QVERIFY(entry("embedded.showonly").key.isEmpty());
    QVERIFY(QFile::exists(QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath("Test-Maker-Bar.qxf")));
    delete doc;
    AppUtil::updateShowEmbeddingStatus(nullptr);
    restoreHome(m_realHome, m_realProfile);
}

void ShowDifferences_Test::reviewSwitchesTheCompatibleAndKeepsTheRest()
{
    QTemporaryDir root;
    const QString path = buildShow(this, root.path());

    // B: its own edit of the definition (same channels), and of the script
    // (without the look's "speed").
    Doc *doc = computer(this, root.filePath("B"), definition("B"), script(false));
    open(doc, path);
    AppUtil::updateShowEmbeddingStatus(doc);
    const ShowStatus::Entry warn = entry("embedded.differ");
    QCOMPARE(int(warn.severity), int(ShowStatus::Warning));
    QVERIFY2(warn.summary.contains("2 item"), qPrintable(warn.summary));

    ShowDifferencesDialog dlg(doc);
    QCOMPARE(dlg.keys().size(), 2);
    dlg.useComputersForAllCompatible();
    // The definition switched (compatible); the script did not.
    QCOMPARE(dlg.keys(), QStringList() << "effect:bar-chase.js");
    foreach (Fixture *fx, doc->fixtures())
        QVERIFY(!fx->fixtureDef()->definitionSourceFile().contains(doc->showEmbedding()->workingDirectory()));

    dlg.select(dlg.keys());
    dlg.keepShows();
    QVERIFY(dlg.keys().isEmpty());
    AppUtil::updateShowEmbeddingStatus(doc);
    QVERIFY(entry("embedded.differ").key.isEmpty());
    delete doc;
    restoreHome(m_realHome, m_realProfile);
}

/* The app itself: Save as .qlcc carries the show's own definitions, .qxw
   does not; opening it on a computer whose copy differs says so. */
void ShowDifferences_Test::theAppCarriesThemInAQlccOnly()
{
    QTemporaryDir root;
    setHome(root.filePath("A"));
    write(QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath("Test-Maker-Bar.qxf"), definition("A"));
    App *app = new App();
    app->init();
    Doc *doc = app->doc();
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef("Test Maker", "Bar");
    QVERIFY(def != nullptr);
    Fixture *fx = new Fixture(doc);
    fx->setName("Bar 1");
    fx->setUniverse(0);
    fx->setAddress(0);
    fx->setFixtureDefinition(def, def->mode("2ch"));
    QVERIFY(doc->addFixture(fx));

    const QString qlcc = root.filePath("show.qlcc"), qxw = root.filePath("show.qxw");
    QCOMPARE(app->saveXML(qlcc), QFile::NoError);
    QCOMPARE(app->saveXML(qxw), QFile::NoError);
    QFile a(qlcc), b(qxw);
    QVERIFY(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly));
    const QString withIt = QString::fromUtf8(a.readAll()), without = QString::fromUtf8(b.readAll());
    QVERIFY(withIt.contains("<Embedded>"));
    QVERIFY(withIt.contains(ShowEmbedding::sha256(definition("A"))));
    QVERIFY(!without.contains("<Embedded>"));

    // This computer's copy changes; the show, opened again, keeps its own.
    write(QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath("Test-Maker-Bar.qxf"), definition("A, edited"));
    app->clearDocument();
    QCOMPARE(app->loadXML(qlcc), QFile::NoError);
    QCOMPARE(doc->fixtures().size(), 1);
    QVERIFY(doc->fixtures().first()->fixtureDef()->definitionSourceFile()
                .startsWith(doc->showEmbedding()->workingDirectory()));
    QCOMPARE(int(entry("embedded.differ").severity), int(ShowStatus::Warning));

    app->clearDocument();
    QVERIFY(entry("embedded.differ").key.isEmpty());
    delete app;
    restoreHome(m_realHome, m_realProfile);
}

QTEST_MAIN(ShowDifferences_Test)
