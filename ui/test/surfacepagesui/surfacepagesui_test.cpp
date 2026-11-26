/*
  Q Light Controller Plus - Unit test
  surfacepagesui_test.cpp

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
#include <QApplication>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QSignalSpy>
#include <QComboBox>
#include <QSettings>
#include <QBuffer>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QtTest>

#include "surfacepagesui_test.h"
#include "simmididevice.h"
#include "programmingoutline.h"
#include "encoderattributes.h"
#include "inputoutputmap.h"
#include "surfacepages.h"
#include "surfacechips.h"
#include "surfacemap.h"
#include "encoderbank.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "paletteops.h"
#include "workbench.h"
#include "pageset.h"
#include "ioplugincache.h"
#include "qlcioplugin.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");
const quint32 kKnob1 = 16, kKnob2 = 17;     // device knobs 1 and 2: Pan, Pan fine
const quint32 kSends = 128 + 88;            // the Beam page's key
const quint32 kFader1 = 7;                  // Red on every page
const quint32 kRedChannel = 5;              // the mover's red

void settle()
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

QLCChannel *channel(QLCFixtureDef *def, QLCFixtureMode *mode, int index, const char *name,
                    QLCChannel::Group group, QLCChannel::ControlByte byte = QLCChannel::MSB,
                    QLCChannel::PrimaryColour colour = QLCChannel::NoColour)
{
    QLCChannel *ch = new QLCChannel();
    ch->setName(name);
    ch->setGroup(group);
    ch->setControlByte(byte);
    if (colour != QLCChannel::NoColour)
        ch->setColour(colour);
    def->addChannel(ch);
    mode->insertChannel(ch, index);
    return ch;
}

QTreeWidgetItem *findKind(QTreeWidgetItem *from, int kind)
{
    for (int i = 0; i < from->childCount(); ++i)
    {
        QTreeWidgetItem *c = from->child(i);
        if (c->data(0, ProgrammingOutline::KindRole).toInt() == kind)
            return c;
        if (QTreeWidgetItem *deep = findKind(c, kind))
            return deep;
    }
    return nullptr;
}

float panOf(Doc *doc, Scene *s, Fixture *f)
{
    float n = -1.0f;
    EncoderAttributes::read(doc, s, { f->id() }, EncoderBank::PositionBank, 0, &n);
    return n;
}

QString knob(quint32 ch) { return QStringLiteral("#%1").arg(ch); }
}

void SurfacePagesUi_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void SurfacePagesUi_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
    m_userSets = new QTemporaryDir();
    PageSet::setUserDirectory(m_userSets->path());
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();
}

void SurfacePagesUi_Test::cleanup()
{
    delete m_apc;
    m_apc = nullptr;
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

SimMidiDevice *SurfacePagesUi_Test::plugApc()
{
    m_apc = new SimMidiDevice(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc,
                              QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/Akai-APC40-mkII.qxi"));
    if (!m_apc->plug())
    {
        qWarning() << m_apc->lastError();
        return nullptr;
    }
    settle();
    return m_apc;
}

Fixture *SurfacePagesUi_Test::mover(const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Mover " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Full");
    channel(def, mode, 0, "Dimmer", QLCChannel::Intensity);
    channel(def, mode, 1, "Pan", QLCChannel::Pan);
    channel(def, mode, 2, "Pan fine", QLCChannel::Pan, QLCChannel::LSB);
    channel(def, mode, 3, "Tilt", QLCChannel::Tilt);
    channel(def, mode, 4, "Tilt fine", QLCChannel::Tilt, QLCChannel::LSB);
    channel(def, mode, 5, "Red", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Red);
    channel(def, mode, 6, "Green", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Green);
    channel(def, mode, 7, "Blue", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Blue);
    QLCFixtureHead head;
    for (int i = 0; i < 8; ++i)
        head.addChannel(quint32(i));
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(address);
    if (!m_doc->addFixture(fxi))
        return nullptr;
    return fxi;
}

Scene *SurfacePagesUi_Test::sceneWith(Workbench &wb, Fixture *f)
{
    Scene *s = new Scene(m_doc);
    s->setName("Look A");
    m_doc->addFunction(s);
    s->addFixture(f->id());
    wb.show();
    wb.load(s->id());
    settle();
    QTreeWidgetItem *layer = wb.rows()->topLevelItem(0);
    QTreeWidgetItem *row = layer != nullptr ? findKind(layer, ProgrammingOutline::FixtureRow) : nullptr;
    if (row != nullptr)
    {
        wb.rows()->clearSelection();
        wb.rows()->setCurrentItem(row);
        row->setSelected(true);
        settle();
    }
    return s;
}

void SurfacePagesUi_Test::aPageKnobTurnsPanInDesignOnly()
{
    QVERIFY(plugApc() != nullptr);
    Fixture *a = mover("A", 0);
    QVERIFY(a != nullptr);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, a);
    QCOMPARE(wb.encoderTarget().fixtures, QList<quint32>{ a->id() });
    QVERIFY2(wb.encoderLine().contains(QStringLiteral("APC40 · Move")), qPrintable(wb.encoderLine()));

    // Device knob 1 is pan: a turn moves the selected fixture's pan.
    const float before = panOf(m_doc, s, a);
    QVERIFY(m_apc->set(knob(kKnob1), 60));
    QVERIFY(m_apc->set(knob(kKnob1), 66));
    settle();
    const float coarse = panOf(m_doc, s, a) - before;
    QVERIFY2(coarse > 0.0f, qPrintable(QString::number(coarse)));
    QVERIFY(m_doc->isModified());
    // Device knob 2: the same pan, by the fine step.
    const float mid = panOf(m_doc, s, a);
    QVERIFY(m_apc->set(knob(kKnob2), 60));
    QVERIFY(m_apc->set(knob(kKnob2), 66));
    settle();
    const float fine = panOf(m_doc, s, a) - mid;
    QVERIFY2(fine > 0.0f && fine < coarse / 4, qPrintable(QStringLiteral("%1 vs %2").arg(fine).arg(coarse)));

    // The show's own assignment on the Sends key: a trigger.
    Scene *stab = new Scene(m_doc);
    stab->setName("Stab");
    m_doc->addFunction(stab);
    ControlMap *cm = m_doc->controlMap();
    cm->setBinding({ 0, kSends, ControlMap::function(stab->id()), ControlMap::Toggle });
    QSignalSpy fired(cm, &ControlMap::fired);
    SurfacePages *pages = cm->surfacePages();
    // Design: Sends is the page's (Beam), the show's trigger waits.
    QVERIFY(m_apc->tap(knob(kSends)));
    settle();
    QCOMPARE(pages->pageName(0), QStringLiteral("Beam"));
    QCOMPARE(fired.size(), 0);
    QVERIFY(m_apc->tap(QStringLiteral("#%1").arg(128 + 87)));   // Pan: back to Move
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));

    // Operate: the knob turns nothing; Sends is the show's trigger again.
    m_doc->setMode(Doc::Operate);
    settle();
    const float operate = panOf(m_doc, s, a);
    QVERIFY(m_apc->set(knob(kKnob1), 90));
    QVERIFY(m_apc->set(knob(kKnob1), 100));
    settle();
    QCOMPARE(panOf(m_doc, s, a), operate);
    QVERIFY(!wb.encoderLine().contains(QStringLiteral("APC40")));
    QVERIFY(m_apc->tap(knob(kSends)));
    settle();
    QTRY_VERIFY2(fired.size() > 0, "the show's trigger on Sends did not fire in Operate");
    QCOMPARE(pages->pageName(0), QStringLiteral("Move"));
    m_doc->setMode(Doc::Design);
}

void SurfacePagesUi_Test::aPageFaderPicksUpThenSets()
{
    QVERIFY(plugApc() != nullptr);
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, a);
    s->setValue(a->id(), kRedChannel, 128);
    PaletteOps *ops = m_doc->paletteOps();
    QVERIFY(ops != nullptr);
    QVERIFY(!ops->canUndo());

    // Fader 1 is red. Far below red: it waits, and says where to go.
    QVERIFY(m_apc->set(knob(kFader1), 20));
    settle();
    QCOMPARE(int(s->value(a->id(), kRedChannel)), 128);
    QVERIFY2(wb.encoderLine().contains(QStringLiteral("50")), qPrintable(wb.encoderLine()));
    // Through red: picked up, then it follows.
    QVERIFY(m_apc->set(knob(kFader1), 64));
    QVERIFY(m_apc->set(knob(kFader1), 80));
    settle();
    QCOMPARE(int(s->value(a->id(), kRedChannel)), 160);
    // One move, one undo step.
    QVERIFY(ops->canUndo());
    QVERIFY(ops->undo());
    QCOMPARE(int(s->value(a->id(), kRedChannel)), 128);
    QVERIFY2(!ops->canUndo(), "one fader move was more than one step");
}

void SurfacePagesUi_Test::aPageSwitchMakesAFaderPickUpAgain()
{
    QVERIFY(plugApc() != nullptr);
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, a);
    s->setValue(a->id(), kRedChannel, 100);
    QVERIFY(m_apc->set(knob(kFader1), 50));     // 100: at red, taken
    QVERIFY(m_apc->set(knob(kFader1), 60));
    settle();
    QCOMPARE(int(s->value(a->id(), kRedChannel)), 120);
    // Another page: the faders forget, and wait for the value again.
    QVERIFY(m_apc->tap(knob(kSends)));
    s->setValue(a->id(), kRedChannel, 200);
    QVERIFY(m_apc->set(knob(kFader1), 70));
    settle();
    QCOMPARE(int(s->value(a->id(), kRedChannel)), 200);
}

void SurfacePagesUi_Test::theMapAndTheChipNameThePage()
{
    QVERIFY(plugApc() != nullptr);
    SurfaceMaps maps(m_doc);
    SurfaceChips chips(m_doc);
    settle();
    SurfaceMapView *mv = maps.view(0);
    QVERIFY(mv != nullptr);
    QCOMPARE(mv->group(kKnob1), SurfaceMapView::Page);
    QCOMPARE(mv->jobText(kKnob1), QStringLiteral("Pan"));
    QCOMPARE(mv->jobText(kKnob2), QStringLiteral("Pan fine"));
    QCOMPARE(mv->jobText(kFader1), QStringLiteral("Red"));
    QCOMPARE(mv->jobText(kSends), QStringLiteral("Beam"));
    // The colour faders in their colours.
    QVERIFY(mv->pageColour(kFader1).isValid());
    QVERIFY(mv->pageColour(kFader1).red() > mv->pageColour(kFader1).blue());
    // The GM (MASTER) is never the page's.
    QVERIFY(mv->group(14) != SurfaceMapView::Page);
    QVERIFY(chips.chip(0) != nullptr);
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40 · Move"));
    QVERIFY2(maps.pageLine(0).contains(QStringLiteral("Move")), qPrintable(maps.pageLine(0)));

    QVERIFY(m_apc->tap(knob(kSends)));
    settle();
    QCOMPARE(mv->jobText(kKnob1), QStringLiteral("Zoom"));
    QCOMPARE(mv->jobText(kFader1), QStringLiteral("Red"));
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40 · Beam"));
    QVERIFY(maps.pageLine(0).contains(QStringLiteral("Beam")));

    // Operate: the controls are the show's; the chip says the board only.
    m_doc->setMode(Doc::Operate);
    settle();
    QVERIFY(mv->group(kKnob1) != SurfaceMapView::Page);
    QCOMPARE(chips.chip(0)->text(), QStringLiteral("APC40"));
    QVERIFY(maps.pageLine(0).contains(QStringLiteral("Operate")));
    m_doc->setMode(Doc::Design);
}

void SurfacePagesUi_Test::exportAndImportFromTheHub()
{
    QVERIFY(plugApc() != nullptr);
    SurfaceMaps maps(m_doc);
    settle();
    QComboBox *box = maps.findChild<QComboBox *>(QStringLiteral("surfacePageSet0"));
    QVERIFY(box != nullptr);
    QCOMPARE(box->count(), 1);
    QVERIFY(maps.findChild<QPushButton *>(QStringLiteral("surfacePagesExport0")) != nullptr);
    QVERIFY(maps.findChild<QPushButton *>(QStringLiteral("surfacePagesImport0")) != nullptr);

    QTemporaryDir out;
    const QString path = out.filePath(QStringLiteral("apc.pageset.json"));
    QString why;
    QVERIFY2(maps.exportPageSet(0, path, &why), qPrintable(why));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    QCOMPARE(root.value("name").toString(), QStringLiteral("Build"));
    root.insert("name", "Lab");
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(QJsonDocument(root).toJson());
    f.close();

    QVERIFY2(maps.importPageSet(0, path, &why), qPrintable(why));
    settle();
    QCOMPARE(m_doc->controlMap()->surfacePages()->chosenSet(0), QStringLiteral("Lab"));
    QCOMPARE(box->count(), 2);
    QCOMPARE(box->currentData().toString(), QStringLiteral("Lab"));

    // Another board's set is refused, named.
    const QString pmj = out.filePath(QStringLiteral("pmj.pageset.json"));
    QVERIFY(PageSet::defaultFor(QStringLiteral("PMJ Black 1")).save(pmj));
    QVERIFY(!maps.importPageSet(0, pmj, &why));
    QVERIFY2(why.contains(QStringLiteral("PMJ")), qPrintable(why));
}

void SurfacePagesUi_Test::theHubOffersTheShowsSet()
{
    PageSet mine = PageSet::defaultFor(kApc);
    mine.setName(QStringLiteral("Mine"));
    QTemporaryDir out;
    const QString path = out.filePath(QStringLiteral("mine.pageset.json"));
    QVERIFY(mine.save(path));
    QVERIFY(PageSet::importFile(path));
    QVERIFY(plugApc() != nullptr);
    ControlMap *cm = m_doc->controlMap();
    SurfacePages *pages = cm->surfacePages();
    QVERIFY(pages->chooseSet(0, QStringLiteral("Mine")));
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    QVERIFY(cm->saveXML(&w));
    buf.close();
    QVERIFY(pages->chooseSet(0, QStringLiteral("Build")));

    SurfaceMaps maps(m_doc);
    settle();
    QVERIFY(maps.offerLine(0).isEmpty());
    QXmlStreamReader r(buf.data());
    QVERIFY(r.readNextStartElement());
    QVERIFY(cm->loadXML(r));
    settle();
    QVERIFY2(maps.offerLine(0).contains(QStringLiteral("Mine")), qPrintable(maps.offerLine(0)));
    QPushButton *use = maps.findChild<QPushButton *>(QStringLiteral("surfacePageOfferUse0"));
    QVERIFY(use != nullptr);
    use->click();
    settle();
    QCOMPARE(pages->chosenSet(0), QStringLiteral("Mine"));
    QVERIFY(maps.offerLine(0).isEmpty());
}

QTEST_MAIN(SurfacePagesUi_Test)
