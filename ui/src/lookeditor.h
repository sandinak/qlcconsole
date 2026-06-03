/*
  Q Light Controller Plus
  lookeditor.h

  Fork-owned inline editor for a single palette ("look"), shown at the
  bottom of the Programming tab's canvas when a look is selected. Controls
  adapt to the palette type:
    - Color   : color picker
    - Dimmer  : intensity slider
    - PanTilt : X/Y grid (the VC XY pad widget)
    - Gobo/Shutter and other single-value types : slider
  (Named gobo/shutter capability values are a planned follow-up.)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef LOOKEDITOR_H
#define LOOKEDITOR_H

#include <QWidget>

class QStackedWidget;
class QColorDialog;
class VCXYPadArea;
class QSlider;
class QLabel;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class LookEditor final : public QWidget
{
    Q_OBJECT

public:
    LookEditor(Doc *doc, QWidget *parent = nullptr);
    ~LookEditor();

public slots:
    /** Edit the given palette; invalidId() shows the empty page. */
    void setPalette(quint32 paletteId);

signals:
    /** The edited palette's value changed. */
    void paletteChanged(quint32 paletteId);

private slots:
    void slotColorChanged(const QColor &c);
    void slotDimmerChanged(int v);
    void slotPanTiltChanged(const QPointF &p);
    void slotSingleValueChanged(int v);

private:
    Doc *m_doc;
    quint32 m_paletteId;
    bool m_loading;

    QLabel *m_title;
    QStackedWidget *m_stack;
    int m_pageEmpty, m_pageColor, m_pageDimmer, m_pagePanTilt, m_pageSingle;

    QColorDialog *m_colorDialog;
    QSlider *m_dimmerSlider;
    QLabel *m_dimmerValue;
    VCXYPadArea *m_xyPad;
    QSlider *m_singleSlider;
    QLabel *m_singleValue;
};

/** @} */

#endif
