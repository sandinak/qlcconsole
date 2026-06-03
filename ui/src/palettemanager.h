/*
  Q Light Controller Plus
  palettemanager.h

  Fork-owned dialog: the central place to create, name, edit and delete
  QLCPalettes so they're first-class and reusable across scenes (mirrors
  the role qmlui's palette manager plays in QLC+ 5). Launched from the
  Tools menu.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PALETTEMANAGER_H
#define PALETTEMANAGER_H

#include <QDialog>

class QTreeWidget;
class QPushButton;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class PaletteManager final : public QDialog
{
    Q_OBJECT

public:
    PaletteManager(Doc *doc, QWidget *parent = nullptr);
    ~PaletteManager();

private slots:
    void slotNew();
    void slotEdit();
    void slotRename();
    void slotDelete();
    void slotSelectionChanged();

private:
    void reload();
    /** Selected palette id, or 0 if none. */
    quint32 selectedPaletteId() const;
    /** Count of scenes referencing @p paletteId (for delete safety). */
    int referenceCount(quint32 paletteId) const;

private:
    Doc *m_doc;
    QTreeWidget *m_tree;
    QPushButton *m_newButton;
    QPushButton *m_editButton;
    QPushButton *m_renameButton;
    QPushButton *m_deleteButton;
};

/** @} */

#endif
