/*
  Q Light Controller Plus - qlcconsole
  showlevel.h

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

#ifndef SHOWLEVEL_H
#define SHOWLEVEL_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "qlcui_export.h"

class QAction;
class QMenu;
class QWidget;

/** @addtogroup ui UI
 * @{
 */

/**
 * The open show's level (board Q249, docs/design/WORKSPACE_LEVELS.md):
 * Student, Standard or Advanced, beside the view in View ▸ Workspace. The
 * view picks the tabs; the level picks how much each one shows.
 *
 * Saved with the show as Level="student" on <Workspace>. Absent is Standard
 * (every show made before levels, and one saved through QLC+ 4), and
 * nothing is written at Standard, so an untouched show's file does not
 * change. A value this build does not know reads as Standard and is
 * written back as it was.
 *
 * In a Teaching show a raise past the saved level is for this session only
 * (the instructor's PIN unlocks it: App::requestLevel()), never saved: the
 * student copy always reopens at its own level.
 *
 * Classic QLC+ is stock QLC+'s surface: levels don't apply there, and the
 * show acts as Advanced (setClassic()).
 *
 * What a level hides is in ONE table, LevelRegistry (levelregistry.cpp):
 * a widget or action declares its key once, where it is made (bind()), and
 * apply() alone decides what shows. A hidden item is hidden, never
 * stopped, and always has a way to the level that shows it: a greyed
 * control says its level, a folded menu item leaves a "More at Standard…"
 * line, a hidden tab is named in the Window menu.
 *
 * One for the process, as ShowFeatures: App resets it with each show; a
 * part of the UI made without an App (a test) sees Standard.
 */
class QLC_UI_EXPORT ShowLevel final : public QObject
{
    Q_OBJECT

public:
    enum Level { Student = 0, Standard = 1, Advanced = 2 };
    /** How a key hides below its level (LevelRegistry's rows). */
    enum Treatment
    {
        Grey,   ///< stays where it is, greyed, its tooltip saying the level
        Fold,   ///< leaves its menu for the menu's one "More at …" line
        Tab     ///< the whole tab goes; the Window menu names it
    };

    static ShowLevel *instance();

    /** The level saved in the show. */
    Level level() const { return m_level; }
    /** The level the show is at now: the saved one, or this session's
     *  raise (a Teaching show's PIN unlock). */
    Level chosenLevel() const { return m_session >= 0 ? Level(m_session) : m_level; }
    /** What the window shows: chosenLevel(), or Advanced where levels don't
     *  apply (Classic QLC+). */
    Level effectiveLevel() const { return m_classic ? Advanced : chosenLevel(); }
    /** False in Classic QLC+. */
    bool applies() const { return !m_classic; }

    /** The saved level; clears a session raise. */
    void setLevel(Level level);
    /** A raise for this session only (never saved). */
    void setSessionLevel(Level level);
    bool hasSessionLevel() const { return m_session >= 0; }
    /** A Teaching show's PIN was given: until the show is closed. */
    bool unlocked() const { return m_unlocked; }
    void setUnlocked(bool on) { m_unlocked = on; }
    /** Classic QLC+ is in front (App::setWorkspaceView()). */
    void setClassic(bool classic);
    /** A new or opened show: Standard, no raise, no unlock. */
    void resetForShow();

    /** "student", "standard", "advanced": as saved. */
    static QString key(Level level);
    static Level fromKey(const QString &key, bool *ok = nullptr);
    /** What the menus call @p level. */
    static QString name(Level level);

    /** The <Workspace> attribute: empty at Standard (nothing is written). */
    QString attribute() const;
    /** Read the attribute: empty or unknown is Standard (an unknown one is
     *  kept to write back). */
    void setFromAttribute(const QString &attr);

    /** Whether @p key shows at the effective level. A key with no row is
     *  always shown. */
    static bool shown(const QString &key);
    /** The lowest level that shows @p key (Student for a key with no row). */
    static Level levelOf(const QString &key);
    /** "Importing another show or an Eos file: Standard level. View ▸
     *  Workspace ▸ Standard." */
    static QString whyHidden(const QString &key);

    /** @p action follows @p key from now on (put on it now). */
    void bind(QAction *action, const QString &key);
    void bind(QWidget *widget, const QString &key);
    /** @p menu gets the "More at …" line that its folded items leave. */
    void addMoreLine(QMenu *menu);
    /** Never hidden at any level: Stop All, GO, Back, Blackout, the Grand
     *  Master, Blind, the readiness footer (App says which). bind() refuses
     *  them. */
    void setNeverHidden(const QList<QObject *> &objects);
    bool isNeverHidden(const QObject *object) const;

    /** Put the level on every bound action, widget and More line. */
    void apply();
    /** The keys bound so far (a test checks each has a row). */
    QStringList boundKeys() const;
    /** The More line of @p menu, or null. */
    QAction *moreLine(const QMenu *menu) const;
    /** Say what @p key needs, with the switch: emits switchOffered(). */
    void offer(const QString &key);
    /** The More line's list: what the menu has at the level above. */
    void offerMenu(QMenu *menu);

signals:
    /** The level (saved, session or Classic) changed. */
    void changed();
    /** Say @p text in the notice line, with Switch to @p level. */
    void switchOffered(int level, const QString &text);

public:
    /** Use instance(); public only for the process-wide holder. */
    explicit ShowLevel(QObject *parent = nullptr);

private:
    void applyTo(QAction *action);
    void applyTo(QWidget *widget);

    Level m_level = Standard;
    int m_session = -1;
    bool m_unlocked = false;
    bool m_classic = false;
    QString m_unknownAttr;          ///< a newer build's value, written back as read
    QList<QPointer<QAction>> m_actions;
    QList<QPointer<QWidget>> m_widgets;
    QList<QPointer<QMenu>> m_menus;
    QList<QPointer<QObject>> m_neverHidden;
};

/** @} */

#endif
