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
#include <QSpinBox>
#include <QFormLayout>

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
    QVERIFY(plan[0].byGrid);
    QVERIFY(plan[0].subGroups.isEmpty());
    // Not placed: as the old one-click group, a near-square filled row by row.
    QCOMPARE(plan[0].size, QSize(8, 8));
    QCOMPARE(cellOf(plan[0].heads, bars[0]->id(), 0), QLCPoint(0, 0));
    QCOMPARE(cellOf(plan[0].heads, bars[0]->id(), 8), QLCPoint(0, 1));
    QCOMPARE(cellOf(plan[0].heads, bars[1]->id(), 0), QLCPoint(4, 2));
    QCOMPARE(cellOf(plan[0].heads, bars[2]->id(), 19), QLCPoint(3, 7));

    // A grid given: a row per bar.
    opt.grid = QSize(20, 3);
    const QList<HeadTypeGroups::Planned> rows = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(rows[0].size, QSize(20, 3));
    QCOMPARE(cellOf(rows[0].heads, bars[2]->id(), 5), QLCPoint(5, 2));
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
    QCOMPARE(plan[0].name, QString("Bar · RGB pixels"));
    QCOMPARE(plan[0].type, QString("RGB"));
    QCOMPARE(plan[0].heads.size(), 128);
    QCOMPARE(plan[0].size, QSize(16, 8));
    QCOMPARE(plan[1].name, QString("Bar · White pixels"));
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
    QCOMPARE(plan[0].name, QString("Group · RGB pixels"));
    // Only the bars have white heads: named after them.
    QCOMPARE(plan[1].name, QString("Bar · White pixels"));
    QCOMPARE(plan[1].heads.size(), 2 * 4);

    opt.keepModelsApart = true;
    plan = HeadTypeGroups::plan(m_doc, ids, opt);
    QCOMPARE(plan.size(), 3);
    QCOMPARE(plan[0].name, QString("Bar · RGB pixels"));
    QCOMPARE(plan[0].heads.size(), 32);
    QCOMPARE(plan[1].name, QString("Strip · RGB pixels"));
    QCOMPARE(plan[1].heads.size(), 16);
    QCOMPARE(plan[2].name, QString("Bar · White pixels"));
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
    QCOMPARE(plan[0].name, QString("Bar 1 · RGB pixels"));
    QCOMPARE(plan[1].name, QString("Bar 1 · White pixels"));
    QCOMPARE(plan[15].name, QString("Bar 8 · White pixels"));
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
    QCOMPARE(plan[0].name, QString("Upstage · RGB pixels"));
    QCOMPARE(plan[1].name, QString("Upstage · White pixels"));

    opt.split = HeadTypeGroups::OneGroup;
    plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan[0].name, QString("Upstage"));

    // Two fixtures of one name: their groups are told apart.
    bars[1]->setName("Bar 1");
    opt.split = HeadTypeGroups::PerTypePerFixture;
    plan = HeadTypeGroups::plan(m_doc, idsOf(bars), opt);
    QCOMPARE(plan.size(), 4);
    QCOMPARE(plan[0].name, QString("Bar 1 · RGB pixels"));
    QCOMPARE(plan[2].name, QString("Bar 1 · RGB pixels 2"));
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
    QCOMPARE(dlg.m_list->topLevelItem(0)->text(0), QString("Bar · RGB pixels"));
    QCOMPARE(dlg.m_list->topLevelItem(0)->text(1), QString("128"));
    QCOMPARE(dlg.m_list->topLevelItem(1)->text(0), QString("Bar · White pixels"));
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
    QCOMPARE(dlg.m_list->topLevelItem(2)->text(0), QString("Bar 2 · RGB pixels"));

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

/* Accept whichever dialog is up next, as its OK button would. */
static void acceptNextDialog(bool *seen = nullptr)
{
    QTimer::singleShot(0, [seen]() {
        if (QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
        {
            if (seen != nullptr)
                *seen = true;
            d->accept();
        }
    });
}

void HeadTypeGroups_Test::fixtureManagerCreatesFromSelectedGroups()
{
    const QList<Fixture *> bars = addBars(3);
    FixtureGroup *grp = new FixtureGroup(m_doc);
    grp->setName("Upstage bars");
    grp->setSize(QSize(20, 3));
    m_doc->addFixtureGroup(grp);
    foreach (Fixture *f, bars)
        grp->assignFixture(f->id());

    FixtureManager mgr(nullptr, m_doc);

    // A group selected: one group of it, as a block, is offered first.
    bool seen = false;
    QTimer::singleShot(0, [&seen]() {
        CreateGroupsDialog *dlg = qobject_cast<CreateGroupsDialog *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
        {
            if (QWidget *w = QApplication::activeModalWidget())
                w->close();
            return;
        }
        seen = dlg->m_oneRadio->isChecked()
               && dlg->m_keepBlocks != nullptr && dlg->m_keepBlocks->isChecked()
               && dlg->m_baseEdit->text() == QString("Upstage bars")
               && dlg->m_list->topLevelItemCount() == 1
               // The same heads as the group: flagged, but made -- it was asked for.
               && !dlg->m_list->topLevelItem(0)->text(3).isEmpty()
               && dlg->m_list->topLevelItem(0)->checkState(0) == Qt::Checked;
        dlg->m_createButton->click();
    });
    QList<FixtureGroup *> made = mgr.createGroupsFromSelection(QList<quint32>(), { grp->id() });
    QVERIFY(seen);
    QCOMPARE(made.size(), 1);
    QCOMPARE(made[0]->name(), QString("Upstage bars"));
    QCOMPARE(made[0]->headsMap(), grp->headsMap());
    QCOMPARE(made[0]->size(), QSize(20, 3));
    foreach (quint32 tag, made[0]->headSubGroupMap().values())
        QCOMPARE(tag, FixtureGroup::subGroupTagFor(grp->id()));
    QCOMPARE(made[0]->headSubGroupMap().size(), 60);

    // A group per type: its fixtures' heads.
    seen = false;
    QTimer::singleShot(0, [&seen]() {
        CreateGroupsDialog *dlg = qobject_cast<CreateGroupsDialog *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
        {
            if (QWidget *w = QApplication::activeModalWidget())
                w->close();
            return;
        }
        dlg->m_perTypeRadio->setChecked(true);
        seen = !dlg->m_keepBlocks->isEnabled() && dlg->m_list->topLevelItemCount() == 2;
        dlg->m_createButton->click();
    });
    made = mgr.createGroupsFromSelection(QList<quint32>(), { grp->id() });
    QVERIFY(seen);
    QCOMPARE(made.size(), 2);
    QCOMPARE(made[0]->name(), QString("Upstage bars · RGB pixels"));
    QCOMPARE(made[0]->headList().size(), 48);
    QCOMPARE(made[1]->name(), QString("Upstage bars · White pixels"));
    QCOMPARE(m_doc->fixtureGroups().size(), 4);
}

void HeadTypeGroups_Test::pixelsOnlyForManyHeadsOfAType()
{
    const QList<Fixture *> bars = addBars(1);
    Fixture *par = new Fixture(m_doc);
    par->setName("Par 1");
    par->setFixtureDefinition(m_parDef, m_parDef->modes().first());
    par->setUniverse(2);
    m_doc->addFixture(par);

    QVERIFY(HeadTypeGroups::arePixels(m_doc, idsOf(bars), "RGB"));
    QVERIFY(HeadTypeGroups::arePixels(m_doc, idsOf(bars), "W"));
    QVERIFY(!HeadTypeGroups::arePixels(m_doc, { par->id() }, "RGB"));
    // A bar and a par in one group: not every one of them is pixels.
    QVERIFY(!HeadTypeGroups::arePixels(m_doc, { bars[0]->id(), par->id() }, "RGB"));
    // Movers stay movers, however many heads.
    QVERIFY(!HeadTypeGroups::arePixels(m_doc, idsOf(bars), "RGB/PT"));
    QCOMPARE(HeadTypeGroups::groupName("Spot", "RGBW/PT", false), QString("Spot · RGBW movers"));
    QCOMPARE(HeadTypeGroups::groupName("Spot", "PT", false), QString("Spot · Movers"));
    QCOMPARE(HeadTypeGroups::groupName("Bar", "RGB", true), QString("Bar · RGB pixels"));

    HeadTypeGroups::Options opt;
    opt.split = HeadTypeGroups::PerTypePerFixture;
    const QList<HeadTypeGroups::Planned> plan
        = HeadTypeGroups::plan(m_doc, { bars[0]->id(), par->id() }, opt);
    QCOMPARE(plan.size(), 3);
    QCOMPARE(plan[0].name, QString("Bar 1 · RGB pixels"));
    QCOMPARE(plan[1].name, QString("Bar 1 · White pixels"));
    QCOMPARE(plan[2].name, QString("Par 1 · RGB"));
}

QList<Fixture *> HeadTypeGroups_Test::addStrips(int count)
{
    QList<Fixture *> out;
    for (int k = 0; k < count; ++k)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QString("Strip %1").arg(k + 1));
        f->setFixtureDefinition(m_stripDef, m_stripDef->modes().first());
        f->setUniverse(1);
        f->setAddress(quint32(k) * 24);
        m_doc->addFixture(f);
        out << f;
    }
    return out;
}

void HeadTypeGroups_Test::oneGroupOfGroupsKeepsThemAsBlocks()
{
    const QList<Fixture *> strips = addStrips(4);
    Fixture *par = new Fixture(m_doc);
    par->setName("Par 1");
    par->setFixtureDefinition(m_parDef, m_parDef->modes().first());
    par->setUniverse(2);
    m_doc->addFixture(par);

    // A: strips 1 and 2, a row each. B: strip 3 off in a corner of its grid.
    FixtureGroup *a = new FixtureGroup(m_doc);
    a->setName("A");
    a->setSize(QSize(8, 2));
    m_doc->addFixtureGroup(a);
    a->assignFixture(strips[0]->id());
    a->assignFixture(strips[1]->id());
    FixtureGroup *b = new FixtureGroup(m_doc);
    b->setName("B");
    b->setSize(QSize(10, 6));
    m_doc->addFixtureGroup(b);
    b->assignFixture(strips[2]->id(), QLCPoint(2, 5));

    // Strip 1 again (already in A, so skipped), the par and strip 4 loose.
    const QList<quint32> loose = { strips[0]->id(), par->id(), strips[3]->id() };
    HeadTypeGroups::Options opt;
    opt.split = HeadTypeGroups::OneGroup;
    opt.groupIds = { a->id(), b->id() };
    const QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, loose, opt);
    QCOMPARE(plan.size(), 1);
    const HeadTypeGroups::Planned &p = plan[0];
    QVERIFY(p.asBlocks);
    QCOMPARE(p.size, QSize(9, 4));
    QCOMPARE(p.heads.size(), 8 * 4 + 1);
    QCOMPARE(cellOf(p.heads, strips[0]->id(), 0), QLCPoint(0, 0));
    QCOMPARE(cellOf(p.heads, strips[1]->id(), 7), QLCPoint(7, 1));
    QCOMPARE(cellOf(p.heads, strips[2]->id(), 0), QLCPoint(0, 2));   // B to its corner
    QCOMPARE(cellOf(p.heads, par->id(), 0), QLCPoint(0, 3));
    QCOMPARE(cellOf(p.heads, strips[3]->id(), 7), QLCPoint(8, 3));
    QCOMPARE(p.subGroups.value(QLCPoint(3, 1)), FixtureGroup::subGroupTagFor(a->id()));
    QCOMPARE(p.subGroups.value(QLCPoint(3, 2)), FixtureGroup::subGroupTagFor(b->id()));
    QVERIFY(!p.subGroups.contains(QLCPoint(3, 3)));

    // The same as "Add to Group" into an empty group -- the old one-click result.
    FixtureGroup *old = new FixtureGroup(m_doc);
    old->setSize(QSize(1, 1));
    m_doc->addFixtureGroup(old);
    FixtureManager mgr(nullptr, m_doc);
    mgr.copySelectionIntoGroup(old, loose, opt.groupIds);
    QCOMPARE(p.heads, old->headsMap());
    QCOMPARE(p.subGroups, old->headSubGroupMap());
    QCOMPARE(p.size, old->size());

    // Made: tags and all.
    const QList<FixtureGroup *> made = HeadTypeGroups::create(m_doc, plan);
    QCOMPARE(made.size(), 1);
    QCOMPARE(made[0]->headSubGroupMap(), p.subGroups);

    // Not as blocks: every fixture afresh, in a grid.
    opt.groupsAsBlocks = false;
    const QList<HeadTypeGroups::Planned> fresh = HeadTypeGroups::plan(m_doc, loose, opt);
    QVERIFY(!fresh[0].asBlocks);
    QVERIFY(fresh[0].subGroups.isEmpty());
    QCOMPARE(fresh[0].heads.size(), 33);
}

void HeadTypeGroups_Test::oneGroupMatchesTheOldCreateGroup()
{
    const QList<Fixture *> strips = addStrips(3);
    const QList<quint32> ids = idsOf(strips);

    // Off the stage: the old name-and-size dialog, accepted as offered.
    bool seen = false;
    acceptNextDialog(&seen);
    FixtureGroup *old = CreateFixtureGroup::createFromFixtures(m_doc, ids, nullptr);
    QVERIFY(seen);
    QVERIFY(old != nullptr);

    HeadTypeGroups::Options opt;
    opt.split = HeadTypeGroups::OneGroup;
    QList<HeadTypeGroups::Planned> plan = HeadTypeGroups::plan(m_doc, ids, opt);
    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].size, old->size());
    QCOMPARE(plan[0].heads, old->headsMap());
    QCOMPARE(plan[0].size, QSize(5, 5));
    QCOMPARE(cellOf(plan[0].heads, strips[1]->id(), 0), QLCPoint(3, 1));
    QCOMPARE(plan[0].sameAs, old->id());

    // The dialog: one group (one type), the grid offered and used.
    {
        CreateGroupsDialog dlg(m_doc, ids);
        QVERIFY(dlg.m_oneRadio->isChecked());
        QVERIFY(dlg.m_form->isRowVisible(dlg.m_gridRow));
        QCOMPARE(dlg.m_gridWidth->value(), 5);
        QCOMPARE(dlg.m_gridHeight->value(), 5);
        QCOMPARE(dlg.chosenGroups().size(), 1);     // flagged as old's twin, still made
        QCOMPARE(dlg.chosenGroups().first().heads, old->headsMap());
        dlg.m_gridWidth->setValue(24);
        dlg.m_gridHeight->setValue(1);
        QCOMPARE(dlg.chosenGroups().first().size, QSize(24, 1));
        QCOMPARE(cellOf(dlg.chosenGroups().first().heads, strips[2]->id(), 7), QLCPoint(23, 0));
        dlg.m_perTypeRadio->setChecked(true);
        QVERIFY(!dlg.m_form->isRowVisible(dlg.m_gridRow));
    }

    // On the stage: the old stage dialog, accepted as offered (with no twin
    // of it about, which it would ask about).
    QVERIFY(m_doc->deleteFixtureGroup(old->id()));
    MonitorProperties *props = m_doc->monitorProperties();
    Truss *t = props->addTruss();
    t->setType(Truss::Horizontal);
    t->setOrigin(QVector3D(0.0f, 2.0f, 5.0f));
    t->setDirection(QPointF(1.0, 0.0));
    t->setLength(6.0f);
    for (int k = 0; k < strips.size(); ++k)
    {
        props->setFixturePosition(strips[k]->id(), 0, 0, QVector3D(0, 0, 0));
        FixtureRigProps rp;
        rp.trussId = t->id();
        rp.trussOffset = 0.5f + 1.0f * k;
        rp.studioMount = 1;
        rp.studioFaceChosen = true;
        props->setFixtureRigProps(strips[k]->id(), rp);
    }
    seen = false;
    acceptNextDialog(&seen);
    FixtureGroup *stage = CreateFixtureGroup::createFromFixtures(m_doc, ids, nullptr);
    QVERIFY(seen);
    QVERIFY(stage != nullptr);
    {
        CreateGroupsDialog dlg(m_doc, ids);
        QVERIFY(!dlg.m_form->isRowVisible(dlg.m_gridRow));
        const QList<HeadTypeGroups::Planned> chosen = dlg.chosenGroups();
        QCOMPARE(chosen.size(), 1);
        QVERIFY(chosen[0].onStage);
        QCOMPARE(chosen[0].size, stage->size());
        QCOMPARE(chosen[0].heads, stage->headsMap());
        QCOMPARE(chosen[0].size, QSize(24, 1));
    }
}

QTEST_MAIN(HeadTypeGroups_Test)
