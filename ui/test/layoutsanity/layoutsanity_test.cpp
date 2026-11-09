/*
  Q Light Controller Plus - Test Unit
  layoutsanity_test.cpp

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
#include <QAbstractButton>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFocusFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRubberBand>
#include <QScrollArea>
#include <QSizeGrip>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "refinepanel.h"
#undef protected
#undef private

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcfixturedef.h"
#include "qlcfixturedefcache.h"
#include "qlcfile.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "chaser.h"
#include "collection.h"
#include "efx.h"
#include "rgbmatrix.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "effectscriptcache.h"
#include "inputoutputmap.h"

#include "aboutbox.h"
#include "addfixture.h"
#include "addrgbpanel.h"
#include "appsettings.h"
#include "assignhotkey.h"
#include "channelsselection.h"
#include "chasereditor.h"
#include "collectioneditor.h"
#include "connectioneditor.h"
#include "controllertestdialog.h"
#include "controlmappanel.h"
#include "createfixturegroup.h"
#include "cuerunner.h"
#include "cuesheetdialog.h"
#include "definitionupdatesdialog.h"
#include "dmxmonitorwindow.h"
#include "effectspage.h"
#include "efxeditor.h"
#include "fixturegroupeditor.h"
#include "fixtureremap.h"
#include "fixtureselection.h"
#include "focuscallsdialog.h"
#include "formationeditor.h"
#include "functionselection.h"
#include "functionwizard.h"
#include "helpbrowser.h"
#include "inputoutputpatcheditor.h"
#include "livecapturedialog.h"
#include "lookeditor.h"
#include "networkreportdialog.h"
#include "palettemanager.h"
#include "patchinventorydialog.h"
#include "patchlistdialog.h"
#include "powerdistributiondialog.h"
#include "programmingmanager.h"
#include "rgbmatrixeditor.h"
#include "sceneeditor.h"
#include "showdifferencesdialog.h"
#include "showreviewpanel.h"
#include "showsettingsdialog.h"
#include "showstatusdialog.h"
#include "showtreeview.h"
#include "simpledesk.h"
#include "speedmasterspanel.h"
#include "structurestudioview.h"
#include "universeoutputview.h"
#include "venuerigdialog.h"
#include "whyonpanel.h"
#include "gobowheelsdialog.h"
#include "workbench.h"
#include "creategroupsdialog.h"
#include "headtypesdialog.h"
#include "stagegroupdialog.h"
#include "showconverter.h"
#include "networktopologydialog.h"
#include "studiogroupeditor.h"
#include "lineplanview.h"
#include "focusmodepanel.h"
#include "scenegrouplooks.h"
#include "outputrulesdialog.h"
#include "powerprofiledialog.h"
#include "monitorbackgroundselection.h"

#include "layoutsanity_test.h"

namespace
{

/****************************************************************************
 * The checker
 ****************************************************************************/

// Fonts differ by a pixel or two between macOS and Linux (and hinting):
// nothing smaller than this is called an overlap or a cut.
const int kTol = 3;

// The app window's "min": a small laptop's usable area.
const QSize kSmallestWindow(1024, 640);

/** A widget whose insides are its own business (a combo's line edit, a
 *  view's viewport, a spin box's arrows): checked as one box. */
bool isAtomic(const QWidget *w)
{
    if (qobject_cast<const QScrollArea *>(w) != nullptr)
        return false;                       // its content is laid out like any page
    return qobject_cast<const QAbstractButton *>(w) != nullptr
        || qobject_cast<const QLabel *>(w) != nullptr
        || qobject_cast<const QComboBox *>(w) != nullptr
        || qobject_cast<const QAbstractSpinBox *>(w) != nullptr
        || qobject_cast<const QLineEdit *>(w) != nullptr
        || qobject_cast<const QAbstractSlider *>(w) != nullptr
        || qobject_cast<const QProgressBar *>(w) != nullptr
        || qobject_cast<const QTabBar *>(w) != nullptr
        || qobject_cast<const QMenuBar *>(w) != nullptr
        || qobject_cast<const QAbstractScrollArea *>(w) != nullptr;
}

/** Never part of a layout: drawn over the rest on purpose. */
bool isFloating(const QWidget *w)
{
    return qobject_cast<const QSizeGrip *>(w) != nullptr
        || qobject_cast<const QFocusFrame *>(w) != nullptr
        || qobject_cast<const QRubberBand *>(w) != nullptr
        || qobject_cast<const QSplitterHandle *>(w) != nullptr
        || w->property("layoutSanityOverlay").toBool()
        || w->inherits("QToolBarExtension")
        || w->inherits("QTipLabel");
}

QString nameOf(const QWidget *w)
{
    QString s = QString::fromLatin1(w->metaObject()->className());
    if (!w->objectName().isEmpty() && !w->objectName().startsWith(QLatin1String("qt_")))
        s += QLatin1Char('#') + w->objectName();
    return s;
}

/** Where a widget is: its class#name and the nearest named ancestors, enough
 *  to find it in the source. */
QString pathOf(const QWidget *w, const QWidget *top)
{
    QStringList parts;
    parts << nameOf(w);
    int named = 0;
    for (const QWidget *p = w->parentWidget(); p != nullptr && p != top && named < 2;
         p = p->parentWidget())
        if (!p->objectName().isEmpty() && !p->objectName().startsWith(QLatin1String("qt_")))
        {
            parts.prepend(nameOf(p));
            ++named;
        }
    QString text;
    if (const QLabel *l = qobject_cast<const QLabel *>(w))
        text = l->text();
    else if (const QAbstractButton *b = qobject_cast<const QAbstractButton *>(w))
        text = b->text();
    if (!text.isEmpty())
    {
        QTextDocument doc;
        doc.setHtml(text);
        text = doc.toPlainText().simplified();
        if (text.size() > 32)
            text = text.left(30) + QStringLiteral("..");
        parts.last() += QStringLiteral(" \"%1\"").arg(text);
    }
    return parts.join(QLatin1Char('/'));
}

QString rectText(const QRect &r)
{
    return QStringLiteral("(%1,%2 %3x%4)").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

struct Item
{
    QWidget *w;
    QRect full;       // in the top-level's coordinates
    QRect seen;       // that, cut to every ancestor's rect: what is on screen
    bool scrolled;    // inside a scroll area's viewport: may run past it
    bool placed;      // put where the user dropped it (a Virtual Console widget)
};

/** True when @a w can't be seen in @a top though it says it is visible: a
 *  hidden ancestor, a page of a stack not on show. */
bool shown(const QWidget *w, const QWidget *top)
{
    return w->isVisibleTo(top) && !w->isWindow();
}

/** One finding: its kind, the screen-independent text a known-issue row
 *  matches, and the rect to mark on the picture. */
struct Finding
{
    QString text;
    QRect mark;
};

QList<Finding> inspect(QWidget *top)
{
    QList<Item> items;
    const QList<QWidget *> all = top->findChildren<QWidget *>();
    for (QWidget *w : all)
    {
        if (!shown(w, top) || isFloating(w))
            continue;
        bool skip = false, scrolled = false, placed = w->inherits("VCWidget");
        QRect seen(w->mapTo(top, QPoint(0, 0)), w->size());
        const QRect full = seen;
        for (QWidget *p = w->parentWidget(); p != nullptr && p != top; p = p->parentWidget())
        {
            if (isAtomic(p) || isFloating(p) || p->isWindow())
            {
                skip = true;
                break;
            }
            seen &= QRect(p->mapTo(top, QPoint(0, 0)), p->size());
            if (p->inherits("VCWidget"))
                placed = true;
            if (p->parentWidget() != nullptr
                && qobject_cast<QAbstractScrollArea *>(p->parentWidget()) != nullptr
                && static_cast<QAbstractScrollArea *>(p->parentWidget())->viewport() == p)
                scrolled = true;
        }
        if (skip)
            continue;
        seen &= top->rect();
        items << Item{ w, full, seen, scrolled, placed };
    }

    QList<Finding> out;
    QList<int> leaves;
    for (int i = 0; i < items.size(); ++i)
    {
        QWidget *w = items.at(i).w;
        bool leaf = isAtomic(w);
        if (!leaf)
        {
            leaf = true;
            for (QObject *c : w->children())
            {
                QWidget *cw = qobject_cast<QWidget *>(c);
                if (cw != nullptr && shown(cw, top) && !isFloating(cw))
                {
                    leaf = false;
                    break;
                }
            }
        }
        if (leaf)
            leaves << i;
    }

    for (int i : leaves)
    {
        const Item &it = items.at(i);
        QWidget *w = it.w;
        const QSize hint = w->sizeHint();

        // Squeezed to nothing (a collapsed splitter pane is the user's own).
        if ((w->width() <= 0 || w->height() <= 0) && hint.isValid() && !hint.isEmpty()
            && w->maximumWidth() > 0 && w->maximumHeight() > 0
            && qobject_cast<QSplitter *>(w->parentWidget()) == nullptr)
        {
            out << Finding{ QStringLiteral("zero-size %1 %2").arg(pathOf(w, top), rectText(it.full)), it.full };
            continue;
        }
        if (it.full.isEmpty())
            continue;
        if (it.seen.isEmpty() && !it.scrolled)
        {
            out << Finding{ QStringLiteral("off-window %1 %2").arg(pathOf(w, top), rectText(it.full)), it.full };
            continue;
        }

        // Cut off by its window or a parent, with no scroll bar to reach the rest.
        if (!it.scrolled && (it.full.width() - it.seen.width() > kTol
                             || it.full.height() - it.seen.height() > kTol))
            out << Finding{ QStringLiteral("clipped %1 %2 shows %3")
                                .arg(pathOf(w, top), rectText(it.full), rectText(it.seen)), it.full };

        // Text cut short: a label or button smaller than its words.
        const QFontMetrics fm = w->fontMetrics();
        int needW = -1, needH = -1;
        if (QLabel *l = qobject_cast<QLabel *>(w))
        {
            const bool plain = l->textFormat() == Qt::PlainText
                || (l->textFormat() == Qt::AutoText && !Qt::mightBeRichText(l->text()));
            if (!l->text().isEmpty() && !l->wordWrap() && plain
                && l->sizePolicy().horizontalPolicy() != QSizePolicy::Ignored
                && !l->text().contains(QLatin1Char('\n')))
            {
                const QMargins m = l->contentsMargins();
                QString t = l->text();
                if (l->buddy() != nullptr)
                    t.remove(QRegularExpression(QStringLiteral("&(?!&)")));
                needW = fm.horizontalAdvance(t) + m.left() + m.right() + 2 * l->margin();
                needH = fm.height() + m.top() + m.bottom();
            }
        }
        else if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
        {
            QString t = b->text();
            t.remove(QLatin1Char('&'));
            const QToolButton *tb = qobject_cast<QToolButton *>(b);
            const bool textShown = !t.isEmpty()
                && (tb == nullptr || tb->toolButtonStyle() != Qt::ToolButtonIconOnly
                    || b->icon().isNull());
            if (textShown && !t.contains(QLatin1Char('\n')))
            {
                needW = fm.horizontalAdvance(t);
                if (!b->icon().isNull() && (tb == nullptr || tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon))
                    needW += b->iconSize().width() + 2;
                if (qobject_cast<QCheckBox *>(b) != nullptr || qobject_cast<QRadioButton *>(b) != nullptr)
                    needW += 12;               // the box / dot
                needH = fm.height() - 2;
            }
        }
        else if (qobject_cast<QComboBox *>(w) != nullptr || qobject_cast<QLineEdit *>(w) != nullptr
                 || qobject_cast<QAbstractSpinBox *>(w) != nullptr)
        {
            needH = fm.height() - 2;            // the text's own height, at least
        }
        if (needW > 0 && w->width() + kTol < needW)
            out << Finding{ QStringLiteral("text-cut %1 is %2 wide, needs %3")
                                .arg(pathOf(w, top)).arg(w->width()).arg(needW), it.full };
        else if (needH > 0 && w->height() + kTol < needH)
            out << Finding{ QStringLiteral("text-cut %1 is %2 high, needs %3")
                                .arg(pathOf(w, top)).arg(w->height()).arg(needH), it.full };
    }

    // Two things in the same place.
    for (int a = 0; a < leaves.size(); ++a)
    {
        const Item &ia = items.at(leaves.at(a));
        if (ia.seen.isEmpty())
            continue;
        for (int b = a + 1; b < leaves.size(); ++b)
        {
            const Item &ib = items.at(leaves.at(b));
            // The Virtual Console is the user's own drawing: where they put
            // two widgets is theirs to say.
            if (ia.placed && ib.placed)
                continue;
            const QRect x = ia.seen & ib.seen;
            if (x.width() <= kTol || x.height() <= kTol)
                continue;
            out << Finding{ QStringLiteral("overlap %1 %2 with %3 %4")
                                .arg(pathOf(ia.w, top), rectText(ia.seen),
                                     pathOf(ib.w, top), rectText(ib.seen)), x };
        }
    }
    return out;
}

void settle(int rounds = 3)
{
    for (int i = 0; i < rounds; ++i)
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }
}

/** Dismiss anything modal a screen opens on its own (a "no fixtures" note):
 *  never wait for a human. */
void dismissModals()
{
    const auto widgets = QApplication::topLevelWidgets();
    for (QWidget *w : widgets)
        if (QDialog *d = qobject_cast<QDialog *>(w))
            if (d->isVisible() && d->isModal())
                d->reject();
}

/****************************************************************************
 * The screens
 ****************************************************************************/

/** One screen: how to build it as the app does (a top-level widget; owned
 *  by the test unless it is the app window), and anything to do once it is
 *  shown (pick a node, open a look). */
struct Screen
{
    QString name;
    std::function<QWidget *(App *)> make;
    std::function<void(App *, QWidget *)> prepare;
    bool owned = true;
};

/** Known layout problems, each with its TODO line: reported, not failed.
 *  A row no screen hits any more is called out at the end, to be removed.
 *  match: screen name prefix, size ("" = any) and a substring of the finding. */
struct Known
{
    const char *screen;
    const char *size;
    const char *finding;
    const char *todo;
};

const QList<Known> &knownIssues()
{
    static const QList<Known> list = {
    };
    return list;
}

const Known *knownFor(const QString &screen, const QString &size, const QString &finding)
{
    for (const Known &k : knownIssues())
        if (screen.startsWith(QLatin1String(k.screen))
            && (k.size[0] == '\0' || size == QLatin1String(k.size))
            && finding.contains(QLatin1String(k.finding)))
            return &k;
    return nullptr;
}

Doc *docOf(App *app) { return app->doc(); }

Doc *g_ringDoc = nullptr;

template <typename T>
T *firstFunction(Doc *doc, Function::Type type)
{
    for (Function *f : doc->functions())
        if (f->type() == type)
            return qobject_cast<T *>(f);
    return nullptr;
}

quint32 paletteOf(Doc *doc, QLCPalette::PaletteType type)
{
    for (QLCPalette *p : doc->palettes())
        if (p->type() == type)
            return p->id();
    // None in the show: one made as Build's + would.
    QLCPalette *p = new QLCPalette(type, doc);
    p->setName(QStringLiteral("Layout %1").arg(QLCPalette::typeToString(type)));
    if (type == QLCPalette::Color)
        p->setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor()));
    doc->addPalette(p);
    return p->id();
}

QList<quint32> someFixtures(Doc *doc, int n = 4)
{
    QList<quint32> ids;
    for (Fixture *f : doc->fixtures())
        if (ids.size() < n)
            ids << f->id();
    return ids;
}

/** A tab of the app window, brought forward as a click on it would. */
Screen tab(const QString &label, std::function<void(App *, QWidget *)> prepare = nullptr)
{
    Screen s;
    s.name = QStringLiteral("tab %1").arg(label);
    s.owned = false;
    s.make = [label](App *app) -> QWidget * {
        // Functions and Shows are only in View > Workspace > Everything.
        const App::WorkspaceView view = App::workspaceViewTabs(App::ConsoleView).contains(label)
            ? App::ConsoleView : App::EverythingView;
        if (app->workspaceView() != view)
            app->setWorkspaceView(view, false);
        for (int i = 0; i < app->m_tab->count(); ++i)
            if (app->m_tab->tabText(i) == label)
            {
                app->m_tab->setCurrentIndex(i);
                return app;
            }
        return nullptr;
    };
    s.prepare = prepare;
    return s;
}

template <typename F>
Screen screen(const QString &name, F make, std::function<void(App *, QWidget *)> prepare = nullptr)
{
    Screen s;
    s.name = name;
    s.make = make;
    s.prepare = prepare;
    return s;
}

/** A look in the Workbench's Look Editor, as Build opens it. */
Screen lookScreen(const QString &name, QLCPalette::PaletteType type)
{
    return screen(QStringLiteral("Workbench look %1").arg(name), [type](App *app) -> QWidget * {
        Workbench *wb = new Workbench(docOf(app));
        wb->editPalette(paletteOf(docOf(app), type));
        return wb;
    });
}

/** The colour look the user found its ring row under (Q207): a fixture with
 *  a colour wheel and an LED ring, so "Wheel / ring" shows. A show of its
 *  own, as lookeditor_test builds it. */
QWidget *ringColourLook(App *app)
{
    Doc *doc = new Doc(nullptr);
    QDir dir(QStringLiteral(TEST_FIXTURE_DIR));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QStringLiteral("*%1").arg(KExtFixture));
    doc->fixtureDefCache()->load(dir);
    QLCFixtureDef *def = doc->fixtureDefCache()->fixtureDef(QStringLiteral("Branson"),
                                                            QStringLiteral("LED Movinghead+Circle (2 heads)"));
    if (def != nullptr)
    {
        Fixture *f = new Fixture(doc);
        f->setFixtureDefinition(def, def->mode(QStringLiteral("12 Channel")));
        doc->addFixture(f);
    }
    QLCPalette *look = new QLCPalette(QLCPalette::Color, doc);
    look->setName(QStringLiteral("New Color"));
    look->setValue(QLCPalette::colorToString(QColor(Qt::blue), QColor()));
    doc->addPalette(look);
    Q_UNUSED(app);

    // Outlives the screen (its editors hold it to the end): the test's.
    delete g_ringDoc;
    g_ringDoc = doc;
    Workbench *wb = new Workbench(doc);
    wb->editPalette(look->id());
    return wb;
}

QList<Screen> screens()
{
    QList<Screen> list;

    // The app window, tab by tab.
    list << tab(QStringLiteral("Connections"))
         << tab(QStringLiteral("Fixtures"))
         << tab(QStringLiteral("Lighting Studio"))
         << tab(QStringLiteral("Build"))
         << tab(QStringLiteral("Build"), [](App *app, QWidget *) {
                if (Scene *s = firstFunction<Scene>(docOf(app), Function::SceneType))
                    ProgrammingManager::openFunction(s->id());
            })
         << tab(QStringLiteral("Show Tree"))
         << tab(QStringLiteral("Show Tree"), [](App *app, QWidget *) {
                // A cue, as a click on it opens it (the Workbench docks in).
                ShowTreeController *st = docOf(app)->showTree();
                for (const ShowTree::Production &p : st->productions())
                    for (const ShowTree::CueList &l : p.lists)
                        for (const ShowTree::Cue &c : l.cues)
                            if (!c.layers.isEmpty())
                            {
                                ShowTreeView::instance()->selectNode(c.id);
                                return;
                            }
            })
         << tab(QStringLiteral("Show Tree"), [](App *app, QWidget *) {
                // A layer: the layer editor.
                ShowTreeController *st = docOf(app)->showTree();
                for (const ShowTree::Production &p : st->productions())
                    for (const ShowTree::CueList &l : p.lists)
                        for (const ShowTree::Cue &c : l.cues)
                            if (!c.layers.isEmpty())
                            {
                                ShowTreeView::instance()->selectNode(c.layers.first().id, c.id);
                                return;
                            }
            })
         << tab(QStringLiteral("Functions"))
         << tab(QStringLiteral("Shows"))
         << tab(QStringLiteral("Virtual Console"))
         << tab(QStringLiteral("Simple Desk"))
         << tab(QStringLiteral("Run"));
    // The two Build and three Show Tree rows: told apart by their state.
    list[4].name += QStringLiteral(" scene open");
    list[6].name += QStringLiteral(" cue");
    list[7].name += QStringLiteral(" layer");


    // The Look Editor, page by page (in the Workbench, as Build shows it).
    list << lookScreen(QStringLiteral("colour"), QLCPalette::Color)
         << screen(QStringLiteral("Workbench look colour, wheel+ring"), ringColourLook)
         << lookScreen(QStringLiteral("dimmer"), QLCPalette::Dimmer)
         << lookScreen(QStringLiteral("aim"), QLCPalette::Aim)
         << lookScreen(QStringLiteral("pan/tilt"), QLCPalette::PanTilt)
         << lookScreen(QStringLiteral("shutter"), QLCPalette::Shutter)
         << lookScreen(QStringLiteral("strobe"), QLCPalette::Strobe)
         << lookScreen(QStringLiteral("gobo"), QLCPalette::Gobo)
         << lookScreen(QStringLiteral("zoom"), QLCPalette::Zoom)
         << lookScreen(QStringLiteral("beam"), QLCPalette::Beam)
         << lookScreen(QStringLiteral("effect"), QLCPalette::Effect)
         << lookScreen(QStringLiteral("formation"), QLCPalette::Formation);

    // Workbench with a scene, and the stock function editors.
    list << screen(QStringLiteral("Workbench scene"), [](App *app) -> QWidget * {
                Workbench *wb = new Workbench(docOf(app));
                if (Scene *s = firstFunction<Scene>(docOf(app), Function::SceneType))
                    wb->load(s->id());
                return wb;
            })
         << screen(QStringLiteral("Scene editor"), [](App *app) -> QWidget * {
                Scene *s = firstFunction<Scene>(docOf(app), Function::SceneType);
                return s == nullptr ? nullptr : new SceneEditor(nullptr, s, docOf(app), false);
            })
         << screen(QStringLiteral("Chaser editor"), [](App *app) -> QWidget * {
                Chaser *c = firstFunction<Chaser>(docOf(app), Function::ChaserType);
                return c == nullptr ? nullptr : new ChaserEditor(nullptr, c, docOf(app));
            })
         << screen(QStringLiteral("Collection editor"), [](App *app) -> QWidget * {
                Collection *c = firstFunction<Collection>(docOf(app), Function::CollectionType);
                return c == nullptr ? nullptr : new CollectionEditor(nullptr, c, docOf(app));
            })
         << screen(QStringLiteral("EFX editor"), [](App *app) -> QWidget * {
                EFX *e = new EFX(docOf(app));
                docOf(app)->addFunction(e);
                return new EFXEditor(nullptr, e, docOf(app));
            })
         << screen(QStringLiteral("RGB Matrix editor"), [](App *app) -> QWidget * {
                RGBMatrix *m = new RGBMatrix(docOf(app));
                docOf(app)->addFunction(m);
                return new RGBMatrixEditor(nullptr, m, docOf(app));
            });

    // Panels.
    list << screen(QStringLiteral("Formation editor"), [](App *app) -> QWidget * {
                return new FormationEditor(docOf(app));
            })
         << screen(QStringLiteral("Show Review panel"), [](App *app) -> QWidget * {
                return new ShowReviewPanel(docOf(app));
            })
         << screen(QStringLiteral("Why is this on panel"), [](App *app) -> QWidget * {
                // As a right-click on a fixture opens it.
                const QList<quint32> ids = someFixtures(docOf(app), 1);
                return ids.isEmpty() ? nullptr : WhyOnPanel::showFor(docOf(app), ids.first());
            })
         << screen(QStringLiteral("Effects page"), [](App *app) -> QWidget * {
                return new EffectsPage(docOf(app));
            })
         << screen(QStringLiteral("Speed masters panel"), [](App *app) -> QWidget * {
                return new SpeedMastersPanel(docOf(app));
            })
         << screen(QStringLiteral("Control map panel"), [](App *app) -> QWidget * {
                return new ControlMapPanel(docOf(app));
            })
         << screen(QStringLiteral("Palette manager"), [](App *app) -> QWidget * {
                return new PaletteManager(docOf(app));
            })
         << screen(QStringLiteral("Universe output view"), [](App *app) -> QWidget * {
                return new UniverseOutputView(docOf(app));
            })
         << screen(QStringLiteral("Cue runner compact"), [](App *app) -> QWidget * {
                return new CueRunner(docOf(app), CueRunner::Compact);
            })
         << screen(QStringLiteral("Cue runner list"), [](App *app) -> QWidget * {
                return new CueRunner(docOf(app), CueRunner::List);
            })
         << screen(QStringLiteral("Studio stage overview"), [](App *app) -> QWidget * {
                return new StructureStudioView(docOf(app), StructureStudioView::StageKind, 0);
            });

    // Dialogs.
    list << screen(QStringLiteral("Add Fixture"), [](App *app) -> QWidget * {
                return new AddFixture(nullptr, docOf(app));
            })
         << screen(QStringLiteral("Add Fixture WLED pixels"), [](App *app) -> QWidget * {
                /* A WLED Lantern on Network (ArtNet/sACN) in its Pixels mode,
                   400 LEDs: the LEDs row, what the device said and the
                   universes it takes. */
                Doc *doc = docOf(app);
                if (doc->fixtureDefCache()->fixtureDef("WLED", "Lantern") == nullptr)
                {
                    QLCFixtureDef *def = new QLCFixtureDef();
                    if (def->loadXML(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/WLED/WLED-Lantern.qxf")
                            != QFile::NoError || !doc->fixtureDefCache()->addFixtureDef(def))
                        delete def;
                }
                AddFixture *af = new AddFixture(nullptr, doc);
                const QList<QTreeWidgetItem *> found =
                    af->m_tree->findItems("Lantern", Qt::MatchExactly | Qt::MatchRecursive);
                if (found.isEmpty())
                    return af;
                af->m_tree->setCurrentItem(found.first());
                if (QComboBox *conn = af->findChild<QComboBox *>("m_connectionCombo"))
                    conn->setCurrentIndex(conn->findData(
                        AddFixture::connectionKey(Fixture::Network, Fixture::ArtNetTransport)));
                // No IP: nothing is asked of a device from here.
                const int idx = af->m_modeCombo->findText(QStringLiteral("Pixels (Multi RGB)"));
                if (idx >= 0)
                {
                    if (QSpinBox *leds = af->findChild<QSpinBox *>("m_ledsSpin"))
                        leds->setValue(400);
                    af->m_modeCombo->setCurrentIndex(idx);
                    QMetaObject::invokeMethod(af, "slotModeActivated", Q_ARG(int, idx));
                }
                return af;
            })
         << screen(QStringLiteral("Patch inventory"), [](App *app) -> QWidget * {
                return new PatchInventoryDialog(docOf(app), someFixtures(docOf(app)));
            })
         << screen(QStringLiteral("Gobo Wheels"), [](App *app) -> QWidget * {
                // As a right-click on several lights of one kind opens it.
                Doc *doc = docOf(app);
                QList<quint32> ids;
                for (quint32 id : GoboWheelsDialog::withGoboWheels(doc, someFixtures(doc, 1000)))
                {
                    if (GoboWheelsDialog::whyNot(doc, QList<quint32>(ids) << id).isEmpty() && ids.size() < 3)
                        ids << id;
                }
                return ids.isEmpty() ? nullptr : new GoboWheelsDialog(doc, ids);
            })
         << screen(QStringLiteral("Patch list"), [](App *app) -> QWidget * {
                return new PatchListDialog(docOf(app));
            })
         << screen(QStringLiteral("Connection editor"), [](App *app) -> QWidget * {
                return new ConnectionEditor(docOf(app), 0);
            })
         << screen(QStringLiteral("Universe patch editor"), [](App *app) -> QWidget * {
                return new InputOutputPatchEditor(nullptr, 0, docOf(app)->inputOutputMap(), docOf(app));
            })
         << screen(QStringLiteral("Preferences"), [](App *app) -> QWidget * {
                return new AppSettings(app);
            })
         << screen(QStringLiteral("Show Settings"), [](App *app) -> QWidget * {
                return new ShowSettingsDialog(docOf(app));
            })
         << screen(QStringLiteral("Show Status"), [](App *) -> QWidget * {
                return new ShowStatusDialog();
            })
         << screen(QStringLiteral("Show differences"), [](App *app) -> QWidget * {
                return new ShowDifferencesDialog(docOf(app));
            })
         << screen(QStringLiteral("Fixture group editor"), [](App *app) -> QWidget * {
                const QList<FixtureGroup *> groups = docOf(app)->fixtureGroups();
                return groups.isEmpty() ? nullptr
                                        : new FixtureGroupEditor(groups.first(), docOf(app), nullptr);
            })
         << screen(QStringLiteral("Create fixture group"), [](App *) -> QWidget * {
                return new CreateFixtureGroup(nullptr);
            })
         << screen(QStringLiteral("Fixture selection"), [](App *app) -> QWidget * {
                return new FixtureSelection(nullptr, docOf(app));
            })
         << screen(QStringLiteral("Function selection"), [](App *app) -> QWidget * {
                return new FunctionSelection(nullptr, docOf(app));
            })
         << screen(QStringLiteral("Channels selection"), [](App *app) -> QWidget * {
                return new ChannelsSelection(docOf(app));
            })
         << screen(QStringLiteral("Function wizard"), [](App *app) -> QWidget * {
                return new FunctionWizard(nullptr, docOf(app));
            })
         << screen(QStringLiteral("Fixture remap"), [](App *app) -> QWidget * {
                return new FixtureRemap(docOf(app));
            })
         << screen(QStringLiteral("Add RGB panel"), [](App *app) -> QWidget * {
                return new AddRGBPanel(nullptr, docOf(app));
            })
         << screen(QStringLiteral("Focus calls"), [](App *app) -> QWidget * {
                return new FocusCallsDialog(docOf(app));
            })
         << screen(QStringLiteral("Venue rig"), [](App *app) -> QWidget * {
                return new VenueRigDialog(docOf(app), QStringLiteral("Layout Hall"));
            })
         << screen(QStringLiteral("Power distribution"), [](App *app) -> QWidget * {
                return new PowerDistributionDialog(docOf(app));
            })
         << screen(QStringLiteral("Network report"), [](App *app) -> QWidget * {
                return new NetworkReportDialog(docOf(app));
            })
         << screen(QStringLiteral("DMX monitor"), [](App *app) -> QWidget * {
                return new DmxMonitorWindow(docOf(app));
            })
         << screen(QStringLiteral("Controller test"), [](App *app) -> QWidget * {
                return new ControllerTestDialog(docOf(app));
            })
         << screen(QStringLiteral("Definition updates"), [](App *app) -> QWidget * {
                return new DefinitionUpdatesDialog(docOf(app));
            })
         << screen(QStringLiteral("Live capture"), [](App *app) -> QWidget * {
                return new LiveCaptureDialog(docOf(app));
            })
         << screen(QStringLiteral("Cue sheet"), [](App *app) -> QWidget * {
                const QList<ShowTree::Production> &p = docOf(app)->showTree()->productions();
                return p.isEmpty() ? nullptr : new CueSheetDialog(docOf(app), p.first().id);
            })
         << screen(QStringLiteral("Create groups"), [](App *app) -> QWidget * {
                return new CreateGroupsDialog(docOf(app), someFixtures(docOf(app), 8));
            })
         << screen(QStringLiteral("Head types"), [](App *app) -> QWidget * {
                return new HeadTypesDialog(docOf(app), someFixtures(docOf(app), 8), QStringLiteral("Layout"));
            })
         << screen(QStringLiteral("Stage group"), [](App *app) -> QWidget * {
                return new StageGroupDialog(docOf(app), someFixtures(docOf(app), 8), QStringLiteral("Layout"));
            })
         << screen(QStringLiteral("Output rules"), [](App *app) -> QWidget * {
                return new OutputRulesDialog(docOf(app), someFixtures(docOf(app)));
            })
         << screen(QStringLiteral("Power profile"), [](App *app) -> QWidget * {
                return new PowerProfileDialog(docOf(app), someFixtures(docOf(app)));
            })
         << screen(QStringLiteral("Show converter"), [](App *app) -> QWidget * {
                return new ShowConverter(docOf(app));
            })
         << screen(QStringLiteral("Network topology"), [](App *app) -> QWidget * {
                return new NetworkTopologyDialog(docOf(app));
            })
         << screen(QStringLiteral("Studio group editor"), [](App *app) -> QWidget * {
                const QList<FixtureGroup *> groups = docOf(app)->fixtureGroups();
                return groups.isEmpty() ? nullptr : new StudioGroupEditor(docOf(app), groups.first()->id());
            })
         << screen(QStringLiteral("Line plan"), [](App *app) -> QWidget * {
                return new LinePlanView(docOf(app));
            })
         << screen(QStringLiteral("Refine panel"), [](App *app) -> QWidget * {
                return RefinePanel::showPanel(docOf(app), nullptr);
            })
         << screen(QStringLiteral("Focus mode setup"), [](App *app) -> QWidget * {
                Scene *s = firstFunction<Scene>(docOf(app), Function::SceneType);
                return s == nullptr ? nullptr
                                    : new FocusModeSetupDialog(docOf(app), s->id(), someFixtures(docOf(app)));
            })
         << screen(QStringLiteral("Scene group looks"), [](App *app) -> QWidget * {
                Scene *s = firstFunction<Scene>(docOf(app), Function::SceneType);
                return s == nullptr ? nullptr : new SceneGroupLooks(s, docOf(app));
            })
         << screen(QStringLiteral("Studio background"), [](App *app) -> QWidget * {
                return new MonitorBackgroundSelection(nullptr, docOf(app));
            })
         << screen(QStringLiteral("Assign hot key"), [](App *) -> QWidget * {
                return new AssignHotKey(nullptr);
            })
         << screen(QStringLiteral("About"), [](App *) -> QWidget * {
                return new AboutBox(nullptr);
            })
         << screen(QStringLiteral("Help browser"), [](App *) -> QWidget * {
                return new HelpBrowser();
            });
    return list;
}

struct Size
{
    const char *name;
    QSize size;          // empty: the screen's minimum
};

const QList<Size> &sizes()
{
    static const QList<Size> list = {
        { "min", QSize() },
        { "laptop", QSize(1280, 800) },
        { "wide", QSize(1920, 1080) },
    };
    return list;
}

QString shotsDir()
{
    return qEnvironmentVariable("LAYOUT_SANITY_SHOTS");
}

void saveShot(QWidget *top, const QString &screen, const QString &size, const QList<Finding> &found)
{
    const QString dir = shotsDir();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QPixmap pm = top->grab();
    if (!found.isEmpty())
    {
        QPainter p(&pm);
        p.setPen(QPen(Qt::red, 2));
        for (const Finding &f : found)
            p.drawRect(f.mark.adjusted(1, 1, -1, -1));
    }
    QString file = screen + QLatin1Char('-') + size;
    file.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    pm.save(QDir(dir).filePath(file + QStringLiteral(".png")));
}

} // namespace

/****************************************************************************
 * Tests
 ****************************************************************************/

void LayoutSanity_Test::initTestCase()
{
    // A HOME of our own: never the real user's folders or settings.
    static QTemporaryDir home;
    QVERIFY(home.isValid());
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("USERPROFILE", home.path().toLocal8Bit());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    EffectScriptCache::setSystemDirectoriesOverride(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts",
                                                    QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts");

    m_app = new App();
    m_app->init();
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    // A finished course show: fixtures, groups, looks, scenes, a production.
    QCOMPARE(m_app->loadXML(QStringLiteral(COURSE_DIR "/shows/lesson-3-9-finish.qlcc")), QFile::NoError);
    m_app->doc()->resetModified();
    m_app->showNormal();
    settle();
}

void LayoutSanity_Test::cleanupTestCase()
{
    for (const Known &k : knownIssues())
    {
        const QString key = QStringLiteral("%1|%2|%3").arg(k.screen, k.size, k.finding);
        if (!m_seenKnown.contains(key))
            qWarning().noquote() << "layoutsanity: known issue no longer seen, remove its row:"
                                 << k.screen << k.size << k.finding << k.todo;
    }
    delete g_ringDoc;
    g_ringDoc = nullptr;
    if (m_app != nullptr)
    {
        m_app->doc()->resetModified();
        delete m_app;
        m_app = nullptr;
    }
}

void LayoutSanity_Test::theCheckerFindsWhatItShould()
{
    // Clean: a column of a label, a button and a stack with a hidden page.
    {
        QWidget top;
        QVBoxLayout *v = new QVBoxLayout(&top);
        v->addWidget(new QLabel(QStringLiteral("A label")));
        v->addWidget(new QPushButton(QStringLiteral("A button")));
        QStackedWidget *stack = new QStackedWidget;
        stack->addWidget(new QPushButton(QStringLiteral("On show")));
        stack->addWidget(new QPushButton(QStringLiteral("Hidden page, same place")));
        v->addWidget(stack);
        QScrollArea *scroll = new QScrollArea;
        QWidget *body = new QWidget;
        QVBoxLayout *bv = new QVBoxLayout(body);
        for (int i = 0; i < 20; ++i)
            bv->addWidget(new QPushButton(QStringLiteral("Row %1").arg(i)));
        scroll->setWidget(body);
        v->addWidget(scroll);
        // An overlay on purpose, marked.
        QLabel *badge = new QLabel(QStringLiteral("Overlay"), &top);
        badge->setProperty("layoutSanityOverlay", true);
        top.resize(300, 300);
        top.show();
        settle();
        badge->move(10, 10);
        badge->show();
        const QList<Finding> f = inspect(&top);
        QStringList t;
        for (const Finding &x : f)
            t << x.text;
        QVERIFY2(f.isEmpty(), qPrintable(t.join(QLatin1Char('\n'))));
    }

    // Broken: a button laid over a label, one hanging out of the window,
    // a label squeezed short of its words.
    {
        QWidget top;
        top.resize(200, 100);
        QLabel *under = new QLabel(QStringLiteral("Under"), &top);
        under->setGeometry(10, 10, 80, 24);
        QPushButton *over = new QPushButton(QStringLiteral("Over"), &top);
        over->setGeometry(20, 12, 80, 24);
        QPushButton *out = new QPushButton(QStringLiteral("Out"), &top);
        out->setGeometry(150, 60, 120, 24);
        QLabel *squeezed = new QLabel(QStringLiteral("A rather long label text"), &top);
        squeezed->setGeometry(10, 60, 40, 24);
        top.show();
        settle();
        const QList<Finding> f = inspect(&top);
        QStringList t;
        for (const Finding &x : f)
            t << x.text;
        const QString all = t.join(QLatin1Char('\n'));
        QVERIFY2(all.contains(QStringLiteral("overlap QLabel \"Under\"")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("clipped QPushButton \"Out\"")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("text-cut QLabel \"A rather long label text\"")), qPrintable(all));
    }
}

void LayoutSanity_Test::theShowTreeShowsCueNamesWithFreshSettings()
{
    /* initTestCase() runs on a HOME of its own and nothing here has moved a
       splitter: these are the default sizes. */
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    m_app->switchToTabContaining(view);
    QSplitter *split = view->findChild<QSplitter *>(QStringLiteral("stSplitter"));
    QTreeWidget *tree = nullptr;
    for (QTreeWidget *t : view->findChildren<QTreeWidget *>())
        if (tree == nullptr && t->headerItem() != nullptr && t->headerItem()->text(0) == QLatin1String("Show"))
            tree = t;
    QVERIFY(split != nullptr && tree != nullptr);
    for (const QSize &z : { QSize(1280, 800), QSize(1024, 640) })
    {
        m_app->resize(z);
        settle();
        QTest::qWait(100);
        settle();
        // Cue rows: "1.0.1 Verse" -- number, then a name. At 1280 every one of
        // a usual length is shown whole; at 1024 the first is.
        static const QRegularExpression cueText(QStringLiteral("^\\d+\\.\\d+\\.\\d+(\\.\\d+)? \\S"));
        QList<QTreeWidgetItem *> cues;
        for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
            if (cueText.match((*it)->text(0)).hasMatch() && (*it)->text(0).size() > 8
                && (*it)->text(0).size() <= 18)
                cues << *it;
        QVERIFY2(!cues.isEmpty(), "no cue row in the Show Tree");
        QString what;
        for (QTreeWidgetItem *cue : z.width() >= 1280 ? cues : cues.mid(0, 1))
        {
            // It is shown whole: its rect holds the text and the room it needs.
            const QRect r = tree->visualItemRect(cue);
            const int need = tree->fontMetrics().horizontalAdvance(cue->text(0)) + 24;
            what = QStringLiteral("%1x%2: \"%3\" needs %4 px, the row has %5 (panes %6)")
                       .arg(z.width()).arg(z.height()).arg(cue->text(0)).arg(need).arg(r.width())
                       .arg(QStringLiteral("%1").arg(split->sizes().at(1)));
            QVERIFY2(r.width() >= need, qPrintable(what));
        }
        // ...and the page is not starved to give it.
        QVERIFY2(split->sizes().at(2) >= 400, qPrintable(what));
    }
    m_app->resize(1280, 800);
    settle();
}

void LayoutSanity_Test::theFooterChipsNeverOverlap()
{
    QStatusBar *sb = m_app->statusBar();
    QVERIFY(sb != nullptr);
    // Several chips on at once: a Design show with an output warning, the
    // unsaved mark, the Load chip and an override.
    m_app->doc()->setModified();
    for (const int w : { 1280, 1024, m_app->minimumSizeHint().width() })
    {
        m_app->resize(w, 800);
        settle();
        QTest::qWait(100);
        settle();
        QList<QWidget *> chips;
        for (QWidget *c : sb->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
            if (c->isVisibleTo(sb) && c->width() > 0 && !qobject_cast<QSizeGrip *>(c))
                chips << c;
        QVERIFY2(!chips.isEmpty(), "no chips in the footer");
        QStringList bad;
        auto name = [](QWidget *c) {
            return QStringLiteral("%1 \"%2\"").arg(QLatin1String(c->metaObject()->className()), c->objectName());
        };
        for (int i = 0; i < chips.size(); ++i)
        {
            if (!QRect(QPoint(0, 0), sb->size()).contains(chips.at(i)->geometry()))
                bad << QStringLiteral("%1 is outside the footer (%2)").arg(name(chips.at(i)))
                           .arg(QStringLiteral("%1,%2 %3x%4").arg(chips.at(i)->x()).arg(chips.at(i)->y())
                                    .arg(chips.at(i)->width()).arg(chips.at(i)->height()));
            for (int j = i + 1; j < chips.size(); ++j)
                if (chips.at(i)->geometry().intersects(chips.at(j)->geometry()))
                    bad << QStringLiteral("%1 overlaps %2").arg(name(chips.at(i)), name(chips.at(j)));
        }
        QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral("%1 wide:\n  ").arg(w) + bad.join(QStringLiteral("\n  "))));
        // What stays first.
        QVERIFY2(m_app->m_statusStage->isVisible() && m_app->m_statusStage->width() > 0,
                 qPrintable(QStringLiteral("the stage chip at %1 wide").arg(w)));
        QVERIFY2(m_app->m_statusDirtyLabel->isVisible() && m_app->m_statusDirtyLabel->width() > 0,
                 qPrintable(QStringLiteral("Unsaved at %1 wide").arg(w)));
    }
    m_app->doc()->resetModified();
    m_app->resize(1280, 800);
    settle();
}

void LayoutSanity_Test::theTabStripFitsTheWindow()
{
    QTabWidget *tabs = m_app->m_tab;
    QTabBar *bar = tabs->tabBar();
    const int oldMode = m_app->tabLabelMode();
    const int oldView = m_app->m_workspaceView;
    const int oldTheme = m_app->theme();
    m_app->setTheme(App::ThemeVSCodeDark);
    /* TABFIT_BEFORE=1: pictures of the old strip (Icons and text, the strip
       left as the style has it) for the before/after check; no assertions. */
    const bool before = qEnvironmentVariableIsSet("TABFIT_BEFORE");
    m_app->setTabLabelMode(before ? App::TabIconAndText : App::TabAuto);
    if (before)
    {
        bar->setUsesScrollButtons(bar->style()->styleHint(QStyle::SH_TabBar_PreferNoArrows) == 0);
        bar->setElideMode(Qt::ElideRight);
    }
    const QString shots = qEnvironmentVariable("TABFIT_SHOTS");
    const QString tag = qEnvironmentVariable("TABFIT_TAG", QStringLiteral("after"));
    const QStringList viewNames = { QStringLiteral("classic"), QStringLiteral("qlcconsole"),
                                    QStringLiteral("everything") };

    for (int v = 0; v < 3; ++v)
    {
        m_app->setWorkspaceView(App::WorkspaceView(v), false);
        int lastFit = -1;
        for (const int w : { 1920, 1280, 1100, 1024, 900, 0 })
        {
            const int width = w > 0 ? w : m_app->minimumSizeHint().width();
            const QString where = QStringLiteral("%1 view, %2 wide").arg(viewNames.at(v)).arg(width);
            m_app->resize(width, 800);
            settle();
            QTest::qWait(50);
            settle();
            if (!shots.isEmpty())
            {
                QDir().mkpath(shots);
                m_app->grab().save(
                    QStringLiteral("%1/%2-tabs-%3-%4.png").arg(shots, tag, viewNames.at(v)).arg(width));
            }

            if (before)
                continue;
            const int fit = m_app->tabFit();
            QVERIFY2(fit >= lastFit, qPrintable(where + ": wording came back as the window narrowed"));
            lastFit = fit;
            if (width >= 1920)
                QCOMPARE(fit, int(App::TabFitFull));

            // No scroll buttons showing.
            for (QToolButton *b : bar->findChildren<QToolButton *>())
                QVERIFY2(!b->isVisibleTo(bar), qPrintable(where + ": the tab strip scrolls"));

            bool sawRun = false;
            QVERIFY(bar->count() >= 6);
            for (int i = 0; i < bar->count(); ++i)
            {
                const QString label = bar->tabData(i).toString();
                const QString full = m_app->tabDisplayLabel(label);
                const QString expect = fit == App::TabFitFull ? full
                                     : fit == App::TabFitShort ? m_app->tabShortLabel(label) : QString();
                QCOMPARE(bar->tabText(i), expect);
                QCOMPARE(bar->tabToolTip(i), full);
                const QRect r = bar->tabRect(i);
                QVERIFY2(r.left() >= 0 && r.right() < bar->width() && r.width() > 0,
                         qPrintable(QStringLiteral("%1: tab \"%2\" is outside the strip (%3..%4 of %5)")
                                        .arg(where, label).arg(r.left()).arg(r.right()).arg(bar->width())));
                if (fit != App::TabFitIcons)
                {
                    // Whole: the text is not cut short.
                    const int need = bar->fontMetrics().horizontalAdvance(expect);
                    QVERIFY2(r.width() > need, qPrintable(where + ": " + label + " is cut"));
                }
                if (label == m_app->tr("Run"))
                    sawRun = true;
            }
            QVERIFY2(sawRun || v == 0, qPrintable(where + ": no Run tab"));
        }
    }

    if (before)
    {
        m_app->setTabLabelMode(oldMode);
        return;
    }
    // The explicit modes still mean what they said.
    m_app->resize(1920, 800);
    settle();
    m_app->setTabLabelMode(App::TabIconOnly);
    QVERIFY(bar->tabText(0).isEmpty());
    QVERIFY(!bar->tabToolTip(0).isEmpty());
    m_app->setTabLabelMode(App::TabTextOnly);
    QVERIFY(!bar->tabText(0).isEmpty());
    QVERIFY(bar->tabIcon(0).isNull());
    m_app->setTabLabelMode(App::TabIconAndText);
    QCOMPARE(bar->tabText(0), m_app->tabDisplayLabel(bar->tabData(0).toString()));

    // A tab put in its own window keeps its full name in the title.
    m_app->setTabLabelMode(App::TabAuto);
    m_app->resize(900, 800);
    settle();
    const QString label = bar->tabData(1).toString();
    m_app->slotDetachContext(1);
    settle();
    bool titled = false;
    for (DetachedContext *dw : m_app->findChildren<DetachedContext *>())
        if (dw->windowTitle().contains(m_app->tabDisplayLabel(label)))
            titled = true;
    QVERIFY2(titled, "the detached window's title keeps the full name");
    for (DetachedContext *dw : m_app->findChildren<DetachedContext *>())
        dw->close();
    settle();

    m_app->setTabLabelMode(oldMode);
    m_app->setTheme(oldTheme);
    m_app->setWorkspaceView(App::WorkspaceView(oldView), false);
    m_app->resize(1280, 800);
    settle();
}

void LayoutSanity_Test::everyScreenIsLaidOutCleanly_data()
{
    QTest::addColumn<int>("index");
    const QList<Screen> list = screens();
    for (int i = 0; i < list.size(); ++i)
        QTest::newRow(qPrintable(list.at(i).name)) << i;
}

void LayoutSanity_Test::everyScreenIsLaidOutCleanly()
{
    QFETCH(int, index);
    const Screen s = screens().at(index);

    QTimer modals;
    connect(&modals, &QTimer::timeout, [] { dismissModals(); });
    modals.start(20);

    QWidget *top = s.make(m_app);
    if (top == nullptr)
        QSKIP("not in this show");
    QScopedPointer<QWidget> owner(s.owned ? top : nullptr);
    if (s.owned)
        top->setAttribute(Qt::WA_DontShowOnScreen, false);
    top->show();
    settle();
    if (s.prepare)
    {
        s.prepare(m_app, top);
        settle();
    }

    QStringList failures;
    for (const Size &z : sizes())
    {
        QSize want = z.size;
        if (!want.isValid())
        {
            want = top->minimumSizeHint().expandedTo(top->minimumSize());
            if (!want.isValid() || want.isEmpty())
                want = top->sizeHint();
            // The app window would shrink to a postage stamp; its smallest
            // is the smallest screen it is used on.
            if (!s.owned)
                want = want.expandedTo(kSmallestWindow);
        }
        top->resize(want);
        settle();

        const QList<Finding> found = inspect(top);
        saveShot(top, s.name, QLatin1String(z.name), found);
        for (const Finding &f : found)
        {
            const QString line = QStringLiteral("%1 @ %2 %3x%4: %5")
                                     .arg(s.name, QLatin1String(z.name))
                                     .arg(top->width()).arg(top->height()).arg(f.text);
            if (const Known *k = knownFor(s.name, QLatin1String(z.name), f.text))
            {
                const QString key = QStringLiteral("%1|%2|%3").arg(k->screen, k->size, k->finding);
                if (!m_seenKnown.contains(key))
                    m_seenKnown << key;
                qInfo().noquote() << "known (" << k->todo << "):" << line;
            }
            else
                failures << line;
        }
    }
    if (!s.owned)
        m_app->resize(1280, 800);
    QVERIFY2(failures.isEmpty(), qPrintable(QLatin1Char('\n') + failures.join(QLatin1Char('\n'))));
}

QTEST_MAIN(LayoutSanity_Test)
