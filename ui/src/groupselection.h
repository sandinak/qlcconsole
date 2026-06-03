/*
  Q Light Controller Plus
  groupselection.h

  Fork-owned modal dialog to pick a SET of fixture groups (multi-select),
  analogous to the fixture-selection dialog but for groups. Used by the
  Scene Editor's group-looks panel to choose the scene's target groups.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef GROUPSELECTION_H
#define GROUPSELECTION_H

#include <QDialog>
#include <QList>

class QListWidget;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Modal multi-select dialog over all fixture groups. Seeded with the
 * currently-selected group IDs; selection() returns the chosen IDs after
 * the dialog is accepted.
 */
class GroupSelection final : public QDialog
{
    Q_OBJECT

public:
    GroupSelection(Doc *doc, const QList<quint32> &selected,
                   QWidget *parent = nullptr);
    ~GroupSelection();

    /** Chosen group IDs (valid after accept()). */
    QList<quint32> selection() const { return m_selection; }

private slots:
    void accept() override;
    void slotSelectAll();
    void slotSelectNone();

private:
    Doc *m_doc;
    QListWidget *m_list;
    QList<quint32> m_selection;
};

/** @} */

#endif
