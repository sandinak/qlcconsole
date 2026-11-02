/*
  Q Light Controller Plus
  structureduplicatedialog.h

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

#ifndef STRUCTUREDUPLICATEDIALOG_H
#define STRUCTUREDUPLICATEDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>

#include "../qlcui_export.h"
#include "structureduplicate.h"

class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QTreeWidget;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * "Duplicate Setup to...": a structure's fixtures copied onto the other
 * structures of its kind (T1 to T2, T3). Pick the targets, where the copies
 * are patched, what happens to the groups and to a target that already
 * carries fixtures; the preview below shows every copy -- its name and
 * address -- before anything is made. See engine/src/structureduplicate.h
 * and docs/features/lighting-studio.md.
 */
class QLC_UI_EXPORT StructureDuplicateDialog final : public QDialog
{
    Q_OBJECT

public:
    StructureDuplicateDialog(Doc *doc, const QString &sourceKey, QWidget *parent = nullptr);

    /** The structures ticked, in the order listed. */
    QStringList targets() const;
    /** What the dialog says to do. */
    StructureDuplicate::Options options() const;
    /** The preview as it stands. */
    const StructureDuplicate::Preview &preview() const { return m_preview; }

    /** Tick @p keys (and only them). */
    void setTargets(const QStringList &keys);
    void setPatchMode(StructureDuplicate::PatchMode mode);
    void setOffsets(int addressStep, int universeStep);
    void setFixtureGroupMode(StructureDuplicate::GroupMode mode);
    void setMapGroupMode(StructureDuplicate::GroupMode mode);
    void setExisting(const QString &key, StructureDuplicate::Existing how);

private slots:
    void refresh();

private:
    Doc *m_doc;
    QString m_source;
    QStringList m_keys;                 // every target offered, in order
    StructureDuplicate::Preview m_preview;

    QTableWidget *m_targets = nullptr;
    QRadioButton *m_nextFree = nullptr;
    QRadioButton *m_offset = nullptr;
    QRadioButton *m_unpatched = nullptr;
    QSpinBox *m_addressStep = nullptr;
    QSpinBox *m_universeStep = nullptr;
    QComboBox *m_fixtureGroups = nullptr;
    QComboBox *m_mapGroups = nullptr;
    QTreeWidget *m_rows = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_warning = nullptr;
    QPushButton *m_ok = nullptr;
};

/** @} */

#endif
