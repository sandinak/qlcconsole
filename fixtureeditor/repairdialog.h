/*
  Q Light Controller Plus
  repairdialog.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef REPAIRDIALOG_H
#define REPAIRDIALOG_H

#include <QDialog>
#include <QByteArray>

#include "fixturedefcheck.h"

class QPlainTextEdit;
class QListWidget;
class QPushButton;
class QLabel;
class QTimer;

/**
 * A definition file that will not load, opened anyway: where it breaks and
 * why, the file itself with that line marked, and repairs that were tried
 * before being offered (FixtureDefCheck::repairs()). Apply one -- or edit
 * the text by hand -- and it is checked again; repairs chain, so a file
 * with two things wrong gets the second fix once the first is in. When it
 * reads, "Open in Editor" takes the repaired definition to the editor,
 * unsaved, to be looked over and saved.
 */
class RepairDialog final : public QDialog
{
    Q_OBJECT

public:
    RepairDialog(const QString &path, const QByteArray &xml, QWidget *parent = nullptr);

    /** The file as repaired so far. */
    QByteArray xml() const;

    /** Apply proposal @p index (as listed). */
    void applyRepair(int index);

private:
    void recheck();

    QString m_path;
    QLabel *m_problem;
    QWidget *m_autoBar;          ///< "Fixed automatically: ...  [Undo]"
    QLabel *m_autoNote;
    QByteArray m_original;       ///< as it was on disk, for Undo
    QPlainTextEdit *m_text;
    QListWidget *m_fixes;
    QPushButton *m_apply;
    QPushButton *m_open;
    QTimer *m_recheck;
    QList<FixtureDefCheck::Repair> m_repairs;
    bool m_settingText;
};

#endif
