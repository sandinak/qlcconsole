/*
  Q Light Controller Plus
  livecapturedialog.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef LIVECAPTUREDIALOG_H
#define LIVECAPTUREDIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>

#include "capturemanager.h"

class Doc;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLineEdit;

class LiveCaptureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LiveCaptureDialog(Doc* doc, QWidget* parent = nullptr);

private slots:
    void slotApply();
    void slotSaveAsNew();
    void slotItemChanged(QTreeWidgetItem* item, int column);

private:
    void buildTree();
    QString channelLabel(quint32 fxi, quint32 ch) const;
    QString fixtureLabel(quint32 fxi) const;

    /** Build a copy of m_plan filtered to only include rows the user
        left checked. Channels skipped (chaser-driven, or unchecked by
        the user) are dropped. Scenes with no surviving overrides are
        dropped too. */
    QList<CaptureManager::ScenePlan> filteredPlan() const;

private:
    Doc* m_doc;
    QList<CaptureManager::ScenePlan> m_plan;
    QTreeWidget* m_tree;
    QHash<quint32, QLineEdit*> m_renameFields;
    QPushButton* m_applyButton;
    QPushButton* m_saveAsNewButton;
};

#endif
