/*
  Q Light Controller Plus - Unit test
  fixturedefupdates_test.cpp

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
#include <QSettings>

#include "fixturedefupdates_test.h"
#include "fixturedefupdates.h"
#include "qlcfixturedefcache.h"
#include "showembedding.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcchannel.h"
#include "scenevalue.h"
#include "fixture.h"
#include "qlcfile.h"
#include "scene.h"
#include "doc.h"

#include "../common/resource_paths.h"

using namespace FixtureDefUpdates;

namespace
{
    const QString kMake = QStringLiteral("Test Maker");
    const QString kSpot = QStringLiteral("Spot");
    const QString kOld = QStringLiteral("4.14.2+qlcconsole.1393");
    const QString kNew = QStringLiteral("4.14.2+qlcconsole.1500");
    const QString kAuthor = QStringLiteral("Test Author");

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

    struct Ch
    {
        QString name;
        QString group;
        int byte;
        int def;
    };
    typedef QList<QPair<QString, QStringList> > Modes;

    QByteArray qxf(const QString &version, const QList<Ch> &chans, const Modes &modes,
                   const QString &model = kSpot, const QString &author = kAuthor)
    {
        QString x = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE FixtureDefinition>\n"
                    "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
                    " <Creator>\n  <Name>qlcconsole</Name>\n  <Version>" + version + "</Version>\n"
                    "  <Author>" + author + "</Author>\n </Creator>\n"
                    " <Manufacturer>" + kMake + "</Manufacturer>\n <Model>" + model + "</Model>\n"
                    " <Type>Moving Head</Type>\n";
        for (const Ch &c : chans)
            x += QString(" <Channel Name=\"%1\" Default=\"%4\">\n  <Group Byte=\"%3\">%2</Group>\n"
                         "  <Capability Min=\"0\" Max=\"255\">All</Capability>\n </Channel>\n")
                     .arg(c.name, c.group).arg(c.byte).arg(c.def);
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

    /* The 36LED story: Pan was saved as a fine channel; the fix makes it
       coarse, with sensible defaults, and adds a mode. */
    QByteArray v1(const QString &version = kOld, const QString &author = kAuthor)
    {
        return qxf(version, { { "Pan", "Pan", 1, 0 }, { "Tilt", "Tilt", 0, 0 }, { "Dimmer", "Intensity", 0, 0 } },
                   { { "3ch", { "Pan", "Tilt", "Dimmer" } } }, kSpot, author);
    }
    QByteArray v2Fixed(const QString &version = kNew)
    {
        return qxf(version, { { "Pan", "Pan", 0, 128 }, { "Tilt", "Tilt", 0, 32 }, { "Dimmer", "Intensity", 0, 0 },
                              { "Speed", "Speed", 0, 0 } },
                   { { "3ch", { "Pan", "Tilt", "Dimmer" } }, { "4ch", { "Pan", "Tilt", "Dimmer", "Speed" } } });
    }
    /* Newer, and every mode the old one had is as it was. */
    QByteArray v2Clean(const QString &version = kNew)
    {
        return qxf(version, { { "Pan", "Pan", 1, 0 }, { "Tilt", "Tilt", 0, 32 }, { "Dimmer", "Intensity", 0, 0 },
                              { "Speed", "Speed", 0, 0 } },
                   { { "3ch", { "Pan", "Tilt", "Dimmer" } }, { "4ch", { "Pan", "Tilt", "Dimmer", "Speed" } } });
    }
    QByteArray v2Longer()
    {
        return qxf(kNew, { { "Pan", "Pan", 1, 0 }, { "Tilt", "Tilt", 0, 0 }, { "Dimmer", "Intensity", 0, 0 },
                           { "Speed", "Speed", 0, 0 } },
                   { { "3ch", { "Pan", "Tilt", "Dimmer", "Speed" } } });
    }
    QByteArray v2Renamed()
    {
        return qxf(kNew, { { "Pan", "Pan", 1, 0 }, { "Tilt", "Tilt", 0, 0 }, { "Dimmer", "Intensity", 0, 0 } },
                   { { "3-channel", { "Pan", "Tilt", "Dimmer" } } });
    }

    bool write(const QString &path, const QByteArray &bytes, const QDateTime &when = QDateTime())
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
            return false;
        f.flush();          // or closing it would make it "now" again
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

    QDateTime daysAgo(int n)
    {
        return QDateTime::currentDateTime().addDays(-n);
    }

    Doc *startDoc(QObject *parent, const QStringList &shipped = QStringList())
    {
        Doc *doc = new Doc(parent);
        doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
        for (const QString &s : shipped)
            doc->fixtureDefCache()->loadQXF(s, false);
        return doc;
    }

    Fixture *addFixture(Doc *doc, const QString &name, quint32 address, const QString &mode = "3ch")
    {
        QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(kMake, kSpot);
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

    Scene *addScene(Doc *doc, const QString &name, Fixture *fx)
    {
        Scene *s = new Scene(doc);
        s->setName(name);
        s->setValue(fx->id(), 0, 200);
        doc->addFunction(s);
        return s;
    }

    const Candidate *candidateFor(const Scan &s, const QString &path)
    {
        for (const Candidate &c : s.candidates)
            if (QFileInfo(c.mine.path).absoluteFilePath() == QFileInfo(path).absoluteFilePath())
                return &c;
        return nullptr;
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

    /* What App::loadXML does: the show's copies first, then its contents. */
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

    const QString kKey = QStringLiteral("fixture:Test Maker|Spot");
}

void FixtureDefUpdates_Test::initTestCase()
{
    m_realHome = qgetenv("HOME");
    m_realProfile = qgetenv("USERPROFILE");
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    QCoreApplication::setApplicationName(QStringLiteral("fixturedefupdates_test"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    ShowEmbedding::setDeleteInsteadOfTrash(true);
}

void FixtureDefUpdates_Test::cleanupTestCase()
{
    restoreHome(m_realHome, m_realProfile);
    ShowEmbedding::setDeleteInsteadOfTrash(false);
}

void FixtureDefUpdates_Test::init()
{
    m_root = new QTemporaryDir();
    QVERIFY(m_root->isValid());
    setHome(m_root->filePath("home"));
    QDir().mkpath(QLCFixtureDefCache::userDefinitionDirectory().absolutePath());
    clearDismissals();
}

void FixtureDefUpdates_Test::cleanup()
{
    restoreHome(m_realHome, m_realProfile);
    delete m_root;
    m_root = nullptr;
}

void FixtureDefUpdates_Test::versions()
{
    bool known = false;
    QCOMPARE(compareVersions("4.12.6", "4.14.2+qlcconsole.1393", &known), -1);
    QVERIFY(known);
    QCOMPARE(compareVersions("4.14.2", "4.14.2+qlcconsole.1", &known), -1);
    QCOMPARE(compareVersions("4.14.2+qlcconsole.1500", "4.14.2+qlcconsole.1393"), 1);
    QCOMPARE(compareVersions("4.13.0 GIT", "4.13.0"), 0);
    QCOMPARE(compareVersions("4.10", "4.9.9"), 1);
    // A later QLC+ is another line than a qlcconsole build: can't tell.
    QCOMPARE(compareVersions("4.14.4", "4.14.2+qlcconsole.1500", &known), 0);
    QVERIFY(!known);
    QCOMPARE(compareVersions("4.14.2+qlcconsole.1500", "4.14.4", &known), 0);
    QVERIFY(!known);
    QCOMPARE(compareVersions("4.14.2+qlcconsole.1", "4.14.2", &known), 1);
    QVERIFY(known);
    // Since 0.3.1 a build is based on 4.14.4: newer than 4.14.4 itself and
    // than every build before it, whose base was 4.14.2.
    QCOMPARE(compareVersions("4.14.4+qlcconsole.2390", "4.14.4", &known), 1);
    QVERIFY(known);
    QCOMPARE(compareVersions("4.14.4+qlcconsole.2390", "4.14.2+qlcconsole.2347", &known), 1);
    QVERIFY(known);
    QCOMPARE(compareVersions("", "4.12.0", &known), 0);
    QVERIFY(!known);
}

void FixtureDefUpdates_Test::olderIsNotJustDifferent()
{
    const Info current = infoFromBytes(v2Clean());
    QCOMPARE(current.creatorVersion, kNew);
    QCOMPARE(current.author, kAuthor);
    QCOMPARE(current.manufacturer, kMake);
    QCOMPARE(current.model, kSpot);

    // An older version of the same author's: out of date.
    QCOMPARE(int(judge(infoFromBytes(v1()), current)), int(OutOfDate));
    // The same bytes: nothing to do.
    QCOMPARE(int(judge(infoFromBytes(v2Clean()), current)), int(Identical));
    // Someone else's: customised, never "old".
    QCOMPARE(int(judge(infoFromBytes(v1(kOld, "Somebody Else")), current)), int(Customised));
    // Older, but with a mode of its own: made on purpose.
    const QByteArray extra = qxf(kOld, { { "Pan", "Pan", 1, 0 }, { "Tilt", "Tilt", 0, 0 }, { "Dimmer", "Intensity", 0, 0 } },
                                 { { "3ch", { "Pan", "Tilt", "Dimmer" } }, { "My mode", { "Dimmer" } } });
    QCOMPARE(int(judge(infoFromBytes(extra), current)), int(Customised));
    // Newer than what is called current.
    QCOMPARE(int(judge(infoFromBytes(v2Clean("4.15.0+qlcconsole.1")), current)), int(Newer));
    // Saved by a later QLC+ than the build grew from: the times decide.
    {
        Info qlcplus = infoFromBytes(v1("4.14.4"));
        Info ours = current;
        qlcplus.modified = daysAgo(100);
        ours.modified = daysAgo(5);
        QCOMPARE(int(judge(qlcplus, ours)), int(OutOfDate));
        QCOMPARE(int(compare(ours, qlcplus)), 1);
    }

    // The same version: the user's own files go by the time they changed...
    Info mine = infoFromBytes(v1(kNew));
    Info cur = current;
    mine.modified = daysAgo(10);
    cur.modified = daysAgo(1);
    QCOMPARE(int(judge(mine, cur)), int(OutOfDate));
    // ...a shipped one's time means nothing: can't tell, so not "old".
    cur.shipped = true;
    QCOMPARE(int(judge(mine, cur)), int(Customised));
}

void FixtureDefUpdates_Test::theDiffSaysWhatChanges()
{
    const QList<Change> changes = diff(v1(), v2Fixed());
    QStringList texts;
    bool layout3ch = false, added4ch = false, panChanged = false, speedAdded = false;
    for (const Change &c : changes)
    {
        texts << c.text();
        if (c.area == Change::Mode && c.name == "3ch" && c.kind == Change::Changed && c.layout)
            layout3ch = c.detail.contains("channel 1 is Pan, was Pan (fine)");
        if (c.area == Change::Mode && c.name == "4ch" && c.kind == Change::Added)
            added4ch = true;
        if (c.area == Change::Channel && c.name == "Pan" && c.kind == Change::Changed)
            panChanged = c.detail.contains("default 0 → 128");
        if (c.area == Change::Channel && c.name == "Speed" && c.kind == Change::Added)
            speedAdded = true;
    }
    QVERIFY2(layout3ch, qPrintable(texts.join('\n')));
    QVERIFY2(added4ch, qPrintable(texts.join('\n')));
    QVERIFY2(panChanged, qPrintable(texts.join('\n')));
    QVERIFY2(speedAdded, qPrintable(texts.join('\n')));
    QCOMPARE(changes.first().area, Change::Mode);     // modes first

    QVERIFY(diff(v1(), v1()).isEmpty());

    QLCFixtureDef a, b, c, d;
    QVERIFY(a.loadXMLData(v1()));
    QVERIFY(b.loadXMLData(v2Clean()));
    QVERIFY(c.loadXMLData(v2Longer()));
    QVERIFY(d.loadXMLData(v2Renamed()));
    QCOMPARE(int(modeFit(&a, &b, "3ch")), int(SameLayout));
    QCOMPARE(int(modeFit(&a, &c, "3ch")), int(CountChanged));
    QCOMPARE(int(modeFit(&a, &d, "3ch")), int(ModeMissing));
}

void FixtureDefUpdates_Test::scanFindsOldCopiesUsedOrNot()
{
    // Two of the user's own copies of one fixture: the older is out of date.
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    QVERIFY(write(user("Test-Maker-Spot 2.qxf"), v1(kOld), daysAgo(30)));
    // One that comes with the app in a newer version, and the user's older copy.
    const QString shippedWash = m_root->filePath("app/Fixtures/Test-Maker-Wash.qxf");
    QVERIFY(write(shippedWash, qxf(kNew, { { "Dimmer", "Intensity", 0, 0 }, { "Strobe", "Shutter", 0, 0 } },
                                   { { "1ch", { "Dimmer" } }, { "2ch", { "Dimmer", "Strobe" } } }, "Wash")));
    QVERIFY(write(user("Test-Maker-Wash.qxf"), qxf(kOld, { { "Dimmer", "Intensity", 0, 0 } },
                                                   { { "1ch", { "Dimmer" } } }, "Wash"), daysAgo(5)));
    // One someone changed on purpose (their name on it).
    const QString shippedBar = m_root->filePath("app/Fixtures/Test-Maker-Bar.qxf");
    QVERIFY(write(shippedBar, qxf(kNew, { { "Dimmer", "Intensity", 0, 0 } }, { { "1ch", { "Dimmer" } } }, "Bar")));
    QVERIFY(write(user("Test-Maker-Bar.qxf"), qxf(kOld, { { "Dimmer", "Intensity", 0, 0 } },
                                                  { { "1ch", { "Dimmer" } } }, "Bar", "Mine Truly")));
#ifndef Q_OS_WIN
    // A link to a file that is not there.
    QVERIFY(QFile::link(m_root->filePath("nowhere/Gone.qxf"), user("Gone.qxf")));
#endif

    Doc *doc = startDoc(this, { shippedWash, shippedBar });
    // The show uses the user's Wash.
    QLCFixtureDef *wash = doc->fixtureDefCache()->fixtureDef(kMake, "Wash");
    QVERIFY(wash != nullptr && wash->source() == QLCFixtureDef::Local);
    Fixture *fx = new Fixture(doc);
    fx->setName("Wash 1");
    fx->setFixtureDefinition(wash, wash->mode("1ch"));
    doc->addFixture(fx);

    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 2);
    QCOMPARE(QFileInfo(s.candidates.at(0).mine.path).fileName(), QString("Test-Maker-Wash.qxf"));
    QVERIFY(s.candidates.at(0).usedByShow);
    QVERIFY(s.candidates.at(0).current.shipped);
    QCOMPARE(int(s.candidates.at(0).verdict), int(OutOfDate));
    QCOMPARE(QFileInfo(s.candidates.at(1).mine.path).fileName(), QString("Test-Maker-Spot 2.qxf"));
    QVERIFY(!s.candidates.at(1).usedByShow);
    QCOMPARE(QFileInfo(s.candidates.at(1).current.path).fileName(), QString("Test-Maker-Spot.qxf"));
    QVERIFY(!s.candidates.at(1).changes.isEmpty());
#ifndef Q_OS_WIN
    QCOMPARE(s.unreachable, QStringList() << "Gone.qxf");
#endif

    // The customised one only when asked for, and never as out of date.
    const Scan all = scan(doc, true);
    const Candidate *bar = candidateFor(all, user("Test-Maker-Bar.qxf"));
    QVERIFY(bar != nullptr);
    QCOMPARE(int(bar->verdict), int(Customised));
    delete doc;
}

void FixtureDefUpdates_Test::updateKeepsABackupAndUndoes()
{
    const QDateTime old = daysAgo(30);
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    QVERIFY(write(user("Test-Maker-Spot 2.qxf"), v1(kOld), old));
    Doc *doc = startDoc(this);

    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);
    QVERIFY(lastUndoable().isEmpty());
    const Result r = update(doc, s.candidates, UpdateLinkedOriginal);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QCOMPARE(r.updated.size(), 1);
    QCOMPARE(read(user("Test-Maker-Spot 2.qxf")), v2Clean(kOld));

    // The old file, as it was, in a dated folder under .backup.
    QVERIFY(r.backupFolder.startsWith(backupRoot().absolutePath()));
    QCOMPARE(read(QDir(r.backupFolder).absoluteFilePath("Test-Maker-Spot 2.qxf")), v1(kOld));
    QVERIFY(QFile::exists(QDir(r.backupFolder).absoluteFilePath("manifest.json")));
    QCOMPARE(lastUndoable(), r.backupFolder);
    QVERIFY(scan(doc).candidates.isEmpty());
    // Never loaded as a definition of its own.
    QVERIFY(!QFileInfo(user("manifest.json")).exists());

    // Undo: the file as it was, its time too, and offered again.
    const Result u = undo(doc, r.backupFolder);
    QVERIFY2(u.failed.isEmpty(), qPrintable(u.failed.join('\n')));
    QCOMPARE(read(user("Test-Maker-Spot 2.qxf")), v1(kOld));
    QVERIFY(qAbs(QFileInfo(user("Test-Maker-Spot 2.qxf")).lastModified().secsTo(old)) <= 2);
    QVERIFY(lastUndoable().isEmpty());
    QCOMPARE(scan(doc).candidates.size(), 1);

    // An update over a file changed since it was checked: refused.
    const Scan again = scan(doc);
    QVERIFY(write(user("Test-Maker-Spot 2.qxf"), v1(kOld) + "\n"));
    const Result stale = update(doc, again.candidates, UpdateLinkedOriginal);
    QVERIFY(stale.updated.isEmpty());
    QCOMPARE(stale.failed.size(), 1);
    QVERIFY(stale.backupFolder.isEmpty());
    delete doc;
}

void FixtureDefUpdates_Test::keepMineAndIgnoreThisVersion()
{
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    QVERIFY(write(user("Test-Maker-Spot 2.qxf"), v1(kOld), daysAgo(30)));
    Doc *doc = startDoc(this);

    Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);
    keepMine(s.candidates.first());
    QVERIFY(scan(doc).candidates.isEmpty());
    // Mine changed: asked again.
    QVERIFY(write(user("Test-Maker-Spot 2.qxf"), v1(kOld) + "\n", daysAgo(30)));
    s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);

    ignoreVersion(s.candidates.first());
    QVERIFY(scan(doc).candidates.isEmpty());
    // A newer current version: offered.
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kNew), daysAgo(1)));
    QCOMPARE(scan(doc).candidates.size(), 1);
    delete doc;
}

void FixtureDefUpdates_Test::aLinkUpdatesItsOriginal()
{
#ifdef Q_OS_WIN
    QSKIP("symbolic links");
#endif
    // The old QLC+ folder: synced, linked in by mergeLegacyUserData().
    const QString original = m_root->filePath("home/QLC+/Fixtures/Test-Maker-Spot 2.qxf");
    QVERIFY(write(original, v1(kOld), daysAgo(30)));
    QVERIFY(QFile::link(original, user("Test-Maker-Spot 2.qxf")));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    Doc *doc = startDoc(this);

    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);
    QVERIFY(s.candidates.first().mine.isLink);
    QVERIFY(s.candidates.first().mine.linkIntoLegacy);
    const Result r = update(doc, s.candidates, UpdateLinkedOriginal);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QCOMPARE(r.updated.size(), 1);
    QVERIFY2(r.updated.first().contains("syncs"), qPrintable(r.updated.first()));
    QVERIFY(QFileInfo(user("Test-Maker-Spot 2.qxf")).isSymLink());
    QCOMPARE(read(original), v2Clean(kOld));

    QVERIFY(undo(doc, r.backupFolder).failed.isEmpty());
    QVERIFY(QFileInfo(user("Test-Maker-Spot 2.qxf")).isSymLink());
    QCOMPARE(read(original), v1(kOld));
    delete doc;
}

void FixtureDefUpdates_Test::aLinkCanBeReplacedInstead()
{
#ifdef Q_OS_WIN
    QSKIP("symbolic links");
#endif
    const QString original = m_root->filePath("home/QLC+/Fixtures/Test-Maker-Spot 2.qxf");
    QVERIFY(write(original, v1(kOld), daysAgo(30)));
    QVERIFY(QFile::link(original, user("Test-Maker-Spot 2.qxf")));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    Doc *doc = startDoc(this);

    const Result r = update(doc, scan(doc).candidates, ReplaceLink);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QCOMPARE(r.updated.size(), 1);
    QVERIFY2(r.updated.first().contains("file of its own"), qPrintable(r.updated.first()));
    QVERIFY(!QFileInfo(user("Test-Maker-Spot 2.qxf")).isSymLink());
    QCOMPARE(read(user("Test-Maker-Spot 2.qxf")), v2Clean(kOld));
    QCOMPARE(read(original), v1(kOld));         // the synced original, left alone

    QVERIFY(undo(doc, r.backupFolder).failed.isEmpty());
    QVERIFY(QFileInfo(user("Test-Maker-Spot 2.qxf")).isSymLink());
    QCOMPARE(QFileInfo(user("Test-Maker-Spot 2.qxf")).symLinkTarget(), QFileInfo(original).absoluteFilePath());
    QCOMPARE(read(user("Test-Maker-Spot 2.qxf")), v1(kOld));
    delete doc;
}

void FixtureDefUpdates_Test::neverWritesAShippedFile()
{
#ifdef Q_OS_WIN
    QSKIP("symbolic links");
#endif
    // A link into a source tree's resources/fixtures: the app's own.
    const QString shipped = m_root->filePath("src/resources/fixtures/Test_Maker/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, v1(kOld), daysAgo(30)));
    QVERIFY(isShippedPath(shipped));
    QVERIFY(QFile::link(shipped, user("Test-Maker-Spot 2.qxf")));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    Doc *doc = startDoc(this);

    const Result r = update(doc, scan(doc).candidates, UpdateLinkedOriginal);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QCOMPARE(read(shipped), v1(kOld));          // never written
    QVERIFY(!QFileInfo(user("Test-Maker-Spot 2.qxf")).isSymLink());
    QCOMPARE(read(user("Test-Maker-Spot 2.qxf")), v2Clean(kOld));
    delete doc;
}

/* A link edited into a file of its own leaves the synced original behind,
   old, for every computer still linking to it. */
void FixtureDefUpdates_Test::aStaleCopyInTheQlcPlusFolderIsOffered()
{
    const QString legacy = legacyFixturesDirectory().absoluteFilePath("Test-Maker-Spot.qxf");
    QVERIFY(write(legacy, v1(kOld), daysAgo(300)));
    QVERIFY(write(legacyFixturesDirectory().absoluteFilePath("Other.qxf"),
                  qxf(kOld, { { "Dimmer", "Intensity", 0, 0 } }, { { "1ch", { "Dimmer" } } }, "Other")));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kOld), daysAgo(1)));
    Doc *doc = startDoc(this);

    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);       // not "Other": nothing here has it
    QVERIFY(s.candidates.first().mine.inLegacyFolder);
    QCOMPARE(QFileInfo(s.candidates.first().mine.path).absoluteFilePath(), QFileInfo(legacy).absoluteFilePath());
    const Result r = update(doc, s.candidates, UpdateLinkedOriginal);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QCOMPARE(r.updated.size(), 1);
    QVERIFY2(r.updated.first().contains("syncs"), qPrintable(r.updated.first()));
    QCOMPARE(read(legacy), v2Clean(kOld));
    QVERIFY(scan(doc).candidates.isEmpty());
    QVERIFY(undo(doc, r.backupFolder).failed.isEmpty());
    QCOMPARE(read(legacy), v1(kOld));
    delete doc;
}

void FixtureDefUpdates_Test::theShowFollowsAnUpdateModeByMode()
{
    const QString shipped = m_root->filePath("app/Fixtures/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, v2Clean(kNew)));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld), daysAgo(30)));
    Doc *doc = startDoc(this, { shipped });
    Fixture *fx = addFixture(doc, "Spot 1", 0);
    QVERIFY(fx != nullptr);
    QLCFixtureDef *before = fx->fixtureDef();
    QCOMPARE(before->source(), QLCFixtureDef::Local);
    Scene *look = addScene(doc, "Look A", fx);

    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);
    QVERIFY(s.candidates.first().usedByShow);
    QVERIFY(impactOf(doc, s.candidates.first()).clean());

    const Result r = update(doc, s.candidates, UpdateLinkedOriginal);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QVERIFY(fx->fixtureDef() != before);
    QCOMPARE(fx->fixtureDef(), doc->fixtureDefCache()->fixtureDef(kMake, kSpot));
    QCOMPARE(fx->fixtureMode()->name(), QString("3ch"));
    QCOMPARE(fx->fixtureDef()->mode("4ch") != nullptr, true);     // the new version's
    QCOMPARE(fx->channels(), quint32(3));
    QCOMPARE(look->value(fx->id(), 0), uchar(200));

    // Undo: the old version back under the fixture.
    QVERIFY(undo(doc, r.backupFolder).failed.isEmpty());
    QCOMPARE(fx->fixtureDef()->mode("4ch"), nullptr);
    QCOMPARE(fx->fixtureMode()->name(), QString("3ch"));
    delete doc;
}

void FixtureDefUpdates_Test::aChangedLayoutNeedsConfirming()
{
    const QString shipped = m_root->filePath("app/Fixtures/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, v2Fixed(kNew)));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld), daysAgo(30)));
    Doc *doc = startDoc(this, { shipped });
    Fixture *fx = addFixture(doc, "Spot 1", 0);
    addScene(doc, "Look A", fx);
    // A look that only targets the fixture (no values of its own) counts too.
    Scene *target = new Scene(doc);
    target->setName("Look B");
    target->addFixture(fx->id());
    doc->addFunction(target);

    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);
    QVERIFY(functionsUsing(doc, { fx }).contains("Look B"));
    const Impact im = impactOf(doc, s.candidates.first());
    QVERIFY(im.refused.isEmpty());
    QCOMPARE(im.confirm.size(), 1);
    QVERIFY2(im.confirm.first().contains("Spot 1") && im.confirm.first().contains("Look A")
             && im.confirm.first().contains("channel 1 is Pan, was Pan (fine)"), qPrintable(im.confirm.first()));

    // Not without saying so.
    Result r = update(doc, s.candidates, UpdateLinkedOriginal, false);
    QVERIFY(r.updated.isEmpty());
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), v1(kOld));
    QVERIFY(lastUndoable().isEmpty());

    r = update(doc, s.candidates, UpdateLinkedOriginal, true);
    QVERIFY2(r.failed.isEmpty(), qPrintable(r.failed.join('\n')));
    QCOMPARE(fx->fixtureMode()->channel(0)->controlByte(), QLCChannel::MSB);
    QCOMPARE(fx->fixtureMode()->channel(0)->defaultValue(), uchar(128));
    delete doc;
}

void FixtureDefUpdates_Test::aMissingModeIsRefused()
{
    const QString shipped = m_root->filePath("app/Fixtures/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, v2Renamed()));
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld), daysAgo(30)));
    Doc *doc = startDoc(this, { shipped });
    Fixture *fx = addFixture(doc, "Spot 1", 0);
    QLCFixtureDef *before = fx->fixtureDef();

    // Its mode gone from the new version: customised, not old -- listed
    // only when asked for, and refused even then.
    QVERIFY(scan(doc).candidates.isEmpty());
    const Scan s = scan(doc, true);
    QCOMPARE(s.candidates.size(), 1);
    QCOMPARE(int(s.candidates.first().verdict), int(Customised));
    QCOMPARE(impactOf(doc, s.candidates.first()).refused.size(), 1);
    const Result r = update(doc, s.candidates, UpdateLinkedOriginal, true);
    QVERIFY(r.updated.isEmpty());
    QCOMPARE(r.failed.size(), 1);
    QVERIFY2(r.failed.first().contains("3ch"), qPrintable(r.failed.first()));
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), v1(kOld));
    QCOMPARE(fx->fixtureDef(), before);
    delete doc;
}

void FixtureDefUpdates_Test::anOlderEmbeddedCopySwitchesAndUndoes()
{
    // Saved when this computer had the old version...
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld), daysAgo(30)));
    Doc *a = startDoc(this);
    addFixture(a, "Spot 1", 0);
    addFixture(a, "Spot 2", 10);
    const QString show = m_root->filePath("shows/show.qlcc");
    save(a, show);
    delete a;

    // ...opened after it got a newer one with the same modes.
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kNew)));
    Doc *doc = startDoc(this);
    open(doc, show);
    ShowEmbedding *e = doc->showEmbedding();
    QCOMPARE(e->differences().size(), 1);
    const ShowEmbedding::Item it = e->item(kKey);
    QCOMPARE(int(e->newer(it)), int(ShowEmbedding::ComputerNewer));
    QVERIFY(e->compatibility(kKey).compatible);

    QVERIFY(!doc->fixtures().isEmpty());
    Fixture *fx = doc->fixtures().first();
    QLCFixtureDef *showDef = fx->fixtureDef();
    e->clearUndo();
    QVERIFY(e->useComputers(kKey));
    QVERIFY(e->canUndo());
    QCOMPARE(fx->fixtureDef()->definitionSourceFile(), user("Test-Maker-Spot.qxf"));
    QCOMPARE(fx->fixtureMode()->name(), QString("3ch"));

    // One step back: the show's copy, each fixture on its mode.
    QVERIFY(e->undoChoices());
    QVERIFY(!e->canUndo());
    QCOMPARE(fx->fixtureDef(), showDef);
    QCOMPARE(fx->fixtureMode()->name(), QString("3ch"));
    QVERIFY(e->item(kKey).usingShow);

    // Switched, and saved: the show carries this computer's.
    QVERIFY(e->useComputers(kKey));
    save(doc, show);
    QCOMPARE(e->item(kKey).sha256, e->item(kKey).computerSha);
    delete doc;
}

void FixtureDefUpdates_Test::anEmbeddedCopyWithAChangedLayoutNamesWhatItTouches()
{
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld), daysAgo(30)));
    Doc *a = startDoc(this);
    Fixture *fx = addFixture(a, "Spot 1", 0);
    addScene(a, "Look A", fx);
    const QString show = m_root->filePath("shows/show.qlcc");
    save(a, show);
    delete a;

    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Fixed(kNew)));
    Doc *doc = startDoc(this);
    open(doc, show);
    const ShowEmbedding::Compatibility c = doc->showEmbedding()->compatibility(kKey);
    QVERIFY(!c.compatible);
    QVERIFY2(c.problems.join('\n').contains("Spot 1") && c.problems.join('\n').contains("Look A"),
             qPrintable(c.problems.join('\n')));
    delete doc;

    // A mode the new version does not have: refused outright.
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Renamed()));
    doc = startDoc(this);
    open(doc, show);
    QString error;
    QVERIFY(!doc->showEmbedding()->useComputers(kKey, &error));
    QVERIFY2(error.contains("3ch"), qPrintable(error));
    delete doc;
}

void FixtureDefUpdates_Test::aNewerEmbeddedCopyCanBeInstalled()
{
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kNew)));
    Doc *a = startDoc(this);
    addFixture(a, "Spot 1", 0);
    const QString show = m_root->filePath("shows/show.qlcc");
    save(a, show);
    delete a;

    // This computer has an older one (written later, all the same).
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld)));
    Doc *doc = startDoc(this);
    open(doc, show);
    ShowEmbedding *e = doc->showEmbedding();
    QCOMPARE(int(e->newer(e->item(kKey))), int(ShowEmbedding::ShowNewer));
    QVERIFY(e->installShows(kKey));
    QCOMPARE(read(user("Test-Maker-Spot.qxf")), v2Clean(kNew));
    QVERIFY(e->differences().isEmpty());
    delete doc;
}

void FixtureDefUpdates_Test::theUserFolderUpdateReachesAnOpenShow()
{
    // The show carries the version that ships here...
    const QString shipped = m_root->filePath("app/Fixtures/Test-Maker-Spot.qxf");
    QVERIFY(write(shipped, v2Clean(kNew)));
    const QString show = m_root->filePath("shows/show.qlcc");
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v2Clean(kNew)));
    Doc *a = startDoc(this);
    addFixture(a, "Spot 1", 0);
    save(a, show);
    delete a;

    // ...this computer has an old copy of its own, and the shipped one.
    QVERIFY(write(user("Test-Maker-Spot.qxf"), v1(kOld), daysAgo(30)));
    Doc *doc = startDoc(this, { shipped });
    open(doc, show);
    ShowEmbedding *e = doc->showEmbedding();
    QCOMPARE(e->differences().size(), 1);
    QCOMPARE(int(e->newer(e->item(kKey))), int(ShowEmbedding::ShowNewer));

    // Updating the user's copy from the shipped one: now the same as the show's.
    const Scan s = scan(doc);
    QCOMPARE(s.candidates.size(), 1);
    QVERIFY(!s.candidates.first().usedByShow);     // the show's own copy is in use
    QVERIFY(update(doc, s.candidates, UpdateLinkedOriginal).failed.isEmpty());
    QVERIFY(e->differences().isEmpty());
    QCOMPARE(int(e->item(kKey).state), int(ShowEmbedding::Identical));
    delete doc;
}

/* Branson's own case, on COPIES: QLC_DEFUPDATE_REAL_HOME is a scratch HOME
   holding a copy of the user folder, QLC_DEFUPDATE_REAL_SHOW a copy of a
   show. Skipped unless both are set; never the real folders. */
void FixtureDefUpdates_Test::realDefinitions()
{
    const QString home = qEnvironmentVariable("QLC_DEFUPDATE_REAL_HOME");
    const QString show = qEnvironmentVariable("QLC_DEFUPDATE_REAL_SHOW");
    if (home.isEmpty() || show.isEmpty())
        QSKIP("QLC_DEFUPDATE_REAL_HOME / QLC_DEFUPDATE_REAL_SHOW not set");
    setHome(home);
    Doc *doc = new Doc(this);
    doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
    doc->fixtureDefCache()->loadMap(QDir(INTERNAL_FIXTUREDIR));

    const Scan before = scan(doc, true);
    qInfo() << "unreachable links:" << before.unreachable.size();
    for (const Candidate &c : before.candidates)
    {
        qInfo().noquote() << verdictText(c.verdict) << ":" << c.mine.path << c.mine.versionText()
                          << "->" << c.current.path << c.current.versionText();
        for (const Change &ch : c.changes)
            qInfo().noquote() << "    " << ch.text() << (ch.layout ? "[layout]" : "");
    }

    open(doc, show);
    ShowEmbedding *e = doc->showEmbedding();
    for (const ShowEmbedding::Item &it : e->items())
    {
        if (it.kind != ShowEmbedding::FixtureDefinition || it.state != ShowEmbedding::Different)
            continue;
        qInfo().noquote() << "embedded differs:" << it.displayName() << "--"
                          << ShowEmbedding::newerText(e->newer(it));
        const ShowEmbedding::Compatibility c = e->compatibility(it.key());
        qInfo().noquote() << "    compatible:" << c.compatible << c.problems.join(" | ");
        for (const Change &ch : diff(it.bytes, read(it.computerPath)))
            qInfo().noquote() << "    " << ch.text();
        if (it.usingShow)
        {
            Fixture *first = nullptr;
            foreach (Fixture *fx, doc->fixtures())
                if (fx->fixtureDef() != nullptr && fx->fixtureDef()->manufacturer() == it.manufacturer
                    && fx->fixtureDef()->model() == it.model)
                    first = fx;
            QLCFixtureDef *was = first != nullptr ? first->fixtureDef() : nullptr;
            e->clearUndo();
            QString error;
            const bool ok = e->useComputers(it.key(), &error);
            qInfo().noquote() << "    use this computer's:" << ok << error;
            if (ok)
            {
                QVERIFY(e->undoChoices());
                if (first != nullptr)
                    QCOMPARE(first->fixtureDef(), was);
            }
        }
    }
    delete doc;
}

QTEST_MAIN(FixtureDefUpdates_Test)
