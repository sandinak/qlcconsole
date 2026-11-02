/*
  Q Light Controller Plus - Unit test
  encoderbankui_test.cpp

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

#include <QtTest>
#include <QSignalSpy>
#include <QTreeWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <climits>

#include "encoderbankui_test.h"
#include "programmingoutline.h"
#include "controlmappanel.h"
#include "inputoutputmap.h"
#include "encoderbank.h"
#include "workbench.h"
#include "paletteops.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlccapability.h"
#include "fixturegroup.h"
#include "controlmap.h"
#include "lookeditor.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
const quint32 kTurn = 16;           // the generic preset: CC 16-23
const quint32 kNext = 24;
const quint32 kPush = 128 + 40;
const quint32 kRing = 80;

EncoderBank::Device generic()
{
    EncoderBank::Device d;
    d.name = "Test";
    d.encoding = EncoderBank::TwosComplement;
    d.encoders = kTurn;
    d.pushes = qint32(kPush);
    d.rings = qint32(kRing);
    d.bankNext = qint32(kNext);
    return d;
}

uchar dmx(int midi) { return midi == 127 ? 255 : uchar(midi << 1); }

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

QTreeWidgetItem *findKind(QTreeWidgetItem *from, int kind, int nth = 0)
{
    for (int i = 0; i < from->childCount(); ++i)
    {
        QTreeWidgetItem *c = from->child(i);
        if (c->data(0, ProgrammingOutline::KindRole).toInt() == kind && nth-- == 0)
            return c;
        if (QTreeWidgetItem *deep = findKind(c, kind, nth))
            return deep;
    }
    return nullptr;
}

void select(ProgrammingOutline *o, QTreeWidgetItem *it)
{
    o->clearSelection();
    o->setCurrentItem(it);
    it->setSelected(true);
    QCoreApplication::processEvents();
}
}

void EncoderBankUi_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    enc->setCoalesceMs(0);
    enc->setDevice(generic());
}

void EncoderBankUi_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Fixture *EncoderBankUi_Test::mover(const QString &name, quint32 address)
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
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
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

Scene *EncoderBankUi_Test::sceneWith(Workbench &wb, const QList<Fixture *> &fixtures)
{
    Scene *s = new Scene(m_doc);
    s->setName("Look A");
    m_doc->addFunction(s);
    for (Fixture *f : fixtures)
        s->addFixture(f->id());
    wb.show();
    wb.load(s->id());
    QCoreApplication::processEvents();
    QTreeWidgetItem *layer = wb.rows()->topLevelItem(0);
    QTreeWidgetItem *row = layer != nullptr ? findKind(layer, ProgrammingOutline::FixtureRow) : nullptr;
    if (row != nullptr)
        select(wb.rows(), row);
    return s;
}

void EncoderBankUi_Test::turn(int n, int detents)
{
    // Two's complement: +n is n, -n is 128 - n, and a click carries 63 at most.
    while (detents != 0)
    {
        const int d = qBound(-63, detents, 63);
        detents -= d;
        const int v = d >= 0 ? d : 128 + d;
        m_doc->controlMap()->encoderBank()->handleInput(0, kTurn + quint32(n), dmx(v));
    }
}

void EncoderBankUi_Test::push(int n, bool down)
{
    m_doc->controlMap()->encoderBank()->handleInput(0, kPush + quint32(n), down ? 255 : 0);
}

void EncoderBankUi_Test::gather()
{
    m_doc->controlMap()->encoderBank()->flush();
    QCoreApplication::processEvents();
}

void EncoderBankUi_Test::aTurnEditsTheSelectedFixture()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    QCOMPARE(wb.currentPage(), Workbench::TargetPage);
    QVERIFY(wb.encoderTarget().scene == s);
    QCOMPARE(wb.encoderTarget().fixtures, QList<quint32>{ a->id() });

    s->setValue(a->id(), 0, 100);
    turn(0, 3);                                 // Intensity bank, encoder 1
    gather();
    QVERIFY(int(s->value(a->id(), 0)) > 100);
    const int up = s->value(a->id(), 0);
    turn(0, -1);
    gather();
    QVERIFY(int(s->value(a->id(), 0)) < up);

    // Colour bank: encoder 6 is Blue.
    m_doc->controlMap()->encoderBank()->setBank(EncoderBank::ColourBank);
    s->setValue(a->id(), 7, 20);
    turn(4, 2);                                 // Blue is encoder 5 (slot 4)
    gather();
    QVERIFY(int(s->value(a->id(), 7)) > 20);
    QVERIFY(m_doc->isModified());
}

void EncoderBankUi_Test::aGestureIsOneUndoStep()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    PaletteOps *ops = m_doc->paletteOps();
    QVERIFY(ops != nullptr);
    s->setValue(a->id(), 0, 100);
    QVERIFY(!ops->canUndo());

    // A turn of twenty clicks, however they are gathered, is one step.
    for (int i = 0; i < 20; ++i)
    {
        turn(0, 1);
        if (i % 4 == 3)
            gather();
    }
    gather();
    const int end = s->value(a->id(), 0);
    QVERIFY(end > 100);
    QVERIFY(ops->canUndo());
    QVERIFY(ops->undo());
    QCOMPARE(int(s->value(a->id(), 0)), 100);
    QVERIFY2(!ops->canUndo(), "twenty clicks were more than one step");
    QVERIFY(ops->redo());
    QCOMPARE(int(s->value(a->id(), 0)), end);

    // Another encoder is another gesture: its own step, undone alone.
    ops->undo();
    m_doc->controlMap()->encoderBank()->setBank(EncoderBank::PositionBank);
    turn(0, 4);
    gather();
    turn(1, 4);
    gather();
    QVERIFY(int(s->value(a->id(), 1)) > 0 || int(s->value(a->id(), 2)) > 0);
    QVERIFY(ops->undo());
    QVERIFY(ops->canUndo());                    // the pan is still its own step
    QVERIFY(ops->undo());
    QVERIFY(!ops->canUndo());
}

void EncoderBankUi_Test::aGroupTurnsEveryMemberAndAPushGoesHome()
{
    Fixture *a = mover("A", 0);
    Fixture *b = mover("B", 10);
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Movers");
    g->setSize(QSize(2, 1));
    m_doc->addFixtureGroup(g);
    g->assignFixture(a->id(), QLCPoint(0, 0));
    g->assignFixture(b->id(), QLCPoint(1, 0));

    Workbench wb(m_doc);
    Scene *s = new Scene(m_doc);
    s->setName("Group look");
    m_doc->addFunction(s);
    s->addFixtureGroup(g->id());
    wb.show();
    wb.load(s->id());
    QCoreApplication::processEvents();
    QTreeWidgetItem *row = findKind(wb.rows()->topLevelItem(0), ProgrammingOutline::GroupRow);
    QVERIFY(row != nullptr);
    select(wb.rows(), row);
    QCOMPARE(wb.encoderTarget().fixtures.size(), 2);

    s->setValue(a->id(), 0, 50);
    s->setValue(b->id(), 0, 200);
    turn(0, 4);
    gather();
    const int da = int(s->value(a->id(), 0)) - 50;
    const int db = int(s->value(b->id(), 0)) - 200;
    QVERIFY(da > 0);
    QVERIFY(db > 0 || int(s->value(b->id(), 0)) == 255);     // each moved from where it was

    // A tap on the push: back to the default.
    push(0, true);
    push(0, false);
    QCoreApplication::processEvents();
    QCOMPARE(int(s->value(a->id(), 0)), 0);
    QCOMPARE(int(s->value(b->id(), 0)), 0);
    QVERIFY(m_doc->paletteOps()->undo());
    QCOMPARE(int(s->value(b->id(), 0)), 200 + db);
}

void EncoderBankUi_Test::aDimmerLookIsEditedByTheLookEditor()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Level");
    dim->setValue(128);
    m_doc->addPalette(dim);
    s->addPalette(dim->id());
    wb.editPalette(dim->id());
    QCOMPARE(wb.currentPage(), Workbench::LookPage);
    QVERIFY(wb.encoderTarget().look != nullptr);

    PaletteOps *ops = m_doc->paletteOps();
    const bool hadSteps = ops->canUndo();
    turn(0, 5);
    gather();
    QVERIFY(dim->intValue1() > 128);
    const int up = dim->intValue1();
    turn(0, 5);
    gather();
    QVERIFY(dim->intValue1() > up);
    // Undo takes the whole turn back, as it does a drag of the Look Editor's own bar.
    QVERIFY(ops->canUndo());
    QVERIFY(ops->undo());
    QCOMPARE(dim->intValue1(), 128);
    Q_UNUSED(hadSteps)

    // Home is 0.
    push(0, true);
    push(0, false);
    QCOMPARE(dim->intValue1(), 0);
    // The ring shows where it is.
    float norm = -1;
    QVERIFY(wb.encoderTarget().look->encoderValue(EncoderBank::IntensityBank, 0, &norm));
    QCOMPARE(norm, 0.0f);
    // And this look has no colour.
    QVERIFY(!wb.encoderTarget().look->encoderHas(EncoderBank::ColourBank, 0));
}

void EncoderBankUi_Test::aColourLookTurnsHueAndRgb()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    QLCPalette *col = new QLCPalette(QLCPalette::Color, m_doc);
    col->setName("Red");
    col->setValue(QColor(255, 0, 0).name());
    m_doc->addPalette(col);
    s->addPalette(col->id());
    wb.editPalette(col->id());
    m_doc->controlMap()->encoderBank()->setBank(EncoderBank::ColourBank);
    QCoreApplication::processEvents();

    const QColor before = col->rgbValue();
    QCOMPARE(before.red(), 255);
    turn(0, 6);                                 // Hue
    gather();
    const QColor after = col->rgbValue();
    QVERIFY2(after.green() > 0, qPrintable(after.name()));
    QVERIFY(after.hue() != before.hue() || after.green() != before.green());

    turn(3, -10);                               // encoder 4: Green down (a no-op at its floor is fine)
    turn(2, 10);                                // encoder 3: Red
    gather();
    QVERIFY(m_doc->paletteOps()->canUndo());
    // Saturation to the floor is white-ish.
    turn(1, -127);
    gather();
    const QColor pale = col->rgbValue();
    QVERIFY(qAbs(pale.red() - pale.green()) < 4);
    float hue = -1;
    QVERIFY(wb.encoderTarget().look->encoderValue(EncoderBank::ColourBank, 0, &hue));
    QVERIFY(hue >= 0.0f && hue <= 1.0f);
}

void EncoderBankUi_Test::aPanTiltLookIsNudgedWithUndo()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    QLCPalette *pt = new QLCPalette(QLCPalette::PanTilt, m_doc);
    pt->setName("Centre");
    pt->setValue(270, 135);
    m_doc->addPalette(pt);
    s->addPalette(pt->id());
    wb.editPalette(pt->id());
    m_doc->controlMap()->encoderBank()->setBank(EncoderBank::PositionBank);
    QCoreApplication::processEvents();

    turn(0, 4);
    gather();
    QVERIFY(pt->intValue1() > 270);
    QCOMPARE(pt->intValue2(), 135);
    turn(1, -4);
    gather();
    QVERIFY(pt->intValue2() < 135);
    turn(0, 127);                               // the end stops
    turn(0, 127);
    gather();
    QCOMPARE(pt->intValue1(), 540);
    QVERIFY(m_doc->paletteOps()->canUndo());
    while (m_doc->paletteOps()->canUndo())
        m_doc->paletteOps()->undo();
    QCOMPARE(pt->intValue1(), 270);
    QCOMPARE(pt->intValue2(), 135);

    // Home is the middle.
    turn(0, 8);
    gather();
    push(0, true);
    push(0, false);
    QCOMPARE(pt->intValue1(), 270);
}

void EncoderBankUi_Test::aBeamLookTakesTheBeamBank()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    QLCPalette *beam = new QLCPalette(QLCPalette::Beam, m_doc);
    beam->setName("Soft");
    beam->setValues(QVariantList() << 10 << 20 << 30);
    m_doc->addPalette(beam);
    s->addPalette(beam->id());
    wb.editPalette(beam->id());
    m_doc->controlMap()->encoderBank()->setBank(EncoderBank::BeamBank);
    QCoreApplication::processEvents();

    turn(1, 3);                                 // Focus
    gather();
    QVERIFY(beam->values().at(0).toInt() > 10);
    QCOMPARE(beam->values().at(1).toInt(), 20);
    turn(2, 3);                                 // Iris
    gather();
    QVERIFY(beam->values().at(2).toInt() > 30);
    QVERIFY(m_doc->paletteOps()->canUndo());    // a Beam look edit is a step too
    m_doc->paletteOps()->undo();
    m_doc->paletteOps()->undo();
    QCOMPARE(beam->values().at(0).toInt(), 10);
    QCOMPARE(beam->values().at(2).toInt(), 30);
    // The Zoom is a look of its own: this one has none.
    QSignalSpy line(&wb, &Workbench::paletteEdited);
    turn(0, 3);
    gather();
    QVERIFY(wb.encoderLine().contains("no zoom"));
}

void EncoderBankUi_Test::aLookDrivenChannelSaysSo()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Level");
    dim->setValue(128);
    m_doc->addPalette(dim);
    s->addPalette(dim->id());
    // Back to the fixture's channels.
    QTreeWidgetItem *row = findKind(wb.rows()->topLevelItem(0), ProgrammingOutline::FixtureRow);
    QVERIFY(row != nullptr);
    select(wb.rows(), row);
    wb.selectFixtures({ a->id() });
    QCOMPARE(wb.currentPage(), Workbench::TargetPage);

    s->setValue(a->id(), 0, 40);
    turn(0, 3);
    gather();
    QCOMPARE(int(s->value(a->id(), 0)), 40);
    QVERIFY2(wb.encoderLine().contains("look drives"), qPrintable(wb.encoderLine()));
}

void EncoderBankUi_Test::operateRefusesAndSaysWhy()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    s->setValue(a->id(), 0, 100);

    m_doc->setMode(Doc::Operate);
    turn(0, 5);
    gather();
    QCOMPARE(int(s->value(a->id(), 0)), 100);
    QVERIFY2(wb.encoderLine().contains("Operate"), qPrintable(wb.encoderLine()));
    push(0, true);
    push(0, false);
    QCOMPARE(int(s->value(a->id(), 0)), 100);
    // The banks still step in Operate.
    m_doc->controlMap()->encoderBank()->handleInput(0, kNext, 255);
    QCOMPARE(m_doc->controlMap()->encoderBank()->bank(), int(EncoderBank::ColourBank));

    m_doc->setMode(Doc::Design);
    m_doc->controlMap()->encoderBank()->setBank(EncoderBank::IntensityBank);
    turn(0, 5);
    gather();
    QVERIFY(int(s->value(a->id(), 0)) > 100);
}

void EncoderBankUi_Test::ringsAndTheLineFollowTheSelection()
{
    Fixture *a = mover("A", 0);
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    QSignalSpy sent(enc, &EncoderBank::feedbackSent);
    Workbench wb(m_doc);
    Scene *s = new Scene(m_doc);
    s->setName("Look A");
    m_doc->addFunction(s);
    s->addFixture(a->id());
    s->setValue(a->id(), 0, 51);
    wb.show();
    wb.load(s->id());
    QCoreApplication::processEvents();
    select(wb.rows(), findKind(wb.rows()->topLevelItem(0), ProgrammingOutline::FixtureRow));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY(!wb.encoderLine().isEmpty());
    QVERIFY2(wb.encoderLine().contains("Intensity"), qPrintable(wb.encoderLine()));
    int ring0 = -1;
    for (const QList<QVariant> &f : std::as_const(sent))
        if (f.at(1).toUInt() == kRing)
            ring0 = f.at(2).toInt();
    QCOMPARE(ring0, 51);                        // the ring shows where the value is

    sent.clear();
    turn(0, 10);
    gather();
    int after = -1;
    for (const QList<QVariant> &f : std::as_const(sent))
        if (f.at(1).toUInt() == kRing)
            after = f.at(2).toInt();
    QCOMPARE(after, int(s->value(a->id(), 0)));         // and follows the turn
    QVERIFY(wb.encoderLine().contains("%"));

    // Undo moves it back.
    sent.clear();
    m_doc->paletteOps()->undo();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    after = -1;
    for (const QList<QVariant> &f : std::as_const(sent))
        if (f.at(1).toUInt() == kRing)
            after = f.at(2).toInt();
    QCOMPARE(after, 51);

    // Off: the line goes.
    enc->setEnabled(false);
    QCoreApplication::processEvents();
    QVERIFY(wb.encoderLine().isEmpty());
}

void EncoderBankUi_Test::aBankChangeMovesWhatTheEncodersTurn()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    s->setValue(a->id(), 0, 100);

    enc->handleInput(0, kNext, 255);            // Colour
    QCoreApplication::processEvents();
    QVERIFY(wb.encoderLine().contains("Colour"));
    QVERIFY(wb.encoderLine().contains("Hue"));
    turn(0, 3);                                 // hue, not intensity
    gather();
    QCOMPARE(int(s->value(a->id(), 0)), 100);
    QVERIFY(int(s->value(a->id(), 5)) > 0);     // black came up red
    enc->handleInput(0, kNext, 255);            // Position
    QCoreApplication::processEvents();
    QVERIFY(wb.encoderLine().contains("Pan"));
}

void EncoderBankUi_Test::nothingSelectedSaysSo()
{
    Workbench wb(m_doc);
    wb.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(wb.encoderTarget().scene == nullptr);
    QVERIFY(!wb.encoderTarget().isValid());
    QVERIFY(wb.encoderLine().contains("select"));
    turn(0, 3);
    gather();
    QVERIFY(wb.encoderLine().contains("nothing selected"));
}

void EncoderBankUi_Test::aHiddenWorkbenchIgnoresTheEncoders()
{
    Fixture *a = mover("A", 0);
    Workbench wb(m_doc);
    Scene *s = sceneWith(wb, { a });
    s->setValue(a->id(), 0, 100);
    wb.hide();
    QCoreApplication::processEvents();
    QVERIFY(!wb.encoderTarget().isValid());
    turn(0, 5);
    gather();
    QCOMPARE(int(s->value(a->id(), 0)), 100);
    wb.show();
    QCoreApplication::processEvents();
    turn(0, 5);
    gather();
    QVERIFY(int(s->value(a->id(), 0)) > 100);
}

void EncoderBankUi_Test::theControlMapPanelSetsTheBankUp()
{
    m_doc->controlMap()->encoderBank()->reset();
    ControlMapPanel panel(m_doc);
    panel.show();
    QGroupBox *box = panel.encoderBox();
    QVERIFY(box != nullptr);
    QComboBox *preset = box->findChild<QComboBox *>("encoderPreset");
    QComboBox *encoding = box->findChild<QComboBox *>("encoderEncoding");
    QCheckBox *invert = box->findChild<QCheckBox *>("encoderInvert");
    QLabel *map = box->findChild<QLabel *>("encoderMap");
    QVERIFY(preset && encoding && invert && map);
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    QVERIFY(!enc->isEnabled());
    QCOMPARE(panel.summary(), QString("None"));
    QVERIFY(map->text().startsWith("Off"));
    QVERIFY(!encoding->isEnabled());

    const int at = preset->findData("xtouch-mini");
    QVERIFY(at > 0);
    preset->setCurrentIndex(at);
    emit preset->activated(at);
    QVERIFY(enc->isEnabled());
    QCOMPARE(enc->presetKey(), QString("xtouch-mini"));
    QCOMPARE(enc->device().encoding, EncoderBank::BinaryOffset);
    QCOMPARE(panel.summary(), QString("Encoder bank"));
    QVERIFY(map->text().contains("CC 1"));
    QVERIFY(box->findChild<QLabel *>("encoderNote")->text().contains("Relative 1"));
    QVERIFY(encoding->isEnabled());

    // Another encoding, and reversed: kept on the device.
    encoding->setCurrentIndex(encoding->findData(int(EncoderBank::SignedBit)));
    emit encoding->activated(encoding->currentIndex());
    invert->click();
    QCOMPARE(enc->device().encoding, EncoderBank::SignedBit);
    QVERIFY(enc->device().invert);

    // Off.
    preset->setCurrentIndex(0);
    emit preset->activated(0);
    QVERIFY(!enc->isEnabled());
    QCOMPARE(panel.summary(), QString("None"));
    // Learn: the next control heard is the first encoder.
    preset->setCurrentIndex(at);
    emit preset->activated(at);
    QPushButton *learn = box->findChild<QPushButton *>("encoderLearn");
    QVERIFY(learn != nullptr);
    learn->click();
    QVERIFY(m_doc->controlMap()->isListening());
    m_doc->controlMap()->handleInput(0, 40, 127);
    QCOMPARE(enc->device().encoders, 40);
    QCOMPARE(enc->presetKey(), QString("custom"));
    QVERIFY(map->text().contains("CC 40"));
}

QTEST_MAIN(EncoderBankUi_Test)
