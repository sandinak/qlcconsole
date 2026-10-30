/*
  Q Light Controller Plus
  hitmap.h

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

#ifndef HITMAP_H
#define HITMAP_H

#include <functional>
#include <QStringList>
#include <QString>
#include <QList>

class VCButton;
class QWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * What a VC button holding scoped hits does across the show (Q157), in
 * words: the map its properties show ("HIT 1 fires Blackout everywhere,
 * except Strobe in cue 2.3 (overrides Blackout)"), the rows of its Hits
 * table, the Show checks entry listing every override, same-place pair and
 * dead hit, and the questions asked when a hit is added.
 */
namespace HitMap
{
    /** One hit of a button, as its Hits table shows it. */
    struct Row
    {
        quint32 hit = 0xFFFFFFFF;
        QString name;       //!< "(deleted)" for a hit gone
        QString when;       //!< "Always", "While in cue 2.3 Chorus"
        QString note;       //!< "↳ overrides Blackout", "shares cue 2.3 with Pop: step through"
    };

    /** @a hits (a button's, in its order) as rows. @a samePlace: the
     *  button's VCButton::SamePlace. */
    QList<Row> rows(Doc *doc, const QList<quint32> &hits, int samePlace);
    /** The map in one sentence, for a button captioned @a caption. */
    QString sentence(Doc *doc, const QString &caption, const QList<quint32> &hits, int samePlace);

    /** The buttons holding hit @a hitId: their captions, for its row in
     *  the tree ("HIT 1, Stabs"). */
    QStringList buttonNames(quint32 hitId);
    /** Whether hit @a hitId overrides a wider hit on some button. */
    bool overridesOnAButton(Doc *doc, quint32 hitId);

    /** The Show checks entry ("hits.places"): every override, pair of hits
     *  sharing a place on one button, and deleted hit a button still
     *  holds. A Warning when a button holds a deleted hit or a pair it was
     *  never told how to fire; else listed for reading only. */
    void refreshChecks(Doc *doc);
    /** Keep the entry up to date as the tree changes (once per show). */
    void watch(Doc *doc);

    /** Hits @a hitIds added to a button's @a hits, asking first (Q157): a
     *  hit at the place of one it has -- Step through, Together (both set
     *  @a samePlace), Replace (the one there goes) or Cancel; a hit
     *  narrower than one it has, or wider -- Override there? or Cancel.
     *  No silent default. False: nothing added. */
    bool askToAdd(Doc *doc, QList<quint32> &hits, int &samePlace, const QList<quint32> &hitIds, QWidget *parent);

    /** Ask the operator: @a choices are the buttons, the last one is
     *  Cancel (Esc). Returns the index chosen. */
    int ask(QWidget *parent, const QString &title, const QString &text, const QStringList &choices);
    /** Tests: answer instead of asking (empty: ask again). */
    typedef std::function<int(const QString &title, const QString &text, const QStringList &choices)> Asker;
    void setAsker(const Asker &asker);
}

/** @} */

#endif
