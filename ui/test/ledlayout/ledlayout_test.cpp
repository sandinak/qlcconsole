/*
  Q Light Controller Plus - Unit test
  ledlayout_test.cpp

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
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QMimeData>
#include <QJSValue>
#include <functional>
#include <memory>

#define protected public
#define private public
#include "ledlayouteditor.h"
#include "headtypesdialog.h"
#include "fixturemanager.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "monitorlayerspanel.h"
#include "structurestudioview.h"
#include "effectinstance.h"
#include "genericdmxsource.h"
#include "app.h"
#undef private
#undef protected

#include "monitorproperties.h"
#include "fixturetreewidget.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlcfixturedefcache.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "qlcpalette.h"
#include "headlayout.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "ledlayout_test.h"

/* The column of the Fixture Manager tree that carries a row's ids. */
#define KColumnName 0

/* The layout Dance Hysteria 0.6 keeps for the XL-450 (165ch), cell by cell
   in head order: the 45 RGB heads on rows 0, 2 and 4, the 30 white ones on
   rows 1 and 3 -- RGB / W / RGB / W / RGB. */
static const char *kDanceHysteriaXl450 =
    "0,0 1,0 2,0 3,0 4,0 5,0 6,0 7,0 8,0 9,0 10,0 11,0 12,0 13,0 14,0 "
    "0,2 1,2 2,2 3,2 4,2 5,2 6,2 7,2 8,2 9,2 10,2 11,2 12,2 13,2 14,2 "
    "0,4 1,4 2,4 3,4 4,4 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 "
    "0,1 1,1 2,1 3,1 4,1 5,1 6,1 7,1 8,1 9,1 10,1 11,1 12,1 13,1 14,1 "
    "0,3 1,3 2,3 3,3 4,3 5,3 6,3 7,3 8,3 9,3 10,3 11,3 12,3 13,3 14,3";

static QString cellText(const HeadLayout &l)
{
    QStringList out;
    for (const QPoint &p : l.cells)
        out << QString("%1,%2").arg(p.x()).arg(p.y());
    return out.join(' ');
}

/* A model of @p rgb RGB heads then @p white white ones, on a declared
   @p grid (invalid: none). */
static QLCFixtureDef *makeDef(const QString &make, const QString &model, const QString &modeName,
                              int rgb, int white, const QSize &grid)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer(make);
    def->setModel(model);
    def->setType(QLCFixtureDef::LEDBarPixels);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName(modeName);
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
    if (grid.isValid())
        ph.setLayoutSize(grid);
    ph.setWidth(401); ph.setHeight(180); ph.setDepth(100);
    mode->setPhysical(ph);
    def->addMode(mode);
    return def;
}

/* Run @p act on the modal dialog that opens next (polled: it may open from
   inside a menu), then close it. */
static void onNextModal(const std::function<void(QWidget *)> &act)
{
    auto poll = std::make_shared<std::function<void()>>();
    auto tries = std::make_shared<int>(0);
    *poll = [act, poll, tries]() {
        QWidget *modal = QApplication::activeModalWidget();
        if (modal == nullptr && ++*tries < 200)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        if (modal != nullptr)
        {
            act(modal);
            if (modal->isVisible())
                modal->close();
        }
    };
    QTimer::singleShot(0, *poll);
}

/* Right-click handling that pops a QMenu up with exec(): @p act gets the
   menu while it is open; it is closed after unless @p act triggered something. */
static void onNextMenu(const std::function<void(QMenu *)> &act)
{
    auto poll = std::make_shared<std::function<void()>>();
    auto tries = std::make_shared<int>(0);
    *poll = [act, poll, tries]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr && ++*tries < 200)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        if (m != nullptr)
        {
            act(m);
            if (m->isVisible())
                m->close();
        }
    };
    QTimer::singleShot(0, *poll);
}

static QAction *actionNamed(QMenu *m, const QString &name)
{
    for (QAction *a : m->actions())
    {
        if (a->objectName() == name)
            return a;
        if (a->menu() != nullptr)
            if (QAction *in = actionNamed(a->menu(), name))
                return in;
    }
    return nullptr;
}

static bool dropFixtureAtCentre(MonitorGraphicsView *gv, quint32 fid)
{
    QByteArray data;
    QDataStream s(&data, QIODevice::WriteOnly);
    s << fid;
    QMimeData mime;
    mime.setData(QStringLiteral("application/x-qlc-fid"), data);
    QDragEnterEvent en(gv->viewport()->rect().center(), Qt::CopyAction, &mime,
                       Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(gv->viewport(), &en);
    QDropEvent ev(QPointF(gv->viewport()->rect().center()), Qt::CopyAction, &mime,
                  Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(gv->viewport(), &ev);
    return ev.isAccepted();
}

/*****************************************************************************
 * Setup
 *****************************************************************************/

void LedLayout_Test::initTestCase()
{
    // Never the operator's own layouts.
    QVERIFY(m_library.isValid());
    HeadLayoutLibrary::setUserDirectoryOverride(m_library.path());
    m_xl = makeDef("Warmdance", "XL-450", "165ch", 45, 30, QSize(15, 5));
    m_strip = makeDef("Testco", "Strip 32", "64ch", 16, 16, QSize());
    m_par = makeDef("Testco", "Par", "3ch", 1, 0, QSize());
}

void LedLayout_Test::cleanupTestCase()
{
    HeadLayoutLibrary::setUserDirectoryOverride(QString());
    delete m_xl;
    delete m_strip;
    delete m_par;
}

void LedLayout_Test::init()
{
    // An empty library and a show of its own for each test.
    QDir lib(m_library.path());
    for (const QString &f : lib.entryList(QDir::Files))
        lib.remove(f);
    HeadLayoutLibrary::setUserDirectoryOverride(m_library.path());
    m_doc = new Doc(this);
}

void LedLayout_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

Fixture *LedLayout_Test::addXl450(const QString &name)
{
    Fixture *f = new Fixture(m_doc);
    f->setName(name);
    f->setFixtureDefinition(m_xl, m_xl->modes().first());
    f->setAddress(quint32(m_doc->fixtures().size()) * 165);
    m_doc->addFixture(f);
    return f;
}

/*****************************************************************************
 * The editor
 *****************************************************************************/

void LedLayout_Test::rowPatternsAreReadAsTyped()
{
    const QStringList kinds = { "RGB", "W" };
    const QStringList want = { "RGB", "W", "RGB", "W", "RGB" };
    QCOMPARE(HeadLayout::parseRowKinds("RGB, W, RGB, W, RGB", kinds), want);
    QCOMPARE(HeadLayout::parseRowKinds("RGB / W / RGB / W / RGB", kinds), want);   // as Branson wrote it
    QCOMPARE(HeadLayout::parseRowKinds("rgb w rgb w rgb", kinds), want);
    QCOMPARE(HeadLayout::parseRowKinds("RGB,White,RGB,white,RGB", kinds), want);
    QCOMPARE(HeadLayout::parseRowKinds("  ", kinds), QStringList());
    QCOMPARE(HeadLayout::parseRowKinds("RGB, UV", kinds), QStringList({ "RGB", "UV" }));
}

void LedLayout_Test::theXl450RowsGiveTheMapTheShowKeeps()
{
    Fixture *bar = addXl450("FART-SR-1");
    QVERIFY(LedLayoutDialog::canEdit(m_doc, bar->id()));
    LedLayoutDialog dlg(m_doc, bar->id());
    LedLayoutEditor *ed = dlg.editor();

    // Nothing saved yet: heads in number order, as the definition alone says.
    QCOMPARE(ed->layout().size, QSize(15, 5));
    QCOMPARE(ed->rowPatternText(), QString("RGB, RGB, RGB, W, W"));
    QVERIFY(dlg.m_source->text().contains("Nothing saved"));

    // Typed as Branson describes the bar -- and in the field, with Apply.
    QVERIFY(ed->applyRowPattern("RGB, W, RGB, W, RGB"));
    QCOMPARE(cellText(ed->layout()), QString(kDanceHysteriaXl450));
    ed->resetHeadOrder();
    ed->m_rowsEdit->setText("RGB/W/RGB/W/RGB");
    QPushButton *apply = ed->findChild<QPushButton *>("ledApplyRows");
    QVERIFY(apply != nullptr);
    apply->click();
    QCOMPARE(cellText(ed->layout()), QString(kDanceHysteriaXl450));
    QCOMPARE(ed->rowPatternText(), QString("RGB, W, RGB, W, RGB"));

    // Saved to the show, written as the show writes it.
    dlg.accept();
    QVERIFY(m_doc->isModified());
    QVERIFY(m_doc->monitorProperties()->hasShowHeadLayout("Warmdance|XL-450|165ch"));
    QCOMPARE(cellText(m_doc->monitorProperties()->headLayoutFor(bar)), QString(kDanceHysteriaXl450));
}

void LedLayout_Test::aPatternThatDoesNotFitSaysWhy()
{
    Fixture *bar = addXl450("Bar");
    LedLayoutEditor ed(m_doc);
    ed.setFixture(bar, HeadLayout::headOrder(bar));
    const HeadLayout before = ed.layout();
    QVERIFY(!ed.applyRowPattern("RGB, RGB, RGB, RGB, W"));
    QVERIFY2(ed.rowProblem().contains("RGB rows hold 60 heads, it has 45"), qPrintable(ed.rowProblem()));
    QCOMPARE(ed.layout(), before);
    QVERIFY(!ed.applyRowPattern("RGB, UV, RGB, W, RGB"));
    QVERIFY2(ed.rowProblem().contains("no UV heads"), qPrintable(ed.rowProblem()));
    QVERIFY(ed.applyRowPattern("RGB, W, RGB, W, RGB"));
    QVERIFY(ed.rowProblem().isEmpty());
}

void LedLayout_Test::clickingSwapsAndMovesHeads()
{
    Fixture *bar = addXl450("Bar");
    LedLayoutEditor ed(m_doc);
    ed.resize(600, 400);
    ed.setFixture(bar, HeadLayout::headOrder(bar));
    ed.show();
    QVERIFY(QTest::qWaitForWindowExposed(&ed));
    QSignalSpy changed(&ed, &LedLayoutEditor::layoutChanged);
    LedMapWidget *grid = ed.m_grid;
    auto click = [grid](const QPoint &cell) {
        QTest::mouseClick(grid, Qt::LeftButton, Qt::NoModifier, grid->cellRect(cell).center().toPoint());
    };
    // Head 1 then head 2: they trade cells.
    click(QPoint(0, 0));
    QCOMPARE(grid->m_picked, 0);
    click(QPoint(1, 0));
    QCOMPARE(ed.layout().cellOf(0), QPoint(1, 0));
    QCOMPARE(ed.layout().cellOf(1), QPoint(0, 0));
    QCOMPARE(changed.count(), 1);
    // Clicking a head twice lets it go.
    click(QPoint(3, 0));
    click(QPoint(3, 0));
    QCOMPARE(grid->m_picked, -1);
    QCOMPARE(changed.count(), 1);

    // A gap: on a wider grid, a head moves to an empty cell.
    QVERIFY(ed.setGridSize(QSize(16, 5)));
    QCOMPARE(ed.layout().cellOf(0), QPoint(1, 0));     // everyone stayed put
    QCOMPARE(grid->headIn(QPoint(15, 0)), -1);
    click(QPoint(2, 0));                                // head 3
    click(QPoint(15, 0));
    QCOMPARE(ed.layout().cellOf(2), QPoint(15, 0));
    QCOMPARE(grid->headIn(QPoint(2, 0)), -1);
}

void LedLayout_Test::flipsMirrorTheGrid()
{
    Fixture *bar = addXl450("Bar");
    LedLayoutEditor ed(m_doc);
    ed.setFixture(bar, HeadLayout::headOrder(bar));
    QVERIFY(ed.applyRowPattern("RGB, W, RGB, W, RGB"));
    const HeadLayout built = ed.layout();
    QSignalSpy changed(&ed, &LedLayoutEditor::layoutChanged);

    ed.findChild<QPushButton *>("ledFlipH")->click();
    QCOMPARE(ed.layout().cellOf(0), QPoint(14, 0));
    QCOMPARE(ed.layout().cellOf(45), QPoint(14, 1));
    ed.findChild<QPushButton *>("ledFlipV")->click();
    QCOMPARE(ed.layout().cellOf(0), QPoint(14, 4));
    QCOMPARE(ed.layout().cellOf(45), QPoint(14, 3));
    // The rows are still RGB / W / RGB / W / RGB, symmetrical as they are.
    QCOMPARE(ed.rowPatternText(), QString("RGB, W, RGB, W, RGB"));
    QCOMPARE(changed.count(), 2);
    ed.flipHorizontally();
    ed.flipVertically();
    QCOMPARE(ed.layout(), built);

    // Head Order puts the definition's order back.
    ed.findChild<QPushButton *>("ledHeadOrder")->click();
    QCOMPARE(ed.layout(), HeadLayout::headOrder(bar));
}

void LedLayout_Test::theGridCanBeResized()
{
    Fixture *bar = addXl450("Bar");
    LedLayoutEditor ed(m_doc);
    ed.setFixture(bar, HeadLayout::headOrder(bar));
    QCOMPARE(ed.m_colsSpin->value(), 15);
    QCOMPARE(ed.m_rowsSpin->value(), 5);
    // Too few cells for 75 heads: refused.
    QVERIFY(!ed.setGridSize(QSize(10, 5)));
    QCOMPARE(ed.layout().size, QSize(15, 5));
    // Through the spin box, the other side grows to fit: 14 columns, 6 rows.
    ed.m_colsSpin->setValue(14);
    QCOMPARE(ed.layout().size, QSize(14, 6));
    QCOMPARE(ed.m_rowsSpin->value(), 6);
    QCOMPARE(ed.layout().cells.size(), 75);
    QSet<QPoint> used;
    for (const QPoint &p : ed.layout().cells)
    {
        QVERIFY(p.x() >= 0 && p.x() < 14 && p.y() >= 0 && p.y() < 6);
        used.insert(p);
    }
    QCOMPARE(used.size(), 75);      // no two heads in one cell
}

void LedLayout_Test::aStripIsLaidOutInRows()
{
    // No declared grid: 32 heads in one row -- "RGB, W" means two rows of 16.
    Fixture *strip = new Fixture(m_doc);
    strip->setName("Washer");
    strip->setFixtureDefinition(m_strip, m_strip->modes().first());
    m_doc->addFixture(strip);
    LedLayoutEditor ed(m_doc);
    ed.setFixture(strip, m_doc->monitorProperties()->headLayoutFor(strip));
    QCOMPARE(ed.layout().size, QSize(32, 1));
    QVERIFY2(ed.applyRowPattern("RGB, W"), qPrintable(ed.rowProblem()));
    QCOMPARE(ed.layout().size, QSize(16, 2));
    QCOMPARE(ed.layout().cellOf(16), QPoint(0, 1));

    // Single-head fixtures have nothing to lay out.
    Fixture *par = new Fixture(m_doc);
    par->setFixtureDefinition(m_par, m_par->modes().first());
    m_doc->addFixture(par);
    QVERIFY(!LedLayoutDialog::canEdit(m_doc, par->id()));
    Fixture *dimmer = new Fixture(m_doc);
    dimmer->setChannels(4);
    m_doc->addFixture(dimmer);
    QVERIFY(!LedLayoutDialog::canEdit(m_doc, dimmer->id()));   // generic: no model to map
}

void LedLayout_Test::thePreviewWalksTheHeadsInOrder()
{
    Fixture *bar = addXl450("Bar");
    LedLayoutEditor ed(m_doc);
    ed.setFixture(bar, HeadLayout::headOrder(bar));
    QVERIFY(ed.applyRowPattern("RGB, W, RGB, W, RGB"));
    QCOMPARE(ed.litHead(), -1);
    ed.findChild<QPushButton *>("ledPreview")->click();
    QVERIFY(ed.previewRunning());
    QCOMPARE(ed.litHead(), 0);
    ed.stepPreview();
    QCOMPARE(ed.litHead(), 1);
    // Clicking a head in the strip lights it: head 46, the first white.
    ed.lightHead(45);
    QCOMPARE(ed.m_grid->lit(), 45);
    QCOMPARE(ed.m_strip->m_lit, 45);
    ed.stepPreview();
    QCOMPARE(ed.litHead(), 46);

    // On the real fixture too: that head's intensity full, the others not.
    ed.m_lightFixture->setChecked(true);
    QVERIFY(ed.m_source != nullptr);
    QCOMPARE(ed.m_source->m_values.value(qMakePair(bar->id(), quint32(135 + 1))), uchar(255));  // White 2
    QCOMPARE(ed.m_source->m_values.value(qMakePair(bar->id(), quint32(0))), uchar(0));          // Red 1
    ed.findChild<QPushButton *>("ledPreview")->click();
    QVERIFY(!ed.previewRunning());
    QCOMPARE(ed.litHead(), -1);
    QVERIFY(ed.m_source == nullptr);        // and the fixture is let go
}

/*****************************************************************************
 * Where it's kept
 *****************************************************************************/

void LedLayout_Test::saveToShowKeepsItInTheShowOnly()
{
    Fixture *bar = addXl450("Bar");
    const QString key = HeadLayout::keyFor(bar);
    LedLayoutDialog dlg(m_doc, bar->id());
    QVERIFY(dlg.editor()->applyRowPattern("RGB, W, RGB, W, RGB"));
    dlg.accept();
    QVERIFY(m_doc->monitorProperties()->hasShowHeadLayout(key));
    QVERIFY(!HeadLayoutLibrary::load(key).isValid());           // not the default
    QVERIFY(QDir(m_library.path()).entryList(QDir::Files).isEmpty());

    // Cancel changes nothing.
    {
        LedLayoutDialog again(m_doc, bar->id());
        again.editor()->flipHorizontally();
        again.reject();
    }
    QCOMPARE(cellText(m_doc->monitorProperties()->headLayoutFor(bar)), QString(kDanceHysteriaXl450));
}

void LedLayout_Test::saveAsDefaultRoundTripsAndTheShowWins()
{
    Fixture *bar = addXl450("Bar");
    const QString key = HeadLayout::keyFor(bar);
    {
        LedLayoutDialog dlg(m_doc, bar->id());
        QVERIFY(dlg.editor()->applyRowPattern("RGB, W, RGB, W, RGB"));
        dlg.findChild<QPushButton *>("ledSaveDefault")->click();
        QVERIFY2(dlg.m_source->text().contains("Saved as your default"), qPrintable(dlg.m_source->text()));
        dlg.reject();                       // not kept in this show
    }
    // On disk, in the library folder, as the library reads it back.
    const QString file = QDir(m_library.path()).absoluteFilePath(HeadLayoutLibrary::fileNameFor(key));
    QVERIFY2(QFile::exists(file), qPrintable(file));
    HeadLayoutLibrary::setUserDirectoryOverride(m_library.path());     // drops the cache
    QCOMPARE(cellText(HeadLayoutLibrary::load(key)), QString(kDanceHysteriaXl450));

    // A show with no layout of its own uses the default...
    QVERIFY(!m_doc->monitorProperties()->hasShowHeadLayout(key));
    QCOMPARE(cellText(m_doc->monitorProperties()->headLayoutFor(bar)), QString(kDanceHysteriaXl450));
    Doc other(this);
    Fixture *elsewhere = new Fixture(&other);
    elsewhere->setFixtureDefinition(m_xl, m_xl->modes().first());
    other.addFixture(elsewhere);
    QCOMPARE(cellText(other.monitorProperties()->headLayoutFor(elsewhere)), QString(kDanceHysteriaXl450));
    {
        LedLayoutDialog dlg(&other, elsewhere->id());
        QVERIFY(dlg.m_source->text().contains("Using your default"));
        QCOMPARE(cellText(dlg.editor()->layout()), QString(kDanceHysteriaXl450));
    }

    // ...and a show's own layout wins over it.
    {
        LedLayoutDialog dlg(m_doc, bar->id());
        dlg.editor()->flipHorizontally();
        dlg.accept();
    }
    QCOMPARE(m_doc->monitorProperties()->headLayoutFor(bar).cellOf(0), QPoint(14, 0));
    QCOMPARE(HeadLayoutLibrary::load(key).cellOf(0), QPoint(0, 0));
    QCOMPARE(other.monitorProperties()->headLayoutFor(elsewhere).cellOf(0), QPoint(0, 0));
    {
        LedLayoutDialog dlg(m_doc, bar->id());
        QVERIFY(dlg.m_source->text().contains("own layout"));
        // Use Default brings the default back into the editor.
        dlg.findChild<QPushButton *>("ledUseDefault")->click();
        QCOMPARE(dlg.editor()->layout().cellOf(0), QPoint(0, 0));
        dlg.accept();
    }
    QCOMPARE(m_doc->monitorProperties()->headLayoutFor(bar).cellOf(0), QPoint(0, 0));
    elsewhere = nullptr;
}

/*****************************************************************************
 * Where it's found
 *****************************************************************************/

void LedLayout_Test::theFixtureManagerOffersItForMultiHeadFixtures()
{
    Fixture *bar = addXl450("Bar");
    Fixture *par = new Fixture(m_doc);
    par->setName("Par");
    par->setFixtureDefinition(m_par, m_par->modes().first());
    par->setAddress(400);
    m_doc->addFixture(par);

    FixtureManager mgr(nullptr, m_doc);
    auto row = [&mgr](quint32 fid) -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
            if ((*it)->data(KColumnName, PROP_ID).isValid()
                && !(*it)->data(KColumnName, PROP_HEAD).isValid()
                && (*it)->data(KColumnName, PROP_ID).toString().toUInt() == fid)
                return *it;
        return nullptr;
    };
    auto rightClick = [&mgr](QTreeWidgetItem *item) {
        mgr.m_fixtures_tree->clearSelection();
        mgr.m_fixtures_tree->setCurrentItem(item);
        item->setSelected(true);
        mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(item).center());
    };
    QVERIFY(row(bar->id()) != nullptr && row(par->id()) != nullptr);

    // A single-head fixture: there, but not enabled.
    bool seen = false, enabled = true;
    onNextMenu([&](QMenu *m) {
        QAction *a = actionNamed(m, "ledLayoutAction");
        seen = a != nullptr;
        enabled = a != nullptr && a->isEnabled();
    });
    rightClick(row(par->id()));
    QVERIFY(seen);
    QVERIFY(!enabled);

    // The XL-450: enabled, and it opens the editor for its model -- no group made.
    QString title, pattern;
    onNextMenu([&](QMenu *m) {
        QAction *a = actionNamed(m, "ledLayoutAction");
        if (a == nullptr || !a->isEnabled())
            return;
        onNextModal([&](QWidget *w) {
            LedLayoutDialog *dlg = qobject_cast<LedLayoutDialog *>(w);
            if (dlg == nullptr)
                return;
            title = dlg->windowTitle();
            dlg->editor()->applyRowPattern("RGB, W, RGB, W, RGB");
            pattern = dlg->editor()->rowPatternText();
            dlg->accept();
        });
        m->close();
        a->trigger();
    });
    const int groups = m_doc->fixtureGroups().size();
    rightClick(row(bar->id()));
    QCOMPARE(title, QString("LED Layout — Warmdance XL-450"));
    QCOMPARE(pattern, QString("RGB, W, RGB, W, RGB"));
    QCOMPARE(m_doc->fixtureGroups().size(), groups);
    QCOMPARE(cellText(m_doc->monitorProperties()->headLayoutFor(bar)), QString(kDanceHysteriaXl450));
}

void LedLayout_Test::thePlotAndStudioTreeOfferIt()
{
    Fixture *bar = addXl450("Bar");
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    QVERIFY(gv != nullptr);
    gv->resize(1000, 700);
    QVERIFY(dropFixtureAtCentre(gv, bar->id()));

    // The plot's fixture menu...
    {
        QMenu menu;
        gv->populateItemMenu(&menu, "fixture", bar->id(), QPointF(), false);
        QAction *a = actionNamed(&menu, "ledLayoutAction");
        QVERIFY(a != nullptr && a->isEnabled());
        QCOMPARE(a->text(), QString("LED Layout…"));
    }
    // ...which is the Lighting Studio tree's too.
    MonitorLayersPanel *panel = mon->m_layersPanel;
    QVERIFY(panel != nullptr);
    panel->reload();
    QTreeWidgetItem *fxRow = nullptr;
    for (QTreeWidgetItemIterator it(panel->m_tree); *it != nullptr; ++it)
        if ((*it)->text(0).startsWith("Bar"))
            fxRow = *it;
    QVERIFY(fxRow != nullptr);
    {
        QMenu menu;
        panel->buildContextMenu(&menu, fxRow);
        QVERIFY(actionNamed(&menu, "ledLayoutAction") != nullptr);
    }

    // The Rig Overview: right-click a fixture.
    bool opened = false;
    mon->toggleStageOverview();
    QVERIFY(mon->m_overviewView != nullptr);
    onNextMenu([&](QMenu *m) {
        QAction *a = actionNamed(m, "ledLayoutAction");
        if (a == nullptr || !a->isEnabled())
            return;
        onNextModal([&](QWidget *w) {
            opened = qobject_cast<LedLayoutDialog *>(w) != nullptr;
            if (QDialog *d = qobject_cast<QDialog *>(w))
                d->reject();
        });
        m->close();
        a->trigger();
    });
    emit mon->m_overviewView->canvasContextMenu(QPoint(10, 10), bar->id());
    QVERIFY(opened);
    if (mon->m_overviewDlg != nullptr)
        mon->m_overviewDlg->close();
    QCoreApplication::processEvents();
    delete mon;
}

void LedLayout_Test::fixturePropertiesOffersIt()
{
    Fixture *bar = addXl450("Bar");
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    gv->resize(1000, 700);
    QVERIFY(dropFixtureAtCentre(gv, bar->id()));

    bool button = false, opened = false;
    onNextModal([&](QWidget *props) {
        QPushButton *b = props->findChild<QPushButton *>("ledLayoutButton");
        button = b != nullptr && b->isEnabled();
        if (!button)
            return;
        onNextModal([&](QWidget *w) {
            opened = qobject_cast<LedLayoutDialog *>(w) != nullptr;
            if (QDialog *d = qobject_cast<QDialog *>(w))
                d->reject();
        });
        b->click();
        if (QDialog *d = qobject_cast<QDialog *>(props))
            d->reject();
    });
    mon->showFixtureItemEditor(bar->id());
    QVERIFY(button);
    QVERIFY(opened);
    delete mon;
}

/*****************************************************************************
 * Everything reads it
 *****************************************************************************/

void LedLayout_Test::theMatrixCanvasUsesTheLayout()
{
    Fixture *bar = addXl450("Bar");
    {
        LedLayoutDialog dlg(m_doc, bar->id());
        QVERIFY(dlg.editor()->applyRowPattern("RGB, W, RGB, W, RGB"));
        dlg.accept();
    }
    Scene *scene = new Scene(m_doc);
    scene->addFixture(bar->id());
    m_doc->addFunction(scene);
    QLCPalette *pal = new QLCPalette(QLCPalette::Effect, m_doc);
    pal->setScriptPath(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/matrix-bars.js");
    QVERIFY(m_doc->addPalette(pal));
    scene->addPalette(pal->id());

    EffectInstance inst(m_doc, scene->id(), pal->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    // The XL-450 as 15 x 5, each head in its cell...
    QCOMPARE(QSize(cols, rows), QSize(15, 5));
    QCOMPARE(cells.size(), 75);
    const HeadLayout map = m_doc->monitorProperties()->headLayoutFor(bar);
    for (const EffectInstance::EffectCell &c : cells)
        QCOMPARE(QPoint(c.col, c.row), map.cellOf(c.head));
    // ...so the effect sees RGB rows of colour and W rows of white.
    QJSValue fixtures = inst.buildFixturesArray(cells, cols, rows);
    for (int i = 0; i < cells.size(); ++i)
    {
        const QJSValue o = fixtures.property(uint(i));
        const bool whiteRow = cells.at(i).row % 2 == 1;
        QCOMPARE(o.property("headWhite").toBool(), whiteRow);
        QCOMPARE(o.property("headRGB").toBool(), !whiteRow);
    }

    // Edited again (flipped top to bottom: rows the same, order reversed):
    // the canvas follows at once.
    {
        LedLayoutDialog dlg(m_doc, bar->id());
        dlg.editor()->flipVertically();
        dlg.accept();
    }
    const QList<EffectInstance::EffectCell> again = inst.effectiveCells(cols, rows);
    for (const EffectInstance::EffectCell &c : again)
        if (c.head == 0)
            QCOMPARE(QPoint(c.col, c.row), QPoint(0, 4));
}

void LedLayout_Test::thePlotDrawsHeadsWhereTheLayoutSays()
{
    Fixture *bar = addXl450("Bar");
    QWidget parent;
    Monitor *mon = new Monitor(&parent, m_doc);
    MonitorGraphicsView *gv = mon->findChild<MonitorGraphicsView *>();
    gv->resize(1000, 700);
    QVERIFY(dropFixtureAtCentre(gv, bar->id()));
    MonitorFixtureItem *item = gv->m_fixtures.value(bar->id());
    QVERIFY(item != nullptr);
    auto top = [item](int head) { return item->m_heads.at(head)->m_item->boundingRect().center().y(); };
    // Head order: head 16 (RGB) is on the 2nd row, head 46 (W 1) on the 4th.
    QVERIFY(top(15) < top(45));
    {
        LedLayoutDialog dlg(m_doc, bar->id());
        QVERIFY(dlg.editor()->applyRowPattern("RGB, W, RGB, W, RGB"));
        dlg.accept();            // the plot hears of it: no reload needed
    }
    // Built: W 1 on the 2nd row, head 16 on the 3rd.
    QVERIFY(top(45) < top(15));
    QCOMPARE(top(45), top(59));             // W 1 and W 15 on one row
    QVERIFY(top(0) < top(45));
    delete mon;
}

void LedLayout_Test::theSplitWizardSharesTheEditor()
{
    QList<quint32> ids;
    ids << addXl450("FART-SR-1")->id() << addXl450("FART-SR-2")->id();
    HeadTypesDialog dlg(m_doc, ids, "FART-SR");
    QVERIFY(dlg.m_editor != nullptr);
    QVERIFY(dlg.m_editor->applyRowPattern("RGB, W, RGB, W, RGB"));
    QVERIFY(dlg.m_saveDefault->isChecked());
    dlg.m_saveDefault->setChecked(false);       // this show only
    dlg.accept();
    const QString key = "Warmdance|XL-450|165ch";
    QCOMPARE(cellText(m_doc->monitorProperties()->headLayoutFor(m_doc->fixture(ids.first()))),
             QString(kDanceHysteriaXl450));
    QVERIFY(!HeadLayoutLibrary::load(key).isValid());
    QCOMPARE(dlg.createdGroups().size(), 2);
}

/*****************************************************************************
 * The real show
 *****************************************************************************/

void LedLayout_Test::theRealShowsXl450()
{
    const QString path = qEnvironmentVariable("LEDLAYOUT_SHOW");
    if (path.isEmpty())
        QSKIP("set LEDLAYOUT_SHOW to a COPY of a show with an XL-450 (Dance Hysteria 0.6)");
    const QString shots = qEnvironmentVariable("LEDLAYOUT_SHOTS");

    App *app = new App();
    app->init();
    app->resize(1280, 800);
    QVERIFY(app->loadXML(path) == QFile::NoError);
    app->doc()->resetModified();
    Doc *doc = app->doc();

    Fixture *bar = nullptr;
    foreach (Fixture *f, doc->fixtures())
        if (f->fixtureDef() != nullptr && f->fixtureDef()->model() == "XL-450"
            && f->fixtureMode() != nullptr && f->fixtureMode()->name() == "165ch")
        {
            bar = f;
            break;
        }
    QVERIFY2(bar != nullptr, "no XL-450 in 165ch mode in that show");
    const QString key = HeadLayout::keyFor(bar);
    QVERIFY(doc->monitorProperties()->hasShowHeadLayout(key));
    const HeadLayout stored = doc->monitorProperties()->headLayoutFor(bar);
    QCOMPARE(cellText(stored), QString(kDanceHysteriaXl450));

    LedLayoutDialog dlg(doc, bar->id());
    dlg.resize(760, 560);
    dlg.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dlg));
    QCOMPARE(dlg.editor()->layout(), stored);
    QCOMPARE(dlg.editor()->rowPatternText(), QString("RGB, W, RGB, W, RGB"));
    auto shoot = [&](const QString &name) {
        if (shots.isEmpty())
            return;
        QDir().mkpath(shots);
        QCoreApplication::processEvents();
        QVERIFY(dlg.grab().save(QDir(shots).absoluteFilePath(name)));
    };
    shoot("xl450-stored.png");

    // Head order, then the rows typed: the same map as the show keeps.
    dlg.editor()->resetHeadOrder();
    shoot("xl450-head-order.png");
    dlg.editor()->m_rowsEdit->setText("RGB, W, RGB, W, RGB");
    QVERIFY(dlg.editor()->applyRowPattern("RGB, W, RGB, W, RGB"));
    QCOMPARE(dlg.editor()->layout(), stored);

    // The preview part way along the first white row.
    dlg.editor()->lightHead(51);
    shoot("xl450-preview.png");
    dlg.editor()->lightHead(-1);
    QVERIFY(!dlg.editor()->applyRowPattern("RGB, RGB, W, W, W"));
    shoot("xl450-refused.png");

    dlg.reject();
    app->doc()->resetModified();
    delete app;
}

int main(int argc, char **argv)
{
    // A home of its own: nothing here touches the operator's folders.
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    LedLayout_Test test;
    return QTest::qExec(&test, argc, argv);
}
