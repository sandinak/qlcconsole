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

QRegularExpression rx(const QString &pattern);
QString widgetText(QWidget *w);

/** One plain selector against one widget: an objectName; "text:<regex>",
 *  a button (or label) reading so; "placeholder:<regex>", a line edit
 *  asking so; "class:<Class>", a widget of that class; "*input", anything a
 *  person types into or picks from (not a label). */
bool matchesSel(QWidget *w, const QString &sel)
{
    if (sel.startsWith(QLatin1String("text:")))
    {
        const QRegularExpression r = rx(sel.mid(5));
        if (QAbstractButton *b = qobject_cast<QAbstractButton *>(w))
            return r.match(b->text().remove(QLatin1Char('&'))).hasMatch();
        if (QLabel *l = qobject_cast<QLabel *>(w))
            return r.match(plain(l->text())).hasMatch();
        return false;
    }
    if (sel.startsWith(QLatin1String("placeholder:")))
    {
        QLineEdit *e = qobject_cast<QLineEdit *>(w);
        return e != nullptr && rx(sel.mid(12)).match(e->placeholderText()).hasMatch();
    }
    if (sel.startsWith(QLatin1String("class:")))
        return w->inherits(sel.mid(6).toLatin1().constData());
    if (sel == QLatin1String("*input"))
        return qobject_cast<QLabel *>(w) == nullptr
               && (qobject_cast<QLineEdit *>(w) != nullptr || qobject_cast<QComboBox *>(w) != nullptr
                   || qobject_cast<QAbstractSpinBox *>(w) != nullptr || qobject_cast<QAbstractButton *>(w) != nullptr);
    return w->objectName() == sel;
}

QWidget *firstShown(QWidget *top, const QString &sel)
{
    if (matchesSel(top, sel))
        return top;
    for (QWidget *w : top->findChildren<QWidget *>())
        if (shownIn(w, top) && matchesSel(w, sel))
            return w;
    return nullptr;
}

/** The widgets of a layout, nested ones included. */
void layoutWidgets(QLayout *l, QList<QWidget *> &out)
{
    for (int i = 0; l != nullptr && i < l->count(); ++i)
    {
        QLayoutItem *it = l->itemAt(i);
        if (it->widget() != nullptr)
            out << it->widget();
        else if (it->layout() != nullptr)
            layoutWidgets(it->layout(), out);
    }
}

/** What sits in the same grid row as @a a (a form's label and its field,
 *  a Show Tree row's value and its buttons), matching @a sel. */
QWidget *inRow(QWidget *a, const QString &sel, QWidget *top)
{
    for (QWidget *w = a; w != nullptr && w != top; w = w->parentWidget())
    {
        QWidget *p = w->parentWidget();
        QGridLayout *g = p != nullptr ? qobject_cast<QGridLayout *>(p->layout()) : nullptr;
        if (g == nullptr)
            continue;
        int row = -1;
        for (int i = 0; i < g->count() && row < 0; ++i)
        {
            QList<QWidget *> ws;
            QLayoutItem *it = g->itemAt(i);
            if (it->widget() != nullptr)
                ws << it->widget();
            else
                layoutWidgets(it->layout(), ws);
            for (QWidget *x : ws)
                if (x == w || x->isAncestorOf(a))
                {
                    int r, c, rs, cs;
                    g->getItemPosition(i, &r, &c, &rs, &cs);
                    row = r;
                }
        }
        if (row < 0)
            continue;
        for (int i = 0; i < g->count(); ++i)
        {
            int r, c, rs, cs;
            g->getItemPosition(i, &r, &c, &rs, &cs);
            if (r != row)
                continue;
            QList<QWidget *> ws;
            QLayoutItem *it = g->itemAt(i);
            if (it->widget() != nullptr)
                ws << it->widget() << it->widget()->findChildren<QWidget *>();
            else
                layoutWidgets(it->layout(), ws);
            for (QWidget *x : ws)
                if (x != a && !x->isAncestorOf(a) && shownIn(x, top) && matchesSel(x, sel))
                    return x;
        }
        return nullptr;
    }
    return nullptr;
}

/** The control a step names, on @a top: a plain selector (matchesSel), or
 *  "label:<text>" (the field beside the label reading so), "a > b" (b
 *  inside a), "a ~ b" (b in a's row). */
QWidget *findVisible(QWidget *top, const QString &sel)
{
    const int gt = sel.indexOf(QLatin1String(" > "));
    if (gt > 0)
    {
        QWidget *a = findVisible(top, sel.left(gt));
        return a == nullptr ? nullptr : findVisible(a, sel.mid(gt + 3));
    }
    const int near = sel.indexOf(QLatin1String(" ~ "));
    if (near > 0)
    {
        QWidget *a = findVisible(top, sel.left(near));
        return a == nullptr ? nullptr : inRow(a, sel.mid(near + 3), top);
    }
    if (sel.startsWith(QLatin1String("label:")))
    {
        const QString want = sel.mid(6);
        for (QLabel *l : top->findChildren<QLabel *>())
            if (shownIn(l, top) && plain(l->text()).trimmed().startsWith(want, Qt::CaseInsensitive))
            {
                if (l->buddy() != nullptr)
                    return l->buddy();
                if (QWidget *f = inRow(l, QStringLiteral("*input"), top))
                    return f;
            }
        return nullptr;
    }
    return firstShown(top, sel);
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

#include "uxwalkdevice.inc"

/** While a step that says "answer" acts: each message box that comes up is
 *  answered with the button whose text starts with one of the answers
 *  (Fix on Device's "Change these settings?" Yes); any other is cancelled.
 *  What each said is kept for the step's detail. */
int g_answering = 0;
class BoxAnswerer
{
public:
    explicit BoxAnswerer(const QStringList &accept) : m_accept(accept)
    {
        if (m_accept.isEmpty())
            return;
        ++g_answering;
        m_timer.setInterval(30);
        QObject::connect(&m_timer, &QTimer::timeout, [this]() {
            QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box == nullptr || m_done.contains(box))
                return;
            m_done.insert(box);
            said << plain(box->text()).left(200);
            for (QAbstractButton *b : box->buttons())
                for (const QString &a : std::as_const(m_accept))
                    if (b->text().remove(QLatin1Char('&')).startsWith(a, Qt::CaseInsensitive))
                    {
                        b->click();
                        return;
                    }
            unexpected << plain(box->text()).left(200);
            box->reject();
        });
        m_timer.start();
    }
    ~BoxAnswerer()
    {
        if (!m_accept.isEmpty())
            --g_answering;
    }
    QStringList said, unexpected;

private:
    QStringList m_accept;
    QTimer m_timer;
    QSet<QMessageBox *> m_done;
};

/****************************************************************************
 * One flow's run
 ****************************************************************************/

struct StepResult
{
    QString status = QStringLiteral("pass");
    QString detail;
    /** The rule a failed check names (a device check's), over the step's. */
    QString rule;
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
        m_steps = m_flow.value(QStringLiteral("steps")).toArray();
        m_out = QVector<QJsonObject>(m_steps.size());
        if (m_flow.contains(QStringLiteral("setup")))
        {
            // What the show needs before the flow starts, as a person sets
            // it up (actions, as a step's "do"), or a code step's name.
            const QJsonValue setup = m_flow.value(QStringLiteral("setup"));
            const StepResult r = setup.isString() ? code(setup.toString(), QJsonObject())
                                                  : actAny(setup, QJsonObject{ { QStringLiteral("control"), QStringLiteral("setup") } }, m_app);
            qInfo().noquote() << "uxwalk-flow:" << m_id << "setup" << r.status << r.detail;
            if (r.status != QLatin1String("pass"))
            {
                m_stopped = true;
                m_firstFail = QStringLiteral("setup: %1").arg(r.detail);
            }
        }
        while (m_next < m_steps.size())
            runStep(m_next++);
        QJsonArray results;
        for (const QJsonObject &o : std::as_const(m_out))
            results.append(o);
        QString status = !m_anyAuto ? QStringLiteral("manual")
                       : m_firstFail.isEmpty() ? QStringLiteral("pass") : QStringLiteral("fail");
        return QJsonObject{ { QStringLiteral("flow"), m_id },
                            { QStringLiteral("title"), m_flow.value(QStringLiteral("title")) },
                            { QStringLiteral("status"), status },
                            { QStringLiteral("firstFail"), m_firstFail },
                            { QStringLiteral("clicks"), m_clicks },
                            { QStringLiteral("budget"), m_flow.value(QStringLiteral("budget")) },
                            { QStringLiteral("manualWhy"), m_flow.value(QStringLiteral("manual")) },
                            { QStringLiteral("steps"), results } };
    }

private:
    QJsonObject step(int i) const { return m_steps.at(i).toObject(); }

    static bool good(const StepResult &r)
    {
        return r.status == QLatin1String("pass") || r.status == QLatin1String("pointed");
    }

    /** Before a step acts: when it began, what the device had. */
    void begin(int i)
    {
        m_step = i + 1;
        m_seen.clear();
        m_stepStartMs = QDateTime::currentMSecsSinceEpoch();
        m_stateBefore = g_device != nullptr ? g_device->state() : QJsonObject();
    }

    void runStep(int i)
    {
        const QJsonObject s = step(i);
        const bool find = s.contains(QStringLiteral("find"));
        if (s.contains(QStringLiteral("manual")))
        {
            finish(i, StepResult{ QStringLiteral("manual"), s.value(QStringLiteral("manual")).toString(), QString() });
            return;
        }
        if (m_stopped)
        {
            finish(i, StepResult{ QStringLiteral("notreached"), QString(), QString() });
            return;
        }
        m_anyAuto = true;
        begin(i);
        m_kept = false;
        m_deferred.clear();
        StepResult r = find ? doFind(s) : doStep(s);
        if (m_kept)
            r = m_keptResult;           // checked in its window, before it closed
        else if (good(r))
            r = expect(s, r);
        // "remember": the cue now selected, and what its looks are, for a
        // later step's "sameAs" (the original after a Duplicate).
        if (good(r) && s.value(QStringLiteral("remember")).toString() == QLatin1String("cue") && showTree() != nullptr)
        {
            m_remembered.insert(QStringLiteral("cue"), showTree()->currentCueId());
            m_rememberedLooks = cueLooks(showTree()->currentCueId());
            r.detail += QStringLiteral(" (its looks: %1)").arg(m_rememberedLooks.left(200));
        }
        finish(i, r);
        // A step done in the window this one opened that closed it: checked
        // now, on what closing it did (Add Fixture's OK: the fixture added).
        const QList<QPair<int, StepResult>> deferred = m_deferred;
        m_deferred.clear();
        for (const auto &d : deferred)
        {
            m_step = d.first + 1;
            StepResult dr = d.second;
            if (good(dr))
                dr = expect(step(d.first), dr);
            finish(d.first, dr);
        }
    }

    /** A step's result: recorded (its screen, unless it was not driven),
     *  counted, and whether the flow goes on. */
    void finish(int i, const StepResult &r, QWidget *where = nullptr)
    {
        const QJsonObject s = step(i);
        const bool find = s.contains(QStringLiteral("find"));
        if (!find)
            m_clicks += s.value(QStringLiteral("clicks")).toInt(1);
        StepResult res = r;
        const bool driven = res.status != QLatin1String("manual") && res.status != QLatin1String("notreached");
        const bool bad = driven && !good(res);
        if (bad && s.contains(QStringLiteral("recover")))
        {
            // What a person stuck here would do to go on (Patch… a WLED that
            // landed on the wrong universe), so the steps after still say
            // what they find. The step stays failed.
            const StepResult rec = code(s.value(QStringLiteral("recover")).toString(), s);
            res.detail += QStringLiteral(" [the walker then: %1]").arg(rec.detail);
        }
        if (driven)
            m_walk.record(where != nullptr ? where : screenTop(m_app), QStringLiteral("flows"),
                          QStringLiteral("%1 step %2: %3").arg(m_id).arg(i + 1)
                              .arg(s.value(QStringLiteral("control")).toString()),
                          QJsonObject{ { QStringLiteral("flow"), m_id }, { QStringLiteral("step"), i + 1 },
                                       { QStringLiteral("stepStatus"), res.status },
                                       { QStringLiteral("stepDetail"), res.detail } });
        if (bad && m_firstFail.isEmpty())
            m_firstFail = QStringLiteral("step %1: %2").arg(i + 1).arg(res.status);
        // A find step, and a step that says so (its failure leaves the next
        // steps able to run: a device check), don't stop the flow.
        if (bad && !find && s.value(QStringLiteral("onFail")).toString() != QLatin1String("continue")
            && !s.contains(QStringLiteral("recover")))
            m_stopped = true;
        qInfo().noquote() << "uxwalk-flow:" << m_id << "step" << (i + 1) << res.status << res.detail;
        QJsonObject o{ { QStringLiteral("n"), i + 1 },
                       { QStringLiteral("screen"), s.value(QStringLiteral("screen")) },
                       { QStringLiteral("control"), s.value(QStringLiteral("control")) },
                       { QStringLiteral("expect"), s.value(QStringLiteral("expect")) },
                       { QStringLiteral("find"), find },
                       { QStringLiteral("clicks"), find ? 0 : s.value(QStringLiteral("clicks")).toInt(1) },
                       { QStringLiteral("status"), res.status },
                       { QStringLiteral("detail"), res.detail } };
        if (!res.rule.isEmpty())
            o.insert(QStringLiteral("rule"), res.rule);
        m_out[i] = o;
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
        m_cuesBefore = cueCount();
        // "waitFor": a check that has to hold before the person can act (the
        // footer has the device's finding, a moment after the add).
        if (s.contains(QStringLiteral("waitFor")))
        {
            const QJsonObject w{ { QStringLiteral("check"), s.value(QStringLiteral("waitFor")) } };
            QElapsedTimer t;
            t.start();
            while (!good(expectIn(w, StepResult(), m_app)) && t.elapsed() < 12000)
                pump(150);
        }
        StepResult r;
        withWindows(s, [&]() { r = actAny(s.value(QStringLiteral("do")), s, m_app); });
        return r;
    }

    /** A step's "do": one action, or a list done in order (the first that
     *  fails stops it); with "answer", the message boxes it brings up are
     *  answered so. */
    StepResult actAny(const QJsonValue &d, const QJsonObject &s, QWidget *top)
    {
        QStringList answers;
        for (const QJsonValue &a : s.value(QStringLiteral("answer")).toArray())
            answers << a.toString();
        BoxAnswerer answerer(answers);
        StepResult r;
        if (d.isArray())
        {
            QStringList details;
            for (const QJsonValue &each : d.toArray())
            {
                r = act(each.toObject(), s, top);
                if (!r.detail.isEmpty())
                    details << r.detail;
                if (!good(r))
                    break;
            }
            if (good(r))
                r.detail = details.join(QStringLiteral("; "));
        }
        else
            r = act(d.toObject(), s, top);
        if (!answerer.said.isEmpty() && good(r))
            r.detail += (r.detail.isEmpty() ? QString() : QStringLiteral("; "))
                        + QStringLiteral("answered: %1").arg(answerer.said.join(QStringLiteral(" | ")).left(300));
        if (!answerer.unexpected.isEmpty() && good(r))
            r = StepResult{ QStringLiteral("expect"),
                            QStringLiteral("a message it didn't expect: %1").arg(answerer.unexpected.join(QStringLiteral(" | "))),
                            QString() };
        return r;
    }

    /** The tree item of @a tree on the path of regexes @a path (each a
     *  level: manufacturer, model), or one anywhere matching @a item. */
    static QTreeWidgetItem *treeItem(QTreeWidget *t, const QJsonArray &path, const QString &item)
    {
        if (!path.isEmpty())
        {
            QTreeWidgetItem *parent = t->invisibleRootItem();
            for (const QJsonValue &p : path)
            {
                QTreeWidgetItem *next = nullptr;
                for (int k = 0; k < parent->childCount() && next == nullptr; ++k)
                    if (!parent->child(k)->isHidden() && rx(p.toString()).match(parent->child(k)->text(0)).hasMatch())
                        next = parent->child(k);
                if (next == nullptr)
                    return nullptr;
                parent = next;
            }
            return parent;
        }
        for (QTreeWidgetItemIterator it(t); *it != nullptr; ++it)
            if (!(*it)->isHidden())
                for (int c = 0; c < t->columnCount(); ++c)
                    if (rx(item).match((*it)->text(c)).hasMatch())
                        return *it;
        return nullptr;
    }

    /** A popup menu that @a open brings up: its items read, @a pick (a
     *  regex, "A ▸ B" for a submenu's) triggered as Return would. */
    bool popupPick(const std::function<void()> &open, const QString &pick, QStringList *items, QString *why)
    {
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
            m_walk.recordMenu(m, QStringLiteral("flows"), QStringLiteral("%1 step %2: menu").arg(m_id).arg(m_step));
            QMenu *in = m;
            QAction *hit = nullptr;
            const QStringList parts = pick.split(QStringLiteral(" ▸ "));
            for (int p = 0; p < parts.size() && in != nullptr; ++p)
            {
                QAction *found = nullptr;
                emit in->aboutToShow();
                for (QAction *a : in->actions())
                {
                    const QString t = a->text().remove(QRegularExpression(QStringLiteral("&(?!&)")));
                    if (!a->isSeparator() && a->isVisible() && a->isEnabled() && rx(parts.at(p)).match(t).hasMatch())
                    {
                        found = a;
                        pickedText = t;
                        break;
                    }
                }
                if (found == nullptr)
                    break;
                if (p == parts.size() - 1)
                    hit = found;
                else
                    in = found->menu();
            }
            m->close();
            if (hit == nullptr)
                return;
            picked = true;
            // The popup closed, its action run while the menu (a local one,
            // gone when its exec() returns) still holds it.
            hit->trigger();
        });
        poll.start();
        open();
        QElapsedTimer t;
        t.start();
        while (!seen && t.elapsed() < 800)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(5);
        }
        poll.stop();
        if (!seen)
        {
            *why = QStringLiteral("no menu opened");
            return false;
        }
        if (!picked)
        {
            *why = QStringLiteral("no \"%1\" in its menu (it has: %2)").arg(pick, items->join(QStringLiteral(" | ")).left(300));
            return false;
        }
        *why = pickedText;
        return true;
    }

    /** One action, in @a top (the app, or a dialog the step opened). */
    StepResult act(const QJsonObject &d, const QJsonObject &s, QWidget *top)
    {
        const QString control = s.value(QStringLiteral("control")).toString();
        if (d.contains(QStringLiteral("type")))
        {
            const QString name = d.value(QStringLiteral("type")).toString();
            QLineEdit *e = qobject_cast<QLineEdit *>(findVisible(top, name));
            if (e == nullptr)
                return notHere(control, name, s);
            if (!e->isEnabled())
                return StepResult{ QStringLiteral("deadend"), QStringLiteral("%1 is greyed").arg(name), QString() };
            const QString text = expand(d.value(QStringLiteral("text")).toString());
            e->setFocus();
            e->selectAll();
            QTest::keyClick(e, Qt::Key_Backspace);
            QTest::keyClicks(e, text);
            pump(200);
            return StepResult{ QStringLiteral("pass"), QStringLiteral("typed %1").arg(text), QString() };
        }
        if (d.contains(QStringLiteral("pick")))
        {
            const QString name = d.value(QStringLiteral("pick")).toString();
            QWidget *w = findVisible(top, name);
            const QString item = d.value(QStringLiteral("item")).toString();
            if (QTreeWidget *t = qobject_cast<QTreeWidget *>(w))
            {
                QTreeWidgetItem *it = treeItem(t, d.value(QStringLiteral("path")).toArray(), item);
                if (it == nullptr)
                    return StepResult{ QStringLiteral("missing"), QStringLiteral("no row %1 in %2")
                        .arg(d.contains(QStringLiteral("path")) ? jsonText(d.value(QStringLiteral("path"))) : item, name), QString() };
                for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
                    p->setExpanded(true);
                t->scrollToItem(it);
                t->clearSelection();
                t->setCurrentItem(it);
                it->setSelected(true);
                emit t->itemClicked(it, 0);
                pump(250);
                return StepResult{ QStringLiteral("pass"), it->text(0), QString() };
            }
            if (QListWidget *l = qobject_cast<QListWidget *>(w))
            {
                for (int k = 0; k < l->count(); ++k)
                    if (!l->item(k)->isHidden() && rx(item).match(l->item(k)->text()).hasMatch())
                    {
                        QListWidgetItem *it = l->item(k);
                        l->scrollToItem(it);
                        l->setCurrentItem(it);
                        emit l->itemClicked(it);
                        pump(300);
                        return StepResult{ QStringLiteral("pass"), it->text(), QString() };
                    }
                QStringList have;
                for (int k = 0; k < l->count() && k < 20; ++k)
                    have << l->item(k)->text();
                return StepResult{ QStringLiteral("missing"), QStringLiteral("no \"%1\" in the list (it has: %2)")
                                       .arg(item, have.join(QStringLiteral(" | ")).left(300)), QString() };
            }
            return notHere(control, name, s);
        }
        if (d.contains(QStringLiteral("popup")))
        {
            const QString name = d.value(QStringLiteral("popup")).toString();
            QAbstractButton *b = qobject_cast<QAbstractButton *>(findVisible(top, name));
            if (b == nullptr)
                return notHere(control, name, s);
            if (!b->isEnabled())
                return StepResult{ QStringLiteral("deadend"), QStringLiteral("%1 is greyed").arg(control), QString() };
            QStringList items;
            QString why;
            if (!popupPick([b]() { b->click(); }, d.value(QStringLiteral("item")).toString(), &items, &why))
                return StepResult{ QStringLiteral("missing"), why, QString() };
            pump(300);
            return StepResult{ QStringLiteral("pass"), why, QString() };
        }
        if (d.contains(QStringLiteral("colour")))
        {
            const QString name = d.value(QStringLiteral("colour")).toString();
            QColorDialog *cd = qobject_cast<QColorDialog *>(findVisible(top, name));
            if (cd == nullptr)
                return notHere(control, name, s);
            cd->setCurrentColor(QColor(d.value(QStringLiteral("value")).toString()));
            pump(300);
            return StepResult{ QStringLiteral("pass"), d.value(QStringLiteral("value")).toString(), QString() };
        }
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
            const QString kind = d.value(QStringLiteral("node")).toString();
            if (stv != nullptr && (kind.startsWith(QLatin1String("library:")) || kind.startsWith(QLatin1String("layer:"))))
            {
                const bool lib = kind.startsWith(QLatin1String("library:"));
                QTreeWidget *tree = lib ? stv->m_library : stv->m_tree;
                const QRegularExpression r = rx(kind.mid(kind.indexOf(QLatin1Char(':')) + 1));
                auto wanted = [&](QTreeWidgetItem *x) {
                    const int k = x->data(0, ShowTreeView::KindRole).toInt();
                    if (lib ? k != ShowTreeView::LibFixture && k != ShowTreeView::LibGroup
                            : k != ShowTreeView::LayerNode && k != ShowTreeView::TrackedLayerNode)
                        return false;
                    return r.match(x->text(0)).hasMatch();
                };
                // A layer of the cue selected first (the copy just made).
                QTreeWidgetItem *row = nullptr;
                std::function<void(QTreeWidgetItem *)> under = [&](QTreeWidgetItem *p) {
                    for (int k = 0; p != nullptr && k < p->childCount() && row == nullptr; ++k)
                    {
                        if (wanted(p->child(k)))
                            row = p->child(k);
                        else
                            under(p->child(k));
                    }
                };
                if (!lib)
                    under(tree->currentItem());
                if (row == nullptr)
                    row = findItem(tree, wanted);
                if (row == nullptr && lib)
                {
                    // Not listed: what the Library does list, top down.
                    QStringList top;
                    for (QTreeWidgetItemIterator x(tree); *x != nullptr && top.size() < 14; ++x)
                        if ((*x)->parent() == nullptr || (*x)->parent()->isExpanded())
                            top << (*x)->text(0);
                    return StepResult{ QStringLiteral("deadend"),
                                       QStringLiteral("no row of the Library reads /%1/ (it lists: %2)")
                                           .arg(kind.mid(8), top.join(QStringLiteral(" | ")).left(300)), QString() };
                }
                if (row == nullptr)
                    return StepResult{ QStringLiteral("missing"), QStringLiteral("no %1 row").arg(kind), QString() };
                if (!tree->isVisible())
                    return notHere(lib ? QStringLiteral("the Library") : QStringLiteral("the Show Tree"), QString(), s);
                for (QTreeWidgetItem *p = row->parent(); p != nullptr; p = p->parent())
                    p->setExpanded(true);
                tree->clearSelection();
                tree->setCurrentItem(row);
                row->setSelected(true);
                tree->scrollToItem(row);
                pump(250);
                return StepResult{ QStringLiteral("pass"), row->text(0), QString() };
            }
            QTreeWidgetItem *it = kind == QLatin1String("remembered")
                ? (stv == nullptr ? nullptr : findItem(stv->m_tree, [&](QTreeWidgetItem *x) {
                      return x->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                             && x->data(0, ShowTreeView::IdRole).toUInt() == m_remembered.value(QStringLiteral("cue"));
                  }))
                : node(kind);
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
            else if (QLabel *l = qobject_cast<QLabel *>(w))
                QTest::mouseClick(l, Qt::LeftButton);       // the footer's chip
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
            const QString itemName = d.value(QStringLiteral("item")).toString();
            QAction *a = b->menu()->findChild<QAction *>(itemName);
            for (QAction *x : b->menu()->actions())
                if (a == nullptr && (x->objectName() == itemName
                                     || (itemName.startsWith(QLatin1String("text:"))
                                         && rx(itemName.mid(5)).match(x->text().remove(QLatin1Char('&'))).hasMatch())))
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
        else if (kind.startsWith(QLatin1String("library:")) || kind.startsWith(QLatin1String("layer:")))
        {
            // A Library row (a fixture, a group) or a cue's layer row, by its text.
            ShowTreeView *stv = showTree();
            const bool lib = kind.startsWith(QLatin1String("library:"));
            QTreeWidget *tree = stv == nullptr ? nullptr : lib ? stv->m_library : stv->m_tree;
            const QRegularExpression r = rx(kind.mid(kind.indexOf(QLatin1Char(':')) + 1));
            QTreeWidgetItem *it = tree == nullptr ? nullptr : findItem(tree, [&](QTreeWidgetItem *x) {
                const int k = x->data(0, ShowTreeView::KindRole).toInt();
                if (!lib && k != ShowTreeView::LayerNode && k != ShowTreeView::TrackedLayerNode)
                    return false;
                return r.match(x->text(0)).hasMatch();
            });
            if (it == nullptr)
            {
                *why = QStringLiteral("no %1 row").arg(kind);
                return false;
            }
            if (!tree->isVisible())
            {
                *why = QStringLiteral("the Show Tree is not on this screen");
                return false;
            }
            for (QTreeWidgetItem *p = it->parent(); p != nullptr; p = p->parent())
                p->setExpanded(true);
            tree->clearSelection();
            tree->setCurrentItem(it);
            it->setSelected(true);
            tree->scrollToItem(it);
            pump(100);
            target = tree->viewport();
            pos = tree->visualItemRect(it).center();
        }
        else
        {
            ShowTreeView *stv = showTree();
            QTreeWidgetItem *it = kind == QLatin1String("remembered")
                ? (stv == nullptr ? nullptr : findItem(stv->m_tree, [&](QTreeWidgetItem *x) {
                      return x->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
                             && x->data(0, ShowTreeView::IdRole).toUInt() == m_remembered.value(QStringLiteral("cue"));
                  }))
                : node(kind);
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
        QString close = s.value(QStringLiteral("close")).toString(QStringLiteral("reject"));
        bool busy = false;
        QElapsedTimer age;
        QTimer poll;
        poll.setInterval(40);
        QObject::connect(&poll, &QTimer::timeout, [&]() {
            if (busy)
                return;
            // The window in front first (a modal one), then the others: a
            // window the step opened can bring a tool window of its own.
            QList<QWidget *> now = windowsNow().values();
            if (QWidget *front = QApplication::activeModalWidget())
                if (now.removeAll(front) > 0)
                    now.prepend(front);
            for (QWidget *w : now)
            {
                if (before.contains(w) || handled.contains(w) || qobject_cast<QMenu *>(w) != nullptr)
                    continue;
                // A panel of the main window shown as a tool window (the focus
                // panel) is the next screen, not a dialog to close.
                if (qobject_cast<QDialog *>(w) == nullptr && !w->isModal())
                    continue;
                // A message box the step answers itself ("answer").
                if (g_answering > 0 && qobject_cast<QMessageBox *>(w) != nullptr)
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
                        StepResult r = actAny(ss.value(QStringLiteral("do")), ss, w);
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
                // "keep": the window stays for the next steps ("in": "dialog"),
                // each a step of its own, done in it; the one that says
                // "close" closes it, and is checked after (runStep).
                if (first && s.value(QStringLiteral("keep")).toBool())
                {
                    m_kept = true;
                    m_keptResult = m_dialogFail.status != QLatin1String("pass") ? m_dialogFail
                                                                                : expectIn(s, StepResult(), w);
                    m_dialogFail = StepResult();
                    close = QStringLiteral("reject");
                    if (good(m_keptResult))
                        while (m_next < m_steps.size()
                               && step(m_next).value(QStringLiteral("in")).toString() == QLatin1String("dialog"))
                        {
                            const int j = m_next++;
                            const QJsonObject sj = step(j);
                            const int outer = m_step;
                            begin(j);
                            StepResult ar = actAny(sj.value(QStringLiteral("do")), sj, w);
                            if (good(ar) && sj.contains(QStringLiteral("close")))
                            {
                                close = sj.value(QStringLiteral("close")).toString();
                                m_deferred << qMakePair(j, ar);
                                m_step = outer;
                                break;
                            }
                            if (good(ar))
                                ar = expectIn(sj, ar, w);
                            finish(j, ar, w);
                            m_step = outer;
                            if (m_stopped)
                                break;
                        }
                }
                if (!first || close == QLatin1String("reject"))
                    dismiss(w);
                else if (close == QLatin1String("accept"))
                {
                    if (QDialog *dl = qobject_cast<QDialog *>(w))
                        dl->accept();
                    else
                        w->close();
                }
                else if (QAbstractButton *b = qobject_cast<QAbstractButton *>(findVisible(w, close)))
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
        if (what == QLatin1String("patchWled"))
        {
            // As Patch… on the fixture does: PatchPlanner, which puts a WLED
            // on a universe of its own with the WLED output at its IP.
            Fixture *fx = addedWled();
            if (fx == nullptr)
                return StepResult{ QStringLiteral("expect"), QStringLiteral("no WLED to patch"), QString() };
            PatchPlanner planner(m_app->doc());
            QStringList problems;
            int n = planner.apply(planner.plan({ fx->id() }), &problems);
            if (!wledPatchProblem().isEmpty())
            {
                fx->setPatched(false);
                n = planner.apply(planner.plan({ fx->id() }), &problems);
            }
            pump(300);
            const QString left = wledPatchProblem();
            return StepResult{ left.isEmpty() ? QStringLiteral("pass") : QStringLiteral("expect"),
                               left.isEmpty() ? QStringLiteral("patched it as Patch… does (universe %1, the WLED output)")
                                                    .arg(fx->universe() + 1)
                                              : QStringLiteral("Patch… could not put it right either: %1 %2")
                                                    .arg(left, problems.join(QStringLiteral("; "))), QString() };
        }
        if (what == QLatin1String("wledLayer"))
        {
            // As dragging the WLED from the plot onto the cue does: a layer
            // on it, selected.
            Fixture *fx = addedWled();
            ShowTreeView *stv = showTree();
            const quint32 cue = stv == nullptr ? ShowTree::invalidId() : stv->currentCueId();
            if (fx == nullptr || cue == ShowTree::invalidId())
                return StepResult{ QStringLiteral("expect"), QStringLiteral("no WLED or no cue selected"), QString() };
            if (!stv->dropTargets(cue, {}, { fx->id() }, false))
                return StepResult{ QStringLiteral("expect"), QStringLiteral("the drop made no layer"), QString() };
            pump(300);
            const StepResult sel = act(QJsonObject{ { QStringLiteral("node"), QStringLiteral("layer:") + QRegularExpression::escape(fx->name()) } },
                                       s, m_app);
            return StepResult{ sel.status, QStringLiteral("dropped the WLED on the cue, as from the plot (%1)").arg(sel.detail), QString() };
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

    /** The step's check on the screen it left; "within": ms, asked again
     *  until it holds or that time is up (a device answers when it does). */
    StepResult expect(const QJsonObject &s, StepResult r)
    {
        if (m_dialogFail.status != QLatin1String("pass"))
            return m_dialogFail;
        const int within = s.value(QStringLiteral("within")).toInt(0);
        QElapsedTimer t;
        t.start();
        StepResult out = expectIn(s, r, screenTop(m_app));
        while (!good(out) && out.status == QLatin1String("expect") && t.elapsed() < within)
        {
            pump(150);
            out = expectIn(s, r, screenTop(m_app));
        }
        return out;
    }

    /** The WLED the flow added: the last fixture with WLED's own transport. */
    Fixture *addedWled() const
    {
        Fixture *out = nullptr;
        for (Fixture *f : m_app->doc()->fixtures())
            if (f->transport() == Fixture::WledTransport && f->connection() == Fixture::Network
                && (out == nullptr || f->id() > out->id()))
                out = f;
        return out;
    }

    /** UX-67: the WLED added is on a universe of its own whose output is
     *  the WLED one, sent to its IP. Empty when so. */
    QString wledPatchProblem() const
    {
        Fixture *fx = addedWled();
        if (fx == nullptr)
            return QStringLiteral("no WLED fixture was added");
        InputOutputMap *io = m_app->doc()->inputOutputMap();
        const quint32 u = fx->universe();
        const QString uname = io->universeNames().value(int(u), QString::number(u + 1));
        if (!fx->isPatched())
            return QStringLiteral("the WLED was added unpatched (staged), on no output");
        OutputPatch *op = io->outputPatch(u);
        const QString out = op == nullptr ? QStringLiteral("no output")
                          : QStringLiteral("%1 %2").arg(op->pluginName(), op->outputName());
        QStringList others;
        for (Fixture *f : m_app->doc()->fixtures())
            if (f != fx && f->isPatched() && f->universe() == u)
                others << f->name();
        if (op == nullptr || op->pluginName() != QLatin1String("WLED"))
            return QStringLiteral("the WLED landed on universe %1 (%2) (its output: %3)%4, so nothing reaches %5")
                .arg(u + 1).arg(uname, out,
                     others.isEmpty() ? QString() : QStringLiteral(", with %1 other fixtures").arg(others.size()),
                     fx->ipAddress());
        const QString ip = op->getPluginParameters().value(QStringLiteral("outputIP")).toString();
        if (ip != fx->ipAddress())
            return QStringLiteral("universe %1's WLED output sends to %2, not %3").arg(u + 1).arg(ip, fx->ipAddress());
        if (!others.isEmpty())
            return QStringLiteral("the WLED shares universe %1 with %2").arg(u + 1).arg(others.join(QStringLiteral(", ")).left(200));
        return QString();
    }

    /** What a cue's own layers show: each look's key and its palettes'
     *  values, so a change to a shared look is seen. */
    QString cueLooks(quint32 cueId) const
    {
        ShowTreeController *st = m_app->doc()->showTree();
        const ShowTree::Cue *c = st->cue(cueId);
        if (c == nullptr)
            return QString();
        QStringList out;
        for (const ShowTree::Layer &l : c->layers)
            for (const ShowTree::Look &lk : l.looks)
            {
                QStringList vals;
                for (quint32 pid : lk.palettes)
                    if (const QLCPalette *p = m_app->doc()->palette(pid))
                    {
                        QStringList vs;
                        for (const QVariant &v : p->values())
                            vs << v.toString();
                        vals << QStringLiteral("%1[%2]").arg(p->name(), vs.join(QLatin1Char(',')));
                    }
                out << QStringLiteral("%1 %2=%3").arg(st->targetLabel(l.target), lk.key, vals.join(QLatin1Char('+')));
            }
        return out.join(QStringLiteral("; "));
    }

    StepResult expectIn(const QJsonObject &s, StepResult r, QWidget *top)
    {
        const QJsonObject c = s.value(QStringLiteral("check")).toObject();
        // A check that names its rule ("rule") says it in its failure.
        const QString checkRule = c.value(QStringLiteral("rule")).toString();
        auto fail = [&](const QString &why) {
            return StepResult{ QStringLiteral("expect"),
                               checkRule.isEmpty() ? why : QStringLiteral("%1: %2").arg(checkRule, why), checkRule };
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
        if (c.contains(QStringLiteral("enabled")))
            for (const QJsonValue &v : c.value(QStringLiteral("enabled")).toArray())
            {
                QWidget *w = findVisible(top, v.toString());
                if (w == nullptr)
                    return fail(QStringLiteral("%1 is not shown").arg(v.toString()));
                if (!w->isEnabled())
                    return fail(QStringLiteral("%1 is greyed").arg(v.toString()));
            }
        auto ruled = [&](const QString &rule, const QString &why) {
            return StepResult{ QStringLiteral("expect"), rule.isEmpty() ? why : QStringLiteral("%1: %2").arg(rule, why), rule };
        };
        if (c.contains(QStringLiteral("status")))
        {
            bool has = false;
            for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
                has = has || e.key == c.value(QStringLiteral("status")).toString();
            if (!has)
                return fail(QStringLiteral("the footer lists no %1").arg(c.value(QStringLiteral("status")).toString()));
        }
        if (c.contains(QStringLiteral("statusGone")))
            for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
                if (e.key == c.value(QStringLiteral("statusGone")).toString())
                    return fail(QStringLiteral("the footer still lists \"%1\" (%2)").arg(e.summary, e.items.join(QStringLiteral(", ")).left(160)));
        if (c.value(QStringLiteral("wledPatch")).toBool())
        {
            const QString why = wledPatchProblem();
            if (!why.isEmpty())
                return ruled(c.value(QStringLiteral("rule")).toString(), why);
        }
        if (c.contains(QStringLiteral("device")))
        {
            const QJsonArray all = c.value(QStringLiteral("device")).isArray() ? c.value(QStringLiteral("device")).toArray()
                                                                               : QJsonArray{ c.value(QStringLiteral("device")) };
            for (const QJsonValue &v : all)
            {
                const QString why = deviceProblem(v.toObject(), m_stepStartMs, m_stateBefore);
                if (!why.isEmpty())
                    return ruled(v.toObject().value(QStringLiteral("rule")).toString(), why);
            }
            if (g_device != nullptr)
                r.detail += (r.detail.isEmpty() ? QString() : QStringLiteral("; "))
                            + QStringLiteral("the WLED has: %1")
                                  .arg(segText(g_device->state().value(QStringLiteral("seg")).toArray().at(0).toObject()));
        }
        if (c.value(QStringLiteral("copyOwn")).toBool())
        {
            // The copy (the cue selected) has looks of its own, or the Look
            // Editor says which are still shared with the original.
            ShowTreeController *st = m_app->doc()->showTree();
            const ShowTree::Cue *orig = st->cue(m_remembered.value(QStringLiteral("cue")));
            const ShowTree::Cue *copy = showTree() != nullptr ? st->cue(showTree()->currentCueId()) : nullptr;
            if (copy == nullptr)
            {
                // The layer row is selected: its cue.
                if (QTreeWidgetItem *it = showTree() != nullptr ? showTree()->m_tree->currentItem() : nullptr)
                    for (QTreeWidgetItem *p = it; p != nullptr && copy == nullptr; p = p->parent())
                        if (p->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode)
                            copy = st->cue(p->data(0, ShowTreeView::IdRole).toUInt());
            }
            QSet<quint32> mine;
            if (orig != nullptr)
                for (const ShowTree::Layer &l : orig->layers)
                    for (const ShowTree::Look &lk : l.looks)
                        for (quint32 pid : lk.palettes)
                            if (st->isInlinePalette(pid))
                                mine.insert(pid);
            QStringList shared;
            if (copy != nullptr && copy != orig)
                for (const ShowTree::Layer &l : copy->layers)
                    for (const ShowTree::Look &lk : l.looks)
                        for (quint32 pid : lk.palettes)
                            if (mine.contains(pid))
                                if (const QLCPalette *p = m_app->doc()->palette(pid))
                                    shared << QStringLiteral("%1 \"%2\"").arg(lk.key, p->name());
            if (!shared.isEmpty())
            {
                QWidget *title = findVisible(m_app, QStringLiteral("lookTitle"));
                QWidget *used = findVisible(m_app, QStringLiteral("lookEditorUsedBy"));
                const QString says = QStringLiteral("%1 %2").arg(title != nullptr ? widgetText(title) : QString(),
                                                                used != nullptr ? widgetText(used) : QString()).trimmed();
                if (!rx(QStringLiteral("used by ([2-9]|\\d\\d)|shared|also in|Own Copy")).match(says).hasMatch())
                    return ruled(c.value(QStringLiteral("rule")).toString(),
                                 QStringLiteral("the copy's %1 is the original's own look, and the Look Editor says only \"%2\"")
                                     .arg(shared.join(QStringLiteral(", ")), says.left(160)));
            }
        }
        if (c.contains(QStringLiteral("sameAs")))
        {
            const QString now = cueLooks(m_remembered.value(QStringLiteral("cue")));
            if (now != m_rememberedLooks)
                return ruled(c.value(QStringLiteral("rule")).toString(),
                             QStringLiteral("the original cue changed with the copy: it had %1, it has %2")
                                 .arg(m_rememberedLooks.left(240), now.left(240)));
        }
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
    QJsonArray m_steps;
    QVector<QJsonObject> m_out;
    int m_next = 0;
    bool m_stopped = false;
    bool m_anyAuto = false;
    int m_clicks = 0;
    QString m_firstFail;
    bool m_kept = false;
    StepResult m_keptResult;
    QList<QPair<int, StepResult>> m_deferred;
    qint64 m_stepStartMs = 0;
    QJsonObject m_stateBefore;
    QHash<QString, quint32> m_remembered;
    QString m_rememberedLooks;
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
    // The WLED output, as the app has it (F-11 adds a WLED through it), and
    // the flows' WLED: his office one on this computer, or QLC_WALK_WLED's.
    if (app->doc()->ioPluginCache()->plugin(QStringLiteral("WLED")) == nullptr)
        app->doc()->ioPluginCache()->load(QDir(QStringLiteral(UXWALK_PLUGINS_DIR), QStringLiteral("*wled*")));
    if (app->doc()->ioPluginCache()->plugin(QStringLiteral("WLED")) == nullptr)
        walk.note(QStringLiteral("flows"), QStringLiteral("WLED"), QStringLiteral("walker-missing"),
                  QStringLiteral("the WLED output plugin did not load from %1").arg(QStringLiteral(UXWALK_PLUGINS_DIR)));
    flows::WalkDevice device;
    flows::g_device = &device;
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
        // A real WLED asked for by name: the person's own say-so to send to
        // it, past a Teaching show's hold on real output.
        if (device.isReal())
            app->doc()->inputOutputMap()->setTeachingHold(false);
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
    device.restore();
    flows::g_device = nullptr;
}
