/*
  Q Light Controller Plus - Unit test
  encoderbank_test.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSignalSpy>
#include <QBuffer>
#include <QtTest>
#include <climits>
#include <cstdlib>
#include <new>

#include "encoderbank_test.h"
#include "encoderattributes.h"
#include "inputoutputmap.h"
#include "qlccapability.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfile.h"
#include "qlcinputsource.h"
#include "qlcfixturehead.h"
#include "qlcphysical.h"
#include "iopluginstub.h"
#include "encoderbank.h"
#include "qlcioplugin.h"
#include "controlmap.h"
#include "inputpatch.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

/* Counts this thread's allocations while a test asks (the MasterTimer and the
   other threads allocate on their own; they are not counted). */
namespace
{
thread_local bool tl_counting = false;
thread_local int tl_allocations = 0;
}

void *operator new(std::size_t size)
{
    if (tl_counting)
        ++tl_allocations;
    void *p = std::malloc(size ? size : 1);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

namespace
{
const quint32 kTurn = 16;           // the generic preset: CC 16-23
const quint32 kNext = 24;           // CC 24, CC 25
const quint32 kPrevious = 25;
const quint32 kPush = 128 + 40;     // pushes: notes 40-47
const quint32 kRing = 80;           // rings: CC 80-87
const quint32 kLamp = 128 + 8;      // bank lamps: notes 8-12
const quint32 kDirect = 128 + 8;    // bank buttons: notes 8-12

EncoderBank::Device generic(EncoderBank::Encoding encoding)
{
    EncoderBank::Device d;
    d.name = "Test";
    d.universe = 0;
    d.encoding = encoding;
    d.encoders = kTurn;
    d.pushes = qint32(kPush);
    d.rings = qint32(kRing);
    d.bankNext = qint32(kNext);
    d.bankPrevious = qint32(kPrevious);
    d.bankDirect = qint32(kDirect);
    d.bankLamps = qint32(kLamp);
    return d;
}

QByteArray saveShow(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    w.writeStartElement("Workspace");
    doc->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return xml;
}

bool loadShow(Doc *doc, const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    r.readNextStartElement();       // Workspace
    r.readNextStartElement();       // Engine
    return doc->loadXML(r, false);
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
}

void EncoderBank_Test::init()
{
    m_doc = new Doc(this);
    QDir dir("../iopluginstub");
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub in ../iopluginstub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();

    InputOutputMap *iom = m_doc->inputOutputMap();
    QVERIFY(iom->setInputPatch(0, m_stub->name(), QString(), 0));
    QVERIFY(iom->setOutputPatch(0, m_stub->name(), QString(), 0, true));
}

void EncoderBank_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
    m_stub = nullptr;
}

EncoderBank *EncoderBank_Test::bank() const
{
    return m_doc->controlMap()->encoderBank();
}

void EncoderBank_Test::send(quint32 channel, uchar value)
{
    m_stub->emitValueChanged(UINT_MAX, 0, channel, value);
    m_doc->inputOutputMap()->inputPatch(0)->flush(0);
    QCoreApplication::processEvents();
}

Fixture *EncoderBank_Test::mover(const QString &name, quint32 address)
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
    channel(def, mode, 8, "White", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::White);
    channel(def, mode, 9, "Zoom", QLCChannel::Beam);
    QLCChannel *gobo = channel(def, mode, 10, "Gobo wheel", QLCChannel::Gobo);
    gobo->addCapability(new QLCCapability(0, 9, "Open"));
    gobo->addCapability(new QLCCapability(10, 19, "Dots"));
    gobo->addCapability(new QLCCapability(20, 29, "Stars"));
    gobo->addCapability(new QLCCapability(30, 255, "Shake"));
    channel(def, mode, 11, "Gobo rotation", QLCChannel::Gobo);
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    for (int i = 0; i < 12; ++i)
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

Fixture *EncoderBank_Test::par(const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Par " + name);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("RGB");
    channel(def, mode, 0, "Red", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Red);
    channel(def, mode, 1, "Green", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Green);
    channel(def, mode, 2, "Blue", QLCChannel::Intensity, QLCChannel::MSB, QLCChannel::Blue);
    QLCFixtureHead head;
    for (int i = 0; i < 3; ++i)
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

/*****************************************************************************
 * The controller side
 *****************************************************************************/

void EncoderBank_Test::decodeTwosComplement()
{
    const auto d = [](int v) { return EncoderBank::decode(EncoderBank::TwosComplement, dmx(v)); };
    QCOMPARE(d(0), 0);
    QCOMPARE(d(1), 1);
    QCOMPARE(d(3), 3);
    QCOMPARE(d(63), 63);
    QCOMPARE(d(64), -64);       // the end of the range, as the input source reads it
    QCOMPARE(d(65), -63);
    QCOMPARE(d(127), -1);
    QCOMPARE(d(126), -2);
}

void EncoderBank_Test::decodeSignedBit()
{
    const auto d = [](int v) { return EncoderBank::decode(EncoderBank::SignedBit, dmx(v)); };
    QCOMPARE(d(0x40), 0);
    QCOMPARE(d(0x41), 1);
    QCOMPARE(d(0x45), 5);
    QCOMPARE(d(0x7F), 63);
    QCOMPARE(d(0x01), -1);
    QCOMPARE(d(0x05), -5);
    QCOMPARE(d(0x00), 0);
}

void EncoderBank_Test::decodeBinaryOffset()
{
    const auto d = [](int v) { return EncoderBank::decode(EncoderBank::BinaryOffset, dmx(v)); };
    QCOMPARE(d(64), 0);
    QCOMPARE(d(65), 1);
    QCOMPARE(d(63), -1);
    QCOMPARE(d(127), 63);
    QCOMPARE(d(0), -64);
    QCOMPARE(d(70), 6);
    QCOMPARE(EncoderBank::decode(EncoderBank::AbsoluteDelta, dmx(70)), 0);
}

void EncoderBank_Test::decodeAgreesWithTheInputSource()
{
    // The relative controls everywhere else (a VC slider's encoder) read the
    // same bytes the same way: one convention.
    const QLCInputSource::RelativeEncoding in[] = { QLCInputSource::TwosComplement,
                                                    QLCInputSource::SignedBit,
                                                    QLCInputSource::BinaryOffset };
    const EncoderBank::Encoding mine[] = { EncoderBank::TwosComplement, EncoderBank::SignedBit,
                                           EncoderBank::BinaryOffset };
    for (int e = 0; e < 3; ++e)
    {
        QLCInputSource src;
        src.setRelativeEncoding(in[e]);
        for (int v = 0; v < 128; ++v)
            QCOMPARE(EncoderBank::decode(mine[e], dmx(v)), src.decodeRelativeDelta(dmx(v)));
    }
}

void EncoderBank_Test::absoluteKnobsReadTheirChange()
{
    EncoderBank *b = bank();
    b->setCoalesceMs(0);
    b->setDevice(generic(EncoderBank::AbsoluteDelta));
    QSignalSpy spy(b, &EncoderBank::adjusted);

    send(kTurn, dmx(60));       // only where the knob is
    QCOMPARE(spy.count(), 0);
    send(kTurn, dmx(63));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(2).toInt(), 3);
    send(kTurn, dmx(61));
    QCOMPARE(spy.last().at(2).toInt(), -2);
    send(kTurn, dmx(61));       // not moved
    QCOMPARE(spy.count(), 2);
}

void EncoderBank_Test::invertTurnsItAround()
{
    EncoderBank *b = bank();
    b->setCoalesceMs(0);
    EncoderBank::Device d = generic(EncoderBank::TwosComplement);
    d.invert = true;
    b->setDevice(d);
    QSignalSpy spy(b, &EncoderBank::adjusted);
    send(kTurn, dmx(2));
    QCOMPARE(spy.last().at(2).toInt(), -2);
    send(kTurn, dmx(126));
    QCOMPARE(spy.last().at(2).toInt(), 2);
}

void EncoderBank_Test::turnsAreGatheredIntoOne()
{
    EncoderBank *b = bank();
    b->setDevice(generic(EncoderBank::TwosComplement));     // the default gather time
    QCOMPARE(b->coalesceMs(), 25);
    QSignalSpy spy(b, &EncoderBank::adjusted);
    // send() runs the event loop, so under load the 25 ms gather could end
    // mid-burst. Hold the gather open for the burst; the timer itself is
    // checked below with a single turn.
    b->setCoalesceMs(600000);
    for (int i = 0; i < 6; ++i)
        send(kTurn, dmx(1));
    send(kTurn, dmx(127));
    QCOMPARE(spy.count(), 0);               // held for the gather
    b->flush();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toInt(), int(EncoderBank::IntensityBank));
    QCOMPARE(spy.last().at(2).toInt(), 5);
    // The gather timer delivers on its own (a single turn: nothing to split).
    b->setCoalesceMs(25);
    spy.clear();
    send(kTurn, dmx(1));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
    QCOMPARE(spy.last().at(2).toInt(), 1);
    b->setCoalesceMs(600000);
    spy.clear();
    // Intensity has the one encoder: the second is nothing in this bank.
    send(kTurn + 1, dmx(1));
    b->flush();
    QCOMPARE(spy.count(), 0);

    b->setBank(EncoderBank::ColourBank);
    spy.clear();
    for (int i = 0; i < 6; ++i)
        send(kTurn + 1, dmx(1));
    send(kTurn + 1, dmx(127));
    b->flush();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toInt(), int(EncoderBank::ColourBank));
    QCOMPARE(spy.last().at(1).toInt(), 1);
    QCOMPARE(spy.last().at(2).toInt(), 5);
    QCOMPARE(spy.last().at(3).toBool(), false);
}

void EncoderBank_Test::theInputPathDoesNotAllocate()
{
    EncoderBank *b = bank();
    b->setDevice(generic(EncoderBank::TwosComplement));     // gathered, as it runs
    b->setBank(EncoderBank::ColourBank);
    // Warm: the first turn starts the gather timer.
    b->handleInput(0, kTurn + 2, dmx(1));
    b->flush();

    tl_allocations = 0;
    tl_counting = true;
    for (int i = 0; i < 2000; ++i)
    {
        b->handleInput(0, kTurn + (i % 8), dmx(i % 2 ? 1 : 127));      // turns, both ways, every encoder
        b->handleInput(0, kPush + (i % 8), i % 3 ? 255 : 0);           // pushes
        b->handleInput(0, 99, 255);                                    // a control that is not ours
        b->handleInput(1, kTurn, dmx(1));                              // another universe
    }
    b->flush();
    tl_counting = false;
    QCOMPARE(tl_allocations, 0);

    // The Beam bank has six encoders (Strobe and Shutter are the last two): their turns, and
    // the bank buttons a person learned, take the same path.
    b->setBank(EncoderBank::BeamBank);
    b->handleInput(0, kTurn + 5, dmx(1));
    b->flush();
    tl_allocations = 0;
    tl_counting = true;
    for (int i = 0; i < 2000; ++i)
    {
        b->handleInput(0, kTurn + 4 + (i % 4), dmx(i % 2 ? 1 : 127));  // slots 4-7: the last two count, 6 and 7 do not
        b->handleInput(0, kNext, i % 5 ? 0 : 255);
    }
    b->flush();
    tl_counting = false;
    QCOMPARE(tl_allocations, 0);
    QCOMPARE(EncoderBank::slotCount(EncoderBank::BeamBank), 6);
    QCOMPARE(EncoderBank::slotName(EncoderBank::BeamBank, 4), QString("Strobe"));
    QCOMPARE(EncoderBank::slotName(EncoderBank::BeamBank, 5), QString("Shutter"));
}

void EncoderBank_Test::turnsComeThroughTheInputPatch()
{
    EncoderBank *b = bank();
    b->setCoalesceMs(0);
    b->setDevice(generic(EncoderBank::BinaryOffset));
    QSignalSpy spy(b, &EncoderBank::adjusted);
    send(kTurn, dmx(66));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(1).toInt(), 0);
    QCOMPARE(spy.last().at(2).toInt(), 2);
    // Not this controller's: another universe, another channel.
    b->handleInput(1, kTurn, dmx(66));
    send(kTurn + 9, dmx(66));
    QCOMPARE(spy.count(), 1);
}

void EncoderBank_Test::aBankButtonStepsAndWraps()
{
    EncoderBank *b = bank();
    b->setDevice(generic(EncoderBank::TwosComplement));
    QSignalSpy spy(b, &EncoderBank::bankChanged);
    QCOMPARE(b->bank(), int(EncoderBank::IntensityBank));
    send(kNext, 127);
    QCOMPARE(b->bank(), int(EncoderBank::ColourBank));
    send(kNext, 0);                         // the release does nothing
    QCOMPARE(b->bank(), int(EncoderBank::ColourBank));
    for (int i = 0; i < 3; ++i)
        send(kNext, 127);
    QCOMPARE(b->bank(), int(EncoderBank::GoboBank));
    send(kNext, 127);
    QCOMPARE(b->bank(), int(EncoderBank::IntensityBank));       // round
    send(kPrevious, 127);
    QCOMPARE(b->bank(), int(EncoderBank::GoboBank));
    QCOMPARE(spy.count(), 6);
    QCOMPARE(spy.last().at(0).toInt(), int(EncoderBank::GoboBank));
}

void EncoderBank_Test::directButtonsPickABank()
{
    EncoderBank *b = bank();
    b->setDevice(generic(EncoderBank::TwosComplement));
    send(kDirect + 3, 127);
    QCOMPARE(b->bank(), int(EncoderBank::BeamBank));
    send(kDirect + 1, 127);
    QCOMPARE(b->bank(), int(EncoderBank::ColourBank));
    send(kDirect + 5, 127);                 // a sixth button is nothing
    QCOMPARE(b->bank(), int(EncoderBank::ColourBank));
}

void EncoderBank_Test::pushIsFineWhenTurnedAndHomeWhenTapped()
{
    EncoderBank *b = bank();
    b->setCoalesceMs(0);
    b->setDevice(generic(EncoderBank::TwosComplement));
    b->setBank(EncoderBank::ColourBank);
    QSignalSpy turned(b, &EncoderBank::adjusted);
    QSignalSpy homed(b, &EncoderBank::homed);

    // Tapped: home.
    send(kPush + 2, 127);
    send(kPush + 2, 0);
    QCOMPARE(homed.count(), 1);
    QCOMPARE(homed.last().at(0).toInt(), int(EncoderBank::ColourBank));
    QCOMPARE(homed.last().at(1).toInt(), 2);

    // Held and turned: fine, and not home on the release.
    send(kPush + 2, 127);
    QVERIFY(b->isFine(2));
    send(kTurn + 2, dmx(1));
    QCOMPARE(turned.last().at(3).toBool(), true);
    send(kTurn + 3, dmx(1));                // another encoder is coarse
    QCOMPARE(turned.last().at(3).toBool(), false);
    send(kPush + 2, 0);
    QVERIFY(!b->isFine(2));
    QCOMPARE(homed.count(), 1);
    send(kTurn + 2, dmx(1));
    QCOMPARE(turned.last().at(3).toBool(), false);

    // A push on an encoder the bank doesn't use is nothing.
    b->setBank(EncoderBank::IntensityBank);
    send(kPush + 4, 127);
    send(kPush + 4, 0);
    QCOMPARE(homed.count(), 1);
}

void EncoderBank_Test::operateRefusesTurnsButStepsBanks()
{
    EncoderBank *b = bank();
    b->setCoalesceMs(0);
    b->setDevice(generic(EncoderBank::TwosComplement));
    QSignalSpy turned(b, &EncoderBank::adjusted);
    QSignalSpy homed(b, &EncoderBank::homed);
    QSignalSpy refused(b, &EncoderBank::refused);

    m_doc->setMode(Doc::Operate);
    send(kTurn, dmx(1));
    send(kTurn, dmx(1));
    send(kPush, 127);
    send(kPush, 0);
    QCOMPARE(turned.count(), 0);
    QCOMPARE(homed.count(), 0);
    QCOMPARE(refused.count(), 1);           // told once, not at every click
    QVERIFY(refused.last().at(0).toString().contains("Operate"));
    send(kNext, 127);
    QCOMPARE(b->bank(), int(EncoderBank::ColourBank));

    m_doc->setMode(Doc::Design);
    send(kTurn, dmx(1));
    QCOMPARE(turned.count(), 1);
}

void EncoderBank_Test::offIsNotListening()
{
    EncoderBank *b = bank();
    b->setCoalesceMs(0);
    QVERIFY(!b->isEnabled());
    QSignalSpy spy(b, &EncoderBank::adjusted);
    send(kTurn, dmx(1));
    QCOMPARE(spy.count(), 0);

    b->setDevice(generic(EncoderBank::TwosComplement));
    QVERIFY(b->isEnabled());
    send(kTurn, dmx(1));
    QCOMPARE(spy.count(), 1);

    b->setEnabled(false);
    send(kTurn, dmx(1));
    QCOMPARE(spy.count(), 1);
    b->setEnabled(true);
    send(kTurn, dmx(1));
    QCOMPARE(spy.count(), 2);

    // A mapping without encoders can't be on.
    b->setDevice(EncoderBank::Device());
    QVERIFY(!b->isEnabled());
}

void EncoderBank_Test::ringsAreSentOnlyWhenTheyChange()
{
    EncoderBank *b = bank();
    b->setDevice(generic(EncoderBank::TwosComplement));
    m_stub->m_feedback.clear();
    QSignalSpy sent(b, &EncoderBank::feedbackSent);

    b->setRing(0, 0.5f);
    b->setRing(0, 0.5f);                    // the same: not sent again
    QCOMPARE(sent.count(), 1);
    QCOMPARE(sent.last().at(1).toUInt(), kRing);
    QCOMPARE(sent.last().at(2).toInt(), 128);
    // ... and it reached the controller's feedback line.
    QCOMPARE(m_stub->m_feedback.count(), 1);
    QCOMPARE(m_stub->m_feedback.last().first, kRing);
    QCOMPARE(int(m_stub->m_feedback.last().second), 128);

    b->setRing(1, 1.0f);
    QCOMPARE(sent.last().at(1).toUInt(), kRing + 1);
    QCOMPARE(sent.last().at(2).toInt(), 255);
    b->setRing(1, 7.0f);                    // clamped to the end
    QCOMPARE(sent.count(), 2);
    b->setRing(1, -1.0f);                   // nothing there: dark
    QCOMPARE(sent.last().at(2).toInt(), 0);
    b->setRing(8, 1.0f);                    // there is no ninth
    QCOMPARE(sent.count(), 3);

    sent.clear();
    b->clearRings();
    QVERIFY(sent.count() >= 1);
    QCOMPARE(sent.first().at(1).toUInt(), kRing);       // the one lit goes dark
    QCOMPARE(sent.first().at(2).toInt(), 0);

    // A bank change lights them afresh.
    b->setRing(0, 0.25f);
    const int before = sent.count();
    b->setBank(EncoderBank::ColourBank);
    b->setRing(0, 0.25f);
    QVERIFY(sent.count() > before);
}

void EncoderBank_Test::bankLampsFollowTheBank()
{
    EncoderBank *b = bank();
    b->setDevice(generic(EncoderBank::TwosComplement));
    QSignalSpy sent(b, &EncoderBank::feedbackSent);
    b->setBank(EncoderBank::BeamBank);

    QVector<int> lamps(5, -1);
    for (const QList<QVariant> &s : std::as_const(sent))
    {
        const quint32 ch = s.at(1).toUInt();
        if (ch >= kLamp && ch < kLamp + 5)
            lamps[int(ch - kLamp)] = s.at(2).toInt();
    }
    QCOMPARE(lamps, (QVector<int>{ 0, 0, 0, 255, 0 }));

    // A controller coming back is told again.
    sent.clear();
    QSignalSpy wanted(b, &EncoderBank::ringsWanted);
    b->resendFeedback();
    QCOMPARE(wanted.count(), 1);
    QCOMPARE(sent.count(), 5);
}

void EncoderBank_Test::everyPresetIsAWholeMapping()
{
    const QStringList keys = EncoderBank::presetKeys();
    QVERIFY(keys.contains("xtouch-mini"));
    QVERIFY(keys.contains("twister"));
    QVERIFY(keys.contains("apc40"));
    QVERIFY(keys.contains("generic-twos"));
    QVERIFY(keys.contains("generic-offset"));
    QVERIFY(keys.contains("generic-signmag"));
    for (const QString &key : keys)
    {
        EncoderBank::Device d;
        d.universe = 2;
        QVERIFY(EncoderBank::preset(key, &d));
        QVERIFY2(!EncoderBank::presetName(key).isEmpty(), qPrintable(key));
        QVERIFY2(!EncoderBank::presetNote(key).isEmpty(), qPrintable(key));
        QCOMPARE(d.universe, 2u);                   // the universe is the show's, not the preset's
        QVERIFY2(d.encoders >= 0, qPrintable(key));
        QVERIFY2(d.bankNext >= 0 || d.bankDirect >= 0, qPrintable(key));
        QCOMPARE(d.encoderCount, 8);
    }
    QVERIFY(!EncoderBank::preset("no-such", nullptr));
    EncoderBank::Device d;
    QVERIFY(!EncoderBank::preset("no-such", &d));

    EncoderBank::preset("apc40", &d);
    QCOMPARE(d.encoding, EncoderBank::AbsoluteDelta);
    EncoderBank::preset("generic-signmag", &d);
    QCOMPARE(d.encoding, EncoderBank::SignedBit);
    EncoderBank::preset("xtouch-mini", &d);
    QCOMPARE(d.encoding, EncoderBank::BinaryOffset);
    QCOMPARE(d.encoders, 1);
    QCOMPARE(d.pushes, 128);
    // The Twister's pushes are on its second MIDI channel (an Omni patch).
    EncoderBank::preset("twister", &d);
    QCOMPARE(d.pushes, 4096);
    QVERIFY(EncoderBank::controlName(d.pushes).contains("MIDI ch 2"));
    QCOMPARE(EncoderBank::controlName(16), QString("CC 16"));
    QVERIFY(EncoderBank::controlName(128 + 60).contains("C4"));
    QVERIFY(EncoderBank::controlName(-1).isEmpty());
}

void EncoderBank_Test::savedWithTheControlMapAndLoadedBack()
{
    EncoderBank *b = bank();
    EncoderBank::Device d = generic(EncoderBank::SignedBit);
    d.invert = true;
    d.universe = 0;
    b->setDevice(d, "generic-signmag");
    const QByteArray xml = saveShow(m_doc);
    QVERIFY(xml.contains("<ControlMap>"));
    QVERIFY(xml.contains("<EncoderBank"));

    m_doc->clearContents();
    QVERIFY(!bank()->isEnabled());
    QVERIFY(bank()->device().encoders < 0);

    QVERIFY(loadShow(m_doc, xml));
    b = bank();
    QVERIFY(b->isEnabled());
    QCOMPARE(b->presetKey(), QString("generic-signmag"));
    QCOMPARE(b->device().encoding, EncoderBank::SignedBit);
    QVERIFY(b->device().invert);
    QCOMPARE(b->device().encoders, d.encoders);
    QCOMPARE(b->device().pushes, d.pushes);
    QCOMPARE(b->device().rings, d.rings);
    QCOMPARE(b->device().bankNext, d.bankNext);
    QCOMPARE(b->device().bankPrevious, d.bankPrevious);
    QCOMPARE(b->device().bankDirect, d.bankDirect);
    QCOMPARE(b->device().bankLamps, d.bankLamps);

    // It listens again, with no step in between.
    b->setCoalesceMs(0);
    QSignalSpy spy(b, &EncoderBank::adjusted);
    send(kTurn, dmx(0x42));
    QCOMPARE(spy.count(), 0 + 1);
    QCOMPARE(spy.last().at(2).toInt(), -2);     // inverted sign-magnitude, +2

    // Off is kept off.
    b->setEnabled(false);
    const QByteArray off = saveShow(m_doc);
    m_doc->clearContents();
    QVERIFY(loadShow(m_doc, off));
    QVERIFY(!bank()->isEnabled());
    QVERIFY(bank()->device().encoders >= 0);
}

void EncoderBank_Test::aShowWithoutOneSavesAsBefore()
{
    const QByteArray xml = saveShow(m_doc);
    QVERIFY(!xml.contains("EncoderBank"));
    QVERIFY(!xml.contains("ControlMap"));
}

/*****************************************************************************
 * What a slot means on fixtures
 *****************************************************************************/

void EncoderBank_Test::intensityClampsAtBothEnds()
{
    Fixture *m = mover("A", 0);
    QVERIFY(m != nullptr);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { m->id() };
    using namespace EncoderAttributes;

    s->setValue(m->id(), 0, 250);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 3, false));
    QCOMPARE(int(s->value(m->id(), 0)), 255);               // clamped, not wrapped
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 100, false));
    QCOMPARE(int(s->value(m->id(), 0)), 255);
    s->setValue(m->id(), 0, 4);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, -3, false));
    QCOMPARE(int(s->value(m->id(), 0)), 0);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, -50, true));
    QCOMPARE(int(s->value(m->id(), 0)), 0);

    // Nothing to turn: a slot past the bank's, a slot with no detents.
    QVERIFY(!adjust(m_doc, s, sel, EncoderBank::IntensityBank, 3, 1, false));
    QVERIFY(!adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 0, false));
    QVERIFY(!adjust(m_doc, s, QList<quint32>(), EncoderBank::IntensityBank, 0, 1, false));
}

void EncoderBank_Test::coarseAndFineSteps()
{
    Fixture *m = mover("A", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { m->id() };
    using namespace EncoderAttributes;

    s->setValue(m->id(), 0, 100);
    adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 1, false);
    const int coarse = int(s->value(m->id(), 0)) - 100;
    s->setValue(m->id(), 0, 100);
    adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 1, true);
    const int fine = int(s->value(m->id(), 0)) - 100;
    QVERIFY2(coarse > fine, qPrintable(QString("%1 vs %2").arg(coarse).arg(fine)));
    QCOMPARE(fine, 1);                                      // one DMX value
    QVERIFY(step(EncoderBank::IntensityBank, 0, false) > step(EncoderBank::IntensityBank, 0, true));
    // Detents add up.
    s->setValue(m->id(), 0, 100);
    adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 4, false);
    QCOMPARE(int(s->value(m->id(), 0)) - 100, 4 * coarse);
}

void EncoderBank_Test::hueWrapsAndSaturationClamps()
{
    Fixture *p = par("P", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { p->id() };
    using namespace EncoderAttributes;

    // Red, full.
    s->setValue(p->id(), 0, 255);
    s->setValue(p->id(), 1, 0);
    s->setValue(p->id(), 2, 0);
    float h = -1;
    QVERIFY(read(m_doc, s, sel, EncoderBank::ColourBank, 0, &h));
    QVERIFY(qAbs(h) < 0.01f);

    // A turn the other way goes round the wheel to magenta-red, not to a stop.
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::ColourBank, 0, -6, false));
    QVERIFY(read(m_doc, s, sel, EncoderBank::ColourBank, 0, &h));
    QVERIFY2(h > 0.9f, qPrintable(QString::number(h)));
    QVERIFY(int(s->value(p->id(), 2)) > 0);                 // blue came in

    // Saturation stops at the ends.
    float sat = -1;
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::ColourBank, 1, -200, false));
    QVERIFY(read(m_doc, s, sel, EncoderBank::ColourBank, 1, &sat));
    QVERIFY(sat < 0.001f);
    QCOMPARE(int(s->value(p->id(), 0)), int(s->value(p->id(), 1)));       // white
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::ColourBank, 1, 500, false));
    QVERIFY(read(m_doc, s, sel, EncoderBank::ColourBank, 1, &sat));
    QVERIFY(sat > 0.999f);

    // A hue turn from black brings the colour up rather than doing nothing.
    s->setValue(p->id(), 0, 0);
    s->setValue(p->id(), 1, 0);
    s->setValue(p->id(), 2, 0);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::ColourBank, 0, 1, false));
    QVERIFY(int(s->value(p->id(), 0)) > 0);

    // Home: hue to red, saturation to full.
    QVERIFY(home(m_doc, s, sel, EncoderBank::ColourBank, 0));
    QVERIFY(qAbs(float(s->value(p->id(), 1))) < 1.0f);
}

void EncoderBank_Test::colourChannelsMoveEachFixtureFromWhereItIs()
{
    Fixture *a = par("A", 0);
    Fixture *b = par("B", 10);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { a->id(), b->id() };
    using namespace EncoderAttributes;

    s->setValue(a->id(), 2, 10);
    s->setValue(b->id(), 2, 200);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::ColourBank, 4, 2, false));      // Blue
    const int da = int(s->value(a->id(), 2)) - 10;
    const int db = int(s->value(b->id(), 2)) - 200;
    QVERIFY(da > 0);
    QCOMPARE(da, db == 0 ? 0 : da);
    QVERIFY(int(s->value(b->id(), 2)) <= 255);              // the one near the end clamps alone
    QCOMPARE(int(s->value(a->id(), 0)), 0);                 // red untouched

    // A mover has white; a par has none.
    Fixture *m = mover("M", 20);
    QVERIFY(available(m_doc, { m->id() }, EncoderBank::ColourBank, 5));
    QVERIFY(!available(m_doc, { a->id() }, EncoderBank::ColourBank, 5));
    QVERIFY(!available(m_doc, { a->id() }, EncoderBank::PositionBank, 0));
    QVERIFY(!adjust(m_doc, s, { a->id() }, EncoderBank::ColourBank, 5, 1, false));
}

void EncoderBank_Test::panTiltAreSixteenBitAndHomeIsCentre()
{
    Fixture *m = mover("M", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { m->id() };
    using namespace EncoderAttributes;

    // Home: the middle of both bytes.
    QVERIFY(home(m_doc, s, sel, EncoderBank::PositionBank, 0));
    QCOMPARE(int(s->value(m->id(), 1)), 128);
    QCOMPARE(int(s->value(m->id(), 2)), 0);
    float norm = 0;
    QVERIFY(read(m_doc, s, sel, EncoderBank::PositionBank, 0, &norm));
    QVERIFY(qAbs(norm - 0.5f) < 0.01f);

    // A fine detent is a small part of one coarse byte; coarse a few.
    const int start = (int(s->value(m->id(), 1)) << 8) | int(s->value(m->id(), 2));
    adjust(m_doc, s, sel, EncoderBank::PositionBank, 0, 1, true);
    const int fine = ((int(s->value(m->id(), 1)) << 8) | int(s->value(m->id(), 2))) - start;
    QVERIFY(fine > 0 && fine < 256);
    adjust(m_doc, s, sel, EncoderBank::PositionBank, 0, 1, false);
    const int both = ((int(s->value(m->id(), 1)) << 8) | int(s->value(m->id(), 2))) - start - fine;
    QVERIFY(both > fine);

    // The ends clamp in 16 bits.
    adjust(m_doc, s, sel, EncoderBank::PositionBank, 1, 127 * 8, false);
    adjust(m_doc, s, sel, EncoderBank::PositionBank, 1, 127 * 8, false);
    QCOMPARE(int(s->value(m->id(), 3)), 255);
    QCOMPARE(int(s->value(m->id(), 4)), 255);
    adjust(m_doc, s, sel, EncoderBank::PositionBank, 1, -127 * 20, false);
    QCOMPARE(int(s->value(m->id(), 3)), 0);
    QCOMPARE(int(s->value(m->id(), 4)), 0);
}

void EncoderBank_Test::goboStepsThroughItsRanges()
{
    Fixture *m = mover("M", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { m->id() };
    using namespace EncoderAttributes;

    s->setValue(m->id(), 10, 0);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::GoboBank, 0, 1, false));
    QCOMPARE(int(s->value(m->id(), 10)), 10);               // the next named range
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::GoboBank, 0, 1, false));
    QCOMPARE(int(s->value(m->id(), 10)), 20);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::GoboBank, 0, 5, false));
    QCOMPARE(int(s->value(m->id(), 10)), 30);               // the last one holds
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::GoboBank, 0, -9, false));
    QCOMPARE(int(s->value(m->id(), 10)), 0);
    // Fine is one raw value.
    s->setValue(m->id(), 10, 12);
    adjust(m_doc, s, sel, EncoderBank::GoboBank, 0, 1, true);
    QCOMPARE(int(s->value(m->id(), 10)), 13);
    // Part way into a range, the first turn down goes to its start.
    s->setValue(m->id(), 10, 25);
    adjust(m_doc, s, sel, EncoderBank::GoboBank, 0, -1, false);
    QCOMPARE(int(s->value(m->id(), 10)), 20);

    // The rotation is the other slot, and a raw channel.
    QVERIFY(available(m_doc, sel, EncoderBank::GoboBank, 1));
    QVERIFY(!available(m_doc, sel, EncoderBank::GoboBank, 2));      // no prism
    s->setValue(m->id(), 11, 100);
    adjust(m_doc, s, sel, EncoderBank::GoboBank, 1, 1, false);
    QVERIFY(int(s->value(m->id(), 11)) > 100);
    // Beam: zoom is there, focus is not.
    QVERIFY(available(m_doc, sel, EncoderBank::BeamBank, 0));
    QVERIFY(!available(m_doc, sel, EncoderBank::BeamBank, 1));
}

void EncoderBank_Test::aDimmerlessLightTakesIntensityFromItsColour()
{
    Fixture *p = par("P", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { p->id() };
    using namespace EncoderAttributes;

    QVERIFY(available(m_doc, sel, EncoderBank::IntensityBank, 0));
    // From black, white comes up.
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 2, false));
    QVERIFY(int(s->value(p->id(), 0)) > 0);
    QCOMPARE(int(s->value(p->id(), 0)), int(s->value(p->id(), 1)));
    // A colour keeps its mix while it dims.
    s->setValue(p->id(), 0, 200);
    s->setValue(p->id(), 1, 100);
    s->setValue(p->id(), 2, 0);
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, -5, false));
    const int r = s->value(p->id(), 0);
    const int g = s->value(p->id(), 1);
    QVERIFY(r < 200);
    QVERIFY(qAbs(r - 2 * g) <= 2);
    float norm = 0;
    QVERIFY(read(m_doc, s, sel, EncoderBank::IntensityBank, 0, &norm));
    QVERIFY(qAbs(norm - float(r) / 255.0f) < 0.01f);
    QVERIFY(home(m_doc, s, sel, EncoderBank::IntensityBank, 0));
    QCOMPARE(int(s->value(p->id(), 0)), 0);
}

void EncoderBank_Test::aLookDrivingAChannelIsLeftAlone()
{
    Fixture *m = mover("M", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    s->addFixture(m->id());
    QLCPalette *dim = new QLCPalette(QLCPalette::Dimmer, m_doc);
    dim->setName("Full");
    dim->setValue(255);
    QVERIFY(m_doc->addPalette(dim));
    s->addPalette(dim->id());
    const QList<quint32> sel = { m->id() };
    using namespace EncoderAttributes;

    QVERIFY(drivenByLook(m_doc, s, m->id(), 0));
    QVERIFY(!drivenByLook(m_doc, s, m->id(), 1));
    s->setValue(m->id(), 0, 40);
    QVERIFY(!adjust(m_doc, s, sel, EncoderBank::IntensityBank, 0, 3, false));
    QCOMPARE(int(s->value(m->id(), 0)), 40);                // the look wins; nothing written
    QVERIFY(!home(m_doc, s, sel, EncoderBank::IntensityBank, 0));
    // Its other channels are free.
    QVERIFY(adjust(m_doc, s, sel, EncoderBank::PositionBank, 0, 1, false));
}

void EncoderBank_Test::readGivesTheRingsValue()
{
    Fixture *m = mover("M", 0);
    Scene *s = new Scene(m_doc);
    m_doc->addFunction(s);
    const QList<quint32> sel = { m->id() };
    using namespace EncoderAttributes;

    float norm = -1;
    s->setValue(m->id(), 0, 255);
    QVERIFY(read(m_doc, s, sel, EncoderBank::IntensityBank, 0, &norm));
    QCOMPARE(norm, 1.0f);
    s->setValue(m->id(), 0, 51);
    QVERIFY(read(m_doc, s, sel, EncoderBank::IntensityBank, 0, &norm));
    QVERIFY(qAbs(norm - 0.2f) < 0.001f);
    // Nothing set: the channel's default.
    QVERIFY(read(m_doc, s, sel, EncoderBank::BeamBank, 0, &norm));
    QVERIFY(norm >= 0.0f && norm <= 1.0f);
    // What the fixture lacks has no value to show.
    QVERIFY(!read(m_doc, s, sel, EncoderBank::BeamBank, 3, &norm));
    QVERIFY(!read(m_doc, nullptr, sel, EncoderBank::BeamBank, 0, &norm));
}

QTEST_GUILESS_MAIN(EncoderBank_Test)
