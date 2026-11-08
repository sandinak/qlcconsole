/*
  Q Light Controller Plus
  freestring_test.cpp

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
#include <QApplication>
#include <QFormLayout>
#include <QPushButton>
#include <QScopeGuard>
#include <QMouseEvent>
#include <QHostAddress>
#include <QJsonArray>
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QDialog>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QDir>
#include <QtTest>
#include "../common/fakewled.h"
#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "fixturefields.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "stringanchorsdialog.h"
#include "freestringhandles.h"
#undef private
#undef protected
#include "monitorproperties.h"
#include "measurementunits.h"
#include "inputoutputmap.h"
#include "wleddevicewrite.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "contextmenus.h"
#include "qlcchannel.h"
#include "pixelpath.h"
#include "fixture.h"
#include "doc.h"

#include "freestring_test.h"

namespace
{
/** A string of @p pix RGB LEDs, one head each. */
Fixture *makeLedString(Doc *doc, int pix, const char *name)
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
    f->setAddress(0);
    if (!doc->addFixture(f))
        return nullptr;
    return f;
}

StringAnchor anchorAt(float x, float y, float z, int pin = 0, bool rev = false)
{
    StringAnchor a;
    a.x = x; a.y = y; a.z = z; a.pin = pin; a.reverse = rev;
    return a;
}

/** A plot 40 by 24 m at 30 px a metre, a 60-LED string on it at 5 m, 3 m,
 *  1 m up, and the inspector's rows for it. RAII: a failed QVERIFY returns
 *  out of the test and must not leak the Monitor. */
struct Rig
{
    Doc *doc = nullptr;
    QWidget parent;
    Monitor *mon = nullptr;
    MonitorGraphicsView *gv = nullptr;
    Fixture *string = nullptr;
    QWidget host;
    FixtureFields *fields = nullptr;

    ~Rig()
    {
        delete fields;
        delete mon;
        delete doc;
    }

    bool build(int leds = 60)
    {
        doc = new Doc(nullptr);
        MonitorProperties *props = doc->monitorProperties();
        props->setGridUnits(MonitorProperties::Meters);
        string = makeLedString(doc, leds, "Garland");
        if (string == nullptr)
            return false;
        props->setFixturePosition(string->id(), 0, 0, QVector3D(5000.0f, 3000.0f, 1000.0f));
        mon = new Monitor(&parent, doc);
        gv = mon->findChild<MonitorGraphicsView *>();
        if (gv == nullptr)
            return false;
        parent.resize(1230, 760);
        gv->resize(1230, 760);
        gv->setSnapDivisions(0);
        gv->setGridMetrics(1000.0);
        gv->setGridSize(QSize(40, 24));
        if (gv->fixtureItemForId(string->id()) == nullptr)
            gv->addFixture(string->id(), QPointF(5000, 3000));
        QFormLayout *form = new QFormLayout(&host);
        fields = new FixtureFields(mon, form, &host);
        fields->setFixtures({ string->id() }, string->id());
        return gv->fixtureItemForId(string->id()) != nullptr;
    }

    FixtureRigProps rp() const { return doc->monitorProperties()->fixtureRigProps(string->id()); }
    void setAnchors(const QVector<StringAnchor> &a)
    {
        FixtureRigProps r = rp();
        r.pixelPath = PixelPath::FreeString;
        r.stringAnchors = a;
        doc->monitorProperties()->setFixtureRigProps(string->id(), r);
        gv->updateFixture(string->id());
        fields->reload();
    }
    QPoint viewAt(float x, float y, float z = 0.0f) const
    {
        return gv->mapFromScene(gv->projectMm(x * 1000.0, y * 1000.0, z * 1000.0));
    }
    bool show()
    {
        parent.show();
        return QTest::qWaitForWindowExposed(&parent);
    }
};

/** Answer the next modal dialog of type T with @p fn (then close it). */
template <typename T>
void answerDialog(const std::function<void(T *)> &fn)
{
    QTimer *poll = new QTimer;
    poll->setInterval(10);
    QObject::connect(poll, &QTimer::timeout, poll, [poll, fn]() {
        T *dlg = qobject_cast<T *>(QApplication::activeModalWidget());
        if (dlg == nullptr)
            return;
        poll->stop();
        poll->deleteLater();
        fn(dlg);
    });
    poll->start();
}

void sendMove(QWidget *w, const QPoint &at, Qt::MouseButtons buttons)
{
    QMouseEvent ev(QEvent::MouseMove, QPointF(at), w->mapToGlobal(QPointF(at)),
                   Qt::NoButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}
}

void FreeString_Test::cleanup()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
}

/* A student picks it from the Pixel path list with no fountain anywhere,
   sees something at once (two anchors' worth in front of the fixture) and
   is told in one line what to do next. */
void FreeString_Test::theFreeStringIsOfferedWithoutAFountainAndSaysWhatToDo()
{
    Rig rig;
    QVERIFY(rig.build());
    FixtureFields &f = *rig.fields;
    QVERIFY(!f.m_pixelPath->isHidden());
    QVERIFY(f.m_strBox->isHidden());
    const int idx = f.m_pixelPath->findData(int(PixelPath::FreeString));
    QVERIFY(idx >= 0);
    QCOMPARE(f.m_pixelPath->itemText(idx), QString("Free string (anchors)"));
    auto *model = qobject_cast<QStandardItemModel *>(f.m_pixelPath->model());
    QVERIFY(model != nullptr && model->item(idx)->isEnabled());      // no fountain needed
    QVERIFY(!model->item(f.m_pixelPath->findData(int(PixelPath::SprinkleLoops)))->isEnabled());
    QSignalSpy edits(&f, &FixtureFields::aboutToEdit);
    f.m_pixelPath->setCurrentIndex(idx);
    QCOMPARE(rig.rp().pixelPath, int(PixelPath::FreeString));
    QCOMPARE(edits.count(), 1);
    QVERIFY(!f.m_strBox->isHidden() && !f.m_strNote->isHidden());
    QVERIFY(!f.m_wandStrip->isHidden());                             // String length
    QVERIFY(f.m_wandDiameter->isHidden() && f.m_sprBox->isHidden() && f.m_wandStart->isHidden());
    QCOMPARE(f.m_strCount->text(), QString("None placed"));
    QCOMPARE(f.m_strNote->text(), QString("Click Add Anchor, then click on the plan where the string goes."));
    QVERIFY(f.m_strAdd->isEnabled() && f.m_strEdit->isEnabled());
    QVERIFY(!f.m_strWrite->isEnabled());                             // not a WLED
    // Drawn at once: every LED, in front of (downstage of) the fixture, at its height.
    const QVector<QVector3D> leds = PixelPath::positions(rig.doc, rig.doc->monitorProperties(),
                                                         rig.string->id(), 60);
    QCOMPARE(leds.size(), 60);
    const QVector3D at = rig.doc->monitorProperties()->fixtureRigPosition(rig.string->id());
    QVERIFY(leds.first().y() > at.y() && qAbs(leds.first().z() - at.z()) < 1e-3f);
    // Selected on the plot: the two hollow handles of that default.
    rig.gv->fixtureItemForId(rig.string->id())->setSelected(true);
    FreeStringHandles *h = rig.gv->stringHandles();
    QVERIFY(h != nullptr);
    QCOMPARE(h->count(), 2);
    QVERIFY(!h->ownAnchors());

    // Anchors of its own: counted, and how the string lies on them, in Settings' unit.
    MeasurementUnits::setPreferredLength(MeasurementUnits::Metres);
    FixtureRigProps r = rig.rp();
    r.wandStripLength = 5.9f;          // 59 gaps of 10 cm
    rig.doc->monitorProperties()->setFixtureRigProps(rig.string->id(), r);
    rig.setAnchors({ anchorAt(2, 3, 1), anchorAt(6, 3, 1) });
    QCOMPARE(f.m_strCount->text(), QString("2 anchors"));
    QVERIFY2(f.m_strNote->text().contains(MeasurementUnits::formatLength(5900.0))
             && f.m_strNote->text().contains(MeasurementUnits::formatLength(4000.0))
             && f.m_strNote->text().contains("runs on past the last anchor"),
             qPrintable(f.m_strNote->text()));
    QVERIFY(rig.gv->stringHandles()->ownAnchors());
}

/* Add Anchor, then each click on the plan is the next anchor: the first
   starts the string, each is one undo step, Esc ends it. */
void FreeString_Test::addAnchorThenClicksOnThePlanLayTheString()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    FixtureRigProps r = rig.rp();
    r.pixelPath = PixelPath::FreeString;
    rig.doc->monitorProperties()->setFixtureRigProps(rig.string->id(), r);
    rig.fields->reload();
    QSignalSpy notices(rig.gv, &MonitorGraphicsView::notice);
    rig.fields->m_strAdd->click();
    QVERIFY(rig.gv->addingAnchors());
    QCOMPARE(rig.gv->addingAnchorsTo(), rig.string->id());
    QVERIFY(notices.count() >= 1 && notices.last().first().toString().contains("where the string starts"));

    const QList<QPointF> clicks = { { 2.0, 4.0 }, { 8.0, 4.0 }, { 8.0, 9.0 } };
    for (const QPointF &c : clicks)
        QTest::mouseClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier,
                          rig.viewAt(float(c.x()), float(c.y())));
    QVector<StringAnchor> a = rig.rp().stringAnchors;
    QCOMPARE(a.size(), 3);
    const float height = rig.doc->monitorProperties()->fixtureRigPosition(rig.string->id()).z();
    for (int i = 0; i < 3; ++i)
    {
        QVERIFY2(qAbs(a.at(i).x - float(clicks.at(i).x())) < 0.05f
                 && qAbs(a.at(i).y - float(clicks.at(i).y())) < 0.05f,
                 qPrintable(QString("%1,%2").arg(a.at(i).x).arg(a.at(i).y)));
        QVERIFY(qAbs(a.at(i).z - height) < 1e-4f);                  // the fixture's own height
    }
    QVERIFY(rig.rp().pixelPath == PixelPath::FreeString);
    QCOMPARE(rig.gv->stringHandles()->count(), 3);

    // One click, one undo step.
    QVERIFY(rig.gv->canUndo());
    rig.gv->undoLastMove();
    QCOMPARE(rig.rp().stringAnchors.size(), 2);

    // Esc: done, and a click selects again.
    QTest::keyClick(rig.gv, Qt::Key_Escape);
    QVERIFY(!rig.gv->addingAnchors());
    QTest::mouseClick(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, rig.viewAt(15, 15));
    QCOMPARE(rig.rp().stringAnchors.size(), 2);
}

/* A handle dragged on the plan moves its anchor (across and depth from
   above; its height stays) and the whole drag is one undo. */
void FreeString_Test::draggingAnAnchorMovesItInOneUndoStep()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    rig.setAnchors({ anchorAt(2, 3, 1.5f), anchorAt(6, 3, 1.5f) });
    rig.gv->fixtureItemForId(rig.string->id())->setSelected(true);
    FreeStringHandles *h = rig.gv->stringHandles();
    QVERIFY(h != nullptr);
    const QPoint from = rig.gv->mapFromScene(h->handlePos(1));
    QCOMPARE(from, rig.viewAt(6, 3, 1.5f));
    QCOMPARE(h->handleAt(h->handlePos(1)), 1);

    // 2 m across, 1 m downstage.
    const QPoint to = rig.viewAt(8, 4, 1.5f);
    const QVector3D fixtureWas = rig.doc->monitorProperties()->fixtureRigPosition(rig.string->id());
    QTest::mousePress(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, from);
    sendMove(rig.gv->viewport(), from + QPoint(20, 10), Qt::LeftButton);
    sendMove(rig.gv->viewport(), to, Qt::LeftButton);
    QTest::mouseRelease(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, to);
    const StringAnchor moved = rig.rp().stringAnchors.at(1);
    QVERIFY2(qAbs(moved.x - 8.0f) < 0.05f && qAbs(moved.y - 4.0f) < 0.05f,
             qPrintable(QString("%1,%2").arg(moved.x).arg(moved.y)));
    QVERIFY(qAbs(moved.z - 1.5f) < 1e-4f);
    QVERIFY(qAbs(rig.rp().stringAnchors.at(0).x - 2.0f) < 1e-4f);  // the other stays
    // Its LEDs went with it, and the fixture itself did not move.
    QCOMPARE(rig.doc->monitorProperties()->fixtureRigPosition(rig.string->id()), fixtureWas);

    rig.gv->undoLastMove();
    QVERIFY(qAbs(rig.rp().stringAnchors.at(1).x - 6.0f) < 1e-4f);
    QVERIFY(!rig.gv->canUndo());                                     // one step, not one a move

    // Pressed and let go without moving: nothing to undo.
    rig.gv->fixtureItemForId(rig.string->id())->setSelected(true);
    const QPoint still = rig.gv->mapFromScene(rig.gv->stringHandles()->handlePos(0));
    QTest::mousePress(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, still);
    QTest::mouseRelease(rig.gv->viewport(), Qt::LeftButton, Qt::NoModifier, still);
    QVERIFY(!rig.gv->canUndo());
}

/* Right-click on an anchor: Insert Anchor Before, Pin LED Here…, Unpin,
   Reverse This Span, Delete Anchor -- in the menu canon, one undo each. */
void FreeString_Test::theAnchorMenuPinsReversesInsertsAndDeletes()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    rig.setAnchors({ anchorAt(0, 0, 0), anchorAt(2, 0, 0), anchorAt(3, 0, 0) });
    rig.gv->fixtureItemForId(rig.string->id())->setSelected(true);
    QVERIFY(rig.gv->stringHandles() != nullptr);

    QStringList names, canon;
    bool lastReverseEnabled = true;
    QTimer::singleShot(50, [&]() {
        if (QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget()))
        {
            for (QAction *a : m->actions())
                if (!a->isSeparator())
                    names << a->objectName();
            canon = ContextMenu::checkCanon(m);
            for (QAction *a : m->actions())
                if (a->objectName() == "stringReverseSpan")
                    lastReverseEnabled = a->isEnabled();
            m->close();
        }
    });
    rig.gv->stringHandleMenu(2, rig.gv->mapToGlobal(rig.viewAt(3, 0)));
    QCOMPARE(names, QStringList({ "stringInsertAnchor", "stringPinLed", "stringUnpinLed",
                                  "stringReverseSpan", "stringDeleteAnchor" }));
    QVERIFY2(canon.isEmpty(), qPrintable(canon.join("; ")));
    QVERIFY(!lastReverseEnabled);                                    // no span starts at the last

    const quint32 fid = rig.string->id();
    // Pin: that LED sits on the anchor; one undo takes it off.
    QVERIFY(rig.gv->stringLedNearAnchor(fid, 1) > 1);
    rig.gv->setStringAnchorPin(fid, 1, 30);
    QCOMPARE(rig.rp().stringAnchors.at(1).pin, 30);
    const QVector<QVector3D> leds = PixelPath::positions(rig.doc, rig.doc->monitorProperties(), fid, 60);
    QVERIFY((leds.at(29) - QVector3D(2, 0, 0)).length() < 1e-4f);
    QCOMPARE(rig.gv->stringLedNearAnchor(fid, 1), 30);
    rig.gv->undoLastMove();
    QCOMPARE(rig.rp().stringAnchors.at(1).pin, 0);
    // Reverse the first span; insert one half way before the second; delete it.
    rig.gv->setStringAnchorReverse(fid, 0, true);
    QVERIFY(rig.rp().stringAnchors.at(0).reverse);
    rig.gv->insertStringAnchor(fid, 1);
    QCOMPARE(rig.rp().stringAnchors.size(), 4);
    QVERIFY(qAbs(rig.rp().stringAnchors.at(1).x - 1.0f) < 1e-4f);
    QVERIFY(rig.rp().stringAnchors.at(1).reverse);                   // inside the reversed span
    rig.gv->deleteStringAnchor(fid, 1);
    QCOMPARE(rig.rp().stringAnchors.size(), 3);
    rig.gv->undoLastMove();
    QCOMPARE(rig.rp().stringAnchors.size(), 4);
}

/* Edit Anchors…: exact values in the unit Settings picks (feet here), a
   pin that counts backwards refused in words, OK one undo step. */
void FreeString_Test::theAnchorTableTakesExactValuesInSettingsUnits()
{
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    {
        StringAnchorsDialog dlg({ anchorAt(0, 0, 1), anchorAt(3.048f, 0, 1) }, 60, 1.0f);
        QCOMPARE(dlg.m_table->rowCount(), 2);
        auto *across = qobject_cast<QDoubleSpinBox *>(dlg.m_table->cellWidget(1, 1));
        QVERIFY(across != nullptr);
        QCOMPARE(across->text(), MeasurementUnits::formatLength(3048.0));
        QVERIFY(!across->text().contains("m"));                     // ft and in, not metres
        QLineEdit *edit = across->findChild<QLineEdit *>();
        QVERIFY(edit != nullptr);
        edit->setText("12'");
        QVERIFY(across->hasAcceptableInput());
        across->interpretText();
        QVERIFY(qAbs(across->value() - 3.6576) < 1e-3);
        qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(0, 4))->setValue(20);
        qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 4))->setValue(10);
        dlg.accept();
        QVERIFY(dlg.result() != QDialog::Accepted);
        QVERIFY(dlg.m_error->text().contains("count up"));
        qobject_cast<QSpinBox *>(dlg.m_table->cellWidget(1, 4))->setValue(40);
        qobject_cast<QCheckBox *>(dlg.m_table->cellWidget(1, 5))->setChecked(true);
        dlg.accept();
        QCOMPARE(dlg.result(), int(QDialog::Accepted));
        const QVector<StringAnchor> a = dlg.anchors();
        QCOMPARE(a.size(), 2);
        QCOMPARE(a.at(1).pin, 40);
        QVERIFY(a.at(1).reverse);
        // Add Anchor: after the last, a foot on the way it was going.
        dlg.findChild<QPushButton *>("stringAnchorsAdd")->click();
        QCOMPARE(dlg.m_table->rowCount(), 3);
        QVERIFY(qAbs(dlg.anchors().at(2).x - (3.6576f + 0.3048f)) < 1e-3f);
        QCOMPARE(dlg.anchors().at(2).pin, 0);
    }

    // From the inspector: starts from what is drawn; OK is one edit.
    Rig rig;
    QVERIFY(rig.build());
    FixtureRigProps r = rig.rp();
    r.pixelPath = PixelPath::FreeString;
    rig.doc->monitorProperties()->setFixtureRigProps(rig.string->id(), r);
    rig.fields->reload();
    QSignalSpy edits(rig.fields, &FixtureFields::aboutToEdit);
    int rows = -1;
    answerDialog<StringAnchorsDialog>([&rows](StringAnchorsDialog *d) {
        rows = d->m_table->rowCount();
        qobject_cast<QSpinBox *>(d->m_table->cellWidget(1, 4))->setValue(60);
        d->accept();
    });
    rig.fields->editStringAnchors();
    QCOMPARE(rows, 2);                                               // the default two
    QCOMPARE(rig.rp().stringAnchors.size(), 2);
    QCOMPARE(rig.rp().stringAnchors.at(1).pin, 60);
    QCOMPARE(edits.count(), 1);
    QCOMPARE(rig.fields->m_strCount->text(), QString("2 anchors"));
}

/* Fixture Properties is a dialog over the plan: Add Anchor closes it
   (keeping what was set) so the plan can be clicked; a right-click ends. */
void FreeString_Test::addAnchorFromADialogClosesItSoThePlanCanBeClicked()
{
    Rig rig;
    QVERIFY(rig.build());
    QVERIFY(rig.show());
    FixtureRigProps r = rig.rp();
    r.pixelPath = PixelPath::FreeString;
    rig.doc->monitorProperties()->setFixtureRigProps(rig.string->id(), r);
    QDialog dlg;
    QFormLayout *form = new QFormLayout(&dlg);
    FixtureFields fields(rig.mon, form, &dlg);
    fields.setFixtures({ rig.string->id() }, rig.string->id());
    dlg.show();
    QVERIFY(!fields.m_strAdd->isHidden());
    fields.m_strAdd->click();
    QCOMPARE(dlg.result(), int(QDialog::Accepted));
    QVERIFY(!dlg.isVisible());
    QVERIFY(rig.gv->addingAnchors());
    QTest::mouseClick(rig.gv->viewport(), Qt::RightButton, Qt::NoModifier, rig.viewAt(10, 10));
    QVERIFY(!rig.gv->addingAnchors());
    QVERIFY(rig.rp().stringAnchors.isEmpty());
}

/* Write Segments… for a free string: one segment from each anchor to the
   next, by the LEDs laid on it, each with its anchor's Reverse; read back
   and said so. A fake WLED on this computer: no real device. */
void FreeString_Test::writeSegmentsWritesASegmentPerSpanWithItsOwnReverse()
{
    Rig rig;
    QVERIFY(rig.build());
    // LED 31 tied at the middle anchor; the span from it reversed.
    rig.setAnchors({ anchorAt(0, 0, 1), anchorAt(1, 0, 1, 31, true), anchorAt(2, 0, 1) });
    FakeWled wled;
    QVERIFY(wled.listen(QHostAddress::LocalHost));
    wled.info["leds"] = QJsonObject{ { "count", 60 }, { "maxseg", 32 } };
    rig.string->setConnection(Fixture::Network);
    rig.string->setTransport(Fixture::WledTransport);
    rig.string->setIPAddress(wled.address());
    rig.fields->setFixtures({ rig.string->id() }, rig.string->id());
    FixtureFields &f = *rig.fields;
    QVERIFY(f.m_strWrite->isEnabled());
    QVERIFY(f.m_strWrite->toolTip().contains("one from each anchor"));
    int asked = -1;
    f.m_confirmWrite = [&](const QString &name, const QString &ip, int n, int) {
        asked = n;
        return name == "Garland" && ip == wled.address();
    };
    f.m_strWrite->click();
    QVERIFY2(f.m_strNote->text().startsWith(QString::fromUtf8("⟳ Reading WLED (")), qPrintable(f.m_strNote->text()));
    QTRY_COMPARE(f.m_strNote->text(), QString("Wrote 2 segments to WLED (%1).").arg(wled.address()));
    QCOMPARE(asked, 2);
    const QJsonArray seg = wled.segments();
    QCOMPARE(seg.size(), 2);
    QCOMPARE(seg.at(0).toObject().value("start").toInt(), 0);
    QCOMPARE(seg.at(0).toObject().value("stop").toInt(), 30);
    QCOMPARE(seg.at(0).toObject().value("rev").toBool(), false);
    QCOMPARE(seg.at(1).toObject().value("start").toInt(), 30);
    QCOMPARE(seg.at(1).toObject().value("stop").toInt(), 60);
    QCOMPARE(seg.at(1).toObject().value("rev").toBool(), true);
    QVERIFY(f.m_strWrite->isEnabled());
    // The confirmation says what a free string's segments are.
    {
        QWidget over;
        QTimer::singleShot(50, [&]() {
            if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            {
                QVERIFY(box->text().contains("from one anchor to the next"));
                box->reject();
            }
        });
        QVERIFY(!WledDeviceWrite::confirmSegments(&over, "Garland", wled.address(), 2, 1, true));
    }
    // Pins that count backwards: refused in words, nothing sent.
    rig.setAnchors({ anchorAt(0, 0, 1, 40), anchorAt(1, 0, 1, 31), anchorAt(2, 0, 1) });
    const int posts = wled.posted.size();
    asked = -1;
    f.m_strWrite->click();
    QVERIFY(f.m_strNote->text().startsWith("Nothing was written"));
    QCOMPARE(asked, -1);
    QCOMPARE(wled.posted.size(), posts);
    // A held classroom refuses with the reason.
    rig.setAnchors({ anchorAt(0, 0, 1), anchorAt(2, 0, 1) });
    rig.doc->inputOutputMap()->setTeachingHold(true);
    rig.string->setIPAddress("10.255.255.1");
    rig.fields->setFixtures({ rig.string->id() }, rig.string->id());
    f.m_strWrite->click();
    QVERIFY2(f.m_strNote->text().contains("Teaching mode holds real output"), qPrintable(f.m_strNote->text()));
    rig.doc->inputOutputMap()->setTeachingHold(false);
    f.m_confirmWrite = nullptr;
}

/* Before and after, for review (QLC_SHOT_DIR set): the plot with the
   string drawn as its body, then laid on its anchors, selected, at three
   window sizes; and the inspector's rows at three widths. */
void FreeString_Test::picturesForReview()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    MeasurementUnits::setPreferredLength(MeasurementUnits::Feet);
    Rig rig;
    QVERIFY(rig.build(50));
    MonitorProperties *props = rig.doc->monitorProperties();
    props->setGridUnits(MonitorProperties::Feet);
    // A rainbow along the string, every LED lit.
    QByteArray v(512, char(0));
    for (int h = 0; h < 50; ++h)
    {
        const QColor c = QColor::fromHsv((h * 360 / 50) % 360, 255, 255);
        v[h * 3] = char(c.red()); v[h * 3 + 1] = char(c.green()); v[h * 3 + 2] = char(c.blue());
    }
    rig.string->setChannelValues(v);
    QVERIFY(rig.show());
    rig.gv->setGridSize(QSize(12, 8));
    props->setFixturePosition(rig.string->id(), 0, 0, QVector3D(2000.0f, 1500.0f, 1000.0f));
    const QList<QSize> sizes = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    for (int pass = 0; pass < 2; ++pass)
    {
        FixtureRigProps r = rig.rp();
        r.wandStripLength = 4.877f;    // 50 LEDs on 16 ft
        r.pixelPath = pass == 0 ? int(PixelPath::None) : int(PixelPath::FreeString);
        if (pass == 1)
            r.stringAnchors = { anchorAt(1.5f, 2.5f, 1.0f), anchorAt(3.5f, 4.0f, 2.0f, 20),
                                anchorAt(6.0f, 3.0f, 1.2f, 0, true), anchorAt(7.5f, 5.0f, 0.5f) };
        props->setFixtureRigProps(rig.string->id(), r);
        rig.gv->updateFixture(rig.string->id());
        rig.gv->scene()->clearSelection();
        rig.gv->fixtureItemForId(rig.string->id())->setSelected(true);
        for (const QSize &sz : sizes)
        {
            rig.parent.resize(sz);
            rig.mon->resize(sz);
            QApplication::processEvents();
            rig.gv->setGridSize(QSize(12, 8));
            rig.gv->updateFixture(rig.string->id());
            QApplication::processEvents();
            rig.parent.grab().save(QString("%1/%2-plot-%3x%4.png").arg(dir)
                                       .arg(pass == 0 ? "before-asbody" : "after-freestring")
                                       .arg(sz.width()).arg(sz.height()));
        }
    }
    rig.fields->reload();
    rig.host.show();
    for (int w : { 320, 420, 560 })
    {
        rig.host.resize(w, rig.host.sizeHint().height());
        QApplication::processEvents();
        rig.host.grab().save(QString("%1/after-inspector-rows-%2.png").arg(dir).arg(w));
    }
}

QTEST_MAIN(FreeString_Test)
