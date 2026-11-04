/*
  Q Light Controller Plus - Unit test
  showreview_test.h

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

#ifndef SHOWREVIEW_TEST_H
#define SHOWREVIEW_TEST_H

#include <QObject>
#include <QSet>
#include <QTemporaryDir>

#include "showreview.h"

class FixtureGroup;
class QLCPalette;
class Fixture;
class Doc;
#include "qlcphysical.h"
#include <QVector3D>

class ShowReview_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /* One test per check: a tiny show that trips it, and the near miss
       that must not (docs/design/SHOW_REVIEW.md section 6). */
    void pat01();
    void pat02();
    void pat03();
    void pat04();
    void pat06();
    void fix01();
    void fix05();
    void fix07();
    void fix14();
    void fix15();
    void fix15Geometry();
    void fix16();
    void fix16Rows();
    void fix18();
    void fix18SameNameTwoDefinitions();
    void fix16Single();
    void fix16Sources();
    void fix16Save();
    void fx07();
    void pwr01();
    void pwr03();
    void pwr05();
    void pwr06();
    void file01();
    void file02();
    void name01();
    void name02();
    void pal01();
    void pal02();
    void pal03();
    void pal05();
    void cue01();
    void cue02();
    void cue04();
    void cue05();
    void cue06();
    void cue07();
    void cue08();
    void cue11();
    void vc01();
    void teach01();
    void pwr09();
    void zonesSaveAndLoad();
    void zoneGeometry();
    void laserWindowAndPwr09();
    void pwr10();
    void pwr11();
    void pwr11Mover();
    void maskCutsTheWindow();
    void laserClassPwr09();
    void pwr11Band();
    void pwr11BehindCyc();
    void pwr11BandedPan();
    void pwr12Backwash();
    void pwr12Meant();
    void pwr12WarnStates();
    void pwr12AimAtCyc();
    void pwr12Curtain();
    void sceneryOnPurposeSaves();
    void cue03();
    void cue12();
    void pat05();
    void fix02();
    void fix08();
    void fx02();
    void fx03();
    void file03();
    void moveSpeed();
    void moveSpeedUsesThePointingBand();
    void mov01();
    void mov02();
    void mov08();
    void mov13();
    /** The VcIndex reads the Virtual Console without it */
    void vcIndexReads();
    void vc03();
    void vc04();
    void vc05();
    void vc06();
    void vc08();
    void cue13();
    void vc02();
    void vc07();
    void vc09();
    void cue15();
    void ven01();
    void ven02();
    void ven05();
    void ven09();
    void ven10();
    void blk01();
    /** CUE-12's Virtual Console button */
    void cue12Vc();

    /** Accept -> saved -> loaded: quiet; what it read changes: Changed with
     *  the note; something else changes: still accepted; the subject gone:
     *  dropped at save */
    void acceptanceRoundTrip();
    /** A note is required; reopen; turn a check off and on */
    void acceptRules();
    /** Nothing accepted: nothing written; unknown attributes kept */
    void xmlIsQuietAndKeepsTheUnknown();
    /** The same show gives the same fingerprints, loaded twice */
    void fingerprintsAreStable();
    /** The footer's design entries map to catalog ids; runtime ones don't */
    void footerFoldsUnderCatalogIds();
    /** --review: exit codes, text and JSON, the show never written */
    void cli();

    /** Every check in the catalog fired in a test above, and kept quiet in one */
    void everyCheckIsTested();

private:
    QList<ReviewFinding> findings(const QString &check, const ShowReview::Options &options = ShowReview::Options());
    bool fires(const QString &check, const ShowReview::Options &options = ShowReview::Options());
    bool quiet(const QString &check, const ShowReview::Options &options = ShowReview::Options());
    Fixture *par(const QString &name, quint32 address, quint32 universe = 0);
    Fixture *dimmer(const QString &name, quint32 address);
    Fixture *mover(const QString &name, quint32 address);
    /** A laser (or star projector) on the plot at @a at, thrown @a aimDown degrees below
     *  level toward the audience; @a beamDeg its lens */
    Fixture *laser(const QString &name, quint32 address, QLCPhysical::Emission emission,
                   double beamDeg, const QVector3D &at, double aimDown);
    FixtureGroup *group(const QString &name, const QList<Fixture *> &members);
    QLCPalette *colour(const QColor &c);
    /** Rewrite the Show Tree's saved XML and load it back */
    void editTree(const std::function<void(QString &)> &edit);
    QString saveShow() const;
    /** Give the Doc a Virtual Console of @a widgets (inside its top frame) */
    void console(const QString &widgets, const QString &properties = QString());
    bool loadShow(Doc *doc, const QString &xml) const;

private:
    Doc *m_doc = nullptr;
    QTemporaryDir m_dir;
    static QSet<QString> s_fired;
    static QSet<QString> s_quiet;
};

#endif
