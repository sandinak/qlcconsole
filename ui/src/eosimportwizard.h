/*
  Q Light Controller
  eosimportwizard.h

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

#ifndef EOSIMPORTWIZARD_H
#define EOSIMPORTWIZARD_H

#include <QWizard>
#include <QList>

#include "eosasciiparser.h"
#include "eosfixturemapper.h"
#include "eosimporter.h"

class QTableWidget;
class QListWidget;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class VCFrame;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * File > Import for an ETC Eos show (EOS_IMPORT_DESIGN.md, "UI entry
 * point"). Two pages:
 *
 *   1. Files: the USITT ASCII export (required) and its companions --
 *      magic sheet XMLs (channels onto the 2D map), an Augment3d .esf3d
 *      (v2). Next reads the ASCII file.
 *   2. Fixture types: what the file holds, and each Eos personality ->
 *      the fixture definition to use (saved ones, exact matches, close
 *      matches to confirm, or a generic dimmer of the same size; "Other…"
 *      picks any definition with a mode of that size). The choices can be
 *      remembered in the per-user map file, so a re-import doesn't ask.
 *   3. Special circuits: each channel's role -- Relay (power), House,
 *      Work, Backstage / hall, Normal -- read from its Eos label
 *      (EosCircuit::classify), changed per channel or for a selection.
 *      The roles set the fixtures' output rules and make the groups
 *      Eos/Power relays, Eos/House & Work, Eos/Backstage.
 *
 * Then the caller builds the scratch Doc (load()), hands it to the usual
 * ImportSelectionDialog / QxwImporter merge, and the cue lists to the Show
 * Tree when toShowTree() is ticked.
 */
class EosImportWizard final : public QWizard
{
    Q_OBJECT
    Q_DISABLE_COPY(EosImportWizard)

public:
    EosImportWizard(QWidget *parent, Doc *doc, const QString &asciiPath = QString());
    ~EosImportWizard();

    /** The files picked (and the map file). */
    EosImportBundle bundle() const;
    /** The mapper with the user's choices applied (after accept). */
    EosFixtureMapper *mapper() { return &m_mapper; }
    /** Bring the cue lists into the Show Tree, tracking. */
    bool toShowTree() const;
    /** Save the choices in the per-user map file (tests turn it off). */
    void setRemember(bool on);

    /** Read the ASCII file now (page 1's Next); false with a message. */
    bool readAscii(QString *error = nullptr);
    /** Page 2's table, for tests: the rows' personality keys, in order. */
    QStringList rowKeys() const { return m_rowKeys; }
    /** The choice in row @a row (tests; Other… picks are in it too). */
    EosFixtureChoice rowChoice(int row) const;
    /** Pick @a choice in row @a row (tests; what Other… does). */
    void setRowChoice(int row, const EosFixtureChoice &choice);
    /** What page 2 says the file holds. */
    QString summary() const;
    const EosShowData &showData() const { return m_data; }

    /** Apply the rows to the mapper, and save them when remembered. */
    void applyChoices();

    /** Page 3: a channel's role (EosCircuit::Role). */
    int circuitRole(int channel) const { return m_roles.value(channel, 0); }
    /** Set @a channels to @a role (what "Set selected" does). */
    void setCircuitRoles(const QList<int> &channels, int role);
    /** Page 3's channels, in table order (shown or not). */
    QList<int> circuitChannels() const { return m_circuitChannels; }

    /** After the merge: each sub as a Playback fader on its scene, in a new
     *  VC frame "Eos subs", in a row below what the console holds. @a subs
     *  holds the subs with their scene ids in @a doc (the live show). The
     *  frame (null: no console, or no subs). */
    static VCFrame *addSubFaders(Doc *doc, const QList<EosImportedSub> &subs, QStringList *lines = nullptr);

protected:
    void accept() override;
    bool validateCurrentPage() override;

private:
    QWizardPage *createFilesPage();
    QWizardPage *createTypesPage();
    QWizardPage *createCircuitsPage();
    void fillTypes();
    void fillCircuits();
    void showCircuitRows();
    void fillRow(int row, const EosPersonality &p, const EosFixtureChoice &select);
    void pickOther(int row);

private:
    Doc *m_doc;
    EosFixtureMapper m_mapper;
    EosShowData m_data;
    QString m_readPath;

    QLineEdit *m_asciiEdit = nullptr;
    QListWidget *m_sheetList = nullptr;
    QLineEdit *m_esfEdit = nullptr;

    QLabel *m_summary = nullptr;
    QTableWidget *m_types = nullptr;
    QCheckBox *m_remember = nullptr;
    QCheckBox *m_showTree = nullptr;
    QStringList m_rowKeys;
    QList<QList<EosFixtureChoice>> m_rowChoices;    //!< per row, the combo's entries

    QTableWidget *m_circuits = nullptr;
    QCheckBox *m_allChannels = nullptr;
    QComboBox *m_setRole = nullptr;
    QLabel *m_circuitSummary = nullptr;
    QHash<int, int> m_roles;                        //!< channel -> EosCircuit::Role
    QList<int> m_circuitChannels;                   //!< the table's rows
};

/** @} */

#endif
