/*
  Q Light Controller Plus
  venuefromshowdialog.h

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

#ifndef VENUEFROMSHOWDIALOG_H
#define VENUEFROMSHOWDIALOG_H

#include <QTableWidget>
#include <QDialog>
#include <QHash>

#include "venueimport.h"

class RoomOutputPicker;
class QDialogButtonBox;
class QListWidget;
class QPushButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The other show's groups against this show's (docs/design/VENUE_FROM_SHOW.md
 * section 5): one row each, the show's group it fills (suggested by name)
 * and a tick that confirms it. Choosing a group by hand confirms it.
 */
class GroupMatchTable final : public QTableWidget
{
    Q_OBJECT

public:
    enum Column { NameCol = 0, HeadsCol, GroupCol, ConfirmCol };

    GroupMatchTable(Doc *doc, QWidget *parent = nullptr);

    /** One row per name (with its head count), suggestions as given. */
    void setRows(const QStringList &names, const QList<int> &heads, const QList<VenueImport::Match> &matches);
    QList<VenueImport::Match> matches() const;
    /** Row @p row's group combo / tick (for tests and the shots). */
    QComboBox *groupBox(int row) const;
    void setConfirmed(int row, bool on);

signals:
    void matchesChanged();

private:
    Doc *m_doc;
    QList<VenueImport::MatchKind> m_kinds;
    bool m_loading = false;
};

/**
 * New Venue from a Show (docs/design/VENUE_FROM_SHOW.md): offered from the
 * venue menu's New Venue... dialog (From a Show...). Pick another show
 * file, name the room; its fixtures come in as House, at their plot places,
 * on the one universe this show sets aside for the room, into the new venue
 * only. The show file is not changed.
 */
class VenueFromShowDialog final : public QDialog
{
    Q_OBJECT

public:
    VenueFromShowDialog(Doc *doc, const QString &name, QWidget *parent = nullptr);

    /** Open it, not modal. */
    static VenueFromShowDialog *open(Doc *doc, QWidget *parent, const QString &name = QString());

    /** Read @p path (as Browse... does). False, with the reason on screen,
     *  when it can't be used. */
    bool setFile(const QString &path);

    /** Make the venue and apply it (Make Venue). The venue's name, or empty
     *  (the reason on screen). */
    QString makeVenue();

    QLineEdit *nameEdit() const { return m_name; }
    QComboBox *universeBox() const { return m_universe; }
    /** "Use it anyway": shown when the universe chosen has the show's own
     *  fixtures on it (its output is replaced while the venue is applied). */
    QCheckBox *universeConfirm() const { return m_universeConfirm; }
    RoomOutputPicker *outputPicker() const { return m_output; }
    GroupMatchTable *groupTable() const { return m_groups; }
    QLabel *summaryLabel() const { return m_summary; }
    QListWidget *clashList() const { return m_clashes; }
    QPushButton *makeButton() const { return m_make; }

    /** The closing note's Undo: the venue made off the rig and removed,
     *  @p previous (the venue applied before; empty: None) back on. */
    static bool undoImport(Doc *doc, const QString &venue, const QString &previous);

private:
    void refresh();
    VenueImport::Options options() const;

private:
    Doc *m_doc;
    VenueImport::Source m_source;
    bool m_read = false;
    QLineEdit *m_name;
    QLineEdit *m_file;
    QPushButton *m_browse;
    QComboBox *m_universe;
    QCheckBox *m_universeConfirm;
    QHash<quint32, int> m_showOn;       //!< universe -> the show's fixtures on it
    RoomOutputPicker *m_output;
    GroupMatchTable *m_groups;
    QLabel *m_summary;
    QLabel *m_warnings;
    QListWidget *m_clashes;
    QPushButton *m_make;
};

/** @} */

#endif
