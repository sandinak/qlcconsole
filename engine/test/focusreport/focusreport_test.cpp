/*
  Q Light Controller Plus
  focusreport_test.cpp

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

#include "focusreport_test.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "fixturegroup.h"
#include "focussession.h"
#include "focusreport.h"
#include "rigowner.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "focuscalls.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
/* A 540/270 spot: pan, tilt, dimmer, placed free at (x, y) metres, hung at
   5 m (as focuscalls_test's). */
Fixture *makeSpot(Doc *doc, const QString &name, float x, float y)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Report Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1); head.addChannel(2);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * 3));
    if (doc->addFixture(fxi) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0,
                                                 QVector3D(x * 1000.0f, y * 1000.0f, 5.0f));
    return fxi;
}

StageTarget *makeTarget(Doc *doc, const QString &name, const QVector3D &at)
{
    StageTarget *t = doc->monitorProperties()->addStageTarget();
    t->setName(name);
    t->setPosition(at);
    return t;
}

FixtureGroup *makeGroup(Doc *doc, const QString &name, const QList<Fixture *> &members)
{
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName(name);
    g->setSize(QSize(qMax(1, int(members.size())), 1));
    for (int i = 0; i < members.size(); ++i)
        g->assignFixture(members.at(i)->id(), QLCPoint(i, 0));
    doc->addFixtureGroup(g);
    return g;
}

FocusSession::Reference targetRef(quint32 id)
{
    FocusSession::Reference r;
    r.kind = FocusSession::Reference::Target;
    r.id = id;
    return r;
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
}

void FocusReport_Test::init()
{
    m_doc = new Doc(this);
    m_dir = new QTemporaryDir();
    m_doc->programmer()->focusSession()->setFlashEnabled(false);
}

void FocusReport_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

void FocusReport_Test::aVenueRemembersChecksAndDeadLights()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    v->setCheckedBy(" Sam ");
    QCOMPARE(v->checkedBy(), QString("Sam"));

    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.fixtures = { a->id(), b->id() };
    o.order = FocusSession::AsGiven;
    o.first = targetRef(dsc->id());
    QVERIFY(s->start(o));
    QVERIFY(!v->isChecked(a->id()));
    const QDateTime before = QDateTime::currentDateTime().addSecs(-1);
    // Saved with no change at all is still checked: the light was looked at.
    s->save();
    QVERIFY(v->isChecked(a->id()));
    QCOMPARE(v->check(a->id()).by, QString("Sam"));
    QVERIFY(v->check(a->id()).when >= before);
    QVERIFY(!v->hasCorrection(a->id()));
    // The next one, nudged and saved.
    s->next();
    s->nudge(1.0f, 0.0f);
    s->save();
    QVERIFY(v->isChecked(b->id()));
    s->stop();

    QVERIFY(v->setDead(b->id(), true));
    QVERIFY(v->isDead(b->id()));
    QCOMPARE(v->deadLights(), QList<quint32>({ b->id() }));
    // Dead, and never checked: unmarked, it is forgotten.
    QVERIFY(v->setDead(a->id(), true));
    QVERIFY(v->setDead(a->id(), false));
    QVERIFY(v->isChecked(a->id()));
    QVERIFY(!v->isDead(a->id()));

    const QByteArray side = readFile(v->filePath());
    QVERIFY(side.contains("<Check ID=\"" + QByteArray::number(b->id()) + "\""));
    QVERIFY(side.contains("By=\"Sam\""));
    QVERIFY(side.contains("Dead=\"1\""));

    // Read back, as a restart would.
    VenueCorrections again(m_doc);
    QVERIFY(again.fromXml(side));
    QCOMPARE(again.venue("Lincoln HS").checks.size(), 2);
    QVERIFY(again.venue("Lincoln HS").checks.value(b->id()).dead);
    QCOMPARE(again.venue("Lincoln HS").checks.value(a->id()).by, QString("Sam"));

    // No venue applied: nothing to note them in.
    QVERIFY(v->setActiveVenue(QString()));
    QVERIFY(!v->setDead(a->id(), true));
    QVERIFY(!v->markChecked({ a->id() }));
}

void FocusReport_Test::theReportListsEveryLight()
{
    Fixture *a = makeSpot(m_doc, "Spot 1", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "Spot, 2", 4.0f, 2.0f);
    Fixture *c = makeSpot(m_doc, "Spot 3", 6.0f, 2.0f);
    Fixture *loose = makeSpot(m_doc, "Loose", 8.0f, 2.0f);
    FixtureGroup *movers = makeGroup(m_doc, "Movers", { a, b });
    FixtureGroup *at = makeGroup(m_doc, "AT spots", { c });
    MonitorProperties *props = m_doc->monitorProperties();
    StageTarget *dsc = makeTarget(m_doc, "DSC", props->stageCentre() + QVector3D(0.0f, 1.0f, 0.0f));
    StageTarget *solo = makeTarget(m_doc, "Soloist", props->stageCentre() + QVector3D(2.0f, -1.0f, 0.0f));
    // The show's own calibration on A: the report's correction is from it.
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 2.0f;
    props->setFixtureRigProps(a->id(), rp);

    FocusCalls *calls = m_doc->programmer()->focusCalls();
    FocusCall c1;
    c1.groups << movers->id();
    c1.target = targetRef(dsc->id());
    c1.note = "Cut off the pros";
    FocusCall c2;
    c2.name = "AT to the soloist";
    c2.groups << at->id();
    c2.target = targetRef(solo->id());
    calls->setCalls({ c1, c2 });

    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    v->setCheckedBy("Sam");

    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.calls = calls->sessionCalls();
    o.order = FocusSession::AsGiven;
    QString why;
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QCOMPARE(s->currentFixture(), a->id());
    s->nudge(1.3f, -0.4f);
    s->save();
    s->stop();
    QVERIFY(v->setDead(c->id(), true));
    // A light corrected outside the calls (the fixture dialog, say).
    rp = props->fixtureRigProps(loose->id());
    rp.tiltOffsetDeg = -2.0f;
    props->setFixtureRigProps(loose->id(), rp);
    QVERIFY(v->hasCorrection(loose->id()));

    const QList<FocusReport::Row> rows = FocusReport::rows(m_doc, "Lincoln HS");
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).fid, a->id());
    QCOMPARE(rows.at(0).call, 0);
    QCOMPARE(rows.at(0).target, QString("DSC"));
    // From the stage marks, compact: across, then up or down stage.
    QCOMPARE(rows.at(0).position, props->stagePositionText(dsc->position()));
    QVERIFY(!rows.at(0).position.isEmpty());
    QCOMPARE(rows.at(0).correction, QString("+1.3° pan, −0.4° tilt"));
    QVERIFY(rows.at(0).checked);
    QCOMPARE(rows.at(0).checkedBy, QString("Sam"));
    QVERIFY(!rows.at(0).needsFocus);
    QCOMPARE(rows.at(0).notes, QString("Cut off the pros"));
    QCOMPARE(rows.at(0).address, QString("1.1"));
    QCOMPARE(rows.at(0).type, QString("Test Report Spot Spot 1"));
    // Whose light it is (VENUES.md): nobody said, so the tour's.
    QCOMPARE(rows.at(0).owner, RigOwner::displayName(RigOwner::Tour));
    // B: not looked at -- it needs focus.
    QCOMPARE(rows.at(1).fid, b->id());
    QVERIFY(rows.at(1).correction.isEmpty());
    QVERIFY(!rows.at(1).checked);
    QVERIFY(rows.at(1).needsFocus);
    QCOMPARE(FocusReport::flags(rows.at(1)), QString("needs focus"));
    // C: dead is its own flag, not "needs focus".
    QCOMPARE(rows.at(2).call, 1);
    QCOMPARE(rows.at(2).callName, QString("AT to the soloist"));
    QVERIFY(rows.at(2).dead);
    QCOMPARE(FocusReport::flags(rows.at(2)), QString("dead"));
    // The loose one: outside the calls.
    QCOMPARE(rows.at(3).fid, loose->id());
    QCOMPARE(rows.at(3).call, -1);
    QCOMPARE(rows.at(3).correction, QString("−2.0° tilt"));

    // A swapped fixture (Q133) is flagged so.
    const QString csv = FocusReport::csv(m_doc, "Lincoln HS");
    const QStringList lines = csv.split("\r\n", Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 5);
    QCOMPARE(lines.first(), FocusReport::csvColumns().join(','));
    QVERIFY(lines.first().startsWith("Purpose,Position,Unit#,Instrument Type"));
    QVERIFY(lines.first().endsWith(",Owner"));
    // "Spot, 2" holds a comma: quoted.
    QVERIFY(lines.at(2).contains("\"Spot, 2\""));
    QVERIFY(lines.at(1).contains("Cut off the pros"));
    QVERIFY(lines.at(3).contains(",dead,"));
    QCOMPARE(FocusReport::csvField("say \"hi\""), QString("\"say \"\"hi\"\"\""));

    const QString html = FocusReport::html(m_doc, "Lincoln HS", "Tour");
    QVERIFY(html.startsWith("<!DOCTYPE html>"));
    QVERIFY(html.contains("Focus report: Lincoln HS"));
    QVERIFY(html.contains("Call 1: Movers"));
    QVERIFY(html.contains("Call 2: AT to the soloist"));
    QVERIFY(html.contains("Outside the calls"));
    QVERIFY(html.contains("Spot, 2"));
    QVERIFY(html.contains("4 lights: 1 checked, 1 dead, 2 need focus."));
    QVERIFY(html.contains("Focused by: Sam"));
    QVERIFY(!html.contains("Practice score"));
    QVERIFY(html.contains("<th>Owner</th>"));
}

void FocusReport_Test::aNewVenueStartsFromTheLastOne()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    MonitorProperties *props = m_doc->monitorProperties();
    v->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(v->lastVenue().isEmpty());
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 3.0f;
    props->setFixtureRigProps(a->id(), rp);
    QVERIFY(v->markChecked({ a->id() }));
    QCOMPARE(v->lastVenue(), QString("Lincoln HS"));

    const QString next = v->addVenueFrom("Jefferson MS", "Lincoln HS");
    QCOMPARE(next, QString("Jefferson MS"));
    const VenueCorrections::Venue j = v->venue(next);
    QCOMPARE(j.fixtures.value(a->id()).panOffsetDeg, 3.0f);
    QVERIFY(j.checks.isEmpty());
    QVERIFY(v->setActiveVenue(next));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 3.0f);
    QVERIFY(!v->isChecked(a->id()));
    // An existing name is not overwritten; an unknown source is refused.
    QCOMPARE(v->addVenueFrom("Lincoln HS", next), QString("Lincoln HS"));
    QVERIFY(v->addVenueFrom("Elsewhere", "Nowhere").isEmpty());
    // The show itself never took it.
    QVERIFY(v->setActiveVenue(QString()));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 0.0f);
}

void FocusReport_Test::practiceScramblesAndScores()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    FixtureGroup *movers = makeGroup(m_doc, "Movers", { a, b });
    StageTarget *dsc = makeTarget(m_doc, "DSC", QVector3D(5.0f, 6.0f, 0.0f));
    FocusCall call;
    call.groups << movers->id();
    call.target = targetRef(dsc->id());
    m_doc->programmer()->focusCalls()->setCalls({ call });
    MonitorProperties *props = m_doc->monitorProperties();
    VenueCorrections *v = m_doc->programmer()->venueCorrections();

    // No show file: nowhere to keep a practice.
    QString why;
    QVERIFY(v->startPractice({ a->id() }, 1, 2.0f, 8.0f, &why).isEmpty());
    QVERIFY(!why.isEmpty());

    v->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 1.0f;
    props->setFixtureRigProps(a->id(), rp);
    const QByteArray realBefore = v->toXml();

    const QString practice = v->startPractice({ a->id(), b->id() }, 42, 2.0f, 8.0f, &why);
    QVERIFY2(!practice.isEmpty(), qPrintable(why));
    QVERIFY(practice.startsWith("Practice "));
    QCOMPARE(v->activeVenue(), practice);
    QVERIFY(v->isPracticeActive());
    // A second practice on top of one is refused.
    QVERIFY(v->startPractice({ a->id() }, 1).isEmpty());

    // Each light off its right calibration by 2-8 degrees each way.
    for (Fixture *fx : { a, b })
    {
        RigCalibration truth;
        QVERIFY(v->practiceTruth(fx->id(), truth));
        const RigCalibration now = RigCalibration::of(props->fixtureRigProps(fx->id()));
        const float dp = qAbs(now.panOffsetDeg - truth.panOffsetDeg);
        const float dt = qAbs(now.tiltOffsetDeg - truth.tiltOffsetDeg);
        QVERIFY2(dp >= 1.95f && dp <= 8.05f, qPrintable(QString::number(dp)));
        QVERIFY2(dt >= 1.95f && dt <= 8.05f, qPrintable(QString::number(dt)));
    }
    RigCalibration truthA;
    QVERIFY(v->practiceTruth(a->id(), truthA));
    QCOMPARE(truthA.panOffsetDeg, 1.0f);       // the room's, not the show's
    QVERIFY(v->practiceScore(practice).meanError >= 2.0);

    // The student focuses A back, and leaves B.
    FocusSession *s = m_doc->programmer()->focusSession();
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    o.order = FocusSession::AsGiven;
    QVERIFY2(s->start(o, &why), qPrintable(why));
    QCOMPARE(s->currentFixture(), a->id());
    const FocusSession::Offset live = s->liveOffset(a->id());
    s->nudge(truthA.panOffsetDeg - live.pan, truthA.tiltOffsetDeg - live.tilt);
    s->save();
    s->stop();

    const VenueCorrections::Score sc = v->practiceScore(practice);
    QCOMPARE(sc.lights, 2);
    QCOMPARE(sc.checked, 1);
    QCOMPARE(sc.withinOne, 1);
    QCOMPARE(sc.saves, 1);
    QVERIFY(sc.errors.value(a->id()) < 0.02);
    QVERIFY(sc.errors.value(b->id()) >= 2.0);
    QVERIFY(sc.seconds >= 0.0);

    // In the report: the score, and how far each light is still off.
    const QString html = FocusReport::html(m_doc, practice, "Tour");
    QVERIFY(html.contains("Practice score"));
    QVERIFY(html.contains("1 of 2 lights within 1°"));
    QVERIFY(html.contains("Scrambled from Lincoln HS"));
    QVERIFY(FocusReport::csv(m_doc, practice).contains("practice: "));

    // Ending puts the real venue back, as it was: practice never touched it.
    QVERIFY(v->endPractice());
    QCOMPARE(v->activeVenue(), QString("Lincoln HS"));
    QCOMPARE(props->fixtureRigProps(a->id()).panOffsetDeg, 1.0f);
    QVERIFY(!v->isPracticeActive());
    {
        VenueCorrections a1(m_doc), a2(m_doc);
        QVERIFY(a1.fromXml(realBefore));
        QVERIFY(a2.fromXml(v->toXml()));
        QCOMPARE(a2.venue("Lincoln HS").fixtures.size(), a1.venue("Lincoln HS").fixtures.size());
        QCOMPARE(a2.venue("Lincoln HS").fixtures.value(a->id()).panOffsetDeg,
                 a1.venue("Lincoln HS").fixtures.value(a->id()).panOffsetDeg);
        QVERIFY(a2.venue("Lincoln HS").checks.isEmpty());
        // The practice's results are kept, and read back.
        const VenueCorrections::Venue back = a2.venue(practice);
        QVERIFY(back.practice.on);
        QCOMPARE(back.practice.from, QString("Lincoln HS"));
        QCOMPARE(back.practice.saves, 1);
        QCOMPARE(back.practice.truth.size(), 2);
        QCOMPARE(back.practice.truth.value(a->id()).panOffsetDeg, 1.0f);
    }
    QVERIFY(!v->endPractice());
    /* The reader stops at <Practice>'s own end: an element written after it
       in the same venue is still read. */
    {
        const QByteArray xml =
            "<QlcVenues Version=\"1\"><Venue Name=\"P\">"
            "<Practice From=\"Lincoln HS\" Saves=\"2\"><Truth ID=\"7\" PanOfs=\"1.00\" TiltOfs=\"0.00\"/></Practice>"
            "<Check ID=\"7\" By=\"Sam\" Dead=\"1\"/>"
            "<Fixture ID=\"7\" PanOfs=\"3.00\" TiltOfs=\"0.00\" PanZero=\"0.0\"/>"
            "</Venue><Venue Name=\"Q\"/></QlcVenues>";
        VenueCorrections r(m_doc);
        QVERIFY(r.fromXml(xml));
        const VenueCorrections::Venue p = r.venue("P");
        QVERIFY(p.practice.on);
        QCOMPARE(p.practice.saves, 2);
        QCOMPARE(p.practice.truth.value(7).panOffsetDeg, 1.0f);
        QVERIFY(p.checks.value(7).dead);
        QCOMPARE(p.fixtures.value(7).panOffsetDeg, 3.0f);
        QCOMPARE(r.venueNames(), QStringList({ "P", "Q" }));
    }
    // The last real venue is never a practice one.
    QCOMPARE(v->lastVenue(), QString("Lincoln HS"));
}

QTEST_MAIN(FocusReport_Test)

/* The changelog (Q134): the report gives each light's lens and flags a
   swapped one -- its definition or mode no longer what the venue
   recorded -- as swapped and needing focus, saying what it was focused
   as; in the rows, the CSV (Lens and Flags columns) and the HTML. */
void FocusReport_Test::aSwappedLightAndTheLensAreReported()
{
    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    Fixture *other = makeSpot(m_doc, "Other", 6.0f, 2.0f);
    MonitorProperties *props = m_doc->monitorProperties();
    VenueCorrections *v = m_doc->programmer()->venueCorrections();
    v->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(v->setActiveVenue(v->addVenue("Lincoln HS")));
    // Corrected in the venue (Q111): A by a pan, B's lens set by hand.
    FixtureRigProps rp = props->fixtureRigProps(a->id());
    rp.panOffsetDeg = 1.5f;
    props->setFixtureRigProps(a->id(), rp);
    rp = props->fixtureRigProps(b->id());
    rp.lensDeg = 19.0f;
    props->setFixtureRigProps(b->id(), rp);
    QVERIFY(v->hasCorrection(a->id()));
    QVERIFY(v->hasCorrection(b->id()));

    // A is swapped for another fixture type while the venue is off.
    QVERIFY(v->setActiveVenue(QString()));
    a->setFixtureDefinition(other->fixtureDef(), other->fixtureMode());
    QVERIFY(v->setActiveVenue("Lincoln HS"));
    QVERIFY(v->isSwapped("Lincoln HS", a->id()));
    QVERIFY(!v->isSwapped("Lincoln HS", b->id()));

    const QList<FocusReport::Row> rows = FocusReport::rows(m_doc, "Lincoln HS");
    const FocusReport::Row *ra = nullptr, *rb = nullptr;
    for (const FocusReport::Row &r : rows)
    {
        if (r.fid == a->id())
            ra = &r;
        else if (r.fid == b->id())
            rb = &r;
    }
    QVERIFY(ra != nullptr && rb != nullptr);
    QVERIFY(ra->swapped);
    QVERIFY(ra->needsFocus);
    QCOMPARE(FocusReport::flags(*ra), QString("swapped, needs focus"));
    QVERIFY2(ra->notes.contains("Focused here as Test Report Spot A, 3ch"), qPrintable(ra->notes));
    QVERIFY(!rb->swapped);
    QCOMPARE(rb->lens, QString::fromUtf8("19.0°"));
    QVERIFY(ra->lens.isEmpty());                 // no lens set by hand

    // The CSV, by column.
    auto fields = [](const QString &line) {
        QStringList out;
        QString cur;
        bool quoted = false;
        for (int i = 0; i < line.size(); ++i)
        {
            const QChar ch = line.at(i);
            if (quoted && ch == '"' && i + 1 < line.size() && line.at(i + 1) == '"')
            {
                cur += '"';
                ++i;
            }
            else if (ch == '"')
                quoted = !quoted;
            else if (ch == ',' && !quoted)
            {
                out << cur;
                cur.clear();
            }
            else
                cur += ch;
        }
        out << cur;
        return out;
    };
    const QStringList cols = FocusReport::csvColumns();
    const int lens = int(cols.indexOf("Lens")), flags = int(cols.indexOf("Flags")), id = int(cols.indexOf("Fixture ID"));
    QVERIFY(lens >= 0 && flags >= 0 && id >= 0);
    bool sawA = false, sawB = false;
    for (const QString &line : FocusReport::csv(m_doc, "Lincoln HS").split("\r\n", Qt::SkipEmptyParts).mid(1))
    {
        const QStringList f = fields(line);
        QCOMPARE(f.size(), cols.size());
        if (f.at(id) == QString::number(a->id()))
        {
            sawA = true;
            QCOMPARE(f.at(flags), QString("swapped, needs focus"));
            QVERIFY(f.at(lens).isEmpty());
        }
        else if (f.at(id) == QString::number(b->id()))
        {
            sawB = true;
            QCOMPARE(f.at(lens), QString::fromUtf8("19.0°"));
        }
    }
    QVERIFY(sawA && sawB);
    const QString html = FocusReport::html(m_doc, "Lincoln HS", "Tour");
    QVERIFY(html.contains("<th>Lens</th>"));
    QVERIFY(html.contains(QString::fromUtf8("<td>19.0°</td>")));
    QVERIFY(html.contains("<td class=\"flag\">swapped, needs focus</td>"));
}
