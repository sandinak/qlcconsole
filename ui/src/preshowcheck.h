/*
  Q Light Controller Plus - qlcconsole
  preshowcheck.h

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

#ifndef PRESHOWCHECK_H
#define PRESHOWCHECK_H

#include <QDialog>
#include <QPointer>
#include <QMap>
#include <functional>

#include "inputdevicenotices.h"
#include "systemtuning.h"
#include "qlcui_export.h"

class QPushButton;
class QScrollArea;
class QLabel;
class QTimer;
class Doc;

/**
 * Before the house opens (docs/features/dress-rehearsal.md): one page of
 * what the app already knows about being ready for a show, a line each --
 * Show Review's blockers, every universe reaching its node, the macOS
 * Local Network permission, the controllers, the readiness footer's other
 * entries, mains power, the screen, Wi-Fi, the rest of Settings ▸
 * Performance, autosave -- and the channel check, which only a person in
 * the room can do. Each line says Ready / Check / Not ready / Not used /
 * By hand in a word and a symbol (never colour alone), what is wrong in
 * plain words, and offers the app's own fix where it has one.
 *
 * Nothing new is checked here: each line reads a source that exists
 * (ShowReview, ShowStatus, SystemTuning::check(), InputDeviceNotices, the
 * app's autosave). It is a window, not a tab: opened from Show Review's
 * Before the House Opens… and offered once per session on going to Operate.
 */
class QLC_UI_EXPORT PreshowCheck final : public QDialog
{
    Q_OBJECT

public:
    enum State { Ready, Check, NotReady, NotUsed, ByHand };

    struct Line
    {
        QString key;        ///< stable: "outputs", "review", "pads", ...
        State state = NotUsed;
        QString title;      ///< "Every universe reaches its node"
        QString text;       ///< what it is now, or what is wrong, in plain words
        QStringList items;  ///< the things affected, one line each
        QString fixLabel;   ///< the fix's button; empty: nothing to press
        std::function<void(QWidget *parent)> fix;
    };

    /** What the page reads beyond the Doc, ShowStatus and Show Review: the
     *  app gives them once (setHooks()); a test gives stubs. Any left empty
     *  reads as "not known here". */
    struct Hooks
    {
        std::function<SystemTuning::Probe()> probe;
        std::function<QList<InputDeviceNotices::Device>()> pads;
        std::function<bool()> autosaveOn;
        std::function<int()> autosaveMinutes;
        /** Why the last autosave failed; empty when it didn't */
        std::function<QString()> autosaveFailure;
        std::function<void()> turnAutosaveOn;
        std::function<void(QWidget *parent)> openPerformance;
        std::function<void(QWidget *parent)> quickFocus;
    };
    static void setHooks(const Hooks &hooks);
    static const Hooks &hooks();

    /** Every line, from @a probe and the rest as they stand now. */
    static QList<Line> gather(Doc *doc, const Hooks &hooks, const SystemTuning::Probe &probe);

    static QString stateWord(State state);
    static QString stateSymbol(State state);
    /** The ShowStatus keys the outputs line reads */
    static bool isOutputKey(const QString &statusKey);

    explicit PreshowCheck(Doc *doc, QWidget *parent = nullptr);

    /** Look again: Show Review runs, this computer is probed, every line
     *  redrawn (Check Again). */
    void refresh();
    /** Redraw from the sources, with the last probe. */
    void rebuild();

    QList<Line> lines() const { return m_lines; }
    Line line(const QString &key) const;
    QLabel *summary() const { return m_summary; }
    QLabel *stateLabel(const QString &key) const { return m_stateLabels.value(key); }
    QPushButton *fixButton(const QString &key) const { return m_fixButtons.value(key); }
    QPushButton *checkAgainButton() const { return m_again; }
    QPushButton *checklistButton() const { return m_checklist; }

    /** The app's one page: set once, opened from anywhere. */
    static void setShared(Doc *doc, QWidget *parent);
    static PreshowCheck *openShared();
    static PreshowCheck *shared();
    /** The shared page when the app set one, else a page of its own on
     *  @a doc over @a parent (closes itself away). */
    static PreshowCheck *openFor(Doc *doc, QWidget *parent);

private:
    void runFix(const QString &key);

private:
    Doc *m_doc;
    SystemTuning::Probe m_probe;
    QList<Line> m_lines;
    QLabel *m_summary;
    QLabel *m_when;
    QScrollArea *m_scroll;
    QPushButton *m_again;
    QPushButton *m_checklist;
    QTimer *m_later;
    QMap<QString, QLabel *> m_stateLabels;
    QMap<QString, QPushButton *> m_fixButtons;

    static Hooks s_hooks;
    static QPointer<PreshowCheck> s_shared;
    static Doc *s_doc;
    static QPointer<QWidget> s_parent;
};

#endif
