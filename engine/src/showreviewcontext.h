/*
  Q Light Controller Plus - qlcconsole
  showreviewcontext.h

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

#ifndef SHOWREVIEWCONTEXT_H
#define SHOWREVIEWCONTEXT_H

#include <QHash>
#include <QSet>
#include <QList>
#include <QString>

#include "showreview.h"
#include "showtreemodel.h"

class Doc;

/**
 * What the checks of one Show Review run share (docs/design/SHOW_REVIEW.md
 * section 5.1): built once per run, on demand -- each cue list resolved once,
 * the fixtures something uses, group members. Pure reads; never changes the
 * Doc. Internal to the engine's checks.
 */
class ReviewContext
{
public:
    ReviewContext(Doc *doc, const ShowReview::Options &options);

    Doc *doc() const { return m_doc; }
    const ShowReview::Options &options() const { return m_options; }

    /** Every fixture a scene, a group, a palette or a Show Tree layer
     *  reaches -- what "used" means for PAT-03. */
    const QSet<quint32> &usedFixtures();
    /** Whether universe @a u has an output patch (in the Doc, or as the
     *  file says when the Doc's I/O isn't loaded). */
    bool universeHasOutput(quint32 u) const;
    /** Whether the show patches any output at all. */
    bool anyOutput() const;

    /** Cue list @a list resolved (cached by its id). */
    const QList<ShowTree::ResolvedCue> &resolved(const ShowTree::CueList &list);

    /** The fixtures a Show Tree target lights. */
    QList<quint32> targetFixtures(const ShowTree::Target &target) const;

    QString fixtureName(quint32 id) const;
    QString functionName(quint32 id) const;
    QString paletteName(quint32 id) const;
    /** "cue 2.5 Chorus (Act 1)" */
    QString cueText(const ShowTree::CueList &list, int index) const;

    /** A finding with its targets sorted. */
    static ReviewFinding make(const QString &check, ReviewFinding::Severity severity,
                              const QString &message, QStringList targets,
                              const QByteArray &fingerprint,
                              const QString &jump = QString(),
                              const QVariantList &jumpData = QVariantList());

private:
    Doc *m_doc;
    ShowReview::Options m_options;
    bool m_usedBuilt = false;
    QSet<quint32> m_used;
    QHash<quint32, QList<ShowTree::ResolvedCue>> m_resolved;
};

namespace ShowReviewChecks
{
    /** The checks over the patch, fixtures, effects, power, files and names */
    QList<ShowReview::Check> showChecks();
    /** The checks over the Show Tree: palettes in cues, tracking, hits */
    QList<ShowReview::Check> treeChecks();
    /** Effect looks in scenes that can't run, by design: no script chosen,
     *  or the script isn't on this computer (palette id -> why). */
    QHash<quint32, QString> designEffectProblems(Doc *doc);
}

#endif
