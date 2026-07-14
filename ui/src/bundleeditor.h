/*
  Q Light Controller Plus
  bundleeditor.h

  Dialog for creating and editing Bundles. Opened from:
    - "Save as Bundle…" (new, pre-populated from current look's palettes)
    - Right-click "Edit…" on an existing bundle (metadata editing only)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef BUNDLEEDITOR_H
#define BUNDLEEDITOR_H

#include "qlcbundle.h"

#include <QDialog>
#include <QList>

class QLineEdit;
class QTextEdit;
class QComboBox;
class QListWidget;
class QPushButton;
class BundleCache;
class Scene;
class Doc;

/** @addtogroup ui_programming
 * @{
 */

class BundleEditor : public QDialog
{
    Q_OBJECT

public:
    /**
     * Create mode: pre-populate the palette list from @p sourcePalettes
     * (the IDs of palettes currently in the look, in order).
     */
    BundleEditor(Doc *doc, BundleCache *cache,
                 const QList<quint32> &sourcePalettes,
                 QWidget *parent = nullptr);

    /**
     * Edit mode: pre-populate all fields from @p existing.
     * Palette list is read-only — snapshot is fixed once saved.
     */
    BundleEditor(Doc *doc, BundleCache *cache,
                 const QLCBundle &existing,
                 QWidget *parent = nullptr);

    /**
     * Duplicate mode: create mode with a pre-built entry list (from an
     * existing bundle's palette snapshots) rather than live palette IDs.
     */
    BundleEditor(Doc *doc, BundleCache *cache,
                 const QList<BundleEntry> &entries,
                 QWidget *parent = nullptr);

    ~BundleEditor() = default;

    /** The bundle as configured; only valid after accept(). */
    QLCBundle bundle() const { return m_bundle; }

private slots:
    void slotAccept();
    void slotNameChanged(const QString &text);

private:
    void buildUi(bool editMode);
    void populatePaletteList(const QList<quint32> &paletteIds);
    void populatePaletteList(const QList<BundleEntry> &entries);
    void updateContainsFromEntries();
    void updateOkButton();

    static QString todayIso();

    Doc         *m_doc;
    BundleCache *m_cache;
    QLCBundle    m_bundle;      //!< Built on accept
    bool         m_editMode;

    QLineEdit  *m_nameEdit;
    QTextEdit  *m_descEdit;
    QLineEdit  *m_authorEdit;
    QLineEdit  *m_keywordsEdit;   //!< Comma-separated
    QComboBox  *m_categoryCombo;
    QComboBox  *m_tempoCombo;
    QLineEdit  *m_moodEdit;
    QListWidget *m_paletteList;  //!< Read-only preview of snapshot
    QPushButton *m_okBtn;
};

/** @} */

#endif // BUNDLEEDITOR_H
