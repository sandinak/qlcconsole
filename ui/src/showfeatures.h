/*
  Q Light Controller Plus - qlcconsole
  showfeatures.h

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

#ifndef SHOWFEATURES_H
#define SHOWFEATURES_H

#include <QObject>
#include <QSet>
#include <QString>

/** @addtogroup ui UI
 * @{
 */

/**
 * The open show's features, each on or off (board Q293): View ▸ Workspace ▸
 * Tabs & Features…, saved with the show beside its tab choice (Q262).
 *
 * A feature that is off is HIDDEN, never stopped: its menu items, footer
 * chip and pages go away, the show keeps its data, and switching it back
 * on brings everything back as it was. What a live control does is never
 * hidden or held by a switch -- the emergency stop above all.
 *
 * Every show starts with every feature on: the show file names only the
 * ones turned off, so an older show (and a feature added later) is on.
 *
 * One for the process: App resets it with each show; a part of the UI made
 * without an App (a test) sees every feature on.
 */
class ShowFeatures final : public QObject
{
    Q_OBJECT

public:
    enum Feature
    {
        Venues = 0,         ///< the venue chip, View ▸ Venue…, Focus ▸ Venues, Focus Report
        Power,              ///< the footer power chip, the Power pane and overlay
        GrandMaster,        ///< the footer's Grand Master
        SpeedMasters,       ///< the footer's speed masters
        EngineLoad,         ///< the footer's engine load
        Review,             ///< Show Review: its chip, its marks, its live re-check
        ControlSurfaces,    ///< the Control Map's triggers, encoder bank, Assign to Control…, Pad Run's button
        FocusCalls,         ///< Venue Focus, Edit Focus Calls, a venue focus cue
        FeatureCount
    };

    static ShowFeatures *instance();
    /** Whether @p f is on for the open show. */
    static bool isOn(Feature f);

    bool on(Feature f) const;
    void setOn(Feature f, bool on);
    /** Every feature on (a new show, an older show). */
    void reset();
    QSet<int> offSet() const { return m_off; }
    void setOffSet(const QSet<int> &off);

    /** The show file's word for each, language independent. */
    static QString key(Feature f);
    static QString label(Feature f);
    /** What switching it off hides, in a sentence. */
    static QString description(Feature f);

    /** The show file's attribute: the features turned off, "Venues|Power";
     *  empty with all on. Unknown keys are dropped when read. */
    QString offAttribute() const;
    void setFromOffAttribute(const QString &attr);

signals:
    /** A feature was switched (or the show changed them). */
    void changed();

public:
    /** Use instance(); public only for the process-wide holder. */
    explicit ShowFeatures(QObject *parent = nullptr);

private:
    QSet<int> m_off;
};

/** @} */

#endif
