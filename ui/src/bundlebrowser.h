/*
  Q Light Controller Plus
  bundlebrowser.h

  Panel widget shown in the Programming tab's source area. Lists all
  discovered Bundles in a folder hierarchy (one folder per category), with
  live text search and stamp/CRUD operations.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef BUNDLEBROWSER_H
#define BUNDLEBROWSER_H

#include "qlcbundle.h"

#include <QWidget>
#include <QList>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class BundleCache;
class Doc;

/** @addtogroup ui_programming
 * @{
 */

class BundleBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit BundleBrowser(Doc *doc, BundleCache *cache, QWidget *parent = nullptr);

    /** Reload from cache (call after save/delete). */
    void refresh();

signals:
    void stampRequested(const QString &bundleName);
    void saveAsBundleRequested();

public slots:
    void slotSaveAsBundleRequested();

private slots:
    void slotSearchChanged(const QString &text);
    void slotSelectionChanged();
    void slotItemDoubleClicked(QTreeWidgetItem *item, int column);
    void slotContextMenu(const QPoint &pos);
    void slotStamp();
    void slotEdit();
    void slotDelete();
    void slotDuplicate();
    void slotRevealInFinder();

private:
    void buildUi();
    void rebuildTree();
    QLCBundle selectedBundle() const;
    void updatePreview(const QLCBundle &b);
    QTreeWidgetItem *folderFor(const QString &category);

    Doc         *m_doc;
    BundleCache *m_cache;

    QLineEdit    *m_searchEdit;
    QTreeWidget  *m_tree;
    QLabel       *m_previewLabel;
    QPushButton  *m_stampBtn;
    QPushButton  *m_editBtn;
    QPushButton  *m_deleteBtn;
};

/** @} */

#endif // BUNDLEBROWSER_H
