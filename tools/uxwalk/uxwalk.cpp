/*
  qlcconsole
  uxwalk.cpp

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

/* The UX walker (docs/dev/UX_WALKER.md).
 *
 * Opens a real show offscreen, with settings and a HOME of its own, in one
 * Settings > Units, and walks the app the way a person testing it does:
 * every tab, every menu and submenu, every dialog a menu item that ends in
 * "…" opens, the layoutsanity_test screen registry, right-click menus on
 * representative items, Show Tree layer pages, the effect picker for three
 * kinds of group, and the Build command line. Each state it reaches is
 * written as OUT/screens/<id>.png + <id>.json: every visible widget's text,
 * class, name, geometry, whether its text is cut short, enabled state, and
 * the layout checker's findings. The checks over those records, and the
 * report, are tools/uxwalk/report.py's; the fan-out over cores and hosts is
 * tools/uxwalk/run.py's. This program only records.
 *
 *   uxwalk --show FILE --units feet|mm --out DIR
 *          [--groups tabs,menus,actions,screens,context,showtree,pickers,cmdline]
 *          [--shard I/N]     (round-robin split of the screens inside the groups)
 *          [--list]          (print the groups and exit)
 */

#include <QtTest>
#include <QtWidgets>
#include <QGraphicsItem>

// The app's insides the walker reaches into, before layoutsanity_test.cpp
// includes some of them without.
#include "../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "lookeditor.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "targetitem.h"
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "effectbrowser.h"
#include "workbenchcommand.h"
#include "showtreeview.h"
#include "programmingmanager.h"
#include "functionstreewidget.h"
#undef protected
#undef private

// layoutsanity_test.cpp's screen registry and layout checker, as they are,
// without its main(): new screens there are walked here with no change.
#undef QTEST_MAIN
#define QTEST_MAIN(x)
#include "../../ui/test/layoutsanity/layoutsanity_test.cpp"
#undef QTEST_MAIN

#include <QAbstractItemView>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextEdit>
#include <QThread>


#include "contextmenus.h"
#include "effectfit.h"
#include "effectscriptrunner.h"
#include "measurementunits.h"
#include "showtreemodel.h"

namespace uxwalk
{

/****************************************************************************
 * Options
 ****************************************************************************/

const QStringList kGroups = {
    QStringLiteral("tabs"), QStringLiteral("menus"), QStringLiteral("actions"),
    QStringLiteral("screens"), QStringLiteral("context"), QStringLiteral("contextdialogs"),
    QStringLiteral("showtree"),
    QStringLiteral("pickers"), QStringLiteral("cmdline"),
};

const QSize kLaptop(1280, 800);

/* Menu items never triggered by the "actions" group: they lose or change the
   show, leave the app, or reach the network / the rig. Matched against the
   item's text, & and … removed, case-insensitively, from its start. */
const QStringList kDenyStarts = {
    QStringLiteral("New"), QStringLiteral("Open"), QStringLiteral("Save"),
    QStringLiteral("Quit"), QStringLiteral("Exit"), QStringLiteral("Close"),
    QStringLiteral("Delete"), QStringLiteral("Remove"), QStringLiteral("Clear"),
    QStringLiteral("Reset"), QStringLiteral("Revert"), QStringLiteral("Restart"),
    QStringLiteral("Blackout"), QStringLiteral("Operate"), QStringLiteral("Go Live"),
    QStringLiteral("Import"), QStringLiteral("Export"), QStringLiteral("Print"),
    QStringLiteral("Send"), QStringLiteral("Upload"), QStringLiteral("Publish"),
    QStringLiteral("Report a Bug"), QStringLiteral("Check for Updates"),
    QStringLiteral("Update"), QStringLiteral("Merge"), QStringLiteral("Replace"),
    QStringLiteral("Recover"), QStringLiteral("Load"), QStringLiteral("Detach"),
    QStringLiteral("Put Back"), QStringLiteral("Full Screen"), QStringLiteral("Kiosk"),
};

struct Options
{
    QString show;
    QString units = QStringLiteral("feet");
    QString out;
    QStringList groups = kGroups;
    int shard = 0;
    int shards = 1;
    int skip = 0;
};

/****************************************************************************
 * Helpers
 ****************************************************************************/

void pump(int ms = 0)
{
    settle();
    if (ms > 0)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
        settle();
    }
}

QString plain(const QString &text)
{
    if (!Qt::mightBeRichText(text))
        return text;
    QTextDocument doc;
    doc.setHtml(text);
    return doc.toPlainText();
}

QString slug(const QString &s)
{
    QString out = s;
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    return out.left(90);
}

/** menuwords_test's reading of the canon's Title Case: a line of
 *  information ("Venue: none"), a question or a prompt is not a name; the
 *  app's own name and a unit are written as they are. */
QString titleCaseProblem(const QString &text)
{
    if (text.isEmpty() || text.at(0).isSpace() || text.startsWith(QLatin1Char('('))
        || text.startsWith(QStringLiteral("⚠")) || text.startsWith(QStringLiteral("…"))
        || text.endsWith(QLatin1Char('?')) || text.endsWith(QLatin1Char(':'))
        || text.contains(QStringLiteral(": ")))
        return QString();
    // Not words: a glyph, a number.
    if (!text.contains(QRegularExpression(QStringLiteral("[A-Za-z]{2,}"))))
        return QString();
    QString words = text;
    words.remove(QStringLiteral("qlcconsole"));
    words.remove(QStringLiteral("macOS"));
    words.remove(QRegularExpression(QStringLiteral("\\d+ ms\\b")));
    return ContextMenu::checkTitleCase(words);
}

QJsonArray rectJson(const QRect &r)
{
    return QJsonArray{ r.x(), r.y(), r.width(), r.height() };
}

/****************************************************************************
 * The recorder
 ****************************************************************************/

class Walker
{
public:
    Walker(App *app, const Options &o) : m_app(app), m_opt(o)
    {
        m_tag = QStringLiteral("%1-%2").arg(QFileInfo(o.show).completeBaseName(), o.units);
        QDir().mkpath(screensDir());
    }

    QString screensDir() const { return QDir(m_opt.out).filePath(QStringLiteral("screens")); }

    /** Round-robin share of the screens: this process's? */
    /** This process's share of the screens (round-robin over the shards),
     *  less the first --skip of them (a rerun after a crash). Each one taken
     *  is announced first, so the driver knows which one a crash was in. */
    bool mine(const QString &group, const QString &what)
    {
        if ((m_counter++ % m_opt.shards) != m_opt.shard)
            return false;
        if (m_taken++ < m_opt.skip)
            return false;
        printf("uxwalk-at: %d %s|%s\n", m_taken, qPrintable(group), qPrintable(what));
        fflush(stdout);
        return true;
    }

    int written() const { return m_written; }

    /** One state of a top-level widget: PNG + JSON. */
    void record(QWidget *top, const QString &group, const QString &name,
                const QJsonObject &extra = QJsonObject(), QWidget *focus = nullptr)
    {
        pump();
        QJsonObject o = base(group, name);
        o.insert(QStringLiteral("kind"), QStringLiteral("window"));
        o.insert(QStringLiteral("windowTitle"), top->windowTitle());
        o.insert(QStringLiteral("size"), QJsonArray{ top->width(), top->height() });
        o.insert(QStringLiteral("class"), QLatin1String(top->metaObject()->className()));
        o.insert(QStringLiteral("isDialog"), qobject_cast<QDialog *>(top) != nullptr);
        o.insert(QStringLiteral("isMain"), top == m_app);
        // F1's page (UX-24): the window's, or the nearest ancestor's.
        QString topic;
        for (QWidget *p = top; p != nullptr && topic.isEmpty(); p = p->parentWidget())
            topic = p->property("helpTopic").toString();
        o.insert(QStringLiteral("helpTopic"), topic);
        // A window of the main one (UX-38): parented to it, so it stays on top.
        bool ofMain = false;
        for (QWidget *p = top->parentWidget(); p != nullptr; p = p->parentWidget())
            ofMain = ofMain || p == m_app;
        o.insert(QStringLiteral("parentIsMain"), ofMain || top == m_app);
        QJsonArray widgets;
        const QWidget *root = focus != nullptr ? focus : top;
        collect(top, root, widgets);
        o.insert(QStringLiteral("widgets"), widgets);
        QJsonArray layout;
        for (const Finding &f : inspect(top))
            layout.append(QJsonObject{ { QStringLiteral("text"), f.text },
                                       { QStringLiteral("rect"), rectJson(f.mark) } });
        o.insert(QStringLiteral("layout"), layout);
        for (auto it = extra.begin(); it != extra.end(); ++it)
            o.insert(it.key(), it.value());
        write(o, top->grab());
    }

    /** A menu as it is open: its items (and each submenu, opened in turn). */
    void recordMenu(QMenu *menu, const QString &group, const QString &name,
                    const QJsonObject &extra = QJsonObject(), int depth = 0)
    {
        pump();
        QJsonObject o = base(group, name);
        o.insert(QStringLiteral("kind"), QStringLiteral("menu"));
        o.insert(QStringLiteral("title"), menu->title().remove(QLatin1Char('&')));
        o.insert(QStringLiteral("size"), QJsonArray{ menu->width(), menu->height() });
        QJsonArray items;
        QList<QMenu *> subs;
        for (QAction *a : menu->actions())
        {
            if (!a->isVisible())
                continue;
            QJsonObject it;
            if (a->isSeparator())
            {
                it.insert(QStringLiteral("separator"), true);
                if (!a->text().isEmpty())
                    it.insert(QStringLiteral("text"), a->text());
                items.append(it);
                continue;
            }
            QString t = a->text();
            t.remove(QRegularExpression(QStringLiteral("&(?!&)")));
            it.insert(QStringLiteral("text"), t);
            it.insert(QStringLiteral("enabled"), a->isEnabled());
            if (a->isCheckable())
                it.insert(QStringLiteral("checked"), a->isChecked());
            if (!a->shortcut().isEmpty())
                it.insert(QStringLiteral("shortcut"), a->shortcut().toString(QKeySequence::PortableText));
            // QAction's tooltip falls back to its text: only one of its own.
            if (!a->toolTip().isEmpty() && a->toolTip() != t && a->toolTip() != a->text())
                it.insert(QStringLiteral("tooltip"), plain(a->toolTip()));
            if (!a->statusTip().isEmpty())
                it.insert(QStringLiteral("statusTip"), plain(a->statusTip()));
            if (a->menu() != nullptr)
            {
                it.insert(QStringLiteral("submenu"), true);
                subs << a->menu();
            }
            const QRect r = menu->actionGeometry(a);
            it.insert(QStringLiteral("rect"), rectJson(r));
            // Cut short: a menu is as wide as its widest item, so only a
            // menu squeezed by the screen cuts.
            const int need = menu->fontMetrics().horizontalAdvance(t);
            if (r.width() > 0 && need > r.width())
                it.insert(QStringLiteral("elided"), true);
            const QString tc = titleCaseProblem(t);
            if (!tc.isEmpty())
                it.insert(QStringLiteral("titleCase"), tc);
            items.append(it);
        }
        o.insert(QStringLiteral("items"), items);
        if (group == QLatin1String("context"))
        {
            QJsonArray canon;
            for (const QString &p : ContextMenu::checkCanon(menu))
                canon.append(p);
            o.insert(QStringLiteral("canon"), canon);
        }
        for (auto it = extra.begin(); it != extra.end(); ++it)
            o.insert(it.key(), it.value());
        write(o, menu->grab());

        // The submenus, each as the hover opens it (aboutToShow fills some).
        if (depth >= 3)
            return;
        for (QMenu *sub : subs)
        {
            if (!sub->isEnabled())
                continue;
            // Read, not popped up: a second popup inside a context menu's
            // exec() is not something a person can do, and Qt crashed on it.
            emit sub->aboutToShow();
            sub->adjustSize();
            recordMenu(sub, group, name + QStringLiteral(" ▸ ") + sub->title().remove(QLatin1Char('&')),
                       extra, depth + 1);
            emit sub->aboutToHide();
        }
    }

    /** A finding the walker itself makes (a menu that did not open, ...). */
    void note(const QString &group, const QString &name, const QString &rule, const QString &text)
    {
        QJsonObject o = base(group, name);
        o.insert(QStringLiteral("kind"), QStringLiteral("note"));
        o.insert(QStringLiteral("rule"), rule);
        o.insert(QStringLiteral("text"), text);
        write(o, QPixmap());
    }

private:
    QJsonObject base(const QString &group, const QString &name)
    {
        QJsonObject o;
        o.insert(QStringLiteral("show"), QFileInfo(m_opt.show).fileName());
        o.insert(QStringLiteral("units"), m_opt.units);
        o.insert(QStringLiteral("group"), group);
        o.insert(QStringLiteral("name"), name);
        o.insert(QStringLiteral("tag"), m_tag);
        return o;
    }

    void write(QJsonObject o, const QPixmap &pm)
    {
        const QString id = QStringLiteral("%1__%2__%3").arg(m_tag, o.value(QStringLiteral("group")).toString(),
                                                             slug(o.value(QStringLiteral("name")).toString()));
        QString unique = id;
        for (int n = 2; m_ids.contains(unique); ++n)
            unique = id + QStringLiteral("_%1").arg(n);
        m_ids.insert(unique);
        o.insert(QStringLiteral("id"), unique);
        if (!pm.isNull())
        {
            pm.save(QDir(screensDir()).filePath(unique + QStringLiteral(".png")));
            o.insert(QStringLiteral("png"), unique + QStringLiteral(".png"));
        }
        QFile f(QDir(screensDir()).filePath(unique + QStringLiteral(".json")));
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        ++m_written;
        qInfo().noquote() << "uxwalk:" << o.value(QStringLiteral("group")).toString()
                          << "|" << o.value(QStringLiteral("name")).toString();
    }

    /** Every visible widget under @a root, in @a top's coordinates. */
    void collect(QWidget *top, const QWidget *root, QJsonArray &out)
    {
        QList<QWidget *> all;
        all << const_cast<QWidget *>(root);
        all << root->findChildren<QWidget *>();
        for (QWidget *w : all)
        {
            if (w != top && (!w->isVisibleTo(top) || w->isWindow()))
                continue;
            if (w->width() <= 0 || w->height() <= 0)
                continue;
            // Inside a combo / spin box / scroll bar: that widget's own parts.
            bool inner = false;
            for (QWidget *p = w->parentWidget(); p != nullptr && p != top; p = p->parentWidget())
                if (qobject_cast<QComboBox *>(p) || qobject_cast<QAbstractSpinBox *>(p)
                    || qobject_cast<QHeaderView *>(p) || p->inherits("QScrollBar"))
                    inner = true;
            if (inner)
                continue;
            // On screen: cut to each ancestor.
            QRect seen(w->mapTo(top, QPoint(0, 0)), w->size());
            for (QWidget *p = w->parentWidget(); p != nullptr && p != top; p = p->parentWidget())
                seen &= QRect(p->mapTo(top, QPoint(0, 0)), p->size());
            seen &= top->rect();
            if (seen.isEmpty())
                continue;
            QJsonObject r = widget(w);
            if (r.isEmpty())
                continue;
            r.insert(QStringLiteral("rect"), rectJson(seen));
            out.append(r);
        }
    }

    QJsonObject widget(QWidget *w)
    {
        QJsonObject r;
        QString text;
        bool shown = true;
        int need = -1;
        const QFontMetrics fm = w->fontMetrics();
        QJsonArray items;
        bool view = false, empty = false;
        // A mnemonic "&" before a space or at the end: Qt underlines a
        // space and drops the "&" ("Position & Orientation" shows as
        // "Position _Orientation"); it needs "&&".
        auto mnemonic = [&r](QString raw) {
            raw.remove(QStringLiteral("&&"));
            if (raw.contains(QRegularExpression(QStringLiteral("&(?=\\s|$)"))))
                r.insert(QStringLiteral("mnemonicSpace"), true);
        };

        if (QLabel *l = qobject_cast<QLabel *>(w))
        {
            text = plain(l->text());
            if (!text.isEmpty() && !l->wordWrap() && !text.contains(QLatin1Char('\n'))
                && l->sizePolicy().horizontalPolicy() != QSizePolicy::Ignored)
            {
                QString t = text;
                if (l->buddy() != nullptr)
                {
                    mnemonic(l->text());
                    t.remove(QRegularExpression(QStringLiteral("&(?!&)")));
                }
                const QMargins m = l->contentsMargins();
                need = fm.horizontalAdvance(t) + m.left() + m.right() + 2 * l->margin();
                if (!Qt::mightBeRichText(l->text()) || l->textFormat() == Qt::PlainText)
                    ;
                else
                    need = -1;          // rich text: its own layout, not measured here
            }
        }
        else if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
        {
            mnemonic(b->text());
            text = b->text();
            text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
            const QToolButton *tb = qobject_cast<QToolButton *>(b);
            shown = !text.isEmpty()
                && (tb == nullptr || tb->toolButtonStyle() != Qt::ToolButtonIconOnly || b->icon().isNull());
            if (shown && !text.contains(QLatin1Char('\n')))
            {
                need = fm.horizontalAdvance(text);
                if (!b->icon().isNull() && (tb == nullptr || tb->toolButtonStyle() == Qt::ToolButtonTextBesideIcon))
                    need += b->iconSize().width() + 2;
                if (qobject_cast<QCheckBox *>(b) || qobject_cast<QRadioButton *>(b))
                    need += 12;
            }
            if (shown && qobject_cast<QCheckBox *>(b) == nullptr && qobject_cast<QRadioButton *>(b) == nullptr
                && !w->inherits("QTabBar") && !w->parentWidget()->inherits("QTabBar"))
            {
                const QString tc = titleCaseProblem(text);
                if (!tc.isEmpty())
                    r.insert(QStringLiteral("titleCase"), tc);
            }
            if (b->isCheckable())
                r.insert(QStringLiteral("checked"), b->isChecked());
        }
        else if (QLineEdit *e = qobject_cast<QLineEdit *>(w))
        {
            text = e->text();
            if (!e->placeholderText().isEmpty())
                r.insert(QStringLiteral("placeholder"), e->placeholderText());
        }
        else if (QComboBox *c = qobject_cast<QComboBox *>(w))
        {
            text = c->currentText();
            for (int i = 0; i < c->count() && i < 300; ++i)
                items.append(c->itemText(i));
            empty = c->count() == 0;
            need = fm.horizontalAdvance(text) + 28;
        }
        else if (QAbstractSpinBox *s = qobject_cast<QAbstractSpinBox *>(w))
        {
            text = s->text();
            need = fm.horizontalAdvance(text) + 22;
        }
        else if (QGroupBox *g = qobject_cast<QGroupBox *>(w))
        {
            mnemonic(g->title());
            text = g->title();
            text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
        }
        else if (QTabBar *tb = qobject_cast<QTabBar *>(w))
        {
            for (int i = 0; i < tb->count(); ++i)
            {
                QJsonObject t{ { QStringLiteral("text"), tb->tabText(i) } };
                if (!tb->tabToolTip(i).isEmpty())
                    t.insert(QStringLiteral("tooltip"), plain(tb->tabToolTip(i)));
                const QRect tr = tb->tabRect(i);
                const int tneed = fm.horizontalAdvance(tb->tabText(i));
                if (!tb->tabText(i).isEmpty() && tr.width() < tneed)
                    t.insert(QStringLiteral("elided"), true);
                items.append(t);
            }
        }
        else if (QTextEdit *te = qobject_cast<QTextEdit *>(w))
        {
            text = te->toPlainText().left(4000);
            if (!te->placeholderText().isEmpty())
                r.insert(QStringLiteral("placeholder"), te->placeholderText());
        }
        else if (QPlainTextEdit *pe = qobject_cast<QPlainTextEdit *>(w))
        {
            text = pe->toPlainText().left(4000);
            if (!pe->placeholderText().isEmpty())
                r.insert(QStringLiteral("placeholder"), pe->placeholderText());
        }
        else if (QAbstractItemView *v = qobject_cast<QAbstractItemView *>(w))
        {
            if (qobject_cast<QHeaderView *>(v) != nullptr)
                return QJsonObject();
            view = true;
            QAbstractItemModel *m = v->model();
            if (m != nullptr)
            {
                empty = m->rowCount(v->rootIndex()) == 0;
                // The headers.
                QJsonArray heads;
                if (QTreeView *tv = qobject_cast<QTreeView *>(v))
                    if (!tv->isHeaderHidden())
                        for (int c = 0; c < m->columnCount(); ++c)
                            heads.append(m->headerData(c, Qt::Horizontal).toString());
                if (QTableView *tbl = qobject_cast<QTableView *>(v))
                    if (tbl->horizontalHeader()->isVisible())
                        for (int c = 0; c < m->columnCount(); ++c)
                            heads.append(m->headerData(c, Qt::Horizontal).toString());
                if (!heads.isEmpty())
                    r.insert(QStringLiteral("headers"), heads);
                // The items on screen (a tree: the expanded ones).
                int n = 0;
                std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
                    for (int row = 0; row < m->rowCount(parent) && n < 400; ++row)
                    {
                        for (int col = 0; col < m->columnCount(parent) && col < 8; ++col)
                        {
                            const QModelIndex idx = m->index(row, col, parent);
                            const QRect vr = v->visualRect(idx);
                            const QString t = idx.data(Qt::DisplayRole).toString();
                            if (vr.isEmpty() || !vr.intersects(v->viewport()->rect()))
                                continue;
                            if (t.isEmpty() && idx.data(Qt::ToolTipRole).toString().isEmpty())
                                continue;
                            QJsonObject it{ { QStringLiteral("text"), t } };
                            if (col > 0)
                                it.insert(QStringLiteral("col"), col);
                            const QString tip = idx.data(Qt::ToolTipRole).toString();
                            if (!tip.isEmpty())
                                it.insert(QStringLiteral("tooltip"), plain(tip).left(600));
                            if (!(idx.flags() & Qt::ItemIsEnabled))
                                it.insert(QStringLiteral("enabled"), false);
                            // Cut short: wider than its cell (an icon takes its room).
                            int tneed = v->fontMetrics().horizontalAdvance(t) + 8;
                            if (!idx.data(Qt::DecorationRole).isNull())
                                tneed += v->iconSize().isValid() ? v->iconSize().width() + 4 : 20;
                            if (!t.isEmpty() && !t.contains(QLatin1Char('\n')) && vr.width() + kTol < tneed
                                && !v->inherits("QListView"))
                                it.insert(QStringLiteral("elided"), true);
                            it.insert(QStringLiteral("rect"), rectJson(QRect(v->viewport()->mapTo(v->window(), vr.topLeft()),
                                                                            vr.size())));
                            items.append(it);
                            ++n;
                        }
                        const QModelIndex first = m->index(row, 0, parent);
                        if (QTreeView *tv = qobject_cast<QTreeView *>(v))
                            if (tv->isExpanded(first))
                                walk(first);
                    }
                };
                walk(v->rootIndex());
            }
        }
        else
        {
            // A plain container: only its tooltip, when it has one.
            if (w->toolTip().isEmpty())
                return QJsonObject();
        }

        r.insert(QStringLiteral("class"), QLatin1String(w->metaObject()->className()));
        if (!w->objectName().isEmpty() && !w->objectName().startsWith(QLatin1String("qt_")))
            r.insert(QStringLiteral("objectName"), w->objectName());
        r.insert(QStringLiteral("path"), pathOf(w, w->window()));
        if (!text.isEmpty())
            r.insert(QStringLiteral("text"), text.left(4000));
        if (!shown)
            r.insert(QStringLiteral("textShown"), false);
        if (!w->toolTip().isEmpty())
            r.insert(QStringLiteral("tooltip"), plain(w->toolTip()).left(1000));
        if (!w->statusTip().isEmpty())
            r.insert(QStringLiteral("statusTip"), plain(w->statusTip()).left(400));
        if (!w->whatsThis().isEmpty())
            r.insert(QStringLiteral("whatsThis"), plain(w->whatsThis()).left(400));
        r.insert(QStringLiteral("enabled"), w->isEnabled());
        if (need > 0 && w->width() + kTol < need)
        {
            r.insert(QStringLiteral("elided"), true);
            r.insert(QStringLiteral("need"), need);
        }
        if (!items.isEmpty())
            r.insert(QStringLiteral("items"), items);
        if (view || qobject_cast<QComboBox *>(w))
        {
            r.insert(QStringLiteral("empty"), empty);
            if (empty)
            {
                // What is beside it that could say why: a label in the same
                // parent, or the view's own placeholder / tooltip.
                QStringList near;
                if (QWidget *p = w->parentWidget())
                    for (QLabel *l : p->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly))
                        if (l->isVisibleTo(w->window()) && !l->text().trimmed().isEmpty())
                            near << plain(l->text()).simplified();
                if (!near.isEmpty())
                    r.insert(QStringLiteral("emptyNote"), near.join(QStringLiteral(" | ")).left(400));
            }
        }
        return r;
    }

    App *m_app;
    Options m_opt;
    QString m_tag;
    QSet<QString> m_ids;
    int m_counter = 0;
    int m_taken = 0;
    int m_written = 0;
};

/****************************************************************************
 * Watching for what an action opens
 ****************************************************************************/

/** The visible top-level widgets now (no tooltips, no popups of a combo). */
QSet<QWidget *> windowsNow()
{
    QSet<QWidget *> s;
    for (QWidget *w : QApplication::topLevelWidgets())
        if (w->isVisible() && !w->inherits("QTipLabel") && !w->inherits("QComboBoxPrivateContainer"))
            s.insert(w);
    return s;
}

/** Close @a w the way its Cancel / Esc would. */
void dismiss(QWidget *w)
{
    if (QDialog *d = qobject_cast<QDialog *>(w))
        d->reject();
    else
        w->close();
}

/** Run @a open; each new window it shows (modal: inside its exec()) is
 *  recorded and dismissed. Returns how many were seen. */
int captureWindows(Walker &walk, const QString &group, const QString &name,
                   const std::function<void()> &open, int waitMs = 400, bool leaveMenus = false)
{
    const QSet<QWidget *> before = windowsNow();
    QSet<QWidget *> handled;
    int seen = 0;
    QElapsedTimer age;
    QTimer poll;
    poll.setInterval(30);
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        const QSet<QWidget *> now = windowsNow();
        for (QWidget *w : now)
        {
            if (before.contains(w) || handled.contains(w))
                continue;
            handled.insert(w);
            if (QMenu *m = qobject_cast<QMenu *>(w))
            {
                if (leaveMenus)
                    handled.remove(w);      // the right-click's own: its owner picks
                else
                    m->close();
                continue;
            }
            ++seen;
            // Laid out at the laptop size where it is a big window; a small
            // dialog keeps its own size, as it opens.
            if (w->width() > kLaptop.width() || w->height() > kLaptop.height())
                w->resize(kLaptop);
            pump(120);
            walk.record(w, group, seen == 1 ? name : QStringLiteral("%1 (%2)").arg(name).arg(seen));
            dismiss(w);
        }
        if (age.elapsed() > 15000)
        {
            // Never wait for a human: whatever is still up goes.
            for (QWidget *w : now)
                if (!before.contains(w))
                    dismiss(w);
        }
    });
    age.start();
    poll.start();
    open();
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < waitMs)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    poll.stop();
    // Anything still open that we didn't see shown (shown late): close.
    for (QWidget *w : windowsNow())
        if (!before.contains(w))
            dismiss(w);
    pump();
    return seen;
}

bool denied(const QString &text);

/** What a right-click is for: record the menu, list its "…" items, or pick
 *  one of them (contextdialogs: the window it opens is recorded). */
struct CtxMode
{
    enum Mode { Record, List, Pick } mode = Record;
    QString pick;
    QStringList found;
};
CtxMode g_ctx;
QString g_showPath;

/** A right-click at @a pos of @a target: the menu it opens, recorded. */
bool contextAt(Walker &walk, QWidget *target, const QPoint &pos, const QString &name,
               const QJsonObject &extra = QJsonObject())
{
    bool seen = false;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
            for (QWidget *w : QApplication::topLevelWidgets())
                if (QMenu *mm = qobject_cast<QMenu *>(w))
                    if (mm->isVisible())
                        m = mm;
        if (m == nullptr)
            return;
        poll.stop();
        seen = true;
        if (g_ctx.mode == CtxMode::Record)
        {
            walk.recordMenu(m, QStringLiteral("context"), name, extra);
            m->close();
        }
        else if (g_ctx.mode == CtxMode::List)
        {
            // The items that open a window to finish in ("…"), as a person
            // would try each.
            emit m->aboutToShow();
            for (QAction *a : m->actions())
            {
                QString t = a->text();
                t.remove(QRegularExpression(QStringLiteral("&(?!&)")));
                if (!a->isSeparator() && a->isVisible() && a->isEnabled() && a->menu() == nullptr
                    && !a->isCheckable() && t.endsWith(QChar(0x2026)) && !denied(t))
                    g_ctx.found << t;
            }
            m->close();
        }
        else
        {
            QAction *pick = nullptr;
            for (QAction *a : m->actions())
                if (a->text().remove(QRegularExpression(QStringLiteral("&(?!&)"))) == g_ctx.pick)
                    pick = a;
            if (pick == nullptr)
            {
                m->close();
                return;
            }
            // As Return on the highlighted item: the menu closes, the item runs.
            m->setActiveAction(pick);
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(m, &press);
        }
    });
    poll.start();
    QContextMenuEvent ev(QContextMenuEvent::Mouse, pos, target->mapToGlobal(pos));
    QApplication::sendEvent(target, &ev);
    QElapsedTimer t;
    t.start();
    while (!seen && t.elapsed() < 800)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    poll.stop();
    if (!seen && g_ctx.mode == CtxMode::Record)
        walk.note(QStringLiteral("context"), name, QStringLiteral("walker-no-menu"),
                  QStringLiteral("a right-click opened no menu"));
    // Whatever it left open.
    for (QWidget *w : QApplication::topLevelWidgets())
        if (QMenu *mm = qobject_cast<QMenu *>(w))
            if (mm->isVisible())
                mm->close();
    pump();
    return seen;
}

/** Where a row is, by its texts from the top: the same row after the tree
 *  is rebuilt (a selection change rebuilds some). */
QStringList rowPath(QTreeWidgetItem *item)
{
    QStringList p;
    for (QTreeWidgetItem *i = item; i != nullptr; i = i->parent())
        p.prepend(i->text(0));
    return p;
}

QTreeWidgetItem *rowAt(QTreeWidget *tree, const QStringList &path)
{
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
        if (rowPath(*it) == path)
            return *it;
    return nullptr;
}

/** A right-click on a tree row, as a person does it: selected, then the menu. */
bool contextOnItem(Walker &walk, QTreeWidget *tree, QTreeWidgetItem *item, const QString &name)
{
    if (tree == nullptr || item == nullptr)
    {
        walk.note(QStringLiteral("context"), name, QStringLiteral("walker-missing"),
                  QStringLiteral("nothing of that kind in this show"));
        return false;
    }
    const QStringList path = rowPath(item);
    for (QTreeWidgetItem *p = item->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    tree->scrollToItem(item);
    tree->clearSelection();
    tree->setCurrentItem(item);
    pump(50);
    item = rowAt(tree, path);
    if (item == nullptr)
    {
        walk.note(QStringLiteral("context"), name, QStringLiteral("walker-missing"),
                  QStringLiteral("the row went away when selected: %1").arg(path.join(QStringLiteral(" / "))));
        return false;
    }
    item->setSelected(true);
    tree->scrollToItem(item);
    pump();
    const QRect r = tree->visualItemRect(item);
    return contextAt(walk, tree->viewport(), r.center(), name,
                     QJsonObject{ { QStringLiteral("item"), path.join(QStringLiteral(" / ")) } });
}

QTreeWidgetItem *findItem(QTreeWidget *tree, const std::function<bool(QTreeWidgetItem *)> &match)
{
    if (tree == nullptr)
        return nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
        if (match(*it))
            return *it;
    return nullptr;
}

/****************************************************************************
 * The show's parts
 ****************************************************************************/

struct ShowParts
{
    quint32 cue = ShowTree::invalidId();
    quint32 hit = ShowTree::invalidId();
    quint32 hitList = ShowTree::invalidId();
    // Layers by what they hold: id + its cue.
    QList<QPair<QString, QPair<quint32, quint32>>> layers;
};

ShowParts showParts(Doc *doc)
{
    ShowParts sp;
    QSet<QString> have;
    ShowTreeController *st = doc->showTree();
    for (const ShowTree::Production &p : st->productions())
    {
        if (const ShowTree::CueList *looks = p.looks())
            if (!looks->cues.isEmpty() && sp.hit == ShowTree::invalidId())
            {
                sp.hit = looks->cues.first().id;
                sp.hitList = looks->id;
            }
        for (const ShowTree::CueList &l : p.lists)
            for (const ShowTree::Cue &c : l.cues)
            {
                if (!c.layers.isEmpty() && sp.cue == ShowTree::invalidId())
                    sp.cue = c.id;
                for (const ShowTree::Layer &ly : c.layers)
                    for (const ShowTree::Look &lk : ly.looks)
                    {
                        QString kind;
                        if (ShowTree::isEffectKey(lk.key))
                            kind = QStringLiteral("effect");
                        else if (lk.key == ShowTree::KeyPosition)
                            kind = QStringLiteral("aim");
                        else if (lk.key == ShowTree::KeyColour)
                            kind = QStringLiteral("colour");
                        if (kind.isEmpty() || have.contains(kind))
                            continue;
                        have.insert(kind);
                        sp.layers << qMakePair(kind, qMakePair(ly.id, c.id));
                    }
            }
    }
    return sp;
}

/** A group of each kind the effect picker treats apart. */
QMap<QString, FixtureGroup *> groupsByKind(Doc *doc)
{
    QMap<QString, FixtureGroup *> out;
    for (FixtureGroup *g : doc->fixtureGroups())
    {
        const QList<quint32> fids = g->fixtureList();
        if (fids.isEmpty())
            continue;
        bool wled = false, pt = false, all = true;
        for (quint32 id : fids)
        {
            const Fixture *f = doc->fixture(id);
            if (f == nullptr)
                continue;
            const EffectFit::Caps c = EffectFit::capsOf(f);
            wled = wled || c.wled;
            pt = pt || c.panTilt;
            all = all && !c.wled && !c.panTilt;
        }
        qInfo().noquote() << "uxwalk: group" << g->name() << fids.size() << "fixtures, wled" << wled
                          << "pan/tilt" << pt;
        QString kind;
        if (wled)
            kind = QStringLiteral("wled");
        else if (pt)
            kind = QStringLiteral("mover");
        else if (all)
            kind = QStringLiteral("par");
        // The smallest group of a kind: one kind of fixture, most likely.
        if (!kind.isEmpty() && (!out.contains(kind) || out.value(kind)->fixtureList().size() > fids.size()))
            out.insert(kind, g);
    }
    return out;
}

/****************************************************************************
 * The groups
 ****************************************************************************/

void walkTabs(Walker &walk, App *app)
{
    app->setWorkspaceView(App::EverythingView, false);
    pump();
    for (int i = 0; i < app->m_tab->count(); ++i)
    {
        const QString label = app->m_tab->tabBar()->tabData(i).toString();
        if (!walk.mine(QStringLiteral("tabs"), label))
            continue;
        app->m_tab->setCurrentIndex(i);
        for (const QSize &sz : { kLaptop, kSmallestWindow })
        {
            app->resize(sz);
            pump(250);
            walk.record(app, QStringLiteral("tabs"),
                        QStringLiteral("%1 @ %2x%3").arg(label).arg(sz.width()).arg(sz.height()));
        }
    }
    app->resize(kLaptop);
}

QList<QMenu *> topMenus(App *app)
{
    QList<QMenu *> out;
    for (QAction *a : app->menuBar()->actions())
        if (a->menu() != nullptr && a->isVisible())
            out << a->menu();
    return out;
}

void walkMenus(Walker &walk, App *app)
{
    app->setWorkspaceView(App::EverythingView, false);
    app->resize(kLaptop);
    pump();
    for (QMenu *m : topMenus(app))
    {
        if (!walk.mine(QStringLiteral("menus"), m->title()))
            continue;
        m->popup(app->mapToGlobal(QPoint(10, 30)));
        pump(50);
        walk.recordMenu(m, QStringLiteral("menus"), m->title().remove(QLatin1Char('&')));
        m->hide();
        pump();
    }
}

bool denied(const QString &text)
{
    QString t = text;
    t.remove(QLatin1Char('&'));
    t.remove(QChar(0x2026));
    t = t.trimmed();
    for (const QString &d : kDenyStarts)
        if (t.startsWith(d, Qt::CaseInsensitive))
            return true;
    return false;
}

void collectActions(QMenu *m, const QString &path, QList<QPair<QString, QAction *>> &out, int depth = 0)
{
    // A menu filled as it opens.
    emit m->aboutToShow();
    for (QAction *a : m->actions())
    {
        if (a->isSeparator() || !a->isVisible())
            continue;
        QString t = a->text();
        t.remove(QRegularExpression(QStringLiteral("&(?!&)")));
        if (a->menu() != nullptr)
        {
            if (depth < 3)
                collectActions(a->menu(), path + QStringLiteral(" ▸ ") + t, out, depth + 1);
            continue;
        }
        // "…": the canon's mark of an item that opens a window to finish in.
        if (!t.endsWith(QChar(0x2026)) || !a->isEnabled() || a->isCheckable() || denied(t))
            continue;
        out << qMakePair(path + QStringLiteral(" ▸ ") + t, a);
    }
    emit m->aboutToHide();
}

void walkActions(Walker &walk, App *app, const Options &opt)
{
    app->setWorkspaceView(App::EverythingView, false);
    app->resize(kLaptop);
    pump();
    QList<QPair<QString, QAction *>> acts;
    for (QMenu *m : topMenus(app))
        collectActions(m, m->title().remove(QLatin1Char('&')), acts);
    for (const auto &pa : acts)
    {
        if (!walk.mine(QStringLiteral("actions"), pa.first))
            continue;
        QPointer<QAction> a = pa.second;
        if (a.isNull() || !a->isEnabled())
            continue;
        const int n = captureWindows(walk, QStringLiteral("actions"), pa.first, [a]() {
            if (!a.isNull())
                a->trigger();
        });
        if (n == 0)
            walk.note(QStringLiteral("actions"), pa.first, QStringLiteral("walker-no-window"),
                      QStringLiteral("the item ends in \"…\" but opened no window"));
        // Snapshot and restore: an item that changed the show gets it back.
        if (app->doc()->isModified())
        {
            qInfo().noquote() << "uxwalk: reloading the show after" << pa.first;
            app->doc()->resetModified();
            app->loadXML(opt.show);
            app->doc()->resetModified();
            app->setWorkspaceView(App::EverythingView, false);
            pump(200);
        }
    }
}

void walkScreens(Walker &walk, App *app)
{
    const QList<Screen> list = screens();
    for (const Screen &s : list)
    {
        // Plain tabs are the tabs group's.
        if (s.name.startsWith(QLatin1String("tab ")) && !s.prepare)
            continue;
        if (!walk.mine(QStringLiteral("screens"), s.name))
            continue;
        // A note a screen opens by itself ("no fixtures") never waits.
        QTimer modals;
        QObject::connect(&modals, &QTimer::timeout, [] { dismissModals(); });
        modals.start(20);
        QWidget *top = s.make(app);
        if (top == nullptr)
        {
            walk.note(QStringLiteral("screens"), s.name, QStringLiteral("walker-missing"),
                      QStringLiteral("not in this show"));
            continue;
        }
        QScopedPointer<QWidget> owner(s.owned ? top : nullptr);
        top->show();
        pump();
        if (s.prepare)
        {
            s.prepare(app, top);
            pump();
        }
        top->resize(kLaptop);
        pump(150);
        walk.record(top, QStringLiteral("screens"), s.name);
        // And at its smallest, as layoutsanity_test sizes it.
        QSize least = top->minimumSizeHint().expandedTo(top->minimumSize());
        if (!s.owned)
            least = least.expandedTo(kSmallestWindow);
        if (least.isValid() && !least.isEmpty() && least != kLaptop)
        {
            top->resize(least);
            pump(100);
            walk.record(top, QStringLiteral("screens"), s.name + QStringLiteral(" @ min"));
        }
        if (!s.owned)
            app->resize(kLaptop);
        else
            top->hide();
        dismissModals();
        pump();
    }
}

void walkContext(Walker &walk, App *app, bool dialogs)
{
    /* Each right-click target once: in the "context" group its menu is
       recorded; in "contextdialogs" every "…" item of it is picked in turn
       and the window it opens recorded (then Cancel, and the show put back
       if the item changed it). @a open does the right-click from scratch:
       selects the item afresh, as a person would. */
    auto hit = [&](const QString &name, const std::function<void()> &open) {
        if (!dialogs)
        {
            if (walk.mine(QStringLiteral("context"), name))
                open();
            return;
        }
        g_ctx.mode = CtxMode::List;
        g_ctx.found.clear();
        open();
        const QStringList items = g_ctx.found;
        for (const QString &item : items)
        {
            const QString what = name + QStringLiteral(" ▸ ") + item;
            if (!walk.mine(QStringLiteral("contextdialogs"), what))
                continue;
            g_ctx.mode = CtxMode::Pick;
            g_ctx.pick = item;
            const int n = captureWindows(walk, QStringLiteral("contextdialogs"), what, open, 400, true);
            g_ctx.mode = CtxMode::List;
            if (n == 0)
                walk.note(QStringLiteral("contextdialogs"), what, QStringLiteral("walker-no-window"),
                          QStringLiteral("the item ends in \"…\" but opened no window"));
            if (app->doc()->isModified())
            {
                qInfo().noquote() << "uxwalk: reloading the show after" << what;
                app->doc()->resetModified();
                app->loadXML(g_showPath);
                app->doc()->resetModified();
                app->setWorkspaceView(App::EverythingView, false);
                app->resize(kLaptop);
                pump(200);
            }
        }
        g_ctx.mode = CtxMode::Record;
    };
    Doc *doc = app->doc();
    app->setWorkspaceView(App::EverythingView, false);
    app->resize(kLaptop);
    auto toTab = [app](const QString &label) {
        for (int i = 0; i < app->m_tab->count(); ++i)
            if (app->m_tab->tabBar()->tabData(i).toString() == label)
            {
                app->m_tab->setCurrentIndex(i);
                pump(200);
                return true;
            }
        return false;
    };

    // Fixtures: a fixture and a group.
    {
        FixtureManager *fm = FixtureManager::instance();
        const QList<Fixture *> fixtures = doc->fixtures();
        const QList<FixtureGroup *> groups = doc->fixtureGroups();
        QSet<QString> names;
        for (Fixture *f : fixtures)
            names.insert(f->name());
        QSet<QString> gnames;
        for (FixtureGroup *g : groups)
            gnames.insert(g->name());
        auto onRow = [&](const QString &name, const std::function<bool(QTreeWidgetItem *)> &match) {
            hit(name, [&, name]() {
                toTab(QStringLiteral("Fixtures"));
                QTreeWidget *tree = fm != nullptr ? fm->m_fixtures_tree : nullptr;
                contextOnItem(walk, tree, findItem(tree, match), name);
            });
        };
        onRow(QStringLiteral("Fixtures ▸ a fixture"), [&](QTreeWidgetItem *it) {
            return names.contains(it->text(0)) && it->childCount() <= 1; });
        onRow(QStringLiteral("Fixtures ▸ a group"), [&](QTreeWidgetItem *it) {
            return gnames.contains(it->text(0)) && it->childCount() > 0; });
    }

    // The Show Tree: a cue, a layer, a hit.
    {
        const ShowParts sp = showParts(doc);
        auto onNode = [&](const QString &name, int kind, quint32 id) {
            hit(name, [&, name, kind, id]() {
                toTab(QStringLiteral("Show Tree"));
                ShowTreeView *stv = ShowTreeView::instance();
                QTreeWidget *tree = stv != nullptr ? stv->m_tree : nullptr;
                contextOnItem(walk, tree, findItem(tree, [&](QTreeWidgetItem *it) {
                                  return it->data(0, ShowTreeView::KindRole).toInt() == kind
                                      && it->data(0, ShowTreeView::IdRole).toUInt() == id;
                              }), name);
            });
        };
        onNode(QStringLiteral("Show Tree ▸ a cue"), ShowTreeView::CueNode, sp.cue);
        if (!sp.layers.isEmpty())
            onNode(QStringLiteral("Show Tree ▸ a layer"), ShowTreeView::LayerNode, sp.layers.first().second.first);
        onNode(QStringLiteral("Show Tree ▸ a hit"), ShowTreeView::CueNode, sp.hit);
        QSet<QString> groupNames;
        for (FixtureGroup *g : doc->fixtureGroups())
            groupNames.insert(g->name());
        hit(QStringLiteral("Show Tree ▸ Library group"), [&]() {
            toTab(QStringLiteral("Show Tree"));
            ShowTreeView *stv = ShowTreeView::instance();
            QTreeWidget *lib = stv != nullptr ? stv->m_library : nullptr;
            contextOnItem(walk, lib, findItem(lib, [&](QTreeWidgetItem *it) {
                              return groupNames.contains(it->text(0)); }),
                          QStringLiteral("Show Tree ▸ Library group"));
        });
    }

    // Build: a palette (look) and an effect in the navigator.
    {
        ProgrammingManager *pm = ProgrammingManager::instance();
        QSet<QString> looks, effects;
        for (QLCPalette *p : doc->palettes())
            (p->type() == QLCPalette::Effect ? effects : looks).insert(p->name());
        // Each in its own section of the navigator, as a person opens it.
        if (pm != nullptr)
        {
            hit(QStringLiteral("Build ▸ a look"), [&]() {
                toTab(QStringLiteral("Build"));
                pm->setSection(ProgrammingManager::PalettesSection);
                pump(150);
                contextOnItem(walk, pm->m_paletteTree, findItem(pm->m_paletteTree, [&](QTreeWidgetItem *it) {
                                  return looks.contains(it->text(0)); }), QStringLiteral("Build ▸ a look"));
            });
            hit(QStringLiteral("Build ▸ an effect"), [&]() {
                toTab(QStringLiteral("Build"));
                pm->setSection(ProgrammingManager::EffectsSection);
                pump(150);
                QTreeWidget *tree = pm->m_effectsList;
                QTreeWidgetItem *effect = findItem(tree, [&](QTreeWidgetItem *it) {
                    return effects.contains(it->text(0))
                        || effects.contains(it->text(0).remove(QStringLiteral("✦ ")));
                });
                contextOnItem(walk, tree, effect, QStringLiteral("Build ▸ an effect"));
            });
            pm->setSection(ProgrammingManager::ScenesSection);
            pump();
        }
    }

    // The Lighting Studio plot: a fixture and a target.
    {
        toTab(QStringLiteral("Lighting Studio"));
        Monitor *mon = Monitor::instance();
        MonitorGraphicsView *gv = mon != nullptr ? mon->m_graphicsView : nullptr;
        if (gv == nullptr)
            walk.note(QStringLiteral("context"), QStringLiteral("Plot"), QStringLiteral("walker-missing"),
                      QStringLiteral("no plot"));
        else
        {
            QGraphicsItem *fixture = nullptr;
            for (Fixture *f : doc->fixtures())
                if ((fixture = static_cast<QGraphicsItem *>(gv->fixtureItemForId(f->id()))) != nullptr
                    && fixture->isVisible())
                    break;
            const quint32 fixtureId = fixture != nullptr
                ? static_cast<MonitorFixtureItem *>(fixture)->fixtureID() : Fixture::invalidId();
            quint32 targetId = ShowTree::invalidId();
            for (auto it = gv->m_targetItems.begin(); it != gv->m_targetItems.end(); ++it)
                if (it.value() != nullptr)
                {
                    targetId = it.key();
                    break;
                }
            // Each time afresh: a dialog's Cancel can rebuild the plot.
            auto at = [&](const QString &name, bool isFixture) {
                hit(name, [&, name, isFixture]() {
                    toTab(QStringLiteral("Lighting Studio"));
                    Monitor *m = Monitor::instance();
                    MonitorGraphicsView *v = m != nullptr ? m->m_graphicsView : nullptr;
                    QGraphicsItem *item = v == nullptr ? nullptr
                        : isFixture ? static_cast<QGraphicsItem *>(v->fixtureItemForId(fixtureId))
                                    : static_cast<QGraphicsItem *>(v->m_targetItems.value(targetId));
                    if (item == nullptr)
                    {
                        walk.note(QStringLiteral("context"), name, QStringLiteral("walker-missing"),
                                  QStringLiteral("nothing of that kind in this show"));
                        return;
                    }
                    v->centerOn(item);
                    pump(50);
                    v->scene()->clearSelection();
                    item->setSelected(true);
                    const QPoint p = v->mapFromScene(item->sceneBoundingRect().center());
                    contextAt(walk, v->viewport(), p, name);
                });
            };
            at(QStringLiteral("Plot ▸ a fixture"), true);
            at(QStringLiteral("Plot ▸ a target"), false);
            // The empty canvas.
            hit(QStringLiteral("Plot ▸ the canvas"), [&]() {
                toTab(QStringLiteral("Lighting Studio"));
                Monitor *m = Monitor::instance();
                MonitorGraphicsView *v = m != nullptr ? m->m_graphicsView : nullptr;
                if (v == nullptr)
                    return;
                v->scene()->clearSelection();
                contextAt(walk, v->viewport(), QPoint(8, v->viewport()->height() - 8),
                          QStringLiteral("Plot ▸ the canvas"));
            });
        }
    }
}

void walkShowTree(Walker &walk, App *app)
{
    app->setWorkspaceView(App::EverythingView, false);
    app->resize(kLaptop);
    for (int i = 0; i < app->m_tab->count(); ++i)
        if (app->m_tab->tabBar()->tabData(i).toString() == QLatin1String("Show Tree"))
            app->m_tab->setCurrentIndex(i);
    pump(200);
    ShowTreeView *stv = ShowTreeView::instance();
    if (stv == nullptr)
        return;
    const ShowParts sp = showParts(app->doc());
    if (walk.mine(QStringLiteral("showtree"), QStringLiteral("a cue")))
    {
        stv->selectNode(sp.cue);
        pump(250);
        walk.record(app, QStringLiteral("showtree"), QStringLiteral("a cue"));
    }
    if (walk.mine(QStringLiteral("showtree"), QStringLiteral("a hit")) && sp.hit != ShowTree::invalidId())
    {
        stv->selectNode(sp.hit);
        pump(250);
        walk.record(app, QStringLiteral("showtree"), QStringLiteral("a hit"));
    }
    for (const QString &kind : { QStringLiteral("effect"), QStringLiteral("aim"), QStringLiteral("colour") })
    {
        if (!walk.mine(QStringLiteral("showtree"), kind))
            continue;
        bool found = false;
        for (const auto &l : sp.layers)
            if (l.first == kind)
            {
                stv->selectNode(l.second.first, l.second.second);
                pump(300);
                walk.record(app, QStringLiteral("showtree"), QStringLiteral("layer with %1").arg(kind));
                found = true;
            }
        if (!found)
            walk.note(QStringLiteral("showtree"), QStringLiteral("layer with %1").arg(kind),
                      QStringLiteral("walker-missing"), QStringLiteral("no such layer in this show"));
    }
}

void walkPickers(Walker &walk, App *app)
{
    Doc *doc = app->doc();
    const QMap<QString, FixtureGroup *> groups = groupsByKind(doc);
    EffectScriptCache *cache = doc->effectScriptRunner()->cache();
    for (const QString &kind : { QStringLiteral("mover"), QStringLiteral("par"), QStringLiteral("wled") })
    {
        if (!walk.mine(QStringLiteral("pickers"), kind))
            continue;
        const QString name = QStringLiteral("effect picker, %1 group").arg(kind);
        FixtureGroup *g = groups.value(kind);
        if (g == nullptr)
        {
            walk.note(QStringLiteral("pickers"), name, QStringLiteral("walker-missing"),
                      QStringLiteral("no %1 group in this show").arg(kind));
            continue;
        }
        Scene *scene = new Scene(doc);
        scene->setName(QStringLiteral("uxwalk %1").arg(kind));
        for (quint32 fid : g->fixtureList())
            scene->addFixture(fid);
        doc->addFunction(scene);
        QLCPalette *look = new QLCPalette(QLCPalette::Effect, doc);
        look->setName(QStringLiteral("uxwalk effect"));
        doc->addPalette(look);
        {
            LookEditor ed(doc);
            ed.setContextScene(scene);
            ed.setPalette(look->id());
            ed.resize(kLaptop);
            ed.show();
            pump(100);
            ed.openEffectPicker();
            pump(300);
            QWidget *picker = ed.m_effectPicker;
            EffectBrowser *browser = ed.m_effectBrowser;
            if (picker == nullptr || browser == nullptr || !picker->isVisible())
                walk.note(QStringLiteral("pickers"), name, QStringLiteral("walker-missing"),
                          QStringLiteral("the picker did not open"));
            else
            {
                // Every tab, all listed: what is offered for this group.
                QJsonArray offered;
                browser->search()->setText(QString());
                for (int t = 0; t < browser->tabs()->count(); ++t)
                {
                    browser->tabs()->setCurrentIndex(t);
                    pump(30);
                    for (int i = 0; i < browser->list()->count(); ++i)
                    {
                        QListWidgetItem *item = browser->list()->item(i);
                        const int e = item->data(Qt::UserRole + 10).toInt();
                        if (e < 0 || e >= browser->m_entries.size())
                            continue;
                        const auto &en = browser->m_entries.at(e);
                        const bool wled = en.category == QLatin1String("WLED")
                                          || !cache->scriptMeta(en.name).wled.isEmpty();
                        offered.append(QJsonObject{
                            { QStringLiteral("tab"), browser->tabs()->tabText(t) },
                            { QStringLiteral("text"), item->text() },
                            { QStringLiteral("name"), en.name },
                            { QStringLiteral("category"), en.category },
                            { QStringLiteral("wled"), wled },
                            { QStringLiteral("fits"), en.fit.fits },
                        });
                    }
                }
                browser->tabs()->setCurrentIndex(0);
                pump(100);
                QJsonObject extra{
                    { QStringLiteral("target"), kind },
                    { QStringLiteral("group"), QStringLiteral("pickers") },
                    { QStringLiteral("targetGroup"), g->name() },
                    { QStringLiteral("targetWled"), browser->wledTargets() },
                    { QStringLiteral("offered"), offered },
                };
                if (picker->width() < kLaptop.width())
                    picker->resize(qMax(picker->width(), 900), qMax(picker->height(), 640));
                pump(100);
                walk.record(picker, QStringLiteral("pickers"), name, extra);
                ed.closeEffectPicker(false);
            }
            ed.hide();
        }
        doc->deletePalette(look->id());
        doc->deleteFunction(scene->id());
        pump();
    }
}

void walkCommandLine(Walker &walk, App *app)
{
    app->setWorkspaceView(App::EverythingView, false);
    app->resize(kLaptop);
    for (int i = 0; i < app->m_tab->count(); ++i)
        if (app->m_tab->tabBar()->tabData(i).toString() == QLatin1String("Build"))
            app->m_tab->setCurrentIndex(i);
    pump(200);
    ProgrammingManager *pm = ProgrammingManager::instance();
    BuildCommandBar *bar = pm != nullptr ? pm->commandBar() : nullptr;
    if (bar == nullptr)
    {
        walk.note(QStringLiteral("cmdline"), QStringLiteral("Build command line"),
                  QStringLiteral("walker-missing"), QStringLiteral("no command bar"));
        return;
    }
    const QList<QPair<QString, QString>> lines = {
        { QStringLiteral("a typo"), QStringLiteral("pallete colour \"Red\" #ff0000") },
        { QStringLiteral("plain words"), QStringLiteral("make the movers look warm and slow for the ballad") },
    };
    for (const auto &l : lines)
    {
        if (!walk.mine(QStringLiteral("cmdline"), l.first))
            continue;
        app->doc()->resetModified();
        bar->line()->setText(l.second);
        captureWindows(walk, QStringLiteral("cmdline"), QStringLiteral("%1 (opened by it)").arg(l.first), [&]() {
            bar->runLine(l.second);
        }, 300);
        pump(200);
        walk.record(app, QStringLiteral("cmdline"), QStringLiteral("Build command line, %1").arg(l.first),
                    QJsonObject{ { QStringLiteral("line"), l.second },
                                 { QStringLiteral("message"), bar->message() },
                                 { QStringLiteral("aiProvider"), false } });
        bar->line()->clear();
        if (app->doc()->isModified())
            walk.note(QStringLiteral("cmdline"), l.first, QStringLiteral("walker-modified"),
                      QStringLiteral("the line changed the show: %1").arg(l.second));
    }
}

} // namespace uxwalk

using namespace uxwalk;

#if defined(Q_OS_UNIX)
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
/* A crash says where: the driver puts the stack in the report and reruns
   the shard past the screen it was on. */
static void crashed(int sig)
{
    void *frames[64];
    const int n = backtrace(frames, 64);
    const char head[] = "uxwalk-crash: stack follows\n";
    write(2, head, sizeof(head) - 1);
    backtrace_symbols_fd(frames, n, 2);
    _exit(128 + sig);
}
#endif

int main(int argc, char **argv)
{
    // A HOME of its own before anything reads it: never the person's.
    static QTemporaryDir home;
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("USERPROFILE", home.path().toLocal8Bit());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    qputenv("QLCCONSOLE_NO_UPDATES", "1");

    QApplication qapp(argc, argv);
#if defined(Q_OS_UNIX)
    signal(SIGSEGV, crashed);
    signal(SIGBUS, crashed);
    signal(SIGABRT, crashed);
#endif
    Options opt;
    const QStringList args = qapp.arguments();
    for (int i = 1; i < args.size(); ++i)
    {
        const QString a = args.at(i);
        const QString v = i + 1 < args.size() ? args.at(i + 1) : QString();
        if (a == QLatin1String("--show")) { opt.show = v; ++i; }
        else if (a == QLatin1String("--units")) { opt.units = v; ++i; }
        else if (a == QLatin1String("--out")) { opt.out = v; ++i; }
        else if (a == QLatin1String("--groups")) { opt.groups = v.split(QLatin1Char(','), Qt::SkipEmptyParts); ++i; }
        else if (a == QLatin1String("--shard"))
        {
            const QStringList p = v.split(QLatin1Char('/'));
            opt.shard = p.value(0).toInt();
            opt.shards = qMax(1, p.value(1).toInt());
            ++i;
        }
        else if (a == QLatin1String("--skip")) { opt.skip = v.toInt(); ++i; }
        else if (a == QLatin1String("--list"))
        {
            printf("%s\n", qPrintable(kGroups.join(QLatin1Char(','))));
            return 0;
        }
        else
        {
            fprintf(stderr, "uxwalk: unknown argument %s\n", qPrintable(a));
            return 2;
        }
    }
    if (opt.show.isEmpty() || opt.out.isEmpty() || !QFileInfo::exists(opt.show))
    {
        fprintf(stderr, "usage: uxwalk --show FILE --units feet|mm --out DIR [--groups a,b] [--shard I/N]\n");
        return 2;
    }
    opt.show = QFileInfo(opt.show).absoluteFilePath();
    g_showPath = opt.show;
    QElapsedTimer clock;
    clock.start();

    EffectScriptCache::setSystemDirectoriesOverride(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts",
                                                    QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts");
    // Settings > Units, as the person set it, before the show is opened.
    MeasurementUnits::setPreferredLength(opt.units == QLatin1String("mm") ? MeasurementUnits::Millimetres
                                                                           : MeasurementUnits::Feet);

    App *app = new App();
    app->init();
    app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures"));
    if (app->loadXML(opt.show) != QFile::NoError)
    {
        fprintf(stderr, "uxwalk: could not load %s\n", qPrintable(opt.show));
        return 2;
    }
    app->doc()->resetModified();
    app->resize(kLaptop);
    app->showNormal();
    pump(300);
    qInfo().noquote() << "uxwalk: loaded" << opt.show << "in" << clock.elapsed() << "ms,"
                      << app->doc()->fixtures().size() << "fixtures";

    Walker walk(app, opt);
    for (const QString &g : opt.groups)
    {
        QElapsedTimer t;
        t.start();
        const int before = walk.written();
        if (g == QLatin1String("tabs"))
            walkTabs(walk, app);
        else if (g == QLatin1String("menus"))
            walkMenus(walk, app);
        else if (g == QLatin1String("actions"))
            walkActions(walk, app, opt);
        else if (g == QLatin1String("screens"))
            walkScreens(walk, app);
        else if (g == QLatin1String("context"))
            walkContext(walk, app, false);
        else if (g == QLatin1String("contextdialogs"))
            walkContext(walk, app, true);
        else if (g == QLatin1String("showtree"))
            walkShowTree(walk, app);
        else if (g == QLatin1String("pickers"))
            walkPickers(walk, app);
        else if (g == QLatin1String("cmdline"))
            walkCommandLine(walk, app);
        else
            fprintf(stderr, "uxwalk: no group %s\n", qPrintable(g));
        dismissModals();
        pump();
        qInfo().noquote() << QStringLiteral("uxwalk: group %1: %2 records in %3 ms")
                                 .arg(g).arg(walk.written() - before).arg(t.elapsed());
    }

    printf("uxwalk-shard: %s %s %s shard %d/%d: %d records in %.1f s\n",
           qPrintable(QFileInfo(opt.show).fileName()), qPrintable(opt.units),
           qPrintable(opt.groups.join(QLatin1Char(','))), opt.shard, opt.shards,
           walk.written(), clock.elapsed() / 1000.0);
    fflush(stdout);
    app->doc()->resetModified();
    // Leave without tearing the app down: a walk's state is not worth a
    // crash in a destructor, and the OS frees it all.
    _exit(0);
}
