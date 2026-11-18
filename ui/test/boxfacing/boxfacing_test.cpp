/*
  Q Light Controller Plus
  boxfacing_test.cpp

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

#include <QStandardItemModel>
#include "../common/fixtureeditor.h"
#include <QStyleOptionGraphicsItem>
#include <QApplication>
#include <QFormLayout>
#include <QMouseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QToolButton>
#include <QPainter>
#include <QImage>
#include <QDir>
#include <QtTest>
#include <QtMath>
#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "fixturefields.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "platformitem.h"
#include "structurestudioview.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "measurementunits.h"
#include "stageplatform.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "aimsolver.h"
#include "pixelpath.h"
#include "fixture.h"
#include "doc.h"

#include "boxfacing_test.h"

namespace
{
/** A string of @p pix RGB LEDs, one head each. */
Fixture *makeLedString(Doc *doc, int pix, const char *name, quint32 address = 0)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QString("String %1").arg(name));
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("RGB");
    for (int h = 0; h < pix; ++h)
    {
        const QLCChannel::PrimaryColour pc[] = { QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue };
        for (int k = 0; k < 3; ++k)
        {
            QLCChannel *ch = new QLCChannel();
            ch->setName(QString("%1-%2").arg(h).arg(k));
            ch->setGroup(QLCChannel::Intensity);
            ch->setColour(pc[k]);
            def->addChannel(ch);
            mode->insertChannel(ch, h * 3 + k);
        }
        QLCFixtureHead hd;
        hd.addChannel(quint32(h * 3));
        hd.addChannel(quint32(h * 3 + 1));
        hd.addChannel(quint32(h * 3 + 2));
        mode->insertHead(-1, hd);
    }
    QLCPhysical ph;
    ph.setWidth(pix * 17);
    ph.setHeight(12);
    ph.setDepth(5);
    ph.setLayoutSize(QSize(pix, 1));
    mode->setPhysical(ph);
    def->addMode(mode);
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setFixtureDefinition(def, mode);
    f->setAddress(address);
    if (!doc->addFixture(f))
        return nullptr;
    return f;
}

/** A moving head: pan, tilt, 300 x 400 x 300 mm. */
Fixture *makeMover(Doc *doc, const char *name, quint32 address)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel(QString("Head %1").arg(name));
    def->setType(QLCFixtureDef::MovingHead);
    QLCChannel *pan = new QLCChannel();
    pan->setName("Pan"); pan->setGroup(QLCChannel::Pan);
    pan->setControlByte(QLCChannel::MSB);
    def->addChannel(pan);
    QLCChannel *tilt = new QLCChannel();
    tilt->setName("Tilt"); tilt->setGroup(QLCChannel::Tilt);
    tilt->setControlByte(QLCChannel::MSB);
    def->addChannel(tilt);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("2ch");
    QLCPhysical ph;
    ph.setWidth(300); ph.setHeight(400); ph.setDepth(300);
    ph.setFocusPanMax(540); ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertChannel(pan, 0);
    mode->insertChannel(tilt, 1);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setFixtureDefinition(def, mode);
    f->setAddress(address);
    if (!doc->addFixture(f))
        return nullptr;
    return f;
}

/** A plot 40 by 24 m in metres, 1 m a cell. RAII: a failed QVERIFY returns
 *  out of the test and must not leak the Monitor. */
struct Rig
{
    Doc *doc = nullptr;
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;
    MonitorProperties *props = nullptr;

    ~Rig()
    {
        delete mon;
        delete doc;
    }

    bool build()
    {
        doc = new Doc(nullptr);
        props = doc->monitorProperties();
        props->setGridUnits(MonitorProperties::Meters);
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        if (gv == nullptr)
            return false;
        parent.resize(1230, 760);
        gv->resize(1230, 760);
        gv->setSnapDivisions(0);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(40, 24));
        return true;
    }
    bool show()
    {
        parent.show();
        return QTest::qWaitForWindowExposed(&parent);
    }
    /** A Box as Add Box makes one, its corner at @p x, @p y metres. */
    StagePlatform *addBox(float x, float y)
    {
        StagePlatform *p = props->addPlatform();
        p->setName(QString("Box %1").arg(p->id() + 1));
        p->setWidth(0.6096f);
        p->setDepth(0.6096f);
        p->setHeight(0.4572f);
        p->setPropBox(true);
        p->setStackable(true);
        p->setOriginX(x);
        p->setOriginY(y);
        gv->updatePlatforms();
        return p;
    }
    PlatformItem *item(const StagePlatform *p) const { return gv->m_platformItems.value(p->id()); }
    /** Put fixture @p fid on @p p's deck, at its middle. */
    void standOn(quint32 fid, const StagePlatform *p, bool inside = false)
    {
        const QPointF c = p->centre();
        props->setFixturePosition(fid, 0, 0, QVector3D(float(c.x() * 1000.0), float(c.y() * 1000.0), 0.0f));
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.deckPlatformId = p->id();
        rp.placement = inside ? FixtureRigProps::Inside : FixtureRigProps::OnSurface;
        props->setFixtureRigProps(fid, rp);
        if (gv->fixtureItemForId(fid) == nullptr)
            gv->addFixture(fid, QPointF(c.x() * 1000.0, c.y() * 1000.0));
        gv->updateFixture(fid);
    }
    QPoint viewOf(const QPointF &scene) const { return gv->mapFromScene(scene); }
};

/** Answer the next modal dialog of type T with @p fn (then close it). */
template <typename T>
void answerDialog(const std::function<void(T *)> &fn)
{
    QTimer *poll = new QTimer;
    poll->setInterval(10);
    QObject::connect(poll, &QTimer::timeout, poll, [poll, fn]() {
        T *dlg = qobject_cast<T *>(modalOrFixtureEditor());
        if (dlg == nullptr)
            return;
        poll->stop();
        poll->deleteLater();
        fn(dlg);
    });
    poll->start();
}

void sendMove(QWidget *w, const QPoint &at, Qt::MouseButtons buttons,
              Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent ev(QEvent::MouseMove, QPointF(at), w->mapToGlobal(QPointF(at)),
                   Qt::NoButton, buttons, mods);
    QApplication::sendEvent(w, &ev);
}

/** Drag on the plot from @p from to @p to, through a point between. */
void dragOn(QWidget *vp, const QPoint &from, const QPoint &to,
            Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QTest::mousePress(vp, Qt::LeftButton, mods, from);
    sendMove(vp, from + (to - from) / 2, Qt::LeftButton, mods);
    sendMove(vp, to, Qt::LeftButton, mods);
    QTest::mouseRelease(vp, Qt::LeftButton, mods, to);
}

/** The action named @p text in @p menu or a submenu of it, and the one after it. */
bool nextAfter(QMenu *menu, const QString &text, QString *next)
{
    const QList<QAction *> acts = menu->actions();
    for (int i = 0; i < acts.size(); ++i)
    {
        if (acts.at(i)->menu() != nullptr && nextAfter(acts.at(i)->menu(), text, next))
            return true;
        if (QString(acts.at(i)->text()).remove('&') == text)
        {
            *next = i + 1 < acts.size() ? QString(acts.at(i + 1)->text()).remove('&') : QString();
            return true;
        }
    }
    return false;
}

int lumaDiff(const QColor &a, const QColor &b)
{
    return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
}
}

void BoxFacing_Test::cleanup()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
}

/* Add Box sits next to Add Steps, on the toolbar's Add and the plot's Add
   Here; it makes a 2' x 2' x 18" box, clear topped, stackable, and opens
   its editor as one -- Shape: Box, no Top row (its top is always clear). */
void BoxFacing_Test::addBoxSitsBesideStepsWithItsDefaults()
{
    Rig rig;
    QVERIFY(rig.build());
    QString next;
    QVERIFY(rig.mon->m_addBtn != nullptr && rig.mon->m_addBtn->menu() != nullptr);
    QVERIFY(nextAfter(rig.mon->m_addBtn->menu(), "Add Steps", &next));
    QCOMPARE(next, QString("Add Box"));
    QMenu canvas;
    rig.mon->fillCanvasMenu(&canvas, QPointF(100, 100));
    QVERIFY(nextAfter(&canvas, "Steps", &next));
    QCOMPARE(next, QString("Box"));

    QString title;
    int shape = -1;
    bool topShown = true, curveShown = true;
    answerDialog<QDialog>([&](QDialog *d) {
        title = d->windowTitle();
        if (QComboBox *sc = d->findChild<QComboBox *>("platformShape"))
            shape = sc->currentIndex();
        foreach (QComboBox *c, d->findChildren<QComboBox *>())
            if (c->findData(int(StagePlatform::OpenTop)) >= 0 && c->count() == 3
                && c->itemText(1).startsWith("Clear"))
                topShown = c->isVisible();
        if (QWidget *curve = d->findChild<QWidget *>("platformFrontCurve"))
            curveShown = curve->isVisible();
        d->accept();
    });
    const int before = rig.props->platforms().size();
    rig.mon->m_pendingAddScenePos = rig.gv->projectMm(3000.0, 2000.0, 0.0);
    rig.mon->slotAddBox();
    QCOMPARE(rig.props->platforms().size(), before + 1);
    StagePlatform *box = nullptr;
    foreach (StagePlatform *p, rig.props->platforms())
        if (p->isPropBox())
            box = p;
    QVERIFY(box != nullptr);
    QVERIFY(box->name().startsWith("Box "));
    QVERIFY(qAbs(box->width() - 0.6096f) < 3e-3f);
    QVERIFY(qAbs(box->depth() - 0.6096f) < 3e-3f);
    QVERIFY(qAbs(box->height() - 0.4572f) < 3e-3f);
    QCOMPARE(box->topMaterial(), StagePlatform::ClearTop);
    QVERIFY(box->stackable());
    QVERIFY(!box->isSteps());
    QVERIFY(!box->isRotated());
    QVERIFY(qAbs(box->originX() - 3.0f) < 0.01f && qAbs(box->originY() - 2.0f) < 0.01f);
    QVERIFY(rig.item(box) != nullptr);
    QVERIFY2(title.contains("Box"), qPrintable(title));
    QCOMPARE(shape, 2);
    QVERIFY(!topShown);
    QVERIFY(!curveShown);

    // Its editor's Shape: Platform makes it a riser again (the top stays clear).
    answerDialog<QDialog>([](QDialog *d) {
        d->findChild<QComboBox *>("platformShape")->setCurrentIndex(0);
        d->accept();
    });
    rig.mon->slotEditPlatform(box->id());
    QVERIFY(!box->isPropBox());
    QCOMPARE(box->topMaterial(), StagePlatform::ClearTop);
    answerDialog<QDialog>([](QDialog *d) {
        d->findChild<QComboBox *>("platformShape")->setCurrentIndex(2);
        d->accept();
    });
    rig.mon->slotEditPlatform(box->id());
    QVERIFY(box->isPropBox());
}

/* "It can sit on a platform": dragged onto a riser it is not held off it
   (a plain solid platform is), and stands on its deck; locked, it does not
   drag; duplicated, the copy is a Box too, and Ctrl+Z takes the copy away. */
void BoxFacing_Test::aBoxSitsOnARiserAndDuplicatesAsABox()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *riser = rig.props->addPlatform();
    riser->setOriginX(2.0f); riser->setOriginY(2.0f);
    riser->setWidth(4.0f); riser->setDepth(3.0f); riser->setHeight(0.5f);
    StagePlatform *box = rig.addBox(8.0f, 2.5f);
    StagePlatform *plain = rig.props->addPlatform();
    plain->setOriginX(8.0f); plain->setOriginY(8.0f);
    plain->setWidth(0.6f); plain->setDepth(0.6f); plain->setHeight(0.45f);
    rig.gv->updatePlatforms();

    const QPointF onRiser = rig.gv->projectMm(3500.0, 3000.0, 0.0);
    PlatformItem *bi = rig.item(box);
    QVERIFY(bi != nullptr);
    QCOMPARE(bi->itemChange(QGraphicsItem::ItemPositionChange, onRiser).toPointF(), onRiser);
    PlatformItem *pi = rig.item(plain);
    QVERIFY(pi->itemChange(QGraphicsItem::ItemPositionChange, onRiser).toPointF() != onRiser);

    box->setOriginX(2.4f); box->setOriginY(2.4f);
    rig.gv->updatePlatforms();
    QCOMPARE(rig.props->platformBaseZ(box->id()), 0.5f);

    // Locked: held still.
    box->setLocked(true);
    rig.gv->updatePlatforms();
    QVERIFY(!(rig.item(box)->flags() & QGraphicsItem::ItemIsMovable));
    box->setLocked(false);
    rig.gv->updatePlatforms();
    QVERIFY(rig.item(box)->flags() & QGraphicsItem::ItemIsMovable);

    // Duplicated: a Box, the same size, clear on top.
    rig.gv->scene()->clearSelection();
    rig.item(box)->setSelected(true);
    const int before = rig.props->platforms().size();
    QVERIFY(rig.mon->duplicateSelected());
    QCOMPARE(rig.props->platforms().size(), before + 1);
    StagePlatform *copy = nullptr;
    foreach (StagePlatform *p, rig.props->platforms())
        if (p != box && p->isPropBox())
            copy = p;
    QVERIFY(copy != nullptr);
    QCOMPARE(copy->width(), box->width());
    QCOMPARE(copy->height(), box->height());
    QCOMPARE(copy->topMaterial(), StagePlatform::ClearTop);
    QVERIFY(copy->stackable());
    rig.gv->undoLastMove();
    QCOMPARE(rig.props->platforms().size(), before);
}

/* The front -- the face downstage at turn 0 -- stays its front when the box
   turns and moves: in plan the heavy edge is the box's own downstage edge,
   turned with it (at 90 degrees it faces stage right, the plot's left); in
   the front elevation it faces the house at 0 and is a side edge at 90; in
   the side elevation (from stage right) it is the downstage end. */
void BoxFacing_Test::turningAndMovingABoxKeepsItsFront()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *box = rig.addBox(4.0f, 4.0f);
    PlatformItem *bi = rig.item(box);
    QCOMPARE(bi->boxFrontEdge(), QLineF(0, bi->m_pxD, bi->m_pxW, bi->m_pxD));
    auto frontMidScene = [&]() {
        PlatformItem *it = rig.item(box);
        return it->mapToScene(it->boxFrontEdge().center());
    };
    auto worldFrontMid = [&]() {
        const QPointF w = box->toWorld(QPointF(double(box->originX()) + double(box->width()) / 2.0,
                                               double(box->originY()) + double(box->depth())));
        return rig.gv->projectMm(w.x() * 1000.0, w.y() * 1000.0, 0.0);
    };
    QVERIFY(QLineF(frontMidScene(), worldFrontMid()).length() < 1.5);
    const QPointF centre = rig.gv->projectMm(box->centre().x() * 1000.0, box->centre().y() * 1000.0, 0.0);
    QVERIFY(frontMidScene().y() > centre.y());              // downstage: lower on the plot

    rig.props->turnPlatform(box->id(), 90.0f);
    rig.gv->updatePlatforms();
    QCOMPARE(rig.item(box)->rotation(), 90.0);
    QVERIFY2(QLineF(frontMidScene(), worldFrontMid()).length() < 1.5,
             qPrintable(QString("%1,%2").arg(frontMidScene().x()).arg(frontMidScene().y())));
    QVERIFY(frontMidScene().x() < centre.x() - 5);          // stage right: the plot's left

    box->setOriginX(6.0f);
    rig.gv->updatePlatforms();
    QVERIFY(QLineF(frontMidScene(), worldFrontMid()).length() < 1.5);

    // Elevations: Front at 90 sees it edge on, on the left (stage right) ...
    rig.gv->setViewPOV(MonitorGraphicsView::PovFront);
    QVERIFY(!rig.item(box)->boxFrontFacesViewer());
    QCOMPARE(rig.item(box)->boxFrontEdge().x1(), 0.0);
    // ... and at 0 face on, the word FRONT on it.
    rig.props->turnPlatform(box->id(), 0.0f);
    rig.gv->updatePlatforms();
    QVERIFY(rig.item(box)->boxFrontFacesViewer());
    QVERIFY(rig.item(box)->boxFrontEdge().isNull());
    // Side, from stage right: the downstage end, on the right.
    rig.gv->setViewPOV(MonitorGraphicsView::PovSide);
    QVERIFY(!rig.item(box)->boxFrontFacesViewer());
    QCOMPARE(rig.item(box)->boxFrontEdge().x1(), double(rig.item(box)->m_pxW));
    rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
}

/* A WLED string stood on the box with Pixel path "Across a box's front"
   draws its LEDs along the front on the plot, and goes with it when it
   turns; the path is offered only on a box, and Turns reads Rows there. */
void BoxFacing_Test::aStripOnTheFrontLaysItsLedsAcrossIt()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *box = rig.addBox(4.0f, 4.0f);
    Fixture *strip = makeLedString(rig.doc, 20, "Front");
    QVERIFY(strip != nullptr);
    rig.standOn(strip->id(), box);

    QWidget host;
    QFormLayout *form = new QFormLayout(&host);
    FixtureFields *fields = new FixtureFields(rig.mon, form, &host);
    auto dropFields = qScopeGuard([fields]() { delete fields; });
    fields->setFixtures({ strip->id() }, strip->id());
    auto *model = qobject_cast<QStandardItemModel *>(fields->m_pixelPath->model());
    QVERIFY(model != nullptr);
    const int boxRow = fields->m_pixelPath->findData(int(PixelPath::BoxFront));
    QVERIFY(boxRow >= 0);
    QVERIFY(model->item(boxRow)->isEnabled());
    QCOMPARE(fields->m_pixelPath->itemText(boxRow), QString("Across a box's front"));
    fields->m_pixelPath->setCurrentIndex(boxRow);
    QCOMPARE(rig.props->fixtureRigProps(strip->id()).pixelPath, int(PixelPath::BoxFront));
    fields->reload();
    QLabel *rowsLabel = qobject_cast<QLabel *>(form->labelForField(fields->m_pixelTurns));
    QVERIFY(rowsLabel != nullptr);
    QCOMPARE(rowsLabel->text(), QString("Rows:"));
    QVERIFY(!fields->m_pixelTurns->isHidden());
    fields->m_pixelTurns->setValue(2.0);
    QCOMPARE(rig.props->fixtureRigProps(strip->id()).pixelTurns, 2.0f);

    rig.gv->updateFixture(strip->id());
    MonitorFixtureItem *it = rig.gv->fixtureItemForId(strip->id());
    QCOMPARE(it->pathPixels().size(), 20);
    const double frontY = rig.gv->projectMm(0, (double(box->originY() + box->depth()) + 0.005) * 1000.0, 0).y();
    const double left = rig.gv->projectMm(double(box->originX()) * 1000.0, 0, 0).x();
    const double right = rig.gv->projectMm(double(box->originX() + box->width()) * 1000.0, 0, 0).x();
    foreach (const QPointF &px, it->pathPixels())
    {
        const QPointF s = it->mapToScene(px);
        QVERIFY2(qAbs(s.y() - frontY) < 1.5, qPrintable(QString::number(s.y() - frontY)));
        QVERIFY(s.x() > left && s.x() < right);
    }

    // Turned a quarter: the LEDs along the plot's left side of it.
    rig.props->turnPlatform(box->id(), 90.0f);
    rig.gv->updatePlatforms();
    rig.gv->refreshRiserFixtures();
    it = rig.gv->fixtureItemForId(strip->id());
    const double xs = it->mapToScene(it->pathPixels().first()).x();
    const QPointF c = rig.gv->projectMm(box->centre().x() * 1000.0, box->centre().y() * 1000.0, 0.0);
    QVERIFY(xs < c.x() - 5);
    foreach (const QPointF &px, it->pathPixels())
        QVERIFY(qAbs(it->mapToScene(px).x() - xs) < 1.5);

    // Off the box, the path is not offered (unless it is the one it has).
    FixtureRigProps rp = rig.props->fixtureRigProps(strip->id());
    rp.deckPlatformId = FixtureRigProps::invalidPlatformId();
    rp.pixelPath = PixelPath::None;
    rig.props->setFixtureRigProps(strip->id(), rp);
    rig.props->setFixturePosition(strip->id(), 0, 0, QVector3D(20000, 15000, 0));
    fields->reload();
    QVERIFY(!model->item(boxRow)->isEnabled());
}

/* The top is clear: from above, a box is a faint tint over what is in it --
   far less than a riser's deck -- with a fixture inside drawn and lit. */
void BoxFacing_Test::theTopIsClear()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    rig.gv->setGridSize(QSize(12, 8));                 // close enough to see in
    StagePlatform *box = rig.addBox(4.0f, 4.0f);
    StagePlatform *deck = rig.props->addPlatform();
    deck->setOriginX(8.0f); deck->setOriginY(4.0f);
    deck->setWidth(box->width()); deck->setDepth(box->depth()); deck->setHeight(box->height());
    rig.gv->updatePlatforms();

    // Each drawn alone over white: the box's top barely tints it.
    auto tintOf = [&](StagePlatform *p) {
        PlatformItem *pi = rig.item(p);
        const QRectF br = pi->boundingRect();
        QImage img(br.size().toSize() + QSize(2, 2), QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.translate(-br.topLeft());
        QStyleOptionGraphicsItem opt;
        pi->paint(&painter, &opt, nullptr);
        painter.end();
        const QPoint at((pi->geometryRect().width() * 0.7 - br.left()), (pi->geometryRect().height() * 0.4 - br.top()));
        return lumaDiff(img.pixelColor(at), Qt::white);
    };
    const int boxTint = tintOf(box), deckTint = tintOf(deck);
    QVERIFY2(boxTint * 2 < deckTint, qPrintable(QString("box %1, deck %2").arg(boxTint).arg(deckTint)));

    // A light inside it is drawn over it on the plot, not hidden under it.
    Fixture *par = makeLedString(rig.doc, 1, "Inside", 100);
    QVERIFY(par != nullptr);
    rig.standOn(par->id(), box, true);
    MonitorFixtureItem *it = rig.gv->fixtureItemForId(par->id());
    QVERIFY(it != nullptr && it->isVisible());
    QVERIFY(it->zValue() > rig.item(box)->zValue());
    QVERIFY(!it->isObscured());
    const QRectF area = rig.item(box)->sceneBoundingRect();
    auto render = [&]() {
        QImage img(area.size().toSize() + QSize(1, 1), QImage::Format_ARGB32);
        img.fill(Qt::black);
        QPainter painter(&img);
        rig.gv->scene()->render(&painter, QRectF(QPointF(0, 0), area.size()), area);
        return img;
    };
    const QImage with = render();
    it->setVisible(false);
    const QImage without = render();
    it->setVisible(true);
    int changed = 0;
    for (int y = 0; y < with.height(); ++y)
        for (int x = 0; x < with.width(); ++x)
            if (lumaDiff(with.pixelColor(x, y), without.pixelColor(x, y)) > 60)
                ++changed;
    QVERIFY2(changed > 20, qPrintable(QString::number(changed)));
    // Not buried: a clear top is why it is rigged in there.
    QVERIFY(!it->isBuried());
}

/* A label is one line each for the name and the size, never a letter a
   line: what does not fit goes (the size, then the name is elided, then the
   label). A Box's label stands above it, clear of the FRONT on its face. */
void BoxFacing_Test::labelsNeverWrapAndNeverCoverTheFront()
{
    Rig rig;
    QVERIFY(rig.build());
    StagePlatform *riser = rig.props->addPlatform();
    riser->setName("Riser 1");
    riser->setOriginX(2.0f); riser->setOriginY(2.0f);
    riser->setWidth(2.44f); riser->setDepth(1.22f); riser->setHeight(0.4f);
    StagePlatform *steps = rig.props->addPlatform();
    steps->setName("Steps 2");
    steps->setSteps(2, 0.2032f, 0.2032f, 0.9144f, StagePlatform::ClimbUpstage);
    steps->setOriginX(6.0f); steps->setOriginY(2.0f);
    StagePlatform *box = rig.addBox(9.0f, 2.0f);
    const QFontMetricsF fm(QFont("Arial", 10, QFont::Bold));

    auto check = [&](const char *when) {
        for (StagePlatform *p : { riser, steps, box })
        {
            PlatformItem *pi = rig.item(p);
            QVERIFY(pi != nullptr);
            const QString text = pi->labelText();
            const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
            QVERIFY2(lines.size() <= 2, qPrintable(QString("%1 %2: %3").arg(when, p->name(), text)));
            if (text.isEmpty())
                continue;
            // As many lines drawn as written: nothing wrapped.
            const qreal h = pi->labelItem()->boundingRect().height();
            QVERIFY2(h < fm.height() * (lines.size() + 1),
                     qPrintable(QString("%1 %2: %3 high for %4 line(s)").arg(when, p->name()).arg(h).arg(lines.size())));
            const QRectF lr = pi->labelItem()->mapRectToParent(pi->labelItem()->boundingRect());
            if (p->isPropBox())
                QVERIFY2(lr.bottom() <= 0.5, qPrintable(QString("%1 box label at %2").arg(when).arg(lr.bottom())));
            else
                QVERIFY2(lr.width() <= pi->geometryRect().width() + 1.0,
                         qPrintable(QString("%1 %2: %3 wide in %4").arg(when, p->name()).arg(lr.width())
                                        .arg(pi->geometryRect().width())));
        }
    };
    // Far out: a narrow flight of steps has room for little or nothing.
    rig.gv->setGridSize(QSize(80, 48));
    rig.gv->updatePlatforms();
    check("far");
    // Close: the riser says its name and size.
    rig.gv->setGridSize(QSize(14, 8));
    rig.gv->updatePlatforms();
    check("close");
    QVERIFY(rig.item(riser)->labelText().contains("Riser 1\n("));
    // Every view.
    for (auto pov : { MonitorGraphicsView::PovFront, MonitorGraphicsView::PovSide })
    {
        rig.gv->setViewPOV(pov);
        rig.gv->updatePlatforms();
        check(pov == MonitorGraphicsView::PovFront ? "front" : "side");
    }
}

/* At pan zero a head throws where its arrow points -- 90 is stage right,
   the plot's left, as AimSolver::beamDirection() has it. The arrow used to
   be drawn the mirror way round. */
void BoxFacing_Test::theArrowPointsWhereTheBeamGoes()
{
    Rig rig;
    QVERIFY(rig.build());
    Fixture *head = makeMover(rig.doc, "A", 0);
    QVERIFY(head != nullptr);
    rig.props->setFixturePosition(head->id(), 0, 0, QVector3D(5000, 3000, 6.0f));
    rig.gv->addFixture(head->id(), QPointF(5000, 3000));
    MonitorFixtureItem *it = rig.gv->fixtureItemForId(head->id());
    QVERIFY(it != nullptr && it->m_hasPan);
    const QPointF c = it->mapToScene(QPointF(it->m_width / 2.0, it->m_height / 2.0));
    const double perMetreX = rig.gv->projectMm(1000, 0, 0).x() - rig.gv->projectMm(0, 0, 0).x();
    const double perMetreY = rig.gv->projectMm(0, 1000, 0).y() - rig.gv->projectMm(0, 0, 0).y();
    for (float f : { 0.0f, 90.0f, 180.0f, 270.0f, 30.0f })
    {
        FixtureRigProps rp = rig.props->fixtureRigProps(head->id());
        rp.panZeroDir = f;
        it->setFacing(f);
        QVector3D beam;
        QVERIFY(AimSolver::beamDirection(rp, 0.0, AimSolver::floorMounted(rp) ? -45.0 : 45.0, beam));
        const QPointF arrow = it->mapToScene(it->facingTip()) - c;
        const QPointF expect(double(beam.x()) * perMetreX, double(beam.y()) * perMetreY);
        const double cosAngle = (arrow.x() * expect.x() + arrow.y() * expect.y())
                                / (QLineF(QPointF(), arrow).length() * QLineF(QPointF(), expect).length());
        QVERIFY2(cosAngle > 0.999, qPrintable(QString("facing %1: arrow %2,%3 beam %4,%5")
                                              .arg(f).arg(arrow.x()).arg(arrow.y())
                                              .arg(expect.x()).arg(expect.y())));
    }
}

/* Selected, its arrow's head drags round with no modifier: in 15-degree
   steps, freely with Shift; the angle is shown while it turns; one Ctrl+Z
   puts the whole drag back. Alt-drag on the body still turns it; a plain
   drag on the body moves it and does not turn it. */
void BoxFacing_Test::draggingTheArrowHeadTurnsTheFacingInOneUndoStep()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    Fixture *head = makeMover(rig.doc, "A", 0);
    QVERIFY(head != nullptr);
    rig.props->setFixturePosition(head->id(), 0, 0, QVector3D(8000, 6000, 6.0f));
    rig.gv->addFixture(head->id(), QPointF(8000, 6000));
    MonitorFixtureItem *it = rig.gv->fixtureItemForId(head->id());
    QVERIFY(it != nullptr);
    it->setSelected(true);
    QVERIFY(it->facingHandleShown());
    QVERIFY(!rig.gv->canUndo());
    QWidget *vp = rig.gv->viewport();
    const QPointF cItem(it->m_width / 2.0, it->m_height / 2.0);
    auto centreView = [&]() { return rig.viewOf(it->mapToScene(cItem)); };
    auto headView = [&]() {
        const QPointF tip = it->facingTip();
        return rig.viewOf(it->mapToScene(tip + (cItem - tip) * (3.5 / QLineF(cItem, tip).length())));
    };
    auto facing = [&]() { return rig.props->fixtureRigProps(head->id()).panZeroDir; };
    const QPointF posBefore = it->realPosition();

    // To the plot's left: stage right, 90.
    QPoint from = headView();
    QPoint to = centreView() + QPoint(-90, 0);
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, from);
    sendMove(vp, from + (to - from) / 2, Qt::LeftButton);
    sendMove(vp, to, Qt::LeftButton);
    QVERIFY(it->isTurningFacing());                  // the readout is drawn
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, to);
    QVERIFY(!it->isTurningFacing());
    QCOMPARE(facing(), 90.0f);
    QCOMPARE(it->facing(), 90.0f);
    QCOMPARE(it->realPosition(), posBefore);         // turned, not moved
    QCOMPARE(rig.gv->m_moveUndo.size(), 1);

    // 37 degrees from downstage: 30 in steps, 37 with Shift.
    const double a = qDegreesToRadians(37.0);
    const QPoint at37 = centreView() + QPoint(qRound(-qSin(a) * 120.0), qRound(qCos(a) * 120.0));
    dragOn(vp, headView(), at37);
    QCOMPARE(facing(), 30.0f);
    dragOn(vp, headView(), at37, Qt::ShiftModifier);
    QVERIFY2(qAbs(facing() - 37.0f) < 1.0f, qPrintable(QString::number(facing())));
    QCOMPARE(rig.gv->m_moveUndo.size(), 3);

    // One Ctrl+Z a drag.
    rig.gv->undoLastMove();
    QCOMPARE(rig.props->fixtureRigProps(head->id()).panZeroDir, 30.0f);
    rig.gv->undoLastMove();
    QCOMPARE(rig.props->fixtureRigProps(head->id()).panZeroDir, 90.0f);
    rig.gv->undoLastMove();
    QCOMPARE(rig.props->fixtureRigProps(head->id()).panZeroDir, 0.0f);
    QVERIFY(!rig.gv->canUndo());

    // Pressed on the head and let go without turning: nothing to undo.
    it = rig.gv->fixtureItemForId(head->id());
    it->setSelected(true);
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, headView());
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, headView());
    QVERIFY(!rig.gv->canUndo());

    // Alt-drag on the body still turns it (upstage: 180).
    it = rig.gv->fixtureItemForId(head->id());
    dragOn(vp, centreView(), centreView() + QPoint(0, -100), Qt::AltModifier);
    QCOMPARE(facing(), 180.0f);
    QCOMPARE(rig.gv->m_moveUndo.size(), 1);

    // A plain drag on the body moves it and does not turn it.
    it = rig.gv->fixtureItemForId(head->id());
    it->setSelected(true);
    const QPointF was = it->realPosition();
    dragOn(vp, centreView(), centreView() + QPoint(60, 40));
    it = rig.gv->fixtureItemForId(head->id());
    QVERIFY(it->realPosition() != was);
    QCOMPARE(facing(), 180.0f);
}

/* The handle is the head only -- never the body -- and only while it can
   turn: selected, the plot and the fixture unlocked. It has its own
   cursor. With two selected, dragging one's head turns that one. */
void BoxFacing_Test::theHandleIsOnlyTheHeadAndOnlyWhenItCanTurn()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    Fixture *a = makeMover(rig.doc, "A", 0);
    Fixture *b = makeMover(rig.doc, "B", 10);
    QVERIFY(a != nullptr && b != nullptr);
    rig.props->setFixturePosition(a->id(), 0, 0, QVector3D(8000, 6000, 6.0f));
    rig.props->setFixturePosition(b->id(), 0, 0, QVector3D(12000, 6000, 6.0f));
    rig.gv->addFixture(a->id(), QPointF(8000, 6000));
    rig.gv->addFixture(b->id(), QPointF(12000, 6000));
    MonitorFixtureItem *ia = rig.gv->fixtureItemForId(a->id());
    MonitorFixtureItem *ib = rig.gv->fixtureItemForId(b->id());
    const QPointF cItem(ia->m_width / 2.0, ia->m_height / 2.0);
    const QPointF tip = ia->facingTip();
    const QPointF onHead = tip + (cItem - tip) * (3.5 / QLineF(cItem, tip).length());

    QVERIFY(!ia->onFacingHandle(onHead));             // not selected
    ia->setSelected(true);
    QVERIFY(ia->onFacingHandle(onHead));
    QVERIFY(!ia->onFacingHandle(cItem));              // the body moves it
    QVERIFY(!ia->onFacingHandle(QPointF(0.5, 0.5)));
    QVERIFY(ia->shape().contains(onHead));

    // Over the head: its own cursor.
    const QPoint headView = rig.viewOf(ia->mapToScene(onHead));
    sendMove(rig.gv->viewport(), headView + QPoint(1, 0), Qt::NoButton);
    sendMove(rig.gv->viewport(), headView, Qt::NoButton);
    QVERIFY(ia->m_overHandle);
    QCOMPARE(ia->cursor().shape(), Qt::PointingHandCursor);

    // Two selected: only the one dragged turns.
    ib->setSelected(true);
    dragOn(rig.gv->viewport(), headView, rig.viewOf(ia->mapToScene(cItem)) + QPoint(-90, 0));
    QCOMPARE(rig.props->fixtureRigProps(a->id()).panZeroDir, 90.0f);
    QCOMPARE(rig.props->fixtureRigProps(b->id()).panZeroDir, 0.0f);

    // The plot locked: no handle, and a drag there turns nothing.
    ia = rig.gv->fixtureItemForId(a->id());
    ia->setSelected(true);
    rig.gv->setLayoutLocked(true);
    QVERIFY(!ia->facingHandleShown());
    const QPointF tip2 = ia->facingTip();
    const QPointF head2 = tip2 + (cItem - tip2) * (3.5 / QLineF(cItem, tip2).length());
    QVERIFY(!ia->onFacingHandle(head2));
    dragOn(rig.gv->viewport(), rig.viewOf(ia->mapToScene(head2)),
           rig.viewOf(ia->mapToScene(cItem)) + QPoint(0, -90));
    QCOMPARE(rig.props->fixtureRigProps(a->id()).panZeroDir, 90.0f);
    rig.gv->setLayoutLocked(false);
}

/* Fixture Properties' orientation diagram: its green arrow's head drags
   round (stage right on the left, as the plot), the Aim field follows live
   and sets the rig; typing in the field turns the arrow. With several
   fixtures, the dial beside Aim sets them all. */
void BoxFacing_Test::theDialInFixturePropertiesDragsAndFollowsTheField()
{
    Rig rig;
    QVERIFY(rig.build());
    Fixture *a = makeMover(rig.doc, "A", 0);
    Fixture *b = makeMover(rig.doc, "B", 10);
    QVERIFY(a != nullptr && b != nullptr);
    rig.props->setFixturePosition(a->id(), 0, 0, QVector3D(8000, 6000, 6.0f));
    rig.props->setFixturePosition(b->id(), 0, 0, QVector3D(12000, 6000, 6.0f));
    rig.gv->addFixture(a->id(), QPointF(8000, 6000));
    rig.gv->addFixture(b->id(), QPointF(12000, 6000));

    bool turnable = false;
    double afterDrag = -1.0, afterShift = -1.0, liveRig = -1.0;
    QPointF tipAfterTyping;
    answerDialog<QDialog>([&](QDialog *d) {
        QWidget *dial = d->findChild<QWidget *>("fixtureFacingPreview");
        QDoubleSpinBox *aim = d->findChild<QDoubleSpinBox *>("fixtureAim");
        if (dial == nullptr || aim == nullptr)
        {
            d->reject();
            return;
        }
        turnable = dial->property("turnable").toBool();
        const QPointF c = QRectF(dial->rect()).center();
        QPointF tip = dial->property("greenTip").toPointF();
        QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, (c + tip).toPoint());
        sendMove(dial, (c + QPointF(-30, 2)).toPoint(), Qt::LeftButton);
        sendMove(dial, (c + QPointF(-40, 0)).toPoint(), Qt::LeftButton);
        QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, (c + QPointF(-40, 0)).toPoint());
        afterDrag = aim->value();
        liveRig = rig.props->fixtureRigProps(a->id()).panZeroDir;
        // 37 degrees, freely with Shift.
        tip = dial->property("greenTip").toPointF();
        const double r = qDegreesToRadians(37.0);
        const QPoint at37 = (c + QPointF(-qSin(r) * 45.0, qCos(r) * 45.0)).toPoint();
        QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, (c + tip).toPoint());
        sendMove(dial, at37, Qt::LeftButton, Qt::ShiftModifier);
        QTest::mouseRelease(dial, Qt::LeftButton, Qt::ShiftModifier, at37);
        afterShift = aim->value();
        // Typed: the arrow follows -- 180 is upstage, straight up.
        aim->setValue(180.0);
        tipAfterTyping = dial->property("greenTip").toPointF();
        d->accept();
    });
    rig.mon->editFixtureProperties(a->id());
    QVERIFY(turnable);
    QCOMPARE(afterDrag, 90.0);
    QCOMPARE(liveRig, 90.0);
    QVERIFY2(qAbs(afterShift - 37.0) < 2.0, qPrintable(QString::number(afterShift)));
    QVERIFY2(qAbs(tipAfterTyping.x()) < 0.5 && tipAfterTyping.y() < -30.0,
             qPrintable(QString("%1,%2").arg(tipAfterTyping.x()).arg(tipAfterTyping.y())));
    QCOMPARE(rig.props->fixtureRigProps(a->id()).panZeroDir, 180.0f);
    QCOMPARE(rig.gv->fixtureItemForId(a->id())->facing(), 180.0f);
    QCOMPARE(rig.gv->m_moveUndo.size(), 1);          // the dialog: one step
    rig.gv->undoLastMove();
    QCOMPARE(rig.props->fixtureRigProps(a->id()).panZeroDir, 0.0f);

    // Several: the dial sets them all (270, stage left, the plot's right).
    answerDialog<QDialog>([&](QDialog *d) {
        QWidget *dial = d->findChild<QWidget *>("fixtureFacingPreview");
        if (dial == nullptr)
        {
            d->reject();
            return;
        }
        const QPointF c = QRectF(dial->rect()).center();
        const QPointF tip = dial->property("greenTip").toPointF();
        QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, (c + tip).toPoint());
        sendMove(dial, (c + QPointF(40, 0)).toPoint(), Qt::LeftButton);
        QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, (c + QPointF(40, 0)).toPoint());
        d->accept();
    });
    rig.mon->editFixturesProperties({ a->id(), b->id() });
    QCOMPARE(rig.props->fixtureRigProps(a->id()).panZeroDir, 270.0f);
    QCOMPARE(rig.props->fixtureRigProps(b->id()).panZeroDir, 270.0f);
}

/* Shots for review (QLC_SHOT_DIR): the plot with a riser, steps and a box
   side by side -- a light inside the box, a lit strip on its front -- from
   above and from the side, and the box's own editor; Fixture Properties
   with the orientation dial, and a selected head with its arrow's handle;
   each at three sizes. */
void BoxFacing_Test::picturesForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    Rig rig;
    QVERIFY(rig.build());
    rig.props->setGridUnits(MonitorProperties::Feet);
    QVERIFY(rig.show());

    StagePlatform *riser = rig.props->addPlatform();
    riser->setName("Riser 1");
    riser->setOriginX(0.6f); riser->setOriginY(0.6f);
    riser->setWidth(2.44f); riser->setDepth(1.22f); riser->setHeight(0.4f);
    StagePlatform *steps = rig.props->addPlatform();
    steps->setName("Steps 2");
    steps->setSteps(2, 0.2032f, 0.2032f, 0.9144f, StagePlatform::ClimbUpstage);
    steps->setOriginX(3.4f); steps->setOriginY(1.0f);
    StagePlatform *box = rig.addBox(4.8f, 1.0f);
    box->setName("Box 3");
    StagePlatform *box2 = rig.addBox(1.2f, 0.75f);           // a box on the riser, turned
    box2->setName("Box 4");
    rig.props->turnPlatform(box2->id(), 30.0f);
    rig.gv->updatePlatforms();

    Fixture *par = makeLedString(rig.doc, 1, "In the box", 200);
    rig.standOn(par->id(), box, true);
    QByteArray red(3, char(0)); red[0] = char(255); red[2] = char(60);
    par->setChannelValues(red);
    Fixture *strip = makeLedString(rig.doc, 40, "Front strip", 0);
    rig.standOn(strip->id(), box);
    FixtureRigProps rp = rig.props->fixtureRigProps(strip->id());
    rp.pixelPath = PixelPath::BoxFront;
    rp.pixelTurns = 2;
    rig.props->setFixtureRigProps(strip->id(), rp);
    QByteArray v(120, char(0));
    for (int h = 0; h < 40; ++h)
    {
        const QColor c = QColor::fromHsv((h * 360 / 40) % 360, 255, 255);
        v[h * 3] = char(c.red()); v[h * 3 + 1] = char(c.green()); v[h * 3 + 2] = char(c.blue());
    }
    strip->setChannelValues(v);
    Fixture *head = makeMover(rig.doc, "Spot 1", 300);
    rig.props->setFixturePosition(head->id(), 0, 0, QVector3D(2500, 3200, 6.0f));
    rig.gv->addFixture(head->id(), QPointF(2500, 3200));
    FixtureRigProps hp = rig.props->fixtureRigProps(head->id());
    hp.panZeroDir = 60.0f;
    rig.props->setFixtureRigProps(head->id(), hp);
    rig.gv->updateFixture(head->id());

    const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    auto settle = [&]() {
        rig.gv->setGridSize(QSize(24, 14));     // feet: the whole set piece
        rig.gv->updatePlatforms();
        rig.gv->refreshRiserFixtures();
        for (quint32 fid : { par->id(), strip->id(), head->id() })
            if (MonitorFixtureItem *it = rig.gv->fixtureItemForId(fid))
                it->slotUpdateValues();
        QApplication::processEvents();
    };
    for (const QSize &sz : sizes)
    {
        rig.parent.resize(sz);
        rig.mon->resize(sz);
        const QString tag = QString("%1x%2").arg(sz.width()).arg(sz.height());
        rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
        rig.gv->scene()->clearSelection();
        settle();
        rig.parent.grab().save(QString("%1/after-box-plot-top-%2.png").arg(dir, tag));
        rig.gv->setViewPOV(MonitorGraphicsView::PovSide);
        settle();
        rig.parent.grab().save(QString("%1/after-box-plot-side-%2.png").arg(dir, tag));
        rig.gv->setViewPOV(MonitorGraphicsView::PovFront);
        settle();
        rig.parent.grab().save(QString("%1/after-box-plot-front-%2.png").arg(dir, tag));
        rig.gv->setViewPOV(MonitorGraphicsView::PovTop);
        settle();
        if (MonitorFixtureItem *it = rig.gv->fixtureItemForId(head->id()))
            it->setSelected(true);
        QApplication::processEvents();
        rig.parent.grab().save(QString("%1/after-facing-handle-plot-%2.png").arg(dir, tag));
        rig.gv->scene()->clearSelection();

        // The box's own editor, its studio pane from the side.
        answerDialog<QDialog>([&](QDialog *d) {
            d->resize(sz);
            if (StructureStudioView *sv = d->findChild<StructureStudioView *>())
                sv->setPlane(StructureStudioView::Side);
            QApplication::processEvents();
            d->grab().save(QString("%1/after-box-editor-side-%2.png").arg(dir, tag));
            if (StructureStudioView *sv = d->findChild<StructureStudioView *>())
                sv->setPlane(StructureStudioView::Top);
            QApplication::processEvents();
            d->grab().save(QString("%1/after-box-editor-top-%2.png").arg(dir, tag));
            d->reject();
        });
        rig.mon->slotEditPlatform(box->id());

        // Fixture Properties, the dial mid-turn.
        answerDialog<QDialog>([&](QDialog *d) {
            d->resize(sz.width() * 2 / 3, sz.height() - 40);
            QApplication::processEvents();
            if (QWidget *dial = d->findChild<QWidget *>("fixtureFacingPreview"))
            {
                const QPointF c = QRectF(dial->rect()).center();
                const QPointF tip = dial->property("greenTip").toPointF();
                QTest::mousePress(dial, Qt::LeftButton, Qt::NoModifier, (c + tip).toPoint());
                sendMove(dial, (c + QPointF(-30, 30)).toPoint(), Qt::LeftButton);
                QApplication::processEvents();
                d->grab().save(QString("%1/after-facing-properties-%2.png").arg(dir, tag));
                QTest::mouseRelease(dial, Qt::LeftButton, Qt::NoModifier, (c + QPointF(-30, 30)).toPoint());
            }
            d->reject();
        });
        rig.mon->editFixtureProperties(head->id());
    }
}

QTEST_MAIN(BoxFacing_Test)
