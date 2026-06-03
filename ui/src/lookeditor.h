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
class QComboBox;
class QLabel;
class QLCChannel;
class Scene;
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

    /** The scene whose targets the edited palette applies to. Used to warn
     *  when e.g. a Gobo look has no target fixture that supports gobos. */
    void setContextScene(Scene *scene);

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
    void slotSingleCapabilityPicked(int index);

private:
    /** Whether any target fixture of the context scene has a channel in the
     *  given QLCChannel::Group. */
    bool targetsHaveChannelGroup(int group) const;

    /** A representative target-fixture channel of the given group, or NULL.
     *  Used to surface capability (gobo/shutter/strobe) names. */
    const QLCChannel *representativeChannel(int group) const;

    /** Capability name covering value v on the channel, or empty. */
    QString capabilityNameAt(const QLCChannel *ch, int v) const;

private:
    Doc *m_doc;
    Scene *m_contextScene;
    quint32 m_paletteId;
    bool m_loading;

    QLabel *m_title;
    QLabel *m_warning;
    QStackedWidget *m_stack;
    int m_pageEmpty, m_pageColor, m_pageDimmer, m_pagePanTilt, m_pageSingle;

    QColorDialog *m_colorDialog;
    QSlider *m_dimmerSlider;
    QLabel *m_dimmerValue;
    QLabel *m_dimmerCap;       //!< capability name at current intensity (strobe etc.)
    VCXYPadArea *m_xyPad;
    QSlider *m_singleSlider;
    QLabel *m_singleValue;
    QComboBox *m_singleCombo;  //!< named gobo/shutter capabilities
};

/** @} */

#endif
