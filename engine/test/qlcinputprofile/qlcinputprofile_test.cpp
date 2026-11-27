/*
  Q Light Controller - Unit tests
  qlcinputprofile_test.cpp

  Copyright (C) Heikki Junnila

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
#include <QtTest>
#include <QBuffer>
#include <QScopedPointer>
#include <QTemporaryDir>

#include "qlcinputprofile_test.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "qlcchannel.h"
#include "surfacelayout.h"
#include "controlsurface.h"
#include "faderpickup.h"

#include "../common/resource_paths.h"

void QLCInputProfile_Test::manufacturer()
{
    QLCInputProfile ip;
    QVERIFY(ip.manufacturer().isEmpty());
    ip.setManufacturer("Behringer");
    QVERIFY(ip.manufacturer() == "Behringer");
}

void QLCInputProfile_Test::model()
{
    QLCInputProfile ip;
    QVERIFY(ip.model().isEmpty());
    ip.setModel("BCF2000");
    QVERIFY(ip.model() == "BCF2000");
}

void QLCInputProfile_Test::name()
{
    QLCInputProfile ip;
    QVERIFY(ip.name() == " ");
    ip.setManufacturer("Behringer");
    QVERIFY(ip.name() == "Behringer ");
    ip.setModel("BCF2000");
    QVERIFY(ip.name() == "Behringer BCF2000");
}

void QLCInputProfile_Test::addChannel()
{
    QLCInputProfile ip;

    QLCInputChannel* ich1 = new QLCInputChannel();
    ip.insertChannel(0, ich1);
    QVERIFY(ip.channel(0) == ich1);
    QVERIFY(ip.channels().size() == 1);

    /* Shouldn't overwrite the existing mapping */
    QLCInputChannel* ich2 = new QLCInputChannel();
    ip.insertChannel(0, ich2);
    QVERIFY(ip.channel(0) == ich1);

    ip.insertChannel(5, ich2);
    QVERIFY(ip.channel(0) == ich1);
    QVERIFY(ip.channel(1) == NULL);
    QVERIFY(ip.channel(2) == NULL);
    QVERIFY(ip.channel(3) == NULL);
    QVERIFY(ip.channel(4) == NULL);
    QVERIFY(ip.channel(5) == ich2);
}

void QLCInputProfile_Test::removeChannel()
{
    QLCInputProfile ip;

    QLCInputChannel* ich1 = new QLCInputChannel();
    ip.insertChannel(0, ich1);
    QVERIFY(ip.channel(0) == ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ip.insertChannel(5, ich2);
    QVERIFY(ip.channel(0) == ich1);
    QVERIFY(ip.channel(5) == ich2);

    QVERIFY(ip.removeChannel(1) == false);
    QVERIFY(ip.removeChannel(2) == false);
    QVERIFY(ip.removeChannel(3) == false);
    QVERIFY(ip.removeChannel(4) == false);

    QVERIFY(ip.channel(0) == ich1);
    QVERIFY(ip.channel(5) == ich2);
    QVERIFY(ip.removeChannel(0) == true);
    QVERIFY(ip.channel(0) == NULL);
    QVERIFY(ip.channel(5) == ich2);
    QVERIFY(ip.removeChannel(5) == true);
    QVERIFY(ip.channel(0) == NULL);
    QVERIFY(ip.channel(5) == NULL);
}

void QLCInputProfile_Test::remapChannel()
{
    QLCInputProfile ip;

    QLCInputChannel* ich1 = new QLCInputChannel();
    ich1->setName("Foobar");
    ip.insertChannel(0, ich1);
    QVERIFY(ip.channel(0) == ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ip.insertChannel(5, ich2);
    QVERIFY(ip.channel(0) == ich1);
    QVERIFY(ip.channel(5) == ich2);

    QVERIFY(ip.remapChannel(ich1, 9000) == true);
    QVERIFY(ip.channel(0) == NULL);
    QVERIFY(ip.channel(5) == ich2);
    QVERIFY(ip.channel(9000) == ich1);
    QVERIFY(ip.channel(9000)->name() == "Foobar");

    QVERIFY(ip.remapChannel(NULL, 9000) == false);
    QVERIFY(ip.channels().size() == 2);
    QVERIFY(ip.channel(0) == NULL);
    QVERIFY(ip.channel(5) == ich2);
    QVERIFY(ip.channel(9000) == ich1);
    QVERIFY(ip.channel(9000)->name() == "Foobar");

    QLCInputChannel* ich3 = new QLCInputChannel();
    QVERIFY(ip.remapChannel(ich3, 5) == false);
    QVERIFY(ip.channels().size() == 2);
    QVERIFY(ip.channel(0) == NULL);
    QVERIFY(ip.channel(5) == ich2);
    QVERIFY(ip.channel(9000) == ich1);
    QVERIFY(ip.channel(9000)->name() == "Foobar");

    delete ich3;
}

void QLCInputProfile_Test::channel()
{
    QLCInputProfile ip;

    QLCInputChannel* ich1 = new QLCInputChannel();
    ip.insertChannel(0, ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ip.insertChannel(5, ich2);

    QVERIFY(ip.channel(0) == ich1);
    QVERIFY(ip.channel(1) == NULL);
    QVERIFY(ip.channel(2) == NULL);
    QVERIFY(ip.channel(3) == NULL);
    QVERIFY(ip.channel(4) == NULL);
    QVERIFY(ip.channel(5) == ich2);
}

void QLCInputProfile_Test::channels()
{
    QLCInputProfile ip;
    QVERIFY(ip.channels().size() == 0);

    QLCInputChannel* ich1 = new QLCInputChannel();
    ip.insertChannel(0, ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ip.insertChannel(5, ich2);

    QVERIFY(ip.channels().size() == 2);
    QVERIFY(ip.channels().contains(0) == true);
    QVERIFY(ip.channels().contains(1) == false);
    QVERIFY(ip.channels().contains(2) == false);
    QVERIFY(ip.channels().contains(3) == false);
    QVERIFY(ip.channels().contains(4) == false);
    QVERIFY(ip.channels().contains(5) == true);
    QVERIFY(ip.channels()[0] == ich1);
    QVERIFY(ip.channels()[5] == ich2);
}

void QLCInputProfile_Test::channelNumber()
{
    QLCInputProfile ip;
    QVERIFY(ip.channels().size() == 0);

    QLCInputChannel* ich1 = new QLCInputChannel();
    ip.insertChannel(0, ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ip.insertChannel(6510, ich2);

    QLCInputChannel* ich3 = new QLCInputChannel();
    ip.insertChannel(5, ich3);

    QCOMPARE(ip.channelNumber(NULL), QLCChannel::invalid());
    QCOMPARE(ip.channelNumber(ich1), quint32(0));
    QCOMPARE(ip.channelNumber(ich2), quint32(6510));
    QCOMPARE(ip.channelNumber(ich3), quint32(5));
}

void QLCInputProfile_Test::copy()
{
    QLCInputProfile ip;
    ip.setManufacturer("Behringer");
    ip.setModel("BCF2000");

    QLCInputChannel* ich1 = new QLCInputChannel();
    ich1->setName("Channel 1");
    ip.insertChannel(0, ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ich2->setName("Channel 2");
    ip.insertChannel(5, ich2);

    QLCInputChannel* ich3 = new QLCInputChannel();
    ich3->setName("Channel 3");
    ip.insertChannel(2, ich3);

    QLCInputChannel* ich4 = new QLCInputChannel();
    ich4->setName("Channel 4");
    ip.insertChannel(9000, ich4);

    QLCInputProfile *copy = ip.createCopy();
    QVERIFY(copy->manufacturer() == "Behringer");
    QVERIFY(copy->model() == "BCF2000");

    QVERIFY(copy->channels().size() == 4);

    /* Verify that it's a deep copy */
    QVERIFY(copy->channel(0) != ich1);
    QVERIFY(copy->channel(0) != NULL);
    QVERIFY(copy->channel(0)->name() == "Channel 1");

    QVERIFY(copy->channel(5) != ich2);
    QVERIFY(copy->channel(5) != NULL);
    QVERIFY(copy->channel(5)->name() == "Channel 2");

    QVERIFY(copy->channel(2) != ich3);
    QVERIFY(copy->channel(2) != NULL);
    QVERIFY(copy->channel(2)->name() == "Channel 3");

    QVERIFY(copy->channel(9000) != ich4);
    QVERIFY(copy->channel(9000) != NULL);
    QVERIFY(copy->channel(9000)->name() == "Channel 4");
}

void QLCInputProfile_Test::assign()
{
    QLCInputProfile ip;
    ip.setManufacturer("Behringer");
    ip.setModel("BCF2000");

    QLCInputChannel* ich1 = new QLCInputChannel;
    ich1->setName("Channel 1");
    ip.insertChannel(0, ich1);

    QLCInputChannel* ich2 = new QLCInputChannel;
    ich2->setName("Channel 2");
    ip.insertChannel(5, ich2);

    QLCInputChannel* ich3 = new QLCInputChannel;
    ich3->setName("Channel 3");
    ip.insertChannel(2, ich3);

    QLCInputChannel* ich4 = new QLCInputChannel;
    ich4->setName("Channel 4");
    ip.insertChannel(9000, ich4);

    QLCInputProfile ip2;
    QLCInputChannel* ich5 = new QLCInputChannel;
    ich5->setName("First channel");
    ip2.insertChannel(0, ich5);
    QCOMPARE(ip2.channels().size(), 1);

    /* Test the assignment operator */
    ip2 = ip;
    QCOMPARE(ip2.channels().size(), 4);
    QVERIFY(ip2.channel(0) != NULL);
    QVERIFY(ip2.channel(0) != ich1);
    QVERIFY(ip2.channel(0) != ip.channel(0));
    QCOMPARE(ip2.channel(0)->name(), QString("Channel 1"));

    QVERIFY(ip2.channel(5) != NULL);
    QVERIFY(ip2.channel(5) != ich2);
    QVERIFY(ip2.channel(5) != ip.channel(5));
    QCOMPARE(ip2.channel(5)->name(), QString("Channel 2"));

    QVERIFY(ip2.channel(2) != NULL);
    QVERIFY(ip2.channel(2) != ich3);
    QVERIFY(ip2.channel(2) != ip.channel(2));
    QCOMPARE(ip2.channel(2)->name(), QString("Channel 3"));

    QVERIFY(ip2.channel(9000) != NULL);
    QVERIFY(ip2.channel(9000) != ich4);
    QVERIFY(ip2.channel(9000) != ip.channel(9000));
    QCOMPARE(ip2.channel(9000)->name(), QString("Channel 4"));
}

void QLCInputProfile_Test::load()
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartDocument();
    xmlWriter.writeDTD("<!DOCTYPE InputProfile>");
    xmlWriter.writeStartElement("InputProfile");
    xmlWriter.writeTextElement("Manufacturer", "Behringer");
    xmlWriter.writeTextElement("Model", "BCF2000");
    xmlWriter.writeStartElement("Channel");
    xmlWriter.writeAttribute("Number", "492");
    xmlWriter.writeTextElement("Name", "Foobar");
    xmlWriter.writeTextElement("Type", "Slider");
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);

    QLCInputProfile ip;
    QVERIFY(ip.loadXML(xmlReader) == true);
    QVERIFY(ip.manufacturer() == "Behringer");
    QVERIFY(ip.model() == "BCF2000");
    QVERIFY(ip.channels().size() == 1);
    QVERIFY(ip.channel(492) != NULL);
    QVERIFY(ip.channel(492)->name() == "Foobar");
    QVERIFY(ip.channel(492)->type() == QLCInputChannel::Slider);
}

void QLCInputProfile_Test::loadNoProfile()
{
    QXmlStreamReader doc;
    QLCInputProfile ip;
    QVERIFY(ip.loadXML(doc) == false);

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartElement("Whatever");

    doc.setDevice(&buffer);
    QVERIFY(ip.loadXML(doc) == false);
}

/* S2, surface types (CONTROL_SCOPES.md section 2): <Surface>, <Role> and
   <Facts> load, save and copy. */
void QLCInputProfile_Test::surfaceTypeLoadsAndSaves()
{
    QByteArray xml =
        "<?xml version='1.0' encoding='UTF-8'?>\n<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Manufacturer>Test</Manufacturer><Model>Box</Model><Type>MIDI</Type>\n"
        " <Surface Version=\"1\" Layout=\"grid\" Columns=\"9\"/>\n"
        " <Channel Number=\"7\"><Name>Fader 9</Name><Type>Slider</Type>"
        "<Role>GrandMaster</Role><Facts Fader=\"Motorised\"/></Channel>\n"
        " <Channel Number=\"8\"><Name>Pad</Name><Type>Button</Type><Role>Select 3</Role></Channel>\n"
        " <Channel Number=\"9\"><Name>Knob</Name><Type>Knob</Type><Facts Fader=\"Relative\"/></Channel>\n"
        " <Channel Number=\"10\"><Name>Odd</Name><Type>Button</Type><Role>Sing 3</Role></Channel>\n"
        "</InputProfile>\n";
    QBuffer buffer(&xml);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    QLCInputProfile ip;
    QVERIFY(ip.loadXML(reader));
    QVERIFY(ip.isSurfaceType());
    QCOMPARE(ip.surfaceVersion(), 1);
    QCOMPARE(ip.surfaceLayout(), QString("grid"));
    QCOMPARE(ip.surfaceColumns(), 9);
    QCOMPARE(ip.channel(7)->role(), QString("GrandMaster"));
    QCOMPARE(ip.channel(7)->faderFact(), FaderFact::Motorised);
    QCOMPARE(ip.channel(8)->role(), QString("Select 3"));
    QCOMPARE(ip.channel(8)->faderFact(), FaderFact::Unknown);
    QCOMPARE(ip.channel(9)->faderFact(), FaderFact::Relative);
    QVERIFY(ip.channel(10)->role().isEmpty());            // not a role: dropped
    QCOMPARE(ip.roleChannel("GrandMaster"), quint32(7));
    QCOMPARE(ip.roleChannel("Go"), QLCChannel::invalid());
    QCOMPARE(ControlSurface::roleFromText(ip.channel(8)->role()),
             ControlSurface::Role(ControlSurface::RoleType::Select, 3));

    // A relative movement is relative, whatever was declared
    ip.channel(7)->setMovementType(QLCInputChannel::Relative);
    QCOMPARE(ip.channel(7)->faderFact(), FaderFact::Relative);
    QCOMPARE(ip.channel(7)->declaredFaderFact(), FaderFact::Motorised);
    ip.channel(7)->setMovementType(QLCInputChannel::Absolute);

    // Saved and read back; a copy and an assignment keep it
    QTemporaryDir dir;
    const QString path = dir.filePath("box.qxi");
    QVERIFY(ip.saveXML(path));
    QScopedPointer<QLCInputProfile> back(QLCInputProfile::loader(path));
    QVERIFY(!back.isNull());
    QVERIFY(back->isSurfaceType());
    QCOMPARE(back->surfaceColumns(), 9);
    QCOMPARE(back->channel(7)->role(), QString("GrandMaster"));
    QCOMPARE(back->channel(7)->declaredFaderFact(), FaderFact::Motorised);
    QCOMPARE(back->channel(8)->role(), QString("Select 3"));
    QScopedPointer<QLCInputProfile> copy(ip.createCopy());
    QCOMPARE(copy->surfaceLayout(), QString("grid"));
    QCOMPARE(copy->channel(7)->role(), QString("GrandMaster"));
    QCOMPARE(copy->channel(7)->declaredFaderFact(), FaderFact::Motorised);
    QLCInputProfile assigned;
    assigned = ip;
    QCOMPARE(assigned.surfaceVersion(), 1);
    QCOMPARE(assigned.channel(8)->role(), QString("Select 3"));
}

/* A profile written before surface types (every QLC+ one) loads with no
   roles and no declared facts, and saves without a tag of them. */
void QLCInputProfile_Test::aProfileWithoutATypeSavesAsBefore()
{
    QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(INTERNAL_PROFILEDIR "Generic-MIDI.qxi"));
    QVERIFY(!prof.isNull());
    QVERIFY(!prof->isSurfaceType());
    for (QLCInputChannel *ich : prof->channels())
    {
        QVERIFY(ich->role().isEmpty());
        QCOMPARE(ich->declaredFaderFact(), FaderFact::Unknown);
    }
    QTemporaryDir dir;
    const QString path = dir.filePath("generic.qxi");
    QVERIFY(prof->saveXML(path));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray text = f.readAll();
    QVERIFY(!text.contains("<Surface"));
    QVERIFY(!text.contains("<Role"));
    QVERIFY(!text.contains("<Facts"));
}

/* A type's rules: one stop, on a button; one Grand Master, on a fader. */
void QLCInputProfile_Test::roleProblemsAreSaid()
{
    QLCInputProfile ip;
    QLCInputChannel *f1 = new QLCInputChannel();
    f1->setType(QLCInputChannel::Slider);
    f1->setRole("GrandMaster");
    ip.insertChannel(0, f1);
    QVERIFY(ip.roleProblems().isEmpty());
    QLCInputChannel *f2 = new QLCInputChannel();
    f2->setType(QLCInputChannel::Slider);
    f2->setRole("EmergencyStop");
    ip.insertChannel(1, f2);
    QLCInputChannel *b = new QLCInputChannel();
    b->setType(QLCInputChannel::Button);
    b->setRole("GrandMaster");
    ip.insertChannel(2, b);
    const QString all = ip.roleProblems().join('\n');
    QVERIFY2(all.contains("the stop must be a button"), qPrintable(all));
    QVERIFY2(all.contains("the Grand Master must be a fader"), qPrintable(all));
    QVERIFY2(all.contains("GrandMaster is on channel 1 already"), qPrintable(all));
}

/* The PMJ Black 1's roles moved out of pmjoverlay.cpp into its profile:
   the table that file held, control for control (golden test). */
void QLCInputProfile_Test::pmjBlack1RolesAreItsOldTable()
{
    struct Row { quint32 ch; const char *name; const char *role; };
    const Row table[] = {
        { 32768, "Master", "GrandMaster" },
        { 32769, "Ch 1", "Level 1" }, { 32770, "Ch 2", "Level 2" }, { 32771, "Ch 3", "Level 3" },
        { 32772, "Ch 4", "Level 4" }, { 32773, "Ch 5", "Level 5" }, { 32774, "Ch 6", "Level 6" },
        { 32775, "Ch 7", "Level 7" }, { 32776, "Ch 8", "Level 8" }, { 32777, "Ch 9", "Level 9" },
        { 32778, "Ch 10", "Level 10" },
        { 32779, "Enc 1", "Param 1" }, { 32780, "Enc 2", "Param 2" },
        { 32781, "Enc 3", "Param 3" }, { 32782, "Enc 4", "Param 4" },
        { 32896, "Go", "Go" }, { 32907, "Back", "Back" }, { 32932, "Left", "Left" },
        { 32933, "Right", "Right" }, { 32919, "Pre Page", "Prev" }, { 32920, "Next Page", "Next" },
        { 32918, "O", "Blackout" }, { 32945, "Set", "Blind" }, { 32944, "Favorites", "" },
        { 32946, "Effects", "Page 0" }, { 32947, "Groups", "Page 1" }, { 32948, "Looks", "Page 2" },
        { 32949, "Macros", "Page 3" }, { 32931, "Fix Cont", "Page 4" },
    };
    // Both copies: the installed profile and the one built into the engine
    const QStringList paths = { QString(INTERNAL_PROFILEDIR "PMJ-Black-1.qxi"),
                                SurfaceLayout::builtInTypePath("PMJ-Black-1.qxi") };
    for (const QString &path : paths)
    {
        QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(path));
        QVERIFY2(!prof.isNull(), qPrintable(path));
        QVERIFY(prof->isSurfaceType());
        QCOMPARE(prof->channels().size(), 69);
        for (const Row &r : table)
        {
            const QLCInputChannel *ich = prof->channel(r.ch);
            QVERIFY2(ich != nullptr, qPrintable(QString::number(r.ch)));
            QCOMPARE(ich->name(), QString(r.name));
            QCOMPARE(ich->role(), QString(r.role));
        }
        // The strips: Select / Load N, Up = Reset N; the Down row is free
        for (int n = 1; n <= 10; ++n)
        {
            QCOMPARE(prof->channel(32933 + n)->role(), QString("Select %1").arg(n));
            QCOMPARE(prof->channel(32920 + n)->role(), QString("Load %1").arg(n));
            QCOMPARE(prof->channel(32907 + n)->role(), QString("Reset %1").arg(n));
            QVERIFY(prof->channel(32896 + n)->role().isEmpty());
            QCOMPARE(prof->channel(32768 + n)->faderFact(), FaderFact::Absolute);
        }
        QVERIFY(prof->roleProblems().isEmpty());
    }
}

/* The first shipped types (CONTROL_SCOPES.md section 2): the Grand Master
   on the last fader, the APC minis' GO, Back and stop on their scene keys,
   every fader said, and each type's rules hold. */
void QLCInputProfile_Test::shippedSurfaceTypesHoldTheirRules()
{
    struct Type { const char *file; quint32 gm; FaderFact fader; quint32 go, back, stop; };
    const quint32 none = QLCChannel::invalid();
    const Type types[] = {
        { "Akai-APCMini.qxi", 56, FaderFact::Absolute, 210, 211, 217 },
        { "Akai-APCMini-mk2.qxi", 56, FaderFact::Absolute, 240, 241, 247 },
        { "PMJ-9-Faders-Controller.qxi", 5, FaderFact::Absolute, none, none, none },
        { "KORG-nanoKONTROL2.qxi", 7, FaderFact::Absolute, none, none, none },
        { "Behringer-BCF2000.qxi", 88, FaderFact::Motorised, none, none, none },
    };
    for (const Type &t : types)
    {
        QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(INTERNAL_PROFILEDIR + QString(t.file)));
        QVERIFY2(!prof.isNull(), t.file);
        QVERIFY2(prof->isSurfaceType(), t.file);
        QVERIFY2(prof->roleProblems().isEmpty(), qPrintable(prof->roleProblems().join('\n')));
        QCOMPARE(prof->roleChannel("GrandMaster"), t.gm);
        QCOMPARE(prof->roleChannel("Go"), t.go);
        QCOMPARE(prof->roleChannel("Back"), t.back);
        QCOMPARE(prof->roleChannel("EmergencyStop"), t.stop);
        for (QLCInputChannel *ich : prof->channels())
            if (ich->type() == QLCInputChannel::Slider && ich->name().startsWith("Slider"))
                QCOMPARE(ich->faderFact(), t.fader);
            else if (ich->type() == QLCInputChannel::Slider)
                QCOMPARE(ich->faderFact(), FaderFact::Absolute);   // the BCF2000's pedal
    }
}

/* The PreSonus FaderPort 2 (Mackie mode, MIDI channel 1): one motor fader
   (pitch bend) that is the Grand Master, Play = GO, Rewind = Back, Stop =
   the stop; the knob is a relative one (1..63 up, 65..127 down); the Play,
   Stop, Record and Loop keys have lamps. Its fader is Motorised, so its
   pick-up sends the value back once and waits for the motor. The fader's
   touch (note 104) has no place in a type (no role or fact for it): it is
   a plain button. */
void QLCInputProfile_Test::faderPort2IsATypeWithAMotorFader()
{
    QScopedPointer<QLCInputProfile> prof(QLCInputProfile::loader(INTERNAL_PROFILEDIR "PreSonus-FaderPort-2.qxi"));
    QVERIFY(!prof.isNull());
    QCOMPARE(prof->name(), QString("PreSonus FaderPort 2"));
    QVERIFY(prof->isSurfaceType());
    QVERIFY2(prof->roleProblems().isEmpty(), qPrintable(prof->roleProblems().join('\n')));
    QCOMPARE(prof->roleChannel("GrandMaster"), quint32(513));
    QCOMPARE(prof->roleChannel("Go"), quint32(128 + 94));
    QCOMPARE(prof->roleChannel("Back"), quint32(128 + 91));
    QCOMPARE(prof->roleChannel("EmergencyStop"), quint32(128 + 93));
    const QLCInputChannel *fader = prof->channel(513);
    QVERIFY(fader != nullptr);
    QCOMPARE(fader->type(), QLCInputChannel::Slider);
    QCOMPARE(fader->faderFact(), FaderFact::Motorised);
    const QLCInputChannel *knob = prof->channel(16);
    QVERIFY(knob != nullptr);
    QCOMPARE(knob->faderFact(), FaderFact::Relative);
    QCOMPARE(knob->relativeEncoding(), QLCInputChannel::SignedBit);
    QCOMPARE(prof->channel(128 + 104)->name(), QString("Fader touch"));
    QVERIFY(prof->channel(128 + 104)->role().isEmpty());
    for (int note : { 93, 94, 95, 86 })
        QCOMPARE(prof->channel(128 + note)->upperValue(), uchar(127));

    // The pick-up follows the declared fact: it asks for the value to be
    // sent to the motor once, then takes the fader when it arrives.
    FaderPickup pick;
    pick.setFact(fader->faderFact());
    bool sendBack = false;
    QVERIFY(!pick.accept(10, 200, &sendBack));
    QVERIFY(sendBack);
    QVERIFY(pick.accept(199, 200, &sendBack));
}

void QLCInputProfile_Test::loader()
{
    QLCInputProfile* prof = QLCInputProfile::loader("foobar");
    QVERIFY(prof == NULL);

    prof = QLCInputProfile::loader("broken.xml");
    QVERIFY(prof == NULL);

    QString path(INTERNAL_PROFILEDIR "Generic-MIDI.qxi");
    prof = QLCInputProfile::loader(path);
    QVERIFY(prof != NULL);
    QCOMPARE(prof->path(), path);
    QCOMPARE(prof->name(), QString("Generic MIDI"));
    QCOMPARE(prof->channels().size(), 256);
}

/* saveXML() used to end in a global sync(), which flushes every mounted
   volume's dirty buffers, and on a busy machine (a build next door, an
   external disk) one save blocked for 30 s and more. Twenty saves of a
   full 512-channel profile, each flushing only its own file, take well under
   a second; the budget is generous so that a loaded machine can't fail it,
   while a global flush per save can. */
void QLCInputProfile_Test::saveFlushesOnlyItsFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("big.qxi");

    QLCInputProfile ip;
    ip.setManufacturer("TestManufacturer");
    ip.setModel("Big");
    for (quint32 i = 0; i < 512; i++)
    {
        QLCInputChannel *ich = new QLCInputChannel();
        ich->setName(QString("Channel %1").arg(i + 1));
        ip.insertChannel(i, ich);
    }

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < 20; i++)
        QVERIFY(ip.saveXML(path));
    const qint64 ms = timer.elapsed();
    QVERIFY2(ms < 10000, qPrintable(QString("20 saves took %1 ms").arg(ms)));

    /* Written whole, under its own name, nothing left beside it */
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList() << "big.qxi");
    QLCInputProfile *prof = QLCInputProfile::loader(path);
    QVERIFY(prof != NULL);
    QCOMPARE(prof->channels().size(), 512);
    delete prof;
}

void QLCInputProfile_Test::save()
{
    QLCInputProfile ip;
    ip.setManufacturer("TestManufacturer");
    ip.setModel("TestModel");

    QLCInputChannel* ich1 = new QLCInputChannel();
    ich1->setName("Channel 1");
    ip.insertChannel(0, ich1);

    QLCInputChannel* ich2 = new QLCInputChannel();
    ich2->setName("Channel 2");
    ip.insertChannel(5, ich2);

    QLCInputChannel* ich3 = new QLCInputChannel();
    ich3->setName("Channel 3");
    ip.insertChannel(2, ich3);

    QString path("test.qxi");
    QVERIFY(ip.saveXML(path) == true);

#if !defined(WIN32) && !defined(Q_OS_WIN)
    QFile::Permissions perm = QFile::permissions(path);
    QFile::setPermissions(path, QFileDevice::WriteOther);
    QVERIFY(ip.saveXML(path) == false);
    QFile::setPermissions(path, perm);
#endif

    QLCInputProfile* prof = QLCInputProfile::loader(path);
    QVERIFY(prof != NULL);
    QCOMPARE(prof->manufacturer(), ip.manufacturer());
    QCOMPARE(prof->model(), ip.model());
    QCOMPARE(prof->name(), ip.name());
    QCOMPARE(prof->channels().size(), ip.channels().size());
    QCOMPARE(prof->channels()[0]->name(), ich1->name());
    QCOMPARE(prof->channels()[2]->name(), ich3->name());
    QCOMPARE(prof->channels()[5]->name(), ich2->name());

    QVERIFY(QFile::remove(path) == true);
    delete prof;
}

void QLCInputProfile_Test::deviceMatch()
{
    QLCInputProfile ip;
    ip.setManufacturer("Akai");
    ip.setModel("APC40 mkII");
    QVERIFY(!ip.matchesDevice("APC40 mkII"));           // no Match: never chosen by name
    ip.setDeviceMatches(QStringList() << "APC40 mkII" << "APC64 DAW*");
    QVERIFY(ip.matchesDevice("APC40 mkII"));
    QVERIFY(ip.matchesDevice("apc40 MKII"));            // case does not matter
    QVERIFY(ip.matchesDevice("2- APC40 mkII"));         // Windows' unit prefix
    QVERIFY(ip.matchesDevice("APC64 DAW (APC64)"));
    QVERIFY(!ip.matchesDevice("APC40"));                // a glob is the whole name
    QVERIFY(!ip.matchesDevice("APC40 mkII Notes"));
    QVERIFY(!ip.matchesDevice("APC64 Notes (APC64)"));

    // Kept by a copy and by the file.
    QScopedPointer<QLCInputProfile> copy(ip.createCopy());
    QCOMPARE(copy->deviceMatches(), ip.deviceMatches());
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("match.qxi");
    QLCInputChannel *ich = new QLCInputChannel();
    ich->setName("Button");
    ich->setType(QLCInputChannel::Button);
    ip.insertChannel(0, ich);
    QVERIFY(ip.saveXML(path));
    QScopedPointer<QLCInputProfile> back(QLCInputProfile::loader(path));
    QVERIFY(!back.isNull());
    QCOMPARE(back->deviceMatches(), QStringList() << "APC40 mkII" << "APC64 DAW*");
    QVERIFY(back->matchesDevice("APC64 DAW (APC64)"));
}

QTEST_APPLESS_MAIN(QLCInputProfile_Test)
