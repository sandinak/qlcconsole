/*
  Q Light Controller Plus
  teaching_test.cpp

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
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "teaching_test.h"
#include "teachingsettings.h"
#include "showoperate.h"
#include "doc.h"

void Teaching_Test::init()
{
    m_doc = new Doc(this);
}

void Teaching_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

static QString saved(Doc *doc)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    w.writeStartElement("Root");
    doc->teachingSettings()->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return QString::fromUtf8(buf.data());
}

static void loadTeaching(Doc *doc, const QString &xml)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Teaching")))
        r.readNext();
    QVERIFY(r.isStartElement());
    doc->teachingSettings()->loadXML(r);
}

void Teaching_Test::offWritesNothing()
{
    TeachingSettings *t = m_doc->teachingSettings();
    QVERIFY(t != nullptr);
    QVERIFY(!t->enabled());
    QVERIFY(!t->holdsOutput());
    QVERIFY(!saved(m_doc).contains("Teaching"));

    QSignalSpy spy(t, SIGNAL(teachingChanged()));
    t->setEnabled(true);
    QVERIFY(t->enabled());
    QVERIFY(t->holdsOutput());                  // real output is held until allowed
    QVERIFY(m_doc->isModified());
    QCOMPARE(spy.count(), 1);
    QVERIFY(m_doc->inputOutputMap()->teachingHold());

    m_doc->resetModified();
    t->setEnabled(true);                        // no change, no signal, not modified
    QCOMPARE(spy.count(), 1);
    QVERIFY(!m_doc->isModified());

    t->setEnabled(false);
    QVERIFY(!m_doc->inputOutputMap()->teachingHold());
    QVERIFY(!saved(m_doc).contains("Teaching"));
}

void Teaching_Test::roundTrip()
{
    TeachingSettings *t = m_doc->teachingSettings();
    t->setEnabled(true);
    t->setAllowOutput(true);
    QVERIFY(!t->holdsOutput());
    t->setLesson("3.6", "https://example.org/level-3/lesson-06/");
    t->setHandedOut("2026-11-03T09:00:00");
    const QString xml = saved(m_doc);
    QVERIFY2(xml.contains("<Teaching Enabled=\"1\" AllowOutput=\"1\" Lesson=\"3.6\""), qPrintable(xml));

    Doc other(this);
    loadTeaching(&other, xml);
    TeachingSettings *o = other.teachingSettings();
    QVERIFY(o->enabled());
    QVERIFY(o->allowOutput());
    QCOMPARE(o->lesson(), QString("3.6"));
    QCOMPARE(o->link(), QString("https://example.org/level-3/lesson-06/"));
    QCOMPARE(o->handedOut(), QString("2026-11-03T09:00:00"));
    QVERIFY(!other.isModified());
    QVERIFY(!other.inputOutputMap()->teachingHold());       // allowed

    // Disabled in the file: off, whatever else it says
    Doc off(this);
    loadTeaching(&off, "<Teaching Enabled=\"0\" Lesson=\"1.1\"/>");
    QVERIFY(!off.teachingSettings()->enabled());
}

void Teaching_Test::unknownAttributesAreKept()
{
    Doc other(this);
    loadTeaching(&other, "<Teaching Enabled=\"1\" FutureThing=\"yes\"/>");
    QVERIFY(other.teachingSettings()->enabled());
    const QString xml = saved(&other);
    QVERIFY2(xml.contains("FutureThing=\"yes\""), qPrintable(xml));
}

void Teaching_Test::clearContentsResets()
{
    TeachingSettings *t = m_doc->teachingSettings();
    t->setEnabled(true);
    t->setLesson("2.1", QString());
    m_doc->clearContents();
    QVERIFY(!t->enabled());
    QVERIFY(t->lesson().isEmpty());
    QVERIFY(!m_doc->inputOutputMap()->teachingHold());
    QVERIFY(!saved(m_doc).contains("Teaching"));
}

static QByteArray engineXml(Doc *doc)
{
    QByteArray xml;
    QBuffer buffer(&xml);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buffer);
    w.writeStartDocument();
    doc->saveXML(&w);
    w.writeEndDocument();
    return xml;
}

void Teaching_Test::aShowFileCarriesIt()
{
    // Not a teaching show: the file is what it always was.
    QVERIFY(!engineXml(m_doc).contains("Teaching"));

    m_doc->teachingSettings()->setEnabled(true);
    m_doc->operateSettings()->setPin("2468");
    const QByteArray xml = engineXml(m_doc);
    QVERIFY(xml.contains("<Teaching Enabled=\"1\"/>"));
    // First in the Engine, so a loading show holds output before its patch opens
    QVERIFY(xml.indexOf("<Teaching") < xml.indexOf("<InputOutputMap"));
    QVERIFY(xml.contains("<Operate PinHash="));

    Doc other(this);
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Engine")))
        r.readNext();
    QVERIFY(other.loadXML(r));
    QVERIFY(other.teachingSettings()->enabled());
    QVERIFY(other.operateSettings()->checkPin("2468"));
    QVERIFY(other.inputOutputMap()->teachingHold());

    // Teaching and PIN are one: turning it off is the same PIN's job (UI), the
    // engine only keeps them side by side.
    Doc plain(this);
    const QByteArray plainXml = engineXml(&plain);
    QXmlStreamReader pr(plainXml);
    while (!pr.atEnd() && !(pr.isStartElement() && pr.name() == QLatin1String("Engine")))
        pr.readNext();
    QVERIFY(plain.loadXML(pr));
    QVERIFY(!plain.teachingSettings()->enabled());
}

void Teaching_Test::saveOverrideLeavesTheShowAlone()
{
    ShowOperate *op = m_doc->operateSettings();
    QVERIFY(op->setPin("1111"));
    op->setOpenInOperate(true);
    m_doc->resetModified();

    TeachingSettings::SaveOverride teach;
    teach.allowOutput = false;
    teach.lesson = "3.6";
    teach.handedOut = "2026-11-03T09:00:00";
    ShowOperate::SaveOverride lock;
    lock.pinHash = ShowPin::hash("9753");
    lock.lockDesign = false;
    m_doc->teachingSettings()->setSaveOverride(&teach);
    op->setSaveOverride(&lock);
    const QByteArray copy = engineXml(m_doc);
    m_doc->teachingSettings()->setSaveOverride(nullptr);
    op->setSaveOverride(nullptr);

    QVERIFY(copy.contains("<Teaching Enabled=\"1\" Lesson=\"3.6\" HandedOut="));
    QVERIFY(copy.contains("LockDesign=\"0\""));
    QVERIFY(!copy.contains("OpenIn"));

    Doc student(this);
    QXmlStreamReader r(copy);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Engine")))
        r.readNext();
    QVERIFY(student.loadXML(r));
    QVERIFY(student.teachingSettings()->enabled());
    QVERIFY(student.operateSettings()->checkPin("9753"));
    QVERIFY(!student.operateSettings()->checkPin("1111"));
    QVERIFY(!student.operateSettings()->locksDesign());
    QVERIFY(!student.operateSettings()->openInOperate());

    // The open show: as it was, nothing marked, nothing held
    QVERIFY(!m_doc->teachingSettings()->enabled());
    QVERIFY(!m_doc->inputOutputMap()->teachingHold());
    QVERIFY(op->checkPin("1111"));
    QVERIFY(op->openInOperate());
    QVERIFY(!m_doc->isModified());
}

void Teaching_Test::thePinCanSkipTheDesignLock()
{
    ShowOperate *op = m_doc->operateSettings();
    QVERIFY(!op->locksDesign());
    QVERIFY(op->setPin("2468"));
    QVERIFY(op->locksDesign());                 // as before: a PIN locks Design
    op->setLockDesign(false);
    QVERIFY(op->hasPin());
    QVERIFY(!op->locksDesign());

    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    w.writeStartElement("Root");
    op->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    const QString xml = QString::fromUtf8(buf.data());
    QVERIFY(xml.contains("LockDesign=\"0\""));

    // A show from before the setting: a PIN locks Design
    Doc old(this);
    QXmlStreamReader r("<Operate PinHash=\"" + ShowPin::hash("1357") + "\"/>");
    r.readNextStartElement();
    old.operateSettings()->loadXML(r);
    QVERIFY(old.operateSettings()->locksDesign());

    Doc nolock(this);
    QXmlStreamReader r2(xml);
    while (!r2.atEnd() && !(r2.isStartElement() && r2.name() == QLatin1String("Operate")))
        r2.readNext();
    nolock.operateSettings()->loadXML(r2);
    QVERIFY(nolock.operateSettings()->hasPin());
    QVERIFY(!nolock.operateSettings()->locksDesign());
}

void Teaching_Test::theHandoutStateIsKept()
{
    TeachingSettings *t = m_doc->teachingSettings();
    t->setEnabled(true);
    QVERIFY(!t->hasHandoutSnapshot());

    const QByteArray file = "<?xml version='1.0'?>\n<Workspace>" + QByteArray(5000, 'x') + "</Workspace>\n";
    const QByteArray packed = TeachingSettings::packSnapshot(file);
    QVERIFY(packed.size() < file.size() / 4);
    QCOMPARE(TeachingSettings::unpackSnapshot(packed), file);
    QVERIFY(TeachingSettings::unpackSnapshot("not deflated").isEmpty());

    m_doc->resetModified();
    t->setHandoutSnapshot(packed);
    QVERIFY(t->hasHandoutSnapshot());
    QVERIFY(!m_doc->isModified());          // it travels with the file, it is not an edit
    const QString xml = saved(m_doc);
    QVERIFY2(xml.contains("<Teaching Enabled=\"1\">"), qPrintable(xml.left(200)));

    Doc other(this);
    loadTeaching(&other, xml);
    QVERIFY(other.teachingSettings()->enabled());
    QCOMPARE(TeachingSettings::unpackSnapshot(other.teachingSettings()->handoutSnapshot()), file);
    QVERIFY(saved(&other) == xml);          // and it is written back as it was

    // The copy's own write (the override) names its state, or none
    TeachingSettings::SaveOverride ov;
    QVERIFY(!saved(m_doc).isEmpty());
    t->setSaveOverride(&ov);
    QVERIFY(!saved(m_doc).contains("eJ"));  // none: the first pass of a handout
    ov.snapshot = packed;
    QVERIFY(saved(m_doc).contains(QString::fromLatin1(packed.toBase64())));
    t->setSaveOverride(nullptr);

    // A show with attributes after it, and text of another build, still loads
    Doc plain(this);
    loadTeaching(&plain, "<Teaching Enabled=\"1\" Lesson=\"2.1\"/>");
    QVERIFY(!plain.teachingSettings()->hasHandoutSnapshot());

    m_doc->clearContents();
    QVERIFY(!t->hasHandoutSnapshot());
}

QTEST_MAIN(Teaching_Test)
