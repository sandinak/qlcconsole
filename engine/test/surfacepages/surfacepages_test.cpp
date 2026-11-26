/*
  Q Light Controller Plus - Unit test
  surfacepages_test.cpp

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

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>
#include <QSettings>
#include <QBuffer>
#include <QtTest>
#include <QDir>

#include "surfacepages_test.h"
#include "simmididevice.h"
#include "surfacepages.h"
#include "surfacelayout.h"
#include "pageset.h"
#include "encoderbank.h"
#include "controlmap.h"
#include "deskrecord.h"
#include "inputoutputmap.h"
#include "qlcinputprofile.h"
#include "doc.h"

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");
const QString kApc64 = QStringLiteral("Akai APC64");
const QString kPmj = QStringLiteral("PMJ Black 1");
const QString kMaize = QStringLiteral("MaizeDMX Playback Controller");

QString profileFile(const QString &file)
{
    return QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/") + file;
}

QString fileOf(const QString &profile)
{
    if (profile == kApc)
        return profileFile(QStringLiteral("Akai-APC40-mkII.qxi"));
    if (profile == kApc64)
        return profileFile(QStringLiteral("Akai-APC64.qxi"));
    if (profile == kPmj)
        return profileFile(QStringLiteral("PMJ-Black-1.qxi"));
    return profileFile(QStringLiteral("MaizeDMX-Playback-Controller.qxi"));
}

/** A set's JSON with @p edit made to it, read back. */
PageSet edited(const PageSet &from, const std::function<void(QJsonObject &)> &edit)
{
    QJsonObject root = QJsonDocument::fromJson(from.toJson()).object();
    edit(root);
    return PageSet::fromJson(QJsonDocument(root).toJson());
}

QJsonObject binding(quint32 ch, const QString &param)
{
    QJsonObject o;
    o.insert(QStringLiteral("ch"), qint64(ch));
    o.insert(QStringLiteral("param"), param);
    return o;
}

bool anyHas(const QStringList &problems, const QString &word)
{
    for (const QString &p : problems)
        if (p.contains(word))
            return true;
    return false;
}
}

void SurfacePages_Test::initTestCase()
{
    // This computer's choices are kept in the settings: the test's own.
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void SurfacePages_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
    m_userSets = new QTemporaryDir();
    PageSet::setUserDirectory(m_userSets->path());
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();
}

void SurfacePages_Test::cleanup()
{
    qDeleteAll(m_boards);
    m_boards.clear();
    m_stub->m_lineNames.clear();
    m_stub->m_linesAreHardware = false;
    m_stub->m_networkDmx = true;
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
    PageSet::setUserDirectory(QString());
    delete m_userSets;
    m_userSets = nullptr;
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
}

SimMidiDevice *SurfacePages_Test::plug(const QString &name, const QString &profile, const QString &file,
                                       quint32 universe)
{
    SimMidiDevice *d = new SimMidiDevice(m_doc, m_stub, name, universe, profile, file);
    m_boards << d;
    if (!d->plug())
    {
        qWarning() << d->lastError();
        return nullptr;
    }
    return d;
}

/*****************************************************************************
 * The files
 *****************************************************************************/

void SurfacePages_Test::everyBuiltInSetHoldsToItsBoard()
{
    const QList<PageSet> sets = PageSet::builtIns();
    QStringList boards;
    for (const PageSet &s : sets)
        boards << s.profileName();
    for (const QString &p : { kApc, kApc64, kPmj, kMaize })
        QVERIFY2(boards.contains(p), qPrintable(p + QStringLiteral(" has no built-in page set")));
    for (const PageSet &s : sets)
    {
        QVERIFY(s.isBuiltIn());
        QScopedPointer<QLCInputProfile> profile(QLCInputProfile::loader(fileOf(s.profileName())));
        QVERIFY(profile != nullptr);
        const QStringList problems = s.check(SurfaceLayout::forProfile(s.profileName()), profile.data());
        QVERIFY2(problems.isEmpty(), qPrintable(s.profileName() + QStringLiteral(": ") + problems.join("; ")));
    }
}

void SurfacePages_Test::theApc40IsAsDecided()
{
    // Q334: device knobs 1,2 pan + fine, 3,4 focus + zoom, 5,6 tilt + fine, 7,8 gobo + rotation.
    const PageSet s = PageSet::defaultFor(kApc);
    QVERIFY(s.isValid());
    QCOMPARE(s.pages().first().name, QStringLiteral("Move"));
    const QList<QPair<QString, bool>> want = { { "pan", false }, { "pan", true }, { "focus", false },
                                               { "zoom", false }, { "tilt", false }, { "tilt", true },
                                               { "gobo", false }, { "gobo-rotation", false } };
    for (int i = 0; i < want.size(); ++i)
    {
        const PageSet::Binding *b = s.binding(0, quint32(16 + i));
        QVERIFY2(b != nullptr, qPrintable(QStringLiteral("device knob %1 has no job").arg(i + 1)));
        QCOMPARE(b->param, want.at(i).first);
        QCOMPARE(b->fine, want.at(i).second);
    }
    QCOMPARE(s.binding(0, 17)->text(), QStringLiteral("Pan fine"));
    // The faders are colour on every page: R G B W A UV.
    const QList<quint32> faders = { 7, 4103, 8199, 12295, 16391, 20487 };
    const QStringList colours = { "red", "green", "blue", "white", "amber", "uv" };
    for (int page = 0; page < s.pages().size(); ++page)
        for (int i = 0; i < faders.size(); ++i)
            QCOMPARE(s.binding(page, faders.at(i))->param, colours.at(i));
    QVERIFY(!PageSet::paramColour(QStringLiteral("red")).isEmpty());
}

void SurfacePages_Test::theCheckerRefusesTheDesksControls()
{
    const PageSet good = PageSet::defaultFor(kApc);
    const SurfaceLayout layout = SurfaceLayout::forProfile(kApc);
    QVERIFY(good.check(layout).isEmpty());

    // The Grand Master (MASTER, CC 14) on a page: refused, by name.
    const PageSet gm = edited(good, [](QJsonObject &r) {
        QJsonArray a = r.value("always").toArray();
        a.append(binding(14, "intensity"));
        r.insert("always", a);
    });
    QVERIFY(gm.isValid());
    QStringList problems = gm.check(layout);
    QVERIFY2(anyHas(problems, QStringLiteral("GM")), qPrintable(problems.join("; ")));

    // GO (PLAY) on a page, and STOP (STOP ALL CLIPS) as a page key: both refused.
    const PageSet go = edited(good, [](QJsonObject &r) {
        QJsonArray pages = r.value("pages").toArray();
        QJsonObject p = pages.at(0).toObject();
        QJsonArray c = p.value("controls").toArray();
        c.append(binding(219, "pan"));
        p.insert("controls", c);
        p.insert("key", 209);
        pages.replace(0, p);
        r.insert("pages", pages);
    });
    problems = go.check(layout);
    QVERIFY2(anyHas(problems, QStringLiteral("GO")), qPrintable(problems.join("; ")));
    QVERIFY2(anyHas(problems, QStringLiteral("STOP")), qPrintable(problems.join("; ")));
}

void SurfacePages_Test::theCheckerRefusesAJobAControlCantDo()
{
    const PageSet good = PageSet::defaultFor(kApc);
    const SurfaceLayout layout = SurfaceLayout::forProfile(kApc);
    const PageSet bad = edited(good, [](QJsonObject &r) {
        QJsonArray pages = r.value("pages").toArray();
        QJsonObject p = pages.at(1).toObject();
        QJsonArray c;
        QJsonObject fine = binding(24583, "zoom");     // fader 7, marked fine
        fine.insert("fine", true);
        c.append(fine);
        c.append(binding(160, "pan"));                  // a clip pad given an attribute
        c.append(binding(16, "sparkle"));               // an attribute nobody knows
        c.append(binding(99999, "tilt"));               // no such control
        p.insert("controls", c);
        pages.replace(1, p);
        r.insert("pages", pages);
    });
    const QStringList problems = bad.check(layout);
    QVERIFY2(anyHas(problems, QStringLiteral("fine")), qPrintable(problems.join("; ")));
    QVERIFY2(anyHas(problems, QStringLiteral("button")), qPrintable(problems.join("; ")));
    QVERIFY2(anyHas(problems, QStringLiteral("sparkle")), qPrintable(problems.join("; ")));
    QVERIFY2(anyHas(problems, QStringLiteral("99999")), qPrintable(problems.join("; ")));
    // A set for another board does not fit this one.
    QVERIFY(!PageSet::defaultFor(kPmj).check(layout).isEmpty());
}

void SurfacePages_Test::exportImportRoundTrip()
{
    PageSet mine = PageSet::defaultFor(kApc);
    mine.setName(QStringLiteral("Mine"));
    QTemporaryDir out;
    const QString path = out.filePath(QStringLiteral("mine.pageset.json"));
    QString why;
    QVERIFY2(mine.save(path, &why), qPrintable(why));

    // Not there until imported; then it is, the same set.
    QVERIFY(!PageSet::find(kApc, QStringLiteral("Mine")).isValid());
    PageSet imported;
    QVERIFY2(PageSet::importFile(path, &why, &imported), qPrintable(why));
    const PageSet found = PageSet::find(kApc, QStringLiteral("Mine"));
    QVERIFY(found.isValid());
    QVERIFY(!found.isBuiltIn());
    QVERIFY(found.filePath().startsWith(m_userSets->path()));
    QCOMPARE(found.toJson(), mine.toJson());
    QCOMPARE(PageSet::forProfile(kApc).size(), 2);

    // A built-in's name is not taken.
    QVERIFY(PageSet::defaultFor(kApc).save(path));
    QVERIFY(!PageSet::importFile(path, &why));
    QVERIFY2(why.contains(QStringLiteral("built-in")), qPrintable(why));
}

void SurfacePages_Test::importRefusesABrokenSet()
{
    PageSet broken = edited(PageSet::defaultFor(kApc), [](QJsonObject &r) {
        r.insert("name", "Broken");
        QJsonArray a = r.value("always").toArray();
        a.append(binding(14, "red"));
        r.insert("always", a);
    });
    QTemporaryDir out;
    const QString path = out.filePath(QStringLiteral("broken.pageset.json"));
    QVERIFY(broken.save(path));
    QString why;
    QVERIFY(!PageSet::importFile(path, &why));
    QVERIFY2(why.contains(QStringLiteral("GM")), qPrintable(why));
    QVERIFY(!PageSet::find(kApc, QStringLiteral("Broken")).isValid());
    QVERIFY(!PageSet::importFile(out.filePath(QStringLiteral("none.pageset.json")), &why));
}

/*****************************************************************************
 * The boards
 *****************************************************************************/

void SurfacePages_Test::theApc40SwitchesPages()
{
    SimMidiDevice *apc = plug(QStringLiteral("APC40 mkII"), kApc, fileOf(kApc), 0);
    QVERIFY(apc != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QCOMPARE(pages->universes(), QList<quint32>{ 0 });
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));
    QCOMPARE(pages->labels(0).value(16), QStringLiteral("Pan"));
    QCOMPARE(pages->labels(0).value(17), QStringLiteral("Pan fine"));
    QCOMPARE(pages->chipText(0), QStringLiteral("APC40 · Move"));
    QSignalSpy changed(pages, &SurfacePages::pageChanged);

    // Sends is the Beam page: the knobs' labels change, the faders' stay.
    QVERIFY(apc->tap(QStringLiteral("#216")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Beam"));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().at(1).toString(), QStringLiteral("Beam"));
    QCOMPARE(pages->labels(0).value(16), QStringLiteral("Zoom"));
    QCOMPARE(pages->labels(0).value(7), QStringLiteral("Red"));
    // Its lamp lit, Pan's dark.
    QCOMPARE(apc->lamp(QStringLiteral("#216")), 127);
    QCOMPARE(apc->lamp(QStringLiteral("#215")), 0);

    // Device ▶ steps round; Device ◀ back; Pan is Move.
    QVERIFY(apc->tap(QStringLiteral("#187")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));
    QVERIFY(apc->tap(QStringLiteral("#186")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Beam"));
    QVERIFY(apc->tap(QStringLiteral("#215")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));
}

void SurfacePages_Test::theApc64EncoderStepsPages()
{
    SimMidiDevice *apc = plug(QStringLiteral("APC64 DAW"), kApc64, fileOf(kApc64), 0);
    QVERIFY(apc != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QCOMPARE(pages->pageName(0), QStringLiteral("Colour"));
    // Strips 1-7 colour and intensity; strip 8 is the GM, on no page.
    QCOMPARE(pages->labels(0).value(513), QStringLiteral("Red"));
    QCOMPARE(pages->labels(0).value(25089), QStringLiteral("Intensity"));
    QVERIFY(!pages->labels(0).contains(29185));
    QSignalSpy changed(pages, &SurfacePages::pageChanged);
    QVERIFY(apc->set(QStringLiteral("#90"), 1));       // one detent clockwise
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().at(1).toString(), QStringLiteral("Move"));
    QVERIFY(apc->set(QStringLiteral("#90"), 127));     // one back
    QCOMPARE(pages->pageName(0), QStringLiteral("Colour"));
}

void SurfacePages_Test::thePmjModeKeysPickThePage()
{
    SimMidiDevice *pmj = plug(kPmj, kPmj, fileOf(kPmj), 0);
    QVERIFY(pmj != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    // Looks first: the faders' rule of before.
    QCOMPARE(pages->pageName(0), QStringLiteral("Looks"));
    QCOMPARE(pages->bindingFor(0, 32769)->param, QStringLiteral("look-level"));
    QVERIFY(pmj->tap(QStringLiteral("Fix Cont")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Fix Cont"));
    QCOMPARE(pages->bindingFor(0, 32769)->param, QStringLiteral("red"));
    QCOMPARE(pages->bindingFor(0, 32781)->param, QStringLiteral("zoom"));
    // Pressed again: its second page.
    QVERIFY(pmj->tap(QStringLiteral("Fix Cont")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Fix Cont 2"));
    QCOMPARE(pages->bindingFor(0, 32779)->param, QStringLiteral("gobo"));
    QVERIFY(pmj->tap(QStringLiteral("Groups")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Groups"));
    QCOMPARE(pages->bindingFor(0, 32771)->param, QStringLiteral("group-intensity"));
    QCOMPARE(pages->bindingFor(0, 32771)->index, 3);
    QVERIFY(pmj->tap(QStringLiteral("Effects")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Effects"));
    QVERIFY(pages->bindingFor(0, 32769) == nullptr);
    // The Master is the GM: on no page.
    QVERIFY(pages->bindingFor(0, 32768) == nullptr);
}

void SurfacePages_Test::theMaizeDmxFadersAreColour()
{
    SimMidiDevice *maize = plug(QStringLiteral("USB2.0-MIDI Port 1"), kMaize, fileOf(kMaize), 0);
    QVERIFY(maize != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QCOMPARE(pages->pageName(0), QStringLiteral("Colour"));
    QSignalSpy level(pages, &SurfacePages::levelSet);
    QVERIFY(maize->set(QStringLiteral("Fader 1"), 64));
    QCOMPARE(level.size(), 1);
    QCOMPARE(level.first().at(2).toInt(), int(EncoderBank::ColourBank));
    QCOMPARE(level.first().at(3).toInt(), 2);      // red
    QVERIFY(maize->set(QStringLiteral("Fader 10"), 100));
    QCOMPARE(level.size(), 2);
    QCOMPARE(level.last().at(2).toInt(), int(EncoderBank::IntensityBank));
    // The GM fader is the desk's.
    QVERIFY(maize->set(QStringLiteral("GM"), 100));
    QCOMPARE(level.size(), 2);
}

void SurfacePages_Test::aKnobTurnsAndItsNeighbourTurnsFine()
{
    SimMidiDevice *apc = plug(QStringLiteral("APC40 mkII"), kApc, fileOf(kApc), 0);
    QVERIFY(apc != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QSignalSpy turned(pages, &SurfacePages::adjusted);
    // An APC40 knob says where it is: its first value only says so.
    QVERIFY(apc->set(QStringLiteral("#16"), 60));
    QCOMPARE(turned.size(), 0);
    QVERIFY(apc->set(QStringLiteral("#16"), 63));
    QCOMPARE(turned.size(), 1);
    QCOMPARE(turned.last().at(0).toInt(), int(EncoderBank::PositionBank));
    QCOMPARE(turned.last().at(1).toInt(), 0);      // pan
    QCOMPARE(turned.last().at(2).toInt(), 3);
    QCOMPARE(turned.last().at(3).toBool(), false);
    // Knob 2: the same pan, fine; turned down.
    QVERIFY(apc->set(QStringLiteral("#17"), 70));
    QVERIFY(apc->set(QStringLiteral("#17"), 68));
    QCOMPARE(turned.size(), 2);
    QCOMPARE(turned.last().at(1).toInt(), 0);
    QCOMPARE(turned.last().at(2).toInt(), -2);
    QCOMPARE(turned.last().at(3).toBool(), true);
    // The same knob on another track's MIDI channel is the same knob.
    QVERIFY(apc->set(QStringLiteral("#%1").arg(16 + 8 * 4096), 64));
    QCOMPARE(turned.size(), 3);
    // The PMJ's encoders are relative (and turn the other way).
    SimMidiDevice *pmj = plug(kPmj, kPmj, fileOf(kPmj), 1);
    QVERIFY(pmj != nullptr);
    QVERIFY(pmj->set(QStringLiteral("Enc 1"), 1));
    QCOMPARE(turned.size(), 4);
    QCOMPARE(turned.last().at(1).toInt(), 0);
    QCOMPARE(turned.last().at(2).toInt(), -1);
}

void SurfacePages_Test::aFaderIsALevelAndATouchIsNot()
{
    SimMidiDevice *apc = plug(QStringLiteral("APC64 DAW"), kApc64, fileOf(kApc64), 0);
    QVERIFY(apc != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QSignalSpy level(pages, &SurfacePages::levelSet);
    QVERIFY(apc->set(QStringLiteral("Touch strip 1"), 90));
    QCOMPARE(level.size(), 1);
    QCOMPARE(level.first().at(1).toUInt(), 513u);
    QCOMPARE(level.first().at(3).toInt(), 2);      // red
    // Its touch (a note on the same strip) is not a value.
    QVERIFY(apc->press(QStringLiteral("#210")));
    QVERIFY(apc->release(QStringLiteral("#210")));
    QCOMPARE(level.size(), 1);
}

void SurfacePages_Test::operateReleasesTheControlsToTheShow()
{
    SimMidiDevice *apc = plug(QStringLiteral("APC40 mkII"), kApc, fileOf(kApc), 0);
    QVERIFY(apc != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QSignalSpy turned(pages, &SurfacePages::adjusted);
    QSignalSpy show(iom, &InputOutputMap::showInputValueChanged);
    auto showHeard = [&show](quint32 ch) {
        for (const QList<QVariant> &a : show)
            if (a.at(1).toUInt() == ch)
                return true;
        return false;
    };

    // Design: the page has knob 1 and its keys; no show listener hears them.
    QVERIFY(pages->isActive(0));
    QVERIFY(iom->isPageControl(0, 16));
    QVERIFY(iom->isPageControl(0, 216));
    QVERIFY(!iom->isPageControl(0, 160));           // a clip pad is the show's
    QVERIFY(apc->set(QStringLiteral("#16"), 10));
    QVERIFY(apc->set(QStringLiteral("#16"), 12));
    QCOMPARE(turned.size(), 1);
    QVERIFY(!showHeard(16));
    QVERIFY(apc->tap(QStringLiteral("#160")));
    QVERIFY(showHeard(160));

    // Operate: the show's again, and the page does nothing (its keys neither).
    m_doc->setMode(Doc::Operate);
    QVERIFY(!pages->isActive(0));
    QVERIFY(!iom->isPageControl(0, 16));
    show.clear();
    QVERIFY(apc->set(QStringLiteral("#16"), 40));
    QVERIFY(apc->set(QStringLiteral("#16"), 44));
    QCOMPARE(turned.size(), 1);
    QVERIFY(showHeard(16));
    QVERIFY(apc->tap(QStringLiteral("#216")));
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));
    QVERIFY(showHeard(216));

    // Design again: the page has them back.
    m_doc->setMode(Doc::Design);
    QVERIFY(iom->isPageControl(0, 16));
}

void SurfacePages_Test::theShowsEncoderBankOnTheBoardWins()
{
    SimMidiDevice *apc = plug(QStringLiteral("APC40 mkII"), kApc, fileOf(kApc), 0);
    QVERIFY(apc != nullptr);
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    EncoderBank::Device d;
    QVERIFY(EncoderBank::preset(EncoderBank::presetKeys().last(), &d));
    d.universe = 0;
    enc->setDevice(d);
    QVERIFY(enc->isEnabled());
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    QVERIFY(!pages->isActive(0));
    QVERIFY(!m_doc->inputOutputMap()->isPageControl(0, 16));
    QSignalSpy turned(pages, &SurfacePages::adjusted);
    QVERIFY(apc->set(QStringLiteral("#16"), 10));
    QVERIFY(apc->set(QStringLiteral("#16"), 14));
    QCOMPARE(turned.size(), 0);
}

void SurfacePages_Test::aShowOffersTheSetItWasBuiltWith()
{
    PageSet mine = PageSet::defaultFor(kApc);
    mine.setName(QStringLiteral("Mine"));
    QTemporaryDir out;
    const QString path = out.filePath(QStringLiteral("mine.pageset.json"));
    QVERIFY(mine.save(path));
    QVERIFY(PageSet::importFile(path));

    SimMidiDevice *apc = plug(QStringLiteral("APC40 mkII"), kApc, fileOf(kApc), 0);
    QVERIFY(apc != nullptr);
    SurfacePages *pages = m_doc->controlMap()->surfacePages();
    ControlMap *cm = m_doc->controlMap();
    QCOMPARE(pages->chosenSet(0), QStringLiteral("Build"));
    // A show built with the board's default names no set: it saves as before.
    QVERIFY(!pages->hasShowSets());

    QVERIFY(pages->chooseSet(0, QStringLiteral("Mine")));
    QCOMPARE(pages->chosenSet(0), QStringLiteral("Mine"));
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    QVERIFY(cm->saveXML(&w));
    buf.close();
    QVERIFY2(buf.data().contains("Name=\"Mine\""), buf.data().constData());

    // Opened on a computer that uses another set: offered, and used on Use It.
    QVERIFY(pages->chooseSet(0, QStringLiteral("Build")));
    QXmlStreamReader r(buf.data());
    QVERIFY(r.readNextStartElement());
    QSignalSpy sets(pages, &SurfacePages::setsChanged);
    QVERIFY(cm->loadXML(r));
    QVERIFY(sets.size() > 0);
    QCOMPARE(pages->offeredSet(0), QStringLiteral("Mine"));
    pages->useShowSet(0);
    QCOMPARE(pages->chosenSet(0), QStringLiteral("Mine"));
    QCOMPARE(pages->offeredSet(0), QString());
    // This computer remembers it, by the device.
    QCOMPARE(QSettings().childGroups().contains(SurfacePages::settingsGroup())
                 || !QSettings().allKeys().filter(SurfacePages::settingsGroup()).isEmpty(), true);
    // Not Now: the offer goes.
    QVERIFY(pages->chooseSet(0, QStringLiteral("Build")));
    QCOMPARE(pages->offeredSet(0), QStringLiteral("Mine"));
    pages->declineShowSet(0);
    QCOMPARE(pages->offeredSet(0), QString());
}

QTEST_MAIN(SurfacePages_Test)
