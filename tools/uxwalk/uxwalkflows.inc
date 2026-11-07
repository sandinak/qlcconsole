/*
  qlcconsole
  uxwalkflows.inc -- the walker's "flows" group (included by uxwalk.cpp)

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

/* The core flows of docs/dev/ux_flows.json, driven step by step on a canned
 * show, as a person does them: each step's control must be on the screen the
 * step before left (or something there must point to it), it must work, and
 * it must do what the step says. A step that can't be found from the screen
 * before is a dead end (UX-61). The walker only records: each step as a
 * screen record, each flow as a "flow" record with its steps' results;
 * report.py turns them into findings and the flow table.
 *
 * Step results: pass, pointed (not on screen, but a word on it points
 * there), deadend (exists in the app, not reachable from here, nothing
 * points to it), missing (no such control in this build), expect (it ran
 * but didn't do what the step says), manual, notreached (an earlier step
 * failed). A failed "find" step (a findability check, no action) does not
 * stop the flow; any other failure does.
 */

namespace flows
{

QString g_flowsFile;

/* The template exists only on a build with the autosave branch: let the
   offscreen app ask about a recovery file, where the build can. */
template <typename T> auto allowRecoveryAsk(int) -> decltype(T::s_askRecoveryOffscreen = true, bool())
{
    T::s_askRecoveryOffscreen = true;
    return true;
}
template <typename T> bool allowRecoveryAsk(long) { return false; }

/** The window a person is working in: a modal one in front, else the app. */
QWidget *screenTop(App *app)
{
    if (QWidget *m = QApplication::activeModalWidget())
        return m;
    return app;
}

bool shownIn(QWidget *w, QWidget *top)
{
    return w != nullptr && (w == top || (w->isVisibleTo(top) && w->width() > 0 && w->height() > 0));
}

QWidget *findVisible(QWidget *top, const QString &name)
{
    if (top->objectName() == name)
        return top;
    for (QWidget *w : top->findChildren<QWidget *>(name))
        if (shownIn(w, top))
            return w;
    return nullptr;
}

/** Anywhere in the app, shown or not: "built, but not here" vs "missing". */
bool existsAnywhere(const QString &name)
{
    for (QWidget *t : QApplication::topLevelWidgets())
        if (t->objectName() == name || t->findChild<QObject *>(name) != nullptr)
            return true;
    return false;
}

/** What a person can read on @a top: labels, buttons, group titles, combo
 *  texts, the rows of a visible tree or list (not tooltips: a tooltip is
 *  found only by someone already hovering the control). */
QStringList visibleTexts(QWidget *top)
{
    QStringList out;
    QList<QWidget *> all = top->findChildren<QWidget *>();
    all.prepend(top);
    for (QWidget *w : all)
    {
        if (!shownIn(w, top))
            continue;
        if (QLabel *l = qobject_cast<QLabel *>(w))
            out << plain(l->text());
        else if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
            out << b->text().remove(QLatin1Char('&'));
        else if (QGroupBox *g = qobject_cast<QGroupBox *>(w))
            out << g->title().remove(QLatin1Char('&'));
        else if (QComboBox *c = qobject_cast<QComboBox *>(w))
            out << c->currentText();
        else if (QAbstractSpinBox *s = qobject_cast<QAbstractSpinBox *>(w))
            out << s->text();
        else if (QTreeWidget *t = qobject_cast<QTreeWidget *>(w))
        {
            for (QTreeWidgetItemIterator it(t); *it != nullptr; ++it)
            {
                bool open = true;
                for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
                    open = open && p->isExpanded();
                if (open && !(*it)->isHidden())
                    for (int c = 0; c < t->columnCount(); ++c)
                        out << (*it)->text(c);
            }
        }
        else if (QListWidget *lw = qobject_cast<QListWidget *>(w))
            for (int i = 0; i < lw->count(); ++i)
                out << lw->item(i)->text();
        else if (QAbstractItemView *v = qobject_cast<QAbstractItemView *>(w))
        {
            // A model view (the Run tab's cue list): its top rows and their
            // children, every column.
            QAbstractItemModel *m = v->model();
            if (m == nullptr || qobject_cast<QHeaderView *>(v) != nullptr)
                continue;
            std::function<void(const QModelIndex &, int)> rows = [&](const QModelIndex &parent, int depth) {
                for (int r = 0; r < m->rowCount(parent) && r < 400; ++r)
                {
                    for (int c = 0; c < m->columnCount(parent) && c < 10; ++c)
                        out << m->index(r, c, parent).data(Qt::DisplayRole).toString();
                    if (depth < 2)
                        rows(m->index(r, 0, parent), depth + 1);
                }
            };
            rows(v->rootIndex(), 0);
        }
    }
    out.removeAll(QString());
    return out;
}

QString widgetText(QWidget *w)
{
    if (QLabel *l = qobject_cast<QLabel *>(w))
        return plain(l->text());
    if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
        return b->text().remove(QLatin1Char('&'));
    if (QComboBox *c = qobject_cast<QComboBox *>(w))
        return c->currentText();
    if (QAbstractSpinBox *s = qobject_cast<QAbstractSpinBox *>(w))
        return s->text();
    if (QLineEdit *e = qobject_cast<QLineEdit *>(w))
        return e->text();
    return w->windowTitle();
}

QRegularExpression rx(const QString &pattern)
{
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
}

int comboIndex(QComboBox *c, const QString &item)
{
    const QRegularExpression r = rx(item);
    for (int i = 0; i < c->count(); ++i)
        if (r.match(c->itemText(i)).hasMatch())
            return i;
    return -1;
}

bool comboItemEnabled(QComboBox *c, int i)
{
    const QStandardItemModel *m = qobject_cast<const QStandardItemModel *>(c->model());
    return m == nullptr || m->item(i) == nullptr || m->item(i)->isEnabled();
}

/****************************************************************************
 * One flow's run
 ****************************************************************************/

struct StepResult
{
    QString status = QStringLiteral("pass");
    QString detail;
};

class FlowRun
{
public:
    FlowRun(Walker &walk, App *app, const QJsonObject &flow)
        : m_walk(walk), m_app(app), m_flow(flow)
    {
        m_id = flow.value(QStringLiteral("id")).toString();
    }

    QJsonObject run()
    {
        const QJsonArray steps = m_flow.value(QStringLiteral("steps")).toArray();
        QJsonArray results;
        int clicks = 0;
        bool stopped = false;
        QString firstFail;
        bool anyAuto = false;
        for (int i = 0; i < steps.size(); ++i)
        {
            const QJsonObject s = steps.at(i).toObject();
            const bool find = s.contains(QStringLiteral("find"));
            if (!find)
                clicks += s.value(QStringLiteral("clicks")).toInt(1);
            StepResult r;
            if (s.contains(QStringLiteral("manual")))
            {
                r.status = QStringLiteral("manual");
                r.detail = s.value(QStringLiteral("manual")).toString();
            }
            else if (stopped)
                r.status = QStringLiteral("notreached");
            else
            {
                anyAuto = true;
                m_step = i + 1;
                m_seen.clear();
                r = find ? doFind(s) : doStep(s);
                if (r.status == QLatin1String("pass") || r.status == QLatin1String("pointed"))
                    r = expect(s, r);
                m_walk.record(screenTop(m_app), QStringLiteral("flows"),
                              QStringLiteral("%1 step %2: %3").arg(m_id).arg(i + 1)
                                  .arg(s.value(QStringLiteral("control")).toString()),
                              QJsonObject{ { QStringLiteral("flow"), m_id }, { QStringLiteral("step"), i + 1 },
                                           { QStringLiteral("stepStatus"), r.status },
                                           { QStringLiteral("stepDetail"), r.detail } });
                const bool bad = r.status != QLatin1String("pass") && r.status != QLatin1String("pointed");
                if (bad && firstFail.isEmpty())
                    firstFail = QStringLiteral("step %1: %2").arg(i + 1).arg(r.status);
                if (bad && !find)
                    stopped = true;
            }
            qInfo().noquote() << "uxwalk-flow:" << m_id << "step" << (i + 1) << r.status << r.detail;
            results.append(QJsonObject{ { QStringLiteral("n"), i + 1 },
                                        { QStringLiteral("screen"), s.value(QStringLiteral("screen")) },
                                        { QStringLiteral("control"), s.value(QStringLiteral("control")) },
                                        { QStringLiteral("expect"), s.value(QStringLiteral("expect")) },
                                        { QStringLiteral("find"), find },
                                        { QStringLiteral("clicks"), find ? 0 : s.value(QStringLiteral("clicks")).toInt(1) },
                                        { QStringLiteral("status"), r.status },
                                        { QStringLiteral("detail"), r.detail } });
        }
        QString status = !anyAuto ? QStringLiteral("manual")
                       : firstFail.isEmpty() ? QStringLiteral("pass") : QStringLiteral("fail");
        return QJsonObject{ { QStringLiteral("flow"), m_id },
                            { QStringLiteral("title"), m_flow.value(QStringLiteral("title")) },
                            { QStringLiteral("status"), status },
                            { QStringLiteral("firstFail"), firstFail },
                            { QStringLiteral("clicks"), clicks },
                            { QStringLiteral("budget"), m_flow.value(QStringLiteral("budget")) },
                            { QStringLiteral("manualWhy"), m_flow.value(QStringLiteral("manual")) },
                            { QStringLiteral("steps"), results } };
    }

private:
    /************************************************************************
     * Reachability
     ************************************************************************/

    /** The control @a name is not on the screen: is it somewhere else in the
     *  app (dead end, unless a word here points there) or not built? */
    StepResult notHere(const QString &what, const QString &name, const QJsonObject &s)
    {
        StepResult r;
        const QJsonArray pointer = s.value(QStringLiteral("pointer")).toArray();
        const QStringList texts = visibleTexts(screenTop(m_app));
        for (const QJsonValue &p : pointer)
            for (const QString &t : texts)
                if (t.contains(p.toString(), Qt::CaseInsensitive))
                {
                    r.status = QStringLiteral("pointed");
                    r.detail = QStringLiteral("%1 is not on this screen; \"%2\" points there").arg(what, t.left(120));
                    return r;
                }
        if (!name.isEmpty() && !existsAnywhere(name))
        {
            r.status = QStringLiteral("missing");
            r.detail = QStringLiteral("%1 is not in this build").arg(what);
            return r;
        }
        r.status = QStringLiteral("deadend");
        r.detail = QStringLiteral("%1 is not on this screen%2").arg(what,
            pointer.isEmpty() ? QString() : QStringLiteral(", and nothing on it says where it is"));
        return r;
    }

    ShowTreeView *showTree() const { return ShowTreeView::instance(); }

    QTreeWidgetItem *node(const QString &kind)
    {
        ShowTreeView *stv = showTree();
        if (stv == nullptr)
            return nullptr;
        const ShowParts sp = showParts(m_app->doc());
        return findItem(stv->m_tree, [&](QTreeWidgetItem *it) {
            const int k = it->data(0, ShowTreeView::KindRole).toInt();
            const quint32 id = it->data(0, ShowTreeView::IdRole).toUInt();
            if (kind == QLatin1String("production"))
                return k == ShowTreeView::ProductionNode;
            if (kind == QLatin1String("list"))
                return k == ShowTreeView::ListNode && id != sp.hitList;
            if (kind == QLatin1String("cue"))
                return k == ShowTreeView::CueNode && id == sp.cue;
            if (kind == QLatin1String("hit"))
                return k == ShowTreeView::CueNode && id == sp.hit;
            if (kind == QLatin1String("section"))
                return k == ShowTreeView::SectionNode && it->childCount() > 0;
            return false;
        });
    }

    MonitorFixtureItem *plotFixture(const QString &model)
    {
        Monitor *mon = Monitor::instance();
        MonitorGraphicsView *gv = mon != nullptr ? mon->m_graphicsView : nullptr;
        if (gv == nullptr)
            return nullptr;
        const QRegularExpression r = rx(model);
        for (Fixture *f : m_app->doc()->fixtures())
        {
            const QString m = f->fixtureDef() != nullptr ? f->fixtureDef()->model() : f->name();
            if (!r.match(m).hasMatch() && !r.match(f->name()).hasMatch())
                continue;
            if (MonitorFixtureItem *item = gv->fixtureItemForId(f->id()))
                if (item->isVisible())
                    return item;
        }
        return nullptr;
    }

    /************************************************************************
     * Steps
     ************************************************************************/

    /** A findability check: is @a find's control on this screen, or pointed to? */
    StepResult doFind(const QJsonObject &s)
    {
        const QJsonObject f = s.value(QStringLiteral("find")).toObject();
        if (f.contains(QStringLiteral("widget")))
        {
            const QString name = f.value(QStringLiteral("widget")).toString();
            if (findVisible(screenTop(m_app), name) != nullptr)
                return StepResult();
            return notHere(s.value(QStringLiteral("control")).toString(), name, s);
        }
        if (f.contains(QStringLiteral("text")))
        {
            const QRegularExpression r = rx(f.value(QStringLiteral("text")).toString());
            for (const QString &t : visibleTexts(screenTop(m_app)))
                if (r.match(t).hasMatch())
                    return StepResult();
            return notHere(s.value(QStringLiteral("control")).toString(), QString(), s);
        }
        if (f.contains(QStringLiteral("context")))
        {
            QStringList items;
            QString why;
            if (!contextMenu(f.value(QStringLiteral("context")).toString(), QString(), false, &items, &why))
                return StepResult{ QStringLiteral("deadend"), why };
            const QRegularExpression r = rx(f.value(QStringLiteral("item")).toString());
            for (const QString &t : items)
                if (r.match(t).hasMatch())
                    return StepResult{ QStringLiteral("pass"), t };
            StepResult r2 = notHere(s.value(QStringLiteral("control")).toString(), QString(), s);
            r2.detail += QStringLiteral(" (its right-click: %1)").arg(items.join(QStringLiteral(" | ")).left(300));
            return r2;
        }
        return StepResult{ QStringLiteral("expect"), QStringLiteral("a find step with nothing to find") };
    }

    StepResult doStep(const QJsonObject &s)
    {
        const QJsonObject d = s.value(QStringLiteral("do")).toObject();
        m_cuesBefore = cueCount();
        StepResult r;
        withWindows(s, [&]() { r = act(d, s, m_app); });
        return r;
    }

    /** One action, in @a top (the app, or a dialog the step opened). */
    StepResult act(const QJsonObject &d, const QJsonObject &s, QWidget *top)
    {
        const QString control = s.value(QStringLiteral("control")).toString();
        if (d.contains(QStringLiteral("tab")))
        {
            const QString label = d.value(QStringLiteral("tab")).toString();
            for (int i = 0; i < m_app->m_tab->count(); ++i)
                if (m_app->m_tab->tabBar()->tabData(i).toString() == label && m_app->m_tab->isTabVisible(i))
                {
                    m_app->m_tab->setCurrentIndex(i);
                    pump(250);
                    return StepResult();
                }
            return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 tab").arg(label) };
        }
        if (d.contains(QStringLiteral("node")))
        {
            ShowTreeView *stv = showTree();
            QTreeWidgetItem *it = node(d.value(QStringLiteral("node")).toString());
            if (stv == nullptr || it == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 row in this show").arg(d.value(QStringLiteral("node")).toString()) };
            if (!stv->m_tree->isVisible())
                return notHere(QStringLiteral("the Show Tree"), QString(), s);
            for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
                p->setExpanded(true);
            stv->m_tree->clearSelection();
            stv->m_tree->setCurrentItem(it);
            it->setSelected(true);
            stv->m_tree->scrollToItem(it);
            pump(250);
            return StepResult{ QStringLiteral("pass"), it->text(0) };
        }
        if (d.contains(QStringLiteral("fold")))
        {
            QTreeWidgetItem *it = node(d.value(QStringLiteral("fold")).toString());
            if (it == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 row with cues under it").arg(d.value(QStringLiteral("fold")).toString()) };
            it->setExpanded(false);
            pump(100);
            return StepResult{ QStringLiteral("pass"), it->text(0) };
        }
        if (d.contains(QStringLiteral("plot")))
        {
            MonitorFixtureItem *item = plotFixture(d.value(QStringLiteral("plot")).toString());
            if (item == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 on the plot").arg(d.value(QStringLiteral("plot")).toString()) };
            MonitorGraphicsView *gv = Monitor::instance()->m_graphicsView;
            if (!gv->isVisible())
                return notHere(QStringLiteral("the plot"), QString(), s);
            gv->centerOn(item);
            gv->scene()->clearSelection();
            item->setSelected(true);
            pump(300);
            return StepResult();
        }
        if (d.contains(QStringLiteral("click")))
        {
            const QString name = d.value(QStringLiteral("click")).toString();
            QWidget *w = findVisible(top, name);
            if (w == nullptr)
                return notHere(control, name, s);
            if (!w->isEnabled())
                return StepResult{ QStringLiteral("deadend"), QStringLiteral("%1 is greyed%2").arg(control,
                    w->toolTip().isEmpty() ? QString() : QStringLiteral(": ") + plain(w->toolTip()).left(160)) };
            if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
                b->click();
            else
                return StepResult{ QStringLiteral("expect"), QStringLiteral("%1 is not a button").arg(name) };
            pump(250);
            return StepResult();
        }
        if (d.contains(QStringLiteral("choose")))
        {
            const QString name = d.value(QStringLiteral("choose")).toString();
            QComboBox *c = qobject_cast<QComboBox *>(findVisible(top, name));
            if (c == nullptr)
                return notHere(control, name, s);
            const int i = comboIndex(c, d.value(QStringLiteral("item")).toString());
            if (i < 0)
            {
                QStringList items;
                for (int k = 0; k < c->count(); ++k)
                    items << c->itemText(k);
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no entry \"%1\" (it has: %2)")
                                       .arg(d.value(QStringLiteral("item")).toString(), items.join(QStringLiteral(" | ")).left(300)) };
            }
            if (!c->isEnabled() || !comboItemEnabled(c, i))
                return StepResult{ QStringLiteral("deadend"), QStringLiteral("\"%1\" is greyed").arg(c->itemText(i)) };
            c->setCurrentIndex(i);
            emit c->activated(i);
            pump(300);
            return StepResult{ QStringLiteral("pass"), c->itemText(i) };
        }
        if (d.contains(QStringLiteral("context")))
        {
            QStringList items;
            QString why;
            const QString pick = d.value(QStringLiteral("item")).toString();
            if (!contextMenu(d.value(QStringLiteral("context")).toString(), pick, true, &items, &why))
                return StepResult{ why.startsWith(QLatin1String("no ")) ? QStringLiteral("missing") : QStringLiteral("deadend"), why };
            pump(200);
            return StepResult{ QStringLiteral("pass"), why };
        }
        if (d.contains(QStringLiteral("action")))
        {
            const QString name = d.value(QStringLiteral("action")).toString();
            QAction *a = m_app->findChild<QAction *>(name);
            if (a == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 in this build").arg(control) };
            bool inMenu = false;
            for (QMenu *m : topMenus(m_app))
            {
                QList<QAction *> acts;
                collectAll(m, acts);
                inMenu = inMenu || acts.contains(a);
            }
            if (!inMenu && !a->isVisible())
                return notHere(control, QString(), s);
            if (!a->isEnabled())
                return StepResult{ QStringLiteral("deadend"), QStringLiteral("%1 is greyed").arg(control) };
            a->trigger();
            pump(250);
            return StepResult();
        }
        if (d.contains(QStringLiteral("menu")))
        {
            const QString name = d.value(QStringLiteral("menu")).toString();
            QToolButton *b = qobject_cast<QToolButton *>(findVisible(top, name));
            if (b == nullptr || b->menu() == nullptr)
                return notHere(control, name, s);
            emit b->menu()->aboutToShow();
            QAction *a = b->menu()->findChild<QAction *>(d.value(QStringLiteral("item")).toString());
            if (a == nullptr)
                for (QAction *x : b->menu()->actions())
                    if (x->objectName() == d.value(QStringLiteral("item")).toString())
                        a = x;
            if (a == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 in that menu").arg(control) };
            if (!a->isEnabled())
                return StepResult{ QStringLiteral("deadend"), QStringLiteral("%1 is greyed: %2").arg(control, a->toolTip()) };
            emit b->menu()->aboutToHide();
            a->trigger();
            pump(250);
            return StepResult();
        }
        if (d.contains(QStringLiteral("key")))
        {
            const QKeySequence seq(d.value(QStringLiteral("key")).toString());
            if (seq.isEmpty())
                return StepResult{ QStringLiteral("expect"), QStringLiteral("bad key") };
            const QKeyCombination kc = seq[0];
            // Where the person's keyboard is: the step's "to" (the panel they
            // are working in), else the focus, else the window.
            QWidget *target = d.contains(QStringLiteral("to"))
                ? findVisible(top, d.value(QStringLiteral("to")).toString()) : QApplication::focusWidget();
            if (target == nullptr && d.contains(QStringLiteral("to")))
                return notHere(control, d.value(QStringLiteral("to")).toString(), s);
            if (target == nullptr || (!d.contains(QStringLiteral("to")) && target->window() != top->window()))
                target = top;
            // The key may close (and delete) what it was sent to.
            const QString cls = QLatin1String(target->metaObject()->className());
            QTest::keyClick(target, kc.key(), kc.keyboardModifiers());
            pump(300);
            return StepResult{ QStringLiteral("pass"), QStringLiteral("to %1").arg(cls) };
        }
        if (d.contains(QStringLiteral("code")))
            return code(d.value(QStringLiteral("code")).toString(), s);
        if (d.isEmpty())
            return StepResult();        // a look: only the step's check
        return StepResult{ QStringLiteral("expect"), QStringLiteral("a step with nothing to do") };
    }

    static void collectAll(QMenu *m, QList<QAction *> &out, int depth = 0)
    {
        emit m->aboutToShow();
        for (QAction *a : m->actions())
        {
            out << a;
            if (a->menu() != nullptr && depth < 3)
                collectAll(a->menu(), out, depth + 1);
        }
        emit m->aboutToHide();
    }

    /** Right-click a Show Tree row of @a kind, or a plot fixture
     *  ("plot:<model>"): the items (and its submenus'), and pick @a pick
     *  (a regex over a top-level item) when @a doPick. */
    bool contextMenu(const QString &kind, const QString &pick, bool doPick, QStringList *items, QString *why)
    {
        QWidget *target = nullptr;
        QPoint pos;
        if (kind.startsWith(QLatin1String("plot:")))
        {
            MonitorFixtureItem *item = plotFixture(kind.mid(5));
            if (item == nullptr)
            {
                *why = QStringLiteral("no %1 on the plot").arg(kind.mid(5));
                return false;
            }
            MonitorGraphicsView *gv = Monitor::instance()->m_graphicsView;
            if (!gv->isVisible())
            {
                *why = QStringLiteral("the plot is not on this screen");
                return false;
            }
            gv->centerOn(item);
            pump(50);
            gv->scene()->clearSelection();
            item->setSelected(true);
            target = gv->viewport();
            pos = gv->mapFromScene(item->sceneBoundingRect().center());
        }
        else
        {
            ShowTreeView *stv = showTree();
            QTreeWidgetItem *it = node(kind);
            if (stv == nullptr || it == nullptr)
            {
                *why = QStringLiteral("no %1 row in this show").arg(kind);
                return false;
            }
            if (!stv->m_tree->isVisible())
            {
                *why = QStringLiteral("the Show Tree is not on this screen");
                return false;
            }
            for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
                p->setExpanded(true);
            stv->m_tree->clearSelection();
            stv->m_tree->setCurrentItem(it);
            it->setSelected(true);
            stv->m_tree->scrollToItem(it);
            pump(100);
            target = stv->m_tree->viewport();
            pos = stv->m_tree->visualItemRect(it).center();
        }
        bool seen = false, picked = false;
        QString pickedText;
        QTimer poll;
        poll.setInterval(10);
        QObject::connect(&poll, &QTimer::timeout, [&]() {
            QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (m == nullptr)
                return;
            poll.stop();
            seen = true;
            QList<QAction *> all;
            collectAll(m, all);
            for (QAction *a : all)
                if (!a->isSeparator() && a->isVisible())
                    *items << a->text().remove(QRegularExpression(QStringLiteral("&(?!&)")));
            m_walk.recordMenu(m, QStringLiteral("flows"),
                              QStringLiteral("%1 step %2: right-click %3").arg(m_id).arg(m_step).arg(kind));
            QAction *hit = nullptr;
            if (doPick)
                for (QAction *a : m->actions())
                {
                    const QString t = a->text().remove(QRegularExpression(QStringLiteral("&(?!&)")));
                    if (!a->isSeparator() && a->isVisible() && a->isEnabled() && a->menu() == nullptr
                        && rx(pick).match(t).hasMatch())
                    {
                        hit = a;
                        pickedText = t;
                        break;
                    }
                }
            if (hit == nullptr)
            {
                m->close();
                return;
            }
            picked = true;
            m->setActiveAction(hit);
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(m, &press);
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
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMenu *mm = qobject_cast<QMenu *>(w))
                if (mm->isVisible())
                    mm->close();
        if (!seen)
        {
            *why = QStringLiteral("the right-click opened no menu");
            return false;
        }
        if (doPick && !picked)
        {
            *why = QStringLiteral("no \"%1\" in its right-click (it has: %2)").arg(pick, items->join(QStringLiteral(" | ")).left(300));
            return false;
        }
        *why = pickedText;
        return true;
    }

    /** Run @a body; a window it opens (modal: inside its exec()) gets the
     *  step's "dialog" sub-steps, is recorded and closed as "close" says. A
     *  window the step didn't expect is recorded and cancelled. */
    void withWindows(const QJsonObject &s, const std::function<void()> &body)
    {
        const QSet<QWidget *> before = windowsNow();
        QSet<QWidget *> handled;
        const QJsonArray sub = s.value(QStringLiteral("dialog")).toArray();
        const QString close = s.value(QStringLiteral("close")).toString(QStringLiteral("reject"));
        bool busy = false;
        QElapsedTimer age;
        QTimer poll;
        poll.setInterval(40);
        QObject::connect(&poll, &QTimer::timeout, [&]() {
            if (busy)
                return;
            for (QWidget *w : windowsNow())
            {
                if (before.contains(w) || handled.contains(w) || qobject_cast<QMenu *>(w) != nullptr)
                    continue;
                // A panel of the main window shown as a tool window (the focus
                // panel) is the next screen, not a dialog to close.
                if (qobject_cast<QDialog *>(w) == nullptr && !w->isModal())
                    continue;
                // "Loading workspace": the app's own, it goes by itself.
                if (qobject_cast<QProgressDialog *>(w) != nullptr)
                    continue;
                handled.insert(w);
                busy = true;
                pump(150);
                m_seen << w->objectName();
                const bool first = handled.size() == 1;
                if (first)
                    for (const QJsonValue &v : sub)
                    {
                        const QJsonObject ss = v.toObject();
                        StepResult r = act(ss.value(QStringLiteral("do")).toObject(), ss, w);
                        if (r.status == QLatin1String("pass"))
                            r = expectIn(ss, r, w);
                        if (r.status != QLatin1String("pass") && m_dialogFail.status == QLatin1String("pass"))
                        {
                            m_dialogFail = r;
                            m_dialogFail.detail = QStringLiteral("in \"%1\": %2").arg(w->windowTitle(), r.detail);
                        }
                    }
                m_walk.record(w, QStringLiteral("flows"),
                              QStringLiteral("%1 step %2: %3").arg(m_id).arg(m_step).arg(w->windowTitle()),
                              QJsonObject{ { QStringLiteral("flow"), m_id }, { QStringLiteral("step"), m_step } });
                if (!first || close == QLatin1String("reject"))
                    dismiss(w);
                else if (close == QLatin1String("accept"))
                {
                    if (QDialog *dl = qobject_cast<QDialog *>(w))
                        dl->accept();
                    else
                        w->close();
                }
                else if (QAbstractButton *b = w->findChild<QAbstractButton *>(close))
                    b->click();
                else
                {
                    if (m_dialogFail.status == QLatin1String("pass"))
                        m_dialogFail = StepResult{ QStringLiteral("missing"),
                                                   QStringLiteral("no %1 button in \"%2\"").arg(close, w->windowTitle()) };
                    dismiss(w);
                }
                // A dialog that won't close the way the step says (OK greyed
                // until it has heard something): the step fails, Cancel.
                pump(100);
                if (w->isVisible())
                {
                    if (m_dialogFail.status == QLatin1String("pass"))
                        m_dialogFail = StepResult{ QStringLiteral("expect"),
                                                   QStringLiteral("\"%1\" would not close with %2").arg(w->windowTitle(), close) };
                    dismiss(w);
                }
                busy = false;
                return;
            }
            // Never wait for a human: whatever is still up after 15 s goes.
            if (age.elapsed() > 15000)
                for (QWidget *w : windowsNow())
                    if (!before.contains(w) && qobject_cast<QMenu *>(w) == nullptr)
                        dismiss(w);
        });
        m_dialogFail = StepResult();
        age.start();
        poll.start();
        body();
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < (sub.isEmpty() ? 300 : 600))
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
        poll.stop();
        pump();
    }

    /************************************************************************
     * Steps in code (what a JSON step can't say)
     ************************************************************************/

    StepResult code(const QString &what, const QJsonObject &s)
    {
        if (what == QLatin1String("workbenchLoadScene"))
        {
            ProgrammingManager *pm = ProgrammingManager::instance();
            if (pm == nullptr || pm->workbench() == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no Workbench") };
            if (!pm->workbench()->isVisible())
                return notHere(QStringLiteral("the Workbench"), QString(), s);
            // As a click on a scene in Build ▸ Scenes: one with lights in it
            // (values or looks), else the first.
            Scene *pick = nullptr;
            for (Function *f : m_app->doc()->functionsByType(Function::SceneType))
                if (Scene *sc = qobject_cast<Scene *>(f))
                {
                    if (pick == nullptr || (!sc->values().isEmpty() || !sc->palettes().isEmpty()))
                        pick = sc;
                    if (!sc->values().isEmpty() || !sc->palettes().isEmpty())
                        break;
                }
            if (pick == nullptr)
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no scene in this show") };
            pm->workbench()->load(pick->id());
            pump(300);
            return StepResult{ QStringLiteral("pass"), pick->name() };
        }
        if (what == QLatin1String("openWithNewerAutosave"))
        {
            // A saved show with a newer, different autosave beside it (as a
            // crash leaves it), opened from Open Recent.
            m_tmp.reset(new QTemporaryDir());
            const QString show = QDir(m_tmp->path()).filePath(QStringLiteral("Show.qlcc"));
            QFile::copy(g_showPath, show);
            QFile src(show);
            QByteArray xml;
            if (src.open(QIODevice::ReadOnly))
                xml = src.readAll();
            src.close();
            QFile as(show + QStringLiteral(".autosave"));
            if (as.open(QIODevice::WriteOnly))
                as.write(xml + "\n<!-- uxwalk: work since the last save -->\n");
            as.close();
            const QDateTime now = QDateTime::currentDateTime();
            QFile f1(show), f2(show + QStringLiteral(".autosave"));
            if (f1.open(QIODevice::ReadWrite))
                f1.setFileTime(now.addSecs(-120), QFileDevice::FileModificationTime);
            if (f2.open(QIODevice::ReadWrite))
                f2.setFileTime(now.addSecs(-60), QFileDevice::FileModificationTime);
            f1.close();
            f2.close();
            const bool canAsk = allowRecoveryAsk<App>(0);
            qunsetenv("QLC_NO_RECOVERY_PROMPT");
            m_app->doc()->resetModified();
            QAction recent(show);
            m_app->slotRecentFileClicked(&recent);
            pump(300);
            qputenv("QLC_NO_RECOVERY_PROMPT", "1");
            return StepResult{ QStringLiteral("pass"),
                               canAsk ? QStringLiteral("this build can ask offscreen") : QStringLiteral("this build has no offscreen recovery question") };
        }
        return StepResult{ QStringLiteral("expect"), QStringLiteral("no code step %1").arg(what) };
    }

    /************************************************************************
     * Did it do what the step says?
     ************************************************************************/

    int cueCount() const
    {
        int n = 0;
        for (const ShowTree::Production &p : m_app->doc()->showTree()->productions())
            for (const ShowTree::CueList &l : p.lists)
                n += l.cues.size();
        return n;
    }

    StepResult expect(const QJsonObject &s, StepResult r)
    {
        if (m_dialogFail.status != QLatin1String("pass"))
            return m_dialogFail;
        return expectIn(s, r, screenTop(m_app));
    }

    StepResult expectIn(const QJsonObject &s, StepResult r, QWidget *top)
    {
        const QJsonObject c = s.value(QStringLiteral("check")).toObject();
        auto fail = [&](const QString &why) {
            return StepResult{ QStringLiteral("expect"), why };
        };
        if (c.contains(QStringLiteral("visible")))
            for (const QJsonValue &v : c.value(QStringLiteral("visible")).toArray())
                if (findVisible(top, v.toString()) == nullptr && findVisible(m_app, v.toString()) == nullptr
                    && !anyVisible(v.toString()))
                    return existsAnywhere(v.toString()) ? fail(QStringLiteral("%1 is not shown").arg(v.toString()))
                                                        : StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 in this build").arg(v.toString()) };
        if (c.contains(QStringLiteral("hidden")))
            if (anyVisible(c.value(QStringLiteral("hidden")).toString()))
                return fail(QStringLiteral("%1 is still shown").arg(c.value(QStringLiteral("hidden")).toString()));
        if (c.contains(QStringLiteral("widgetText")))
        {
            const QJsonArray a = c.value(QStringLiteral("widgetText")).toArray();
            QWidget *w = findVisible(top, a.at(0).toString());
            if (w == nullptr)
                w = findVisible(m_app, a.at(0).toString());
            if (w == nullptr)
                return existsAnywhere(a.at(0).toString()) ? fail(QStringLiteral("%1 is not shown").arg(a.at(0).toString()))
                                                          : StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 in this build").arg(a.at(0).toString()) };
            const QString t = widgetText(w);
            if (!rx(a.at(1).toString()).match(t).hasMatch())
                return fail(QStringLiteral("%1 reads \"%2\"").arg(a.at(0).toString(), t.left(160)));
        }
        if (c.contains(QStringLiteral("comboHas")))
        {
            const QJsonArray a = c.value(QStringLiteral("comboHas")).toArray();
            QComboBox *cb = qobject_cast<QComboBox *>(findVisible(top, a.at(0).toString()));
            if (cb == nullptr)
                return fail(QStringLiteral("%1 is not shown").arg(a.at(0).toString()));
            if (comboIndex(cb, a.at(1).toString()) < 0)
            {
                QStringList items;
                for (int k = 0; k < cb->count(); ++k)
                    items << cb->itemText(k);
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no entry \"%1\" (it has: %2)")
                                       .arg(a.at(1).toString(), items.join(QStringLiteral(" | ")).left(300)) };
            }
        }
        if (c.contains(QStringLiteral("text")))
        {
            const QRegularExpression r2 = rx(c.value(QStringLiteral("text")).toString());
            bool found = false;
            for (const QString &t : visibleTexts(top))
                found = found || r2.match(t).hasMatch();
            if (!found)
                return fail(QStringLiteral("nothing on the screen reads /%1/").arg(c.value(QStringLiteral("text")).toString()));
        }
        if (c.contains(QStringLiteral("rows")))
            if (node(c.value(QStringLiteral("rows")).toString()) == nullptr)
                return fail(QStringLiteral("no %1 rows in the tree").arg(c.value(QStringLiteral("rows")).toString()));
        if (c.contains(QStringLiteral("folded")))
        {
            QTreeWidgetItem *it = node(c.value(QStringLiteral("folded")).toString());
            if (it == nullptr || it->isExpanded())
                return fail(QStringLiteral("the %1 is not folded").arg(c.value(QStringLiteral("folded")).toString()));
        }
        if (c.value(QStringLiteral("running")).toBool() && m_app->doc()->masterTimer()->runningFunctions() == 0)
            return fail(QStringLiteral("nothing started on the stage"));
        // "*": any window; else the window's objectName.
        if (c.contains(QStringLiteral("window"))
            && (c.value(QStringLiteral("window")).toString() == QLatin1String("*") ? m_seen.isEmpty()
                : !m_seen.contains(c.value(QStringLiteral("window")).toString())))
            return fail(QStringLiteral("no %1 window opened").arg(c.value(QStringLiteral("window")).toString()));
        if (c.contains(QStringLiteral("cues")) && cueCount() != m_cuesBefore + c.value(QStringLiteral("cues")).toInt())
            return fail(QStringLiteral("%1 cues, %2 before").arg(cueCount()).arg(m_cuesBefore));
        if (c.contains(QStringLiteral("menuItem")))
        {
            QStringList items;
            QString why;
            if (!contextMenu(c.value(QStringLiteral("menuItem")).toArray().at(0).toString(), QString(), false, &items, &why))
                return fail(why);
            const QRegularExpression r3 = rx(c.value(QStringLiteral("menuItem")).toArray().at(1).toString());
            bool found = false;
            for (const QString &t : items)
                if (r3.match(t).hasMatch())
                {
                    found = true;
                    r.detail = t;
                }
            if (!found)
                return fail(QStringLiteral("no item reads /%1/ (it has: %2)")
                                .arg(c.value(QStringLiteral("menuItem")).toArray().at(1).toString(),
                                     items.join(QStringLiteral(" | ")).left(300)));
        }
        return r;
    }

    static bool anyVisible(const QString &name)
    {
        for (QWidget *t : QApplication::topLevelWidgets())
            if (t->isVisible())
            {
                if (t->objectName() == name)
                    return true;
                for (QWidget *w : t->findChildren<QWidget *>(name))
                    if (w->isVisibleTo(t))
                        return true;
            }
        return false;
    }

    Walker &m_walk;
    App *m_app;
    QJsonObject m_flow;
    QString m_id;
    int m_step = 0;
    int m_cuesBefore = 0;
    QStringList m_seen;
    StepResult m_dialogFail;
    QScopedPointer<QTemporaryDir> m_tmp;
};

} // namespace flows

/** The flows group: every flow of ux_flows.json on its canned show, each
 *  from the show as saved (reloaded before each), in Design, Settings ▸
 *  Units back to the walk's. */
void walkFlows(Walker &walk, App *app, const Options &opt)
{
    QFile f(flows::g_flowsFile);
    if (!f.open(QIODevice::ReadOnly))
    {
        walk.note(QStringLiteral("flows"), QStringLiteral("ux_flows.json"), QStringLiteral("walker-missing"),
                  QStringLiteral("no flows file %1").arg(flows::g_flowsFile));
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonArray list = root.value(QStringLiteral("flows")).toArray();
    for (const QJsonValue &v : list)
    {
        const QJsonObject flow = v.toObject();
        const QString id = flow.value(QStringLiteral("id")).toString();
        if (!walk.mine(QStringLiteral("flows"), id))
            continue;
        // Each flow from the show as saved, in Design, the walk's units.
        app->doc()->resetModified();
        app->loadXML(opt.show);
        app->doc()->resetModified();
        app->slotModeDesign();
        MeasurementUnits::setPreferredLength(opt.units == QLatin1String("mm") ? MeasurementUnits::Millimetres
                                                                               : MeasurementUnits::Feet);
        app->setWorkspaceView(App::EverythingView, false);
        app->resize(kLaptop);
        pump(300);
        flows::FlowRun run(walk, app, flow);
        QJsonObject res = run.run();
        res.insert(QStringLiteral("kind"), QStringLiteral("flow"));
        walk.writeRecord(res);
        dismissModals();
        app->doc()->masterTimer()->stopAllFunctions();
        pump(100);
    }
}
