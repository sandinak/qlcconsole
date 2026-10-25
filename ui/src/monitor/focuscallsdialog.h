/*
  Q Light Controller Plus
  focuscallsdialog.h

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

#ifndef FOCUSCALLSDIALOG_H
#define FOCUSCALLSDIALOG_H

#include <QDialog>

#include "focuscalls.h"

class QListWidget;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QLineEdit;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The show's focus calls, and starting them (FOCUS_REFINE_DESIGN.md
 * section 4): the calls in the order they are run -- each some fixture
 * groups and the point they focus to, with a note for the crew -- and the
 * venue the corrections are saved for. Start runs them in Focus mode.
 *
 * The calls are the show's (saved with it); the venue is beside it.
 */
class FocusCallsDialog final : public QDialog
{
    Q_OBJECT

public:
    FocusCallsDialog(Doc *doc, QWidget *parent = nullptr);

    /** Open it; Start runs the calls in Focus mode. */
    static void run(Doc *doc, QWidget *parent);

    /** A focus-call cue's Start with no venue applied (Q120): ask for the
     *  venue's name, showing how long until today's date is used; closes
     *  itself when the calls begin. Not modal: the controller still works. */
    static QDialog *askVenue(Doc *doc, QWidget *parent, const QString &fallback, int timeoutMs,
                             const QString &current = QString());

    /** The show opened with a venue applied that was last changed more
     *  than a day ago (board Q129): "Still at <venue>?" -- New venue (the
     *  default), Still here, or None. Not modal; null when nothing needs
     *  asking. */
    static QDialog *confirmVenue(Doc *doc, QWidget *parent);

    /** Gap 8: @p showFile opened with no venue file of its own, but a
     *  sibling *.venues.xml for the same show (its Show= has the same name
     *  but for a version or copy ending): offer to bring its venues over.
     *  Not modal; null when there is nothing to offer. */
    static QDialog *offerVenueImport(Doc *doc, QWidget *parent, const QString &showFile);

    /** The applied venue's corrections, one per line -- each light (needs
     *  focus, or no longer in the show, marked), each trim and target --
     *  and Clear for the one picked: back to the show's own. */
    static QDialog *showCorrections(Doc *doc, QWidget *parent);

    /** The calls as edited (they are the show's once Start or Save). */
    QList<FocusCall> calls() const { return m_calls; }
    void setCalls(const QList<FocusCall> &calls);

    /** The venue chosen: a name (new or existing), or empty for the show's
     *  own calibration. */
    QString venueChoice() const;
    void setVenueChoice(const QString &name);

    /** Write the calls into the show and apply the venue chosen (made if
     *  new). False, with @p why, when a venue is chosen and the show has no
     *  file yet to keep it beside. */
    bool apply(QString *why = nullptr);

    QListWidget *callList() const { return m_list; }

    /** Board Q134: write venue @p venue's focus report to @p path -- a PDF,
     *  a web page (.html) or a Lightwright-style CSV, by its ending. False,
     *  with @p why, when it cannot be written. */
    static bool writeReport(Doc *doc, const QString &venue, const QString &path, QString *why = nullptr);
    /** Ask where, then writeReport(). */
    static void exportReport(Doc *doc, QWidget *parent, const QString &venue);

    /* The venue manager (the buttons beside the venue). */
    /** Apply @p name now (made if new; empty: none). */
    bool switchVenue(const QString &name, QString *why = nullptr);
    bool renameVenue(const QString &from, const QString &to);
    /** Forget a venue and its corrections (refused while Focus mode runs). */
    bool deleteVenue(const QString &name);

private:
    void fillCallList();
    void loadCall(int index);
    void storeCall();
    void addCall();
    void removeCall();
    void moveCall(int by);
    void refreshVenue();
    void fillVenues(const QString &select);
    QString venueAt(int index) const;

private:
    Doc *m_doc;
    QList<FocusCall> m_calls;
    int m_current = -1;
    bool m_loading = false;

    QListWidget *m_list;
    QLineEdit *m_name;
    QListWidget *m_groups;
    QComboBox *m_target;
    QLineEdit *m_note;
    QComboBox *m_venue;
    QLineEdit *m_newVenue;
    QLabel *m_venueNote;
    QPushButton *m_start;
    QPushButton *m_applyVenue = nullptr;
    QPushButton *m_renameVenue = nullptr;
    QPushButton *m_deleteVenue = nullptr;
    QPushButton *m_corrections = nullptr;
    QPushButton *m_report = nullptr;
    QLineEdit *m_checkedBy = nullptr;
    /** New venue: start from the last venue's corrections (Q140). */
    QCheckBox *m_fromLast = nullptr;
    QPushButton *m_practice = nullptr;
    /** The venue a new one is made as: from the last venue when ticked. */
    QString makeVenue(const QString &name);
};

/** @} */

#endif
