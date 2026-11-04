/*
  Q Light Controller Plus
  changejournal.h

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

#ifndef CHANGEJOURNAL_H
#define CHANGEJOURNAL_H

#include <QAtomicInteger>
#include <QStringList>
#include <QObject>
#include <QList>
#include <QHash>
#include <functional>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

/**
 * What changed since the last save, for the footer's "Unsaved changes".
 *
 * It owns no history of its own: the steps are the two undo stacks' (the
 * Show Tree's and PaletteOps', the ones Undo shows), those taken after the
 * last save. The journal adds what the stacks don't keep -- when a step
 * was taken and what it was about -- and a short list of the changes that
 * go through no undo stack (fixtures added, patched, removed...). Anything
 * else that marks the show modified (settings, programmer values, ...) is
 * only counted: it is "other changes", not itemised.
 */
class QLC_ENGINE_EXPORT ChangeJournal : public QObject
{
    Q_OBJECT

public:
    /** What sort of thing changed. */
    enum Kind { Cue, Layer, Look, Target, Fixture, Settings, Other };
    /** Where an entry can take you. */
    enum Jump { NoJump, ToCue, ToPalette, ToFunction, ToFixture };

    /** The thing a step was about. */
    struct Subject
    {
        Jump jump = NoJump;
        quint32 id = 0xFFFFFFFF;        //!< the cue, palette, function or fixture
        quint32 id2 = 0xFFFFFFFF;       //!< a cue's layer
        QString text;                   //!< "1.0.2 Chorus"
    };

    /** One line of the list: every edit of one thing, together. */
    struct Entry
    {
        Kind kind = Other;
        QString title;                  //!< the thing, or what was done when there is no thing
        QString what;                   //!< what was done: "Colour, Dimmer"
        int edits = 0;
        qint64 when = 0;                //!< the newest edit (ms since epoch)
        Subject subject;
        bool itemised = true;           //!< false: "Other changes", only counted
    };

    explicit ChangeJournal(Doc *doc);

    /** The steps taken from now on are changes since a save: called when
     *  the show is saved, loaded or cleared. */
    void markSaved();

    /** The thing the Show Tree's newest step was about (the view's
     *  selection); null: not known. Called while the step is registered. */
    void setSubjectHint(const std::function<Subject()> &hint) { m_hint = hint; }

    /** A change that goes through no undo stack. Call it just before
     *  Doc::setModified(). */
    void noteArea(Kind kind, const QString &what, Jump jump = NoJump, quint32 id = 0xFFFFFFFF);
    /** Doc::setModified(): counted, from any thread. */
    void noteModified();

    /** The changes since the last save, newest first, an edit of the same
     *  thing joined to the others. The "other changes" line, when there are
     *  some, comes last. */
    QList<Entry> entries();
    /** Undo steps since the save (the edits, before joining) and
     *  modifications nothing above accounts for. */
    int editCount();
    /** The undo steps among them (Undo can take those back). */
    int stepCount();
    int otherCount() const;
    /** "3 changes" or "3 changes; some may be listed by area only". */
    QString summary();
    /** The footer item's tooltip: "3 unsaved changes, click to see them". */
    QString tooltip();

    static QString kindName(Kind kind);

    /** Register the steps taken since the last look: when, and about what.
     *  stepsChanged() is connected to the stacks' changed() signals. */
    void scan();
    void stepsChanged();

private:
    struct Meta
    {
        qint64 when = 0;
        Subject subject;
        Kind kind = Other;
    };
    struct Area
    {
        Kind kind;
        QString what;
        Subject subject;
        qint64 when;
    };
    struct Step
    {
        quint64 stamp;
        QString label;
        Subject subject;                //!< the palette's, the function's; else asked of the hint
        bool tree;
    };
    QList<Step> currentSteps() const;
    static Kind kindOf(const QString &label, bool tree);
    static QString actionOf(const QString &label);

    Doc *m_doc;
    std::function<Subject()> m_hint;
    quint64 m_savedStamp = 0;
    QHash<quint64, Meta> m_meta;
    QList<Area> m_areas;
    int m_areasDropped = 0;
    QAtomicInteger<int> m_mods;
    int m_explained = 0;                //!< modifications a step or an area accounts for
    QAtomicInteger<qint64> m_lastMod;
    int m_scanMods = 0;
};

/** @} */

#endif
