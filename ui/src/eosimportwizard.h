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
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * File > Import for an ETC Eos show (EOS_IMPORT_DESIGN.md, "UI entry
 * point"). Two pages:
 *
 *   1. Files: the USITT ASCII export (required) and its companions --
 *      magic sheet XMLs (read in a later version), an Augment3d .esf3d
 *      (v2). Next reads the ASCII file.
 *   2. Fixture types: what the file holds, and each Eos personality ->
 *      the fixture definition to use (saved ones, exact matches, close
 *      matches to confirm, or a generic dimmer of the same size; "Other…"
 *      picks any definition with a mode of that size). The choices can be
 *      remembered in the per-user map file, so a re-import doesn't ask.
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

protected:
    void accept() override;
    bool validateCurrentPage() override;

private:
    QWizardPage *createFilesPage();
    QWizardPage *createTypesPage();
    void fillTypes();
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
};

/** @} */

#endif
