/*
  Q Light Controller Plus
  showreviewmarker.h

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

#ifndef SHOWREVIEWMARKER_H
#define SHOWREVIEWMARKER_H

#include <QCoreApplication>
#include <QString>
#include <QList>

#include "showreview.h"
#include "qlcui_export.h"

class ContextMenu;
class QWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * A Show Review warning where you work (docs/features/show-review.md): the
 * "⚠ cyc" mark on a Show Tree cue, the footer's Review chip, a row of the
 * Review panel. Every place reads the same detail on hover and offers the
 * same answers on a right-click -- the panel's own engine calls, so a
 * warning cleared here is cleared everywhere:
 *
 *   I Meant It        (PWR-12: kept on the layer that lights it; any other
 *                      check: Accept…, with a note)
 *   Not Meant         (PWR-12 marked meant: taken back)
 *   Show Me           (where to fix it: the layer, the fixture, the function)
 *   Open in Show Review
 */
namespace ShowReviewMarker
{
    /** The detail of @p findings for a tooltip: each one's check, what it
     *  says (which fixtures, how much, on what, in which cue), the cues it
     *  marks, and what a right-click offers. Rich text. */
    QLC_UI_EXPORT QString tooltip(Doc *doc, const QList<ReviewFinding> &findings, int max = 6);

    /** The open findings marked on cue @p cueId (PWR-12's runs). */
    QLC_UI_EXPORT QList<ReviewFinding> cueFindings(Doc *doc, quint32 cueId);
    /** Every open warning and blocker of the last run, worst first. */
    QLC_UI_EXPORT QList<ReviewFinding> openFindings(Doc *doc);

    /** The answers for @p findings, into @p menu: one finding's straight in,
     *  several each in a submenu of its own (up to @p max). */
    QLC_UI_EXPORT void fillMenu(ContextMenu &menu, Doc *doc, const QList<ReviewFinding> &findings,
                                QWidget *parent, int max = 10, bool offerOpen = true);

    /* The answers, one call each (what the menu items run). */
    /** I Meant It: PWR-12's mark on its layer; others are accepted with
     *  @p note (asked for when empty and @p parent is set). Then re-check. */
    QLC_UI_EXPORT bool meant(Doc *doc, const ReviewFinding &finding, QWidget *parent = nullptr,
                             const QString &note = QString());
    /** Not Meant: PWR-12's mark taken back; another accepted finding reopened. */
    QLC_UI_EXPORT bool notMeant(Doc *doc, const ReviewFinding &finding);
    /** Show Me: the finding's jump (the panel's Fix). */
    QLC_UI_EXPORT bool showMe(const ReviewFinding &finding, QWidget *parent);
    /** Open the Review panel on it. */
    QLC_UI_EXPORT bool openInReview(const ReviewFinding &finding);
}

/** @} */

#endif
