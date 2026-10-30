/*
  Q Light Controller Plus
  showoperate_test.cpp

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

#include "showoperate_test.h"
#include "showoperate.h"
#include "doc.h"

void ShowOperate_Test::init()
{
    m_doc = new Doc(this);
}

void ShowOperate_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void ShowOperate_Test::aPinIsHashedAndChecked()
{
    QVERIFY(ShowPin::isValid("1234"));
    QVERIFY(ShowPin::isValid("12345678"));
    QVERIFY(!ShowPin::isValid("123"));
    QVERIFY(!ShowPin::isValid("123456789"));
    QVERIFY(!ShowPin::isValid("12a4"));
    QVERIFY(!ShowPin::isValid(" 1234"));

    const QString h = ShowPin::hash("4321");
    QVERIFY(h.startsWith("pbkdf2-sha256$"));
    QVERIFY(!h.contains("4321"));
    QVERIFY(ShowPin::verify("4321", h));
    QVERIFY(!ShowPin::verify("1234", h));
    QVERIFY(!ShowPin::verify("", h));
    // A fresh salt every time: the same PIN never hashes the same twice
    QVERIFY(ShowPin::hash("4321") != h);
}

void ShowOperate_Test::aBadHashOpensNothing()
{
    QVERIFY(!ShowPin::verify("1234", QString()));
    QVERIFY(!ShowPin::verify("1234", "1234"));
    QVERIFY(!ShowPin::verify("1234", "md5$1$abc$def"));
    QVERIFY(!ShowPin::verify("1234", "pbkdf2-sha256$0$c2FsdA==$AAAA"));
    QVERIFY(!ShowPin::verify("1234", "pbkdf2-sha256$x$c2FsdA==$AAAA"));
    QVERIFY(!ShowPin::verify("1234", "pbkdf2-sha256$1000$$AAAA"));
    // A hash cut short
    const QString h = ShowPin::hash("1234");
    QVERIFY(!ShowPin::verify("1234", h.left(h.size() - 4)));
}

void ShowOperate_Test::setAndRemoveThePin()
{
    ShowOperate *op = m_doc->operateSettings();
    QVERIFY(op != nullptr);
    QVERIFY(!op->hasPin());
    QVERIFY(op->checkPin(QString()));       // no PIN: nothing to check
    QVERIFY(!m_doc->isModified());

    QSignalSpy spy(op, SIGNAL(settingsChanged()));
    QVERIFY(op->setPin("2468"));
    QVERIFY(op->hasPin());
    QVERIFY(op->checkPin("2468"));
    QVERIFY(!op->checkPin("1357"));
    QVERIFY(m_doc->isModified());
    QCOMPARE(spy.count(), 1);

    // Not a PIN: nothing changes
    m_doc->resetModified();
    const QString before = op->pinHash();
    QVERIFY(!op->setPin("12"));
    QCOMPARE(op->pinHash(), before);
    QVERIFY(!m_doc->isModified());
    QCOMPARE(spy.count(), 1);

    QVERIFY(op->setPin(QString()));
    QVERIFY(!op->hasPin());
    QVERIFY(m_doc->isModified());

    op->setOpenInOperate(true);
    QVERIFY(op->openInOperate());
    QCOMPARE(spy.count(), 3);
}

static QString saved(Doc *doc)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    QXmlStreamWriter w(&buf);
    w.writeStartDocument();
    w.writeStartElement("Root");
    doc->operateSettings()->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    return QString::fromUtf8(buf.data());
}

void ShowOperate_Test::savedWithTheShowOnlyWhenSet()
{
    ShowOperate *op = m_doc->operateSettings();
    QVERIFY(!saved(m_doc).contains("<Operate"));

    op->setOpenInOperate(true);
    QVERIFY(op->setPin("9753"));
    const QString xml = saved(m_doc);
    QVERIFY2(xml.contains("<Operate OpenIn=\"1\" PinHash=\"pbkdf2-sha256$"), qPrintable(xml));
    QVERIFY(!xml.contains("9753"));

    // Read back into another show
    Doc other(this);
    QXmlStreamReader r(xml);
    while (!r.atEnd() && !(r.isStartElement() && r.name() == QLatin1String("Operate")))
        r.readNext();
    QVERIFY(r.isStartElement());
    other.operateSettings()->loadXML(r);
    QVERIFY(other.operateSettings()->openInOperate());
    QVERIFY(other.operateSettings()->checkPin("9753"));
    QVERIFY(!other.operateSettings()->checkPin("0000"));
    QVERIFY(!other.isModified());

    // A new show forgets it
    m_doc->clearContents();
    QVERIFY(!op->openInOperate());
    QVERIFY(!op->hasPin());
    QVERIFY(!saved(m_doc).contains("<Operate"));
}

QTEST_MAIN(ShowOperate_Test)
