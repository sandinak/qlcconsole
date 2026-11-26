/*
  Q Light Controller Plus - qlcconsole
  showlevel.cpp

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

#include <QAction>
#include <QDebug>
#include <QGlobalStatic>
#include <QHash>
#include <QEvent>
#include <QMenu>
#include <QShortcut>
#include <QWidget>

#include "levelregistry.h"
#include "focusreport.h"
#include "showtreecommand.h"
#include "showlevel.h"

Q_GLOBAL_STATIC(ShowLevel, s_level)

#define KPropLevelKey    "levelKey"
#define KPropFolded      "levelFolded"
#define KPropGreyed      "levelGreyed"
#define KPropToolTip     "levelToolTip"
#define KPropMoreLine    "levelMoreLine"

ShowLevel::ShowLevel(QObject *parent)
    : QObject(parent)
{
    /* The Show Tree's command line: a word the level hides answers with the
       level's words (WORKSPACE_LEVELS.md §4), not "don't know". */
    FocusReport::setLevelNameProvider([]() { return name(ShowLevel::instance()->effectiveLevel()); });
    ShowTreeCommand::setLevelGate([](const QString &word, QString *sentence) -> QString {
        static const QHash<QString, QString> words = {
            { QStringLiteral("wb"), QStringLiteral("showtree.workbench") },
            { QStringLiteral("workbench"), QStringLiteral("showtree.workbench") },
            { QStringLiteral("mib"), QStringLiteral("showtree.mib") },
            { QStringLiteral("nomib"), QStringLiteral("showtree.mib") },
            { QStringLiteral("allowmib"), QStringLiteral("showtree.mib") },
            { QStringLiteral("textcolour"), QStringLiteral("showtree.cue.textcolour") },
            { QStringLiteral("textcolor"), QStringLiteral("showtree.cue.textcolour") },
            { QStringLiteral("detach"), QStringLiteral("showtree.reuse") },
            { QStringLiteral("convert"), QStringLiteral("showtree.reuse") },
        };
        const QString key = words.value(word.toLower());
        if (key.isEmpty() || ShowLevel::shown(key))
            return QString();
        if (sentence != nullptr)
            *sentence = tr("%1 is a %2-level command.").arg(word, name(levelOf(key)));
        return key;
    });
}

ShowLevel *ShowLevel::instance()
{
    return s_level();
}

void ShowLevel::setLevel(Level level)
{
    if (level < Student || level > Advanced)
        return;
    // An unknown level read from a newer build is dropped by any pick.
    if (level == m_level && m_session < 0 && m_unknownAttr.isEmpty())
        return;
    m_level = level;
    m_session = -1;
    m_unknownAttr.clear();
    emit changed();
}

void ShowLevel::setSessionLevel(Level level)
{
    if (level < Student || level > Advanced || (m_session == int(level)))
        return;
    m_session = level == m_level ? -1 : int(level);
    emit changed();
}

void ShowLevel::setClassic(bool classic)
{
    if (m_classic == classic)
        return;
    m_classic = classic;
    emit changed();
}

void ShowLevel::resetForShow()
{
    const bool was = m_level != Standard || m_session >= 0;
    m_level = Standard;
    m_session = -1;
    m_unlocked = false;
    m_unknownAttr.clear();
    if (was)
        emit changed();
}

QString ShowLevel::key(Level level)
{
    switch (level)
    {
    case Student:  return QStringLiteral("student");
    case Advanced: return QStringLiteral("advanced");
    case Standard:
    default:       return QStringLiteral("standard");
    }
}

ShowLevel::Level ShowLevel::fromKey(const QString &key, bool *ok)
{
    const QString k = key.trimmed().toLower();
    if (ok != nullptr)
        *ok = true;
    if (k == QLatin1String("student"))
        return Student;
    if (k == QLatin1String("advanced"))
        return Advanced;
    if (k == QLatin1String("standard"))
        return Standard;
    if (ok != nullptr)
        *ok = false;
    return Standard;
}

QString ShowLevel::name(Level level)
{
    switch (level)
    {
    case Student:  return tr("Student");
    case Advanced: return tr("Advanced");
    case Standard:
    default:       return tr("Standard");
    }
}

QString ShowLevel::attribute() const
{
    if (!m_unknownAttr.isEmpty())
        return m_unknownAttr;
    return m_level == Standard ? QString() : key(m_level);
}

void ShowLevel::setFromAttribute(const QString &attr)
{
    bool ok = true;
    const Level level = attr.trimmed().isEmpty() ? Standard : fromKey(attr, &ok);
    const bool was = m_level != level || m_session >= 0;
    m_level = level;
    m_session = -1;
    m_unknownAttr.clear();
    if (!ok)
    {
        // A newer build's level: Standard here, written back as it was.
        qWarning() << "ShowLevel: unknown level" << attr << "- opening at Standard";
        m_unknownAttr = attr;
    }
    if (was)
        emit changed();
}

bool ShowLevel::shown(const QString &key)
{
    // At exit the holder may be gone: everything shows then.
    if (s_level.isDestroyed())
        return true;
    return s_level()->effectiveLevel() >= levelOf(key);
}

ShowLevel::Level ShowLevel::levelOf(const QString &key)
{
    const LevelRegistry::Row *r = LevelRegistry::row(key);
    return r != nullptr ? r->level : Student;
}

QString ShowLevel::whyHidden(const QString &key)
{
    const LevelRegistry::Row *r = LevelRegistry::row(key);
    if (r == nullptr)
        return QString();
    return tr("%1: %2 level. View \342\226\270 Workspace \342\226\270 %2.").arg(r->words, name(r->level));
}

void ShowLevel::bind(QAction *action, const QString &key)
{
    if (action == nullptr)
        return;
    if (isNeverHidden(action))
    {
        qWarning() << "ShowLevel: never hidden, not bound:" << action->text() << key;
        return;
    }
    if (LevelRegistry::row(key) == nullptr)
        qWarning() << "ShowLevel: no registry row for" << key;
    action->setProperty(KPropLevelKey, key);
    if (!m_actions.contains(action))
        m_actions << action;
    applyTo(action);
}

void ShowLevel::bind(QWidget *widget, const QString &key)
{
    if (widget == nullptr)
        return;
    if (isNeverHidden(widget))
    {
        qWarning() << "ShowLevel: never hidden, not bound:" << widget->objectName() << key;
        return;
    }
    if (LevelRegistry::row(key) == nullptr)
        qWarning() << "ShowLevel: no registry row for" << key;
    widget->setProperty(KPropLevelKey, key);
    if (!m_widgets.contains(widget))
        m_widgets << widget;
    applyTo(widget);
}

void ShowLevel::addMoreLine(QMenu *menu)
{
    if (menu == nullptr || moreLine(menu) != nullptr)
        return;
    QAction *more = new QAction(menu);
    more->setProperty(KPropMoreLine, true);
    more->setObjectName(menu->objectName().isEmpty() ? QStringLiteral("levelMoreLine")
                                                     : menu->objectName() + QStringLiteral("LevelMore"));
    QPointer<QMenu> guard(menu);
    connect(more, &QAction::triggered, this, [this, guard]() {
        if (guard != nullptr)
            offerMenu(guard);
    });
    menu->addAction(more);
    m_menus << menu;
    apply();
}

QAction *ShowLevel::moreLine(const QMenu *menu) const
{
    if (menu == nullptr)
        return nullptr;
    for (QAction *a : menu->actions())
        if (a->property(KPropMoreLine).toBool())
            return a;
    return nullptr;
}

void ShowLevel::setNeverHidden(const QList<QObject *> &objects)
{
    m_neverHidden.clear();
    for (QObject *o : objects)
        if (o != nullptr)
            m_neverHidden << o;
}

bool ShowLevel::isNeverHidden(const QObject *object) const
{
    for (const QPointer<QObject> &o : m_neverHidden)
        if (o == object)
            return true;
    return false;
}

void ShowLevel::applyTo(QAction *a)
{
    const QString key = a->property(KPropLevelKey).toString();
    const LevelRegistry::Row *r = LevelRegistry::row(key);
    const bool show = r == nullptr || effectiveLevel() >= r->level;
    const Treatment t = r != nullptr ? r->treatment : Fold;
    if (t == Grey)
    {
        if (!show && !a->property(KPropGreyed).toBool())
        {
            a->setProperty(KPropGreyed, true);
            a->setProperty(KPropToolTip, a->toolTip());
            a->setEnabled(false);
            a->setToolTip(whyHidden(key));
            if (!a->property("levelGuarded").toBool())
            {
                // A mode change that re-enables it, or a tooltip it sets, is
                // put right again: the level wins while it is greyed.
                a->setProperty("levelGuarded", true);
                QPointer<QAction> guard(a);
                connect(a, &QAction::changed, this, [guard]() {
                    if (guard == nullptr || !guard->property(KPropGreyed).toBool())
                        return;
                    const QString why = whyHidden(guard->property(KPropLevelKey).toString());
                    if (guard->isEnabled())
                        guard->setEnabled(false);
                    if (guard->toolTip() != why)
                    {
                        guard->setProperty(KPropToolTip, guard->toolTip());
                        guard->setToolTip(why);
                    }
                });
            }
        }
        else if (show && a->property(KPropGreyed).toBool())
        {
            a->setProperty(KPropGreyed, false);
            a->setEnabled(true);
            a->setToolTip(a->property(KPropToolTip).toString());
        }
        answerShortcut(a, !show);
        return;
    }
    // Fold and Tab: out of the menu (and its shortcut with it), and back
    // as it was -- an action something else hid stays hidden.
    if (!show && !a->property(KPropFolded).toBool())
    {
        if (a->isVisible())
        {
            a->setProperty(KPropFolded, true);
            a->setVisible(false);
        }
    }
    else if (show && a->property(KPropFolded).toBool())
    {
        a->setProperty(KPropFolded, false);
        a->setVisible(true);
    }
    answerShortcut(a, !show && a->property(KPropFolded).toBool());
}

void ShowLevel::applyTo(QWidget *w)
{
    const QString key = w->property(KPropLevelKey).toString();
    const LevelRegistry::Row *r = LevelRegistry::row(key);
    const bool show = r == nullptr || effectiveLevel() >= r->level;
    const Treatment t = r != nullptr ? r->treatment : Grey;
    if (t == Grey)
    {
        if (!show && !w->property(KPropGreyed).toBool())
        {
            w->setProperty(KPropGreyed, true);
            w->setProperty(KPropToolTip, w->toolTip());
            w->setEnabled(false);
            w->setToolTip(whyHidden(key));
            w->installEventFilter(this);
        }
        else if (show && w->property(KPropGreyed).toBool())
        {
            w->setProperty(KPropGreyed, false);
            w->removeEventFilter(this);
            w->setEnabled(true);
            w->setToolTip(w->property(KPropToolTip).toString());
        }
        return;
    }
    if (!show && !w->property(KPropFolded).toBool())
    {
        if (!w->isHidden())
        {
            w->setProperty(KPropFolded, true);
            w->hide();
        }
    }
    else if (show && w->property(KPropFolded).toBool())
    {
        w->setProperty(KPropFolded, false);
        w->show();
    }
}

bool ShowLevel::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *w = qobject_cast<QWidget *>(watched);
    if (w == nullptr || !w->property(KPropGreyed).toBool())
        return QObject::eventFilter(watched, event);
    if (event->type() == QEvent::EnabledChange)
    {
        // Something else (a page refresh) enabled it: the level wins.
        if (w->isEnabled())
            QMetaObject::invokeMethod(w, [w]() {
                if (w->property(KPropGreyed).toBool())
                    w->setEnabled(false);
            }, Qt::QueuedConnection);
    }
    else if (event->type() == QEvent::ToolTipChange)
    {
        const QString why = whyHidden(w->property(KPropLevelKey).toString());
        if (w->toolTip() != why)
        {
            const QString theirs = w->toolTip();
            QMetaObject::invokeMethod(w, [w, why, theirs]() {
                if (!w->property(KPropGreyed).toBool() || w->toolTip() == why)
                    return;
                w->setProperty(KPropToolTip, theirs);
                w->setToolTip(why);
            }, Qt::QueuedConnection);
        }
    }
    return QObject::eventFilter(watched, event);
}

void ShowLevel::answerShortcut(QAction *a, bool hidden)
{
    // A hidden or disabled action's shortcut is dead in Qt: a shadow
    // shortcut answers it with the level's words.
    const QString key = a->property(KPropLevelKey).toString();
    m_shortcutAnswers.removeAll(QPointer<QObject>());
    bool have = false;
    for (const QPointer<QObject> &o : m_shortcutAnswers)
        if (o != nullptr && o->property("levelAnswerFor").value<QObject *>() == a)
        {
            static_cast<QShortcut *>(o.data())->setEnabled(hidden);
            have = true;
        }
    if (have || !hidden || a->shortcuts().isEmpty())
        return;
    QWidget *host = nullptr;
    for (QObject *o : a->associatedObjects())
        if ((host = qobject_cast<QWidget *>(o)) != nullptr)
            break;
    if (host == nullptr)
        host = qobject_cast<QWidget *>(a->parent());
    if (host == nullptr)
        return;
    for (const QKeySequence &seq : a->shortcuts())
    {
        QShortcut *s = new QShortcut(seq, host->window());
        s->setContext(Qt::WindowShortcut);
        s->setProperty("levelAnswerFor", QVariant::fromValue<QObject *>(a));
        connect(s, &QShortcut::activated, this, [this, key]() { offer(key); });
        m_shortcutAnswers << s;
    }
}

void ShowLevel::apply()
{
    m_actions.removeAll(QPointer<QAction>());
    m_widgets.removeAll(QPointer<QWidget>());
    m_menus.removeAll(QPointer<QMenu>());
    for (const QPointer<QAction> &a : m_actions)
        applyTo(a);
    for (const QPointer<QWidget> &w : m_widgets)
        applyTo(w);

    // Each menu's More line: shown while something in it is folded, naming
    // the lowest level that brings it back.
    for (const QPointer<QMenu> &menu : m_menus)
    {
        QAction *more = moreLine(menu);
        if (more == nullptr)
            continue;
        int lowest = -1;
        for (QAction *a : menu->actions())
        {
            if (!a->property(KPropFolded).toBool())
                continue;
            const Level l = levelOf(a->property(KPropLevelKey).toString());
            if (lowest < 0 || int(l) < lowest)
                lowest = int(l);
        }
        more->setVisible(lowest >= 0);
        if (lowest < 0)
            continue;
        more->setData(lowest);
        more->setText(tr("More at %1…").arg(name(Level(lowest))));
        more->setToolTip(tr("What this menu also has at the %1 level, and the switch to it")
                         .arg(name(Level(lowest))));
        // Last in its menu, below its band's line.
        if (menu->actions().last() != more)
        {
            menu->removeAction(more);
            menu->addAction(more);
        }
    }
}

QStringList ShowLevel::boundKeys() const
{
    QStringList out;
    for (const QPointer<QAction> &a : m_actions)
        if (a != nullptr && !out.contains(a->property(KPropLevelKey).toString()))
            out << a->property(KPropLevelKey).toString();
    for (const QPointer<QWidget> &w : m_widgets)
        if (w != nullptr && !out.contains(w->property(KPropLevelKey).toString()))
            out << w->property(KPropLevelKey).toString();
    return out;
}

void ShowLevel::offer(const QString &key)
{
    const LevelRegistry::Row *r = LevelRegistry::row(key);
    if (r == nullptr)
        return;
    emit switchOffered(int(r->level), tr("%1 is at the %2 level.").arg(r->words, name(r->level)));
}

void ShowLevel::offer(const QString &key, const QString &text)
{
    const LevelRegistry::Row *r = LevelRegistry::row(key);
    if (r == nullptr)
        return;
    emit switchOffered(int(r->level), text);
}

void ShowLevel::offerMenu(QMenu *menu)
{
    QAction *more = moreLine(menu);
    if (more == nullptr)
        return;
    const Level level = Level(more->data().toInt());
    QStringList names;
    for (QAction *a : menu->actions())
        if (a->property(KPropFolded).toBool() && levelOf(a->property(KPropLevelKey).toString()) == level)
        {
            QString t = a->text();
            t.remove(QLatin1Char('&'));
            if (a->menu() != nullptr)
                t += QString::fromUtf8(" \342\226\270");
            names << t;
        }
    const QString where = menu->title().remove(QLatin1Char('&'));
    QString text = where.isEmpty()
        ? tr("At the %1 level this menu also has: %2.").arg(name(level), names.join(QStringLiteral(", ")))
        : tr("At the %1 level, %2 also has: %3.").arg(name(level), where, names.join(QStringLiteral(", ")));
    // "Upgrade This Show…." reads as a typo: the ellipsis ends it.
    if (text.endsWith(QString::fromUtf8("\342\200\246.")))
        text.chop(1);
    emit switchOffered(int(level), text);
}
