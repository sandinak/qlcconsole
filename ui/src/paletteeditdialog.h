/*
  Q Light Controller Plus
  paletteeditdialog.h

  Fork-owned dialog to create or edit a single QLCPalette (name, type,
  value). Shared by the Palette Manager and the Scene Editor's group-look
  panel so palette authoring lives in one place.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PALETTEEDITDIALOG_H
#define PALETTEEDITDIALOG_H

#include <QDialog>
#include <QColor>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QLCPalette;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Create-or-edit one palette. Two modes:
 *  - New:  PaletteEditDialog(parent) — type is chosen in the dialog;
 *          on accept a fresh QLCPalette is built and returned by
 *          result() (ownership passes to the caller, which adds it to
 *          the Doc).
 *  - Edit: PaletteEditDialog(existing, parent) — type is fixed; name and
 *          value are pre-filled and applied to `existing` on accept.
 *
 * Only the palette types the engine can resolve per-fixture are offered:
 * Color, Dimmer, Pan/Tilt, Gobo, Shutter.
 */
class PaletteEditDialog final : public QDialog
{
    Q_OBJECT

public:
    /** New-palette mode. */
    explicit PaletteEditDialog(QWidget *parent = nullptr);
    /** Edit-existing mode. */
    explicit PaletteEditDialog(QLCPalette *existing, QWidget *parent = nullptr);
    ~PaletteEditDialog();

    /** The created (new mode) or edited (edit mode) palette, valid only
        after the dialog was accepted. In new mode the caller owns it. */
    QLCPalette *result() const { return m_palette; }

private slots:
    void slotTypeChanged();
    void slotPickColor();
    void accept() override;

private:
    void buildUi();
    void syncEditorsToType();

private:
    QLCPalette *m_existing;   //!< non-null in edit mode
    QLCPalette *m_palette;    //!< result after accept()
    QColor m_color;           //!< current color for Color type

    QLineEdit *m_nameEdit;
    QComboBox *m_typeCombo;
    QPushButton *m_colorButton;
    QLabel *m_value1Label;
    QSpinBox *m_value1Spin;
    QLabel *m_value2Label;
    QSpinBox *m_value2Spin;
};

/** @} */

#endif
