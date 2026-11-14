/*
  Q Light Controller Plus - Unit test
  preshowcheck_test.cpp

  Before the house opens (docs/features/dress-rehearsal.md), offscreen.

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
#include <QDir>

#include "preshowcheck_test.h"
#include "../common/shotdark.h"
#include "showstatusdialog.h"
#include "showreviewpanel.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "showstatus.h"
#include "showreview.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
Fixture *dimmer(Doc *doc, const QString &name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Lamp");
    def->setType(QLCFixtureDef::Dimmer);
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setPreset(QLCChannel::IntensityDimmer);
    def->addChannel(ch);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Mode");
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *fx = new Fixture(doc);
    fx->setName(name);
    fx->setFixtureDefinition(def, mode);
    fx->setAddress(address);
    doc->addFixture(fx);
    return fx;
}

const QList<QPair<QString, QSize>> kSizes {
    { "1024x768", QSize(1024, 768) }, { "1280x800", QSize(1280, 800) }, { "1920x1080", QSize(1920, 1080) } };
}

void PreshowCheck_Test::init()
{
    m_doc = new Doc(this);
    // A blocker: two lamps on the same channels.
    dimmer(m_doc, "Lamp 1", 0);
    dimmer(m_doc, "Lamp 2", 0);
}

void PreshowCheck_Test::cleanup()
{
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
        ShowStatus::instance()->clearStatus(e.key);
    delete m_doc;
    m_doc = nullptr;
}

/* QLC_SHOT_DIR (the gate's --shots): the Review panel and the readiness
   dialog, where a crew looks before a show, at three sizes. */
void PreshowCheck_Test::screenshots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString tag = qEnvironmentVariable("QLC_SHOT_TAG", "before");
    shotUseDarkTheme();
    ShowStatus::instance()->setStatus("outputs.problems", ShowStatus::Warning, "1 network output problem",
                                      "Art-Net nodes that stopped answering.",
                                      QStringList() << "Art-Net to 172.18.2.50: not answering (universe 1)",
                                      QVariantList() << 0u);
    m_doc->showReview()->run();
    ShowReviewPanel panel(m_doc);
    panel.setFilter(ShowReviewPanel::All);
    ShowStatusDialog status;
    for (const auto &sz : kSizes)
    {
        for (QWidget *w : { static_cast<QWidget *>(&panel), static_cast<QWidget *>(&status) })
        {
            w->resize(sz.second);
            w->show();
        }
        QTest::qWait(40);
        QVERIFY(panel.grab().save(QStringLiteral("%1/%2-review-%3.png").arg(dir, tag, sz.first)));
        QVERIFY(status.grab().save(QStringLiteral("%1/%2-readiness-%3.png").arg(dir, tag, sz.first)));
    }
}

QTEST_MAIN(PreshowCheck_Test)
