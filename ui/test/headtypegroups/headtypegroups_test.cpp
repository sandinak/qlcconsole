/*
  Q Light Controller Plus - Unit test
  headtypegroups_test.cpp

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
#include <QTreeWidget>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTimer>

#define protected public
#define private public
#include "creategroupsdialog.h"
#include "stagegroupdialog.h"
#include "fixturemanager.h"
#undef private
#undef protected

#include "createfixturegroup.h"
#include "monitorproperties.h"
#include "headtypegroups.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "headlayout.h"
#include "fixture.h"
#include "truss.h"
#include "doc.h"

#include "headtypegroups_test.h"

/* A model of @p rgb RGB heads then @p white white ones, in one row. */
static QLCFixtureDef *makeDef(const QString &model, int rgb, int white, int widthMm)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Testco");
    def->setModel(model);
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(QString("%1ch").arg(rgb * 3 + white));
    auto add = [&](const QString &name, QLCChannel::Preset preset) {
        QLCChannel *ch = new QLCChannel();
        ch->setName(name);
        ch->setPreset(preset);
        def->addChannel(ch);
        mode->insertChannel(ch, mode->channels().size());
        return quint32(mode->channels().size() - 1);
    };
    for (int i = 0; i < rgb; ++i)
    {
        QLCFixtureHead h;
        h.addChannel(add(QString("Red %1").arg(i + 1), QLCChannel::IntensityRed));
        h.addChannel(add(QString("Green %1").arg(i + 1), QLCChannel::IntensityGreen));
        h.addChannel(add(QString("Blue %1").arg(i + 1), QLCChannel::IntensityBlue));
        mode->insertHead(-1, h);
    }
    for (int i = 0; i < white; ++i)
    {
        QLCFixtureHead h;
        h.addChannel(add(QString("White %1").arg(i + 1), QLCChannel::IntensityWhite));
        mode->insertHead(-1, h);
    }
    QLCPhysical ph;
    ph.setWidth(widthMm); ph.setHeight(80); ph.setDepth(80);
    mode->setPhysical(ph);
    def->addMode(mode);
    return def;
}

static QLCPoint cellOf(const QMap<QLCPoint, GroupHead> &heads, quint32 fid, int head)
{
    for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
        if (it.value().fxi == fid && it.value().head == head)
            return it.key();
    return QLCPoint(-1, -1);
}

static QList<quint32> idsOf(const QList<Fixture *> &fixtures)
{
    QList<quint32> ids;
    foreach (Fixture *f, fixtures)
        ids << f->id();
    return ids;
}

void HeadTypeGroups_Test::initTestCase()
{
    // Never the user's own LED maps.
    QVERIFY(m_layouts.isValid());
    HeadLayoutLibrary::setUserDirectoryOverride(m_layouts.path());
    m_barDef = makeDef("Bar", 16, 4, 1000);
    m_stripDef = makeDef("Strip", 8, 0, 500);
    m_parDef = makeDef("Par", 1, 0, 200);
}

void HeadTypeGroups_Test::cleanupTestCase()
{
    delete m_barDef;
    delete m_stripDef;
    delete m_parDef;
    HeadLayoutLibrary::setUserDirectoryOverride(QString());
}

void HeadTypeGroups_Test::init()
{
    m_doc = new Doc(this);
}

void HeadTypeGroups_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

QList<Fixture *> HeadTypeGroups_Test::addBars(int count, const QString &prefix)
{
    QList<Fixture *> out;
    for (int k = 0; k < count; ++k)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("%1 %2").arg(prefix).arg(k + 1));
        f->setFixtureDefinition(m_barDef, m_barDef->modes().first());
        f->setUniverse(0);
        f->setAddress(quint32(k) * 52);
        m_doc->addFixture(f);
        out << f;
    }
    return out;
}

void HeadTypeGroups_Test::headTypesAreReadFromTheChannels()
{
    const QLCFixtureMode *bar = m_barDef->modes().first();
    QCOMPARE(HeadKinds::typeOf(bar, 0), QString("RGB"));
    QCOMPARE(HeadKinds::typeOf(bar, 16), QString("W"));
    QCOMPARE(HeadKinds::typeOf(bar, 20), QString());

    // A mover: RGBW in its head, pan and tilt on its body.
    QLCFixtureDef def;
    def.setManufacturer("Testco");
    def.setModel("Mover");
    QLCFixtureMode *mode = new QLCFixtureMode(&def);
    mode->setName("Std");
    QList<QLCChannel::Preset> presets = { QLCChannel::PositionPan, QLCChannel::PositionTilt,
                                          QLCChannel::IntensityRed, QLCChannel::IntensityGreen,
                                          QLCChannel::IntensityBlue, QLCChannel::IntensityWhite,
                                          QLCChannel::GoboWheel };
    for (int i = 0; i < presets.size(); ++i)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(QString("Ch %1").arg(i));
        ch->setPreset(presets.at(i));
        def.addChannel(ch);
        mode->insertChannel(ch, i);
    }
    QLCFixtureHead colour;
    colour.addChannel(2); colour.addChannel(3); colour.addChannel(4); colour.addChannel(5);
    mode->insertHead(-1, colour);
    QLCFixtureHead gobo;
    gobo.addChannel(6);
    mode->insertHead(-1, gobo);
    def.addMode(mode);
    mode->cacheHeads();
    QCOMPARE(HeadKinds::typeOf(mode, 0), QString("RGBW/PT"));
    QCOMPARE(HeadKinds::typeOf(mode, 1), QString("PT"));   // moves with the body

    QCOMPARE(HeadKinds::typeName("RGB"), QString("RGB"));
    QCOMPARE(HeadKinds::typeName("W"), QString("White"));
    QCOMPARE(HeadKinds::typeName("I"), QString("Dimmer"));
    QCOMPARE(HeadKinds::typeName("RGBW/PT"), QString("RGBW movers"));
    QCOMPARE(HeadKinds::typeName("PT"), QString("Movers"));
    QCOMPARE(HeadKinds::typeName("Other"), QString("Other"));
}

void HeadTypeGroups_Test::oneGroupHoldsEveryHead()
{
    const QList<Fixture *> bars = addBars(3);
    HeadTypeGroups::Options opt;
    opt.split = HeadTypeGroups::OneGroup;
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].name, QString("Bar"));
    QCOMPARE(plan[0].heads.size(), 3 * 20);
    QVERIFY(!plan[0].onStage);
    // Not placed: a row per bar, in order.
    QCOMPARE(plan[0].size, QSize(20, 3));
    QCOMPARE(cellOf(plan[0].heads, bars[0]->id(), 0), QLCPoint(0, 0));
    QCOMPARE(cellOf(plan[0].heads, bars[0]->id(), 19), QLCPoint(19, 0));
    QCOMPARE(cellOf(plan[0].heads, bars[2]->id(), 5), QLCPoint(5, 2));
}

void HeadTypeGroups_Test::aGroupPerTypeAcrossTheBars()
{
    const QList<Fixture *> bars = addBars(8);
    const QList<QPair<QString, int> > counts = HeadTypeGroups::typeCounts(m_doc, idsOf(bars));
    QCOMPARE(counts.size(), 2);
    QCOMPARE(counts[0], qMakePair(QString("RGB"), 128));
    QCOMPARE(counts[1], qMakePair(QString("W"), 32));

    HeadTypeGroups::Options opt;      // PerType by default
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan.size(), 2);
    QCOMPARE(plan[0].name, QString("Bar · RGB"));
    QCOMPARE(plan[0].type, QString("RGB"));
    QCOMPARE(plan[0].heads.size(), 128);
    QCOMPARE(plan[0].size, QSize(16, 8));
    QCOMPARE(plan[1].name, QString("Bar · White"));
    QCOMPARE(plan[1].heads.size(), 32);
    QCOMPARE(plan[1].size, QSize(4, 8));
    // The white heads are 16..19 of each bar, squeezed to the left.
    QCOMPARE(cellOf(plan[1].heads, bars[3]->id(), 16), QLCPoint(0, 3));
    QCOMPARE(cellOf(plan[1].heads, bars[3]->id(), 19), QLCPoint(3, 3));
    QCOMPARE(cellOf(plan[1].heads, bars[3]->id(), 0), QLCPoint(-1, -1));
    foreach (const HeadTypeGroups::Planned &p, plan)
        QVERIFY(p.path.isEmpty());
}

void HeadTypeGroups_Test::sameTypeOnAnotherModelSharesItsGroup()
{
    const QList<Fixture *> bars = addBars(2);
    QList<Fixture *> strips;
    for (int k = 0; k < 2; ++k)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Strip %1").arg(k + 1));
        f->setFixtureDefinition(m_stripDef, m_stripDef->modes().first());
        f->setUniverse(1);
        f->setAddress(quint32(k) * 24);
        m_doc->addFixture(f);
        strips << f;
    }
    const QList<quint32> ids = idsOf(bars) + idsOf(strips);

    HeadTypeGroups::Options opt;
    QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, ids, opt);
    QCOMPARE(plan.size(), 2);
    QCOMPARE(plan[0].type, QString("RGB"));
    QCOMPARE(plan[0].heads.size(), 2 * 16 + 2 * 8);
    QCOMPARE(plan[0].fixtures.size(), 4);
    // Bars and strips share no name: the group falls back to "Group".
    QCOMPARE(plan[0].name, QString("Group · RGB"));
    // Only the bars have white heads: named after them.
    QCOMPARE(plan[1].name, QString("Bar · White"));
    QCOMPARE(plan[1].heads.size(), 2 * 4);

    opt.keepModelsApart = true;
    plan = HeadTypeGroups::plan(m_doc, ids, opt);
    QCOMPARE(plan.size(), 3);
    QCOMPARE(plan[0].name, QString("Bar · RGB"));
    QCOMPARE(plan[0].heads.size(), 32);
    QCOMPARE(plan[1].name, QString("Strip · RGB"));
    QCOMPARE(plan[1].heads.size(), 16);
    QCOMPARE(plan[2].name, QString("Bar · White"));
    QVERIFY(plan[0].key != plan[1].key);
}

void HeadTypeGroups_Test::aGroupPerTypeForEachFixture()
{
    const QList<Fixture *> bars = addBars(8);
    HeadTypeGroups::Options opt;
    opt.split = HeadTypeGroups::PerTypePerFixture;
    opt.folder = "Bar";
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan.size(), 16);
    QCOMPARE(plan[0].name, QString("Bar 1 · RGB"));
    QCOMPARE(plan[1].name, QString("Bar 1 · White"));
    QCOMPARE(plan[15].name, QString("Bar 8 · White"));
    QCOMPARE(plan[0].size, QSize(16, 1));
    QCOMPARE(plan[1].size, QSize(4, 1));
    QCOMPARE(plan[1].fixtures, QList<quint32>() << bars[0]->id());
    QCOMPARE(cellOf(plan[1].heads, bars[0]->id(), 16), QLCPoint(0, 0));
    QSet<QString> keys;
    foreach (const HeadTypeGroups::Planned &p, plan)
    {
        QCOMPARE(p.path, QString("Bar"));
        keys.insert(p.key);
    }
    QCOMPARE(keys.size(), 16);
}

void HeadTypeGroups_Test::placedFixturesAreLaidOutByPosition()
{
    // Three bars along a truss, lying along it, one metre long, 1.2 m apart.
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setName("FOH");
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 2.0f, 5.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    const QList<Fixture *> bars = addBars(3);
    for (int k = 0; k < bars.size(); ++k)
    {
        props->setFixturePosition(bars[k]->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = 0.5f + 1.2f * k;
        rp.studioMount = 1;
        rp.studioAngle = 0.0f;
        rp.studioFaceChosen = true;
        props->setFixtureRigProps(bars[k]->id(), rp);
    }
    // Picked right to left: laid out as they stand, left to right.
    QList<quint32> ids = idsOf(bars);
    std::reverse(ids.begin(), ids.end());

    HeadTypeGroups::Options opt;
    opt.view = CreateFixtureGroup::ViewFront;
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, ids, opt);
    QCOMPARE(plan.size(), 2);
    const HeadTypeGroups::Planned &rgb = plan[0];
    QVERIFY(rgb.onStage);
    QCOMPARE(rgb.size, QSize(48, 1));      // gaps closed up: one solid row
    QCOMPARE(cellOf(rgb.heads, bars[0]->id(), 0), QLCPoint(0, 0));
    QCOMPARE(cellOf(rgb.heads, bars[0]->id(), 15), QLCPoint(15, 0));
    QCOMPARE(cellOf(rgb.heads, bars[1]->id(), 0), QLCPoint(16, 0));
    QCOMPARE(cellOf(rgb.heads, bars[2]->id(), 15), QLCPoint(47, 0));

    // To scale, the gap between the bars shows.
    opt.closeGaps = false;
    const QList<HeadTypeGroups::Planned> wide = HeadTypeGroups::plan(m_doc, ids, opt);
    QVERIFY2(cellOf(wide[0].heads, bars[1]->id(), 0).x() > 16,
             qPrintable(QString("bar 2 starts at %1").arg(cellOf(wide[0].heads, bars[1]->id(), 0).x())));
    QVERIFY(cellOf(wide[0].heads, bars[2]->id(), 0).x() > cellOf(wide[0].heads, bars[1]->id(), 0).x());
}

void HeadTypeGroups_Test::singleHeadFixturesFillASquare()
{
    QList<quint32> ids;
    for (int k = 0; k < 10; ++k)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Par %1").arg(k + 1));
        f->setFixtureDefinition(m_parDef, m_parDef->modes().first());
        f->setUniverse(2);
        f->setAddress(quint32(k) * 3);
        m_doc->addFixture(f);
        ids << f->id();
    }
    HeadTypeGroups::Options opt;
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, ids, opt);
    QCOMPARE(plan.size(), 1);   // one type: the same as one group
    QCOMPARE(plan[0].name, QString("Par · RGB"));
    QCOMPARE(plan[0].heads.size(), 10);
    QCOMPARE(plan[0].size, QSize(4, 3));
    QCOMPARE(cellOf(plan[0].heads, ids[5], 0), QLCPoint(1, 1));
}

void HeadTypeGroups_Test::namesFromAGivenBase()
{
    const QList<Fixture *> bars = addBars(2);
    HeadTypeGroups::Options opt;
    opt.baseName = "Upstage";
    QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan[0].name, QString("Upstage · RGB"));
    QCOMPARE(plan[1].name, QString("Upstage · White"));

    opt.split = HeadTypeGroups::OneGroup;
    plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan[0].name, QString("Upstage"));

    // Two fixtures of one name: their groups are told apart.
    bars[1]->setName("Bar 1");
    opt.split = HeadTypeGroups::PerTypePerFixture;
    plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan.size(), 4);
    QCOMPARE(plan[0].name, QString("Bar 1 · RGB"));
    QCOMPARE(plan[2].name, QString("Bar 1 · RGB 2"));
}

void HeadTypeGroups_Test::createMakesThePlannedGroups()
{
    const QList<Fixture *> bars = addBars(4);
    HeadTypeGroups::Options opt;
    opt.split = HeadTypeGroups::PerTypePerFixture;
    opt.folder = "Bars/Front";
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    const QList<FixtureGroup *> made = HeadTypeGroups::create(m_doc, plan);
    QCOMPARE(made.size(), 8);
    QCOMPARE(m_doc->fixtureGroups().size(), 8);
    for (int i = 0; i < made.size(); ++i)
    {
        QCOMPARE(made[i]->name(), plan[i].name);
        QCOMPARE(made[i]->path(), QString("Bars/Front"));
        QCOMPARE(made[i]->size(), plan[i].size);
        QCOMPARE(made[i]->headsMap(), plan[i].heads);
    }

    // Planned again: each knows the group it would duplicate.
    const QList<HeadTypeGroups::Planned> again = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    for (int i = 0; i < again.size(); ++i)
        QCOMPARE(again[i].sameAs, made[i]->id());
}

void HeadTypeGroups_Test::dialogPreviewsAndCreates()
{
    const QList<Fixture *> bars = addBars(8);
    CreateGroupsDialog dlg(m_doc, idsOf(bars));
    // Two types: a group per type is offered first.
    QVERIFY(dlg.m_perTypeRadio->isChecked());
    QCOMPARE(dlg.m_baseEdit->text(), QString("Bar"));
    QCOMPARE(dlg.m_list->topLevelItemCount(), 2);
    QCOMPARE(dlg.m_list->topLevelItem(0)->text(0), QString("Bar · RGB"));
    QCOMPARE(dlg.m_list->topLevelItem(0)->text(1), QString("128"));
    QCOMPARE(dlg.m_list->topLevelItem(1)->text(0), QString("Bar · White"));
    QCOMPARE(dlg.m_list->topLevelItem(1)->text(1), QString("32"));
    QVERIFY(dlg.m_createButton->isEnabled());
    QCOMPARE(dlg.m_preview->m_heads.size(), 128);   // the current one's layout

    dlg.m_oneRadio->setChecked(true);
    QCOMPARE(dlg.m_list->topLevelItemCount(), 1);
    QCOMPARE(dlg.m_list->topLevelItem(0)->text(1), QString("160"));
    QVERIFY(!dlg.m_modelsApart->isEnabled());

    // A group per fixture: 16 of them, in a folder named after the selection.
    dlg.m_perFixtureRadio->setChecked(true);
    QCOMPARE(dlg.m_list->topLevelItemCount(), 16);
    QCOMPARE(dlg.m_folderEdit->text(), QString("Bar"));
    QCOMPARE(dlg.m_list->topLevelItem(2)->text(0), QString("Bar 2 · RGB"));

    // Rename one, leave one out; both survive a re-plan.
    dlg.m_list->topLevelItem(0)->setText(0, QString("Left end RGB"));
    dlg.m_list->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
    dlg.m_closeGaps->setChecked(false);
    QCOMPARE(dlg.m_list->topLevelItem(0)->text(0), QString("Left end RGB"));
    QCOMPARE(dlg.m_list->topLevelItem(1)->checkState(0), Qt::Unchecked);
    QCOMPARE(dlg.chosenGroups().size(), 15);
    QCOMPARE(dlg.m_summary->text(), QString("15 groups to make"));

    dlg.m_createButton->click();
    QCOMPARE(dlg.result(), int(QDialog::Accepted));
    QCOMPARE(dlg.createdGroups().size(), 15);
    QCOMPARE(m_doc->fixtureGroups().size(), 15);
    QCOMPARE(dlg.createdGroups().first()->name(), QString("Left end RGB"));
    QCOMPARE(dlg.createdGroups().first()->path(), QString("Bar"));
    QCOMPARE(dlg.createdGroups().first()->headList().size(), 16);

    // Opened again: what exists already is shown, and left out.
    CreateGroupsDialog again(m_doc, idsOf(bars));
    again.m_perFixtureRadio->setChecked(true);
    QCOMPARE(again.m_list->topLevelItem(0)->checkState(0), Qt::Unchecked);
    QVERIFY(again.m_list->topLevelItem(0)->text(3).contains("Left end RGB"));
    QCOMPARE(again.m_list->topLevelItem(1)->checkState(0), Qt::Checked);   // never made
    QCOMPARE(again.chosenGroups().size(), 1);
}

void HeadTypeGroups_Test::fixtureManagerCreatesFromSelectedGroups()
{
    const QList<Fixture *> bars = addBars(3);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Upstage bars");
    grp->setSize(QSize(1, 1));
    m_doc->addFixtureGroup(grp);
    foreach (Fixture *f, bars)
        grp->assignFixture(f->id());

    FixtureManager mgr(nullptr, m_doc);
    bool seen = false;
    QTimer::singleShot(0, [&seen]() {
        CreateGroupsDialog *dlg = qobject_cast<CreateGroupsDialog *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
        {
            if (QWidget *w = QApplication::activeModalWidget())
                w->close();
            return;
        }
        seen = dlg->m_baseEdit->text() == QString("Upstage bars")
               && dlg->m_list->topLevelItemCount() == 2;
        dlg->m_createButton->click();
    });
    const QList<FixtureGroup *> made = mgr.createGroupsFromSelection(QList<quint32>(), { grp->id() });
    QVERIFY(seen);
    QCOMPARE(made.size(), 2);
    QCOMPARE(made[0]->name(), QString("Upstage bars · RGB"));
    QCOMPARE(made[0]->headList().size(), 48);
    QCOMPARE(made[1]->name(), QString("Upstage bars · White"));
    QCOMPARE(m_doc->fixtureGroups().size(), 3);
}

QTEST_MAIN(HeadTypeGroups_Test)
