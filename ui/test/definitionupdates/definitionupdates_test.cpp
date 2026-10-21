/*
  Q Light Controller Plus - Unit test
  definitionupdates_test.cpp

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
#include <QSettings>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "definitionupdatesdialog.h"
#include "showdifferencesdialog.h"
#include "fixturedefupdates.h"
#include "qlcfixturedefcache.h"
#include "showembedding.h"
#include "qlcfixturedef.h"
#include "showstatus.h"
#include "apputil.h"
#include "fixture.h"
#include "qlcfile.h"
#include "scene.h"
#include "doc.h"

#include "definitionupdates_test.h"

namespace
{
    const QString kMake = QStringLiteral("Test Maker");
    const QString kOld = QStringLiteral("4.14.2+qlcconsole.1393");
    const QString kNew = QStringLiteral("4.14.2+qlcconsole.1500");

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

    /* Pan saved fine (@p panFine) or coarse; @p tiltDefault; a second mode. */
    QByteArray spot(const QString &version, bool panFine, int tiltDefault, bool fourCh,
                    const QString &model = QStringLiteral("Spot"))
    {
        QString x = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE FixtureDefinition>\n"
                    "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
                    " <Creator>\n  <Name>qlcconsole</Name>\n  <Version>" + version + "</Version>\n"
                    "  <Author>Test Author</Author>\n </Creator>\n"
                    " <Manufacturer>" + kMake + "</Manufacturer>\n <Model>" + model + "</Model>\n"
                    " <Type>Moving Head</Type>\n";
        x += QString(" <Channel Name=\"Pan\">\n  <Group Byte=\"%1\">Pan</Group>\n </Channel>\n").arg(panFine ? 1 : 0);
        x += QString(" <Channel Name=\"Tilt\" Default=\"%1\">\n  <Group Byte=\"0\">Tilt</Group>\n </Channel>\n").arg(tiltDefault);
        x += " <Channel Name=\"Dimmer\">\n  <Group Byte=\"0\">Intensity</Group>\n </Channel>\n";
        x += " <Mode Name=\"3ch\">\n  <Channel Number=\"0\">Pan</Channel>\n  <Channel Number=\"1\">Tilt</Channel>\n"
             "  <Channel Number=\"2\">Dimmer</Channel>\n </Mode>\n";
        if (fourCh)
            x += " <Mode Name=\"1ch\">\n  <Channel Number=\"0\">Dimmer</Channel>\n </Mode>\n";
        x += "</FixtureDefinition>\n";
        return x.toUtf8();
    }

    bool write(const QString &path, const QByteArray &bytes, const QDateTime &when = QDateTime())
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
            return false;
        f.flush();
        if (when.isValid())
            f.setFileTime(when, QFileDevice::FileModificationTime);
        return true;
    }

    QByteArray read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    QString user(const QString &name)
    {
        return QLCFixtureDefCache::userDefinitionDirectory().absoluteFilePath(name);
    }

    Doc *startDoc(QObject *parent, const QStringList &shipped = QStringList())
    {
        Doc *doc = new Doc(parent);
        doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
        for (const QString &s : shipped)
            doc->fixtureDefCache()->loadQXF(s, false);
        return doc;
    }

    Fixture *addFixture(Doc *doc, const QString &name, quint32 address)
    {
        QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(kMake, "Spot");
        if (def == nullptr)
            return nullptr;
        Fixture *fx = new Fixture(doc);
        fx->setName(name);
        fx->setUniverse(0);
        fx->setAddress(address);
        fx->setFixtureDefinition(def, def->mode("3ch"));
        doc->addFixture(fx);
        return fx;
    }

    ShowStatus::Entry entry(const QString &key)
    {
        foreach (const ShowStatus::Entry &e, ShowStatus::instance()->allEntries())
            if (e.key == key)
                return e;
        return ShowStatus::Entry();
    }

    void save(Doc *doc, const QString &path)
    {
        doc->showEmbedding()->collect();
        QDir().mkpath(QFileInfo(path).absolutePath());
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
}

void DefinitionUpdates_Test::initTestCase()
{
    m_realHome = qgetenv("HOME");
    m_realProfile = qgetenv("USERPROFILE");
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    QCoreApplication::setApplicationName(QStringLiteral("definitionupdates_test"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    ShowEmbedding::setDeleteInsteadOfTrash(true);
    DefinitionUpdatesDialog::setAutoAnswer(1);
    ShowDifferencesDialog::setAutoAnswer(1);
}

void DefinitionUpdates_Test::cleanupTestCase()
{
    restoreHome(m_realHome, m_realProfile);
    ShowEmbedding::setDeleteInsteadOfTrash(false);
    DefinitionUpdatesDialog::setAutoAnswer(-1);
    ShowDifferencesDialog::setAutoAnswer(-1);
}

void DefinitionUpdates_Test::init()
{
    m_root = new QTemporaryDir();
    QVERIFY(m_root->isValid());
    setHome(m_root->filePath("home"));
    QDir().mkpath(QLCFixtureDefCache::userDefinitionDirectory().absolutePath());
    FixtureDefUpdates::clearDismissals();
}

void DefinitionUpdates_Test::cleanup()
{
    AppUtil::updateDefinitionUpdatesStatus(nullptr);
    restoreHome(m_realHome, m_realProfile);
    delete m_root;
    m_root = nullptr;
}

void DefinitionUpdates_Test::listsUsedAndUnusedAndUpdatesWithUndo()
{
    // Used: the user's old Spot, a newer one shipped (same modes).
    const QString shipped = m_root->filePath("app/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, spot(kNew, true, 32, true)));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), spot(kOld, true, 0, false), QDateTime::currentDateTime().addDays(-30)));
    // Not used: two copies of a Wash of the user's own, the older one set aside.
    QVERIFY(write(user("Test-Maker-Wash.qxf"), spot(kOld, true, 50, true, "Wash"), QDateTime::currentDateTime().addDays(-1)));
    QVERIFY(write(user("Test-Maker-Wash 2.qxf"), spot(kOld, true, 0, false, "Wash"), QDateTime::currentDateTime().addDays(-90)));
    Doc *doc = startDoc(this, { shipped });
    QVERIFY(addFixture(doc, "Spot 1", 0) != nullptr);

    DefinitionUpdatesDialog dlg(doc);
    QCOMPARE(dlg.files().size(), 2);
    QCOMPARE(QFileInfo(dlg.files().at(0)).fileName(), QString("Test-Maker-Spot.qxf"));       // used first
    QCOMPARE(QFileInfo(dlg.files().at(1)).fileName(), QString("Test-Maker-Wash 2.qxf"));

    dlg.select({ dlg.files().at(0) });
    QVERIFY2(dlg.detailText().contains("Mode “1ch”: added"), qPrintable(dlg.detailText()));
    QVERIFY2(dlg.detailText().contains("updating is safe"), qPrintable(dlg.detailText()));

    dlg.updateSelected();
    QVERIFY2(dlg.lastReport().join('\n').contains("updated"), qPrintable(dlg.lastReport().join('\n')));
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), spot(kNew, true, 32, true));
    QCOMPARE(dlg.files().size(), 1);
    QCOMPARE(doc->fixtures().size(), 1);
    QVERIFY(doc->fixtures().first()->fixtureDef()->mode("1ch") != nullptr);

    // Undo Last Update: as it was, and listed again.
    dlg.undoLast();
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), spot(kOld, true, 0, false));
    QCOMPARE(dlg.files().size(), 2);

    // Update All: both, in one backup.
    dlg.updateAll();
    QCOMPARE(dlg.files().size(), 0);
    QCOMPARE(read(user("Test-Maker-Wash 2.qxf")), spot(kOld, true, 50, true, "Wash"));
    delete doc;
}

void DefinitionUpdates_Test::aChangedLayoutIsAskedFirst()
{
    const QString shipped = m_root->filePath("app/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, spot(kNew, false, 32, false)));      // Pan coarse now
    QVERIFY(write(user("Test-Maker-Spot.qxf"), spot(kOld, true, 0, false), QDateTime::currentDateTime().addDays(-30)));
    Doc *doc = startDoc(this, { shipped });
    Fixture *fx = addFixture(doc, "Spot 1", 0);
    Scene *s = new Scene(doc);
    s->setName("Look A");
    s->setValue(fx->id(), 0, 100);
    doc->addFunction(s);

    DefinitionUpdatesDialog dlg(doc);
    QCOMPARE(dlg.files().size(), 1);
    dlg.select(dlg.files());
    QVERIFY2(dlg.detailText().contains("needs confirming") && dlg.detailText().contains("Look A"),
             qPrintable(dlg.detailText()));

    DefinitionUpdatesDialog::setAutoAnswer(0);      // "Cancel"
    dlg.updateSelected();
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), spot(kOld, true, 0, false));
    DefinitionUpdatesDialog::setAutoAnswer(1);      // "Update anyway"
    dlg.updateSelected();
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), spot(kNew, false, 32, false));
    delete doc;
}

void DefinitionUpdates_Test::theFooterSaysSo()
{
    const QString shipped = m_root->filePath("app/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, spot(kNew, true, 32, true)));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), spot(kOld, true, 0, false), QDateTime::currentDateTime().addDays(-30)));
    Doc *doc = startDoc(this, { shipped });

    // Not used by the show: information.
    AppUtil::updateDefinitionUpdatesStatus(doc);
    ShowStatus::Entry e = entry("definitions.outofdate");
    QCOMPARE(int(e.severity), int(ShowStatus::Ok));
    QCOMPARE(e.items.size(), 1);
    QCOMPARE(e.itemData.first().toString(), user("Test-Maker-Spot.qxf"));

    // Used: a warning.
    addFixture(doc, "Spot 1", 0);
    AppUtil::updateDefinitionUpdatesStatus(doc);
    e = entry("definitions.outofdate");
    QCOMPARE(int(e.severity), int(ShowStatus::Warning));
    QCOMPARE(e.itemGroups.first(), QString("Used by this show"));

    // Dealt with: gone.
    FixtureDefUpdates::keepMine(FixtureDefUpdates::scan(doc).candidates.first());
    AppUtil::updateDefinitionUpdatesStatus(doc);
    QVERIFY(entry("definitions.outofdate").key.isEmpty());
    delete doc;
}

void DefinitionUpdates_Test::showDefinitionsListsEverythingAndUndoes()
{
    // Saved with the old version...
    QVERIFY(write(user("Test-Maker-Spot.qxf"), spot(kOld, true, 0, false)));
    QVERIFY(write(user("Test-Maker-Wash.qxf"), spot(kOld, true, 0, false, "Wash")));
    Doc *a = startDoc(this);
    addFixture(a, "Spot 1", 0);
    QLCFixtureDef *wash = a->fixtureDefCache()->fixtureDef(kMake, "Wash");
    Fixture *w = new Fixture(a);
    w->setName("Wash 1");
    w->setUniverse(0);
    w->setAddress(20);
    w->setFixtureDefinition(wash, wash->mode("3ch"));
    a->addFixture(w);
    const QString show = m_root->filePath("shows/show.qlcc");
    save(a, show);
    delete a;

    // ...opened with a newer Spot here (the Wash the same).
    QVERIFY(write(user("Test-Maker-Spot.qxf"), spot(kNew, true, 32, true)));
    Doc *doc = startDoc(this);
    open(doc, show);
    ShowEmbedding *e = doc->showEmbedding();
    const QString key = QStringLiteral("fixture:Test Maker|Spot");

    ShowDifferencesDialog review(doc);
    QCOMPARE(review.keys(), QStringList() << key);

    ShowDifferencesDialog all(doc, nullptr, true);
    QCOMPARE(all.keys().size(), 2);                 // the identical Wash too
    all.select({ key });
    QVERIFY2(all.detailText().contains("This computer's copy, compared with the show's")
             && all.detailText().contains("Mode “1ch”: added"), qPrintable(all.detailText()));

    Fixture *spot1 = nullptr;
    foreach (Fixture *fx, doc->fixtures())
        if (fx->name() == "Spot 1")
            spot1 = fx;
    QVERIFY(spot1 != nullptr);
    QLCFixtureDef *showDef = spot1->fixtureDef();
    all.useComputers();
    QVERIFY(!e->item(key).usingShow);
    QVERIFY(spot1->fixtureDef() != showDef);
    all.undoChoices();
    QVERIFY(e->item(key).usingShow);
    QCOMPARE(spot1->fixtureDef(), showDef);
    QCOMPARE(all.keys().size(), 2);
    delete doc;
}

QTEST_MAIN(DefinitionUpdates_Test)
