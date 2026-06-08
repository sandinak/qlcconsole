/*
  Q Light Controller Plus
  lookeditor.h

  Fork-owned inline editor for a single palette ("look"), shown at the
  bottom of the Programming tab's canvas when a look is selected. Controls
  adapt to the palette type:
    - Color   : color picker
    - Dimmer  : intensity slider
    - PanTilt : X/Y grid (the VC XY pad widget)
    - Beam    : 3 sliders (Focus, Frost, Iris)
    - Gobo/Shutter and other single-value types : slider + named-capability
      picker with gobo/effect image thumbnails and a live preview

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef LOOKEDITOR_H
#define LOOKEDITOR_H

#include <QWidget>

class QStackedWidget;
class QColorDialog;
class VCXYPadArea;
class CapabilityBar;
class QSlider;
class QComboBox;
class QLabel;
class QToolButton;
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
    void slotColorExtraChanged(); //!< White/Amber/UV sliders
    void slotDimmerChanged(int v);
    void slotPanTiltChanged(const QPointF &p);
    void slotSingleValueChanged(int v);
    void slotSingleCapabilityPicked(int index);
    void slotBeamChanged(int);

private:
    /** Whether any target fixture of the context scene has a channel in the
     *  given QLCChannel::Group. */
    bool targetsHaveChannelGroup(int group) const;

    /** A representative target-fixture channel of the given group, or NULL.
     *  Used to surface capability (gobo/shutter/strobe) names. */
    const QLCChannel *representativeChannel(int group) const;

    /** True if any target fixture has the given QLCChannel::PrimaryColour
     *  emitter channel (White/Amber/UV). */
    bool targetsHaveColour(int primaryColour) const;

    /** Store the Color palette from the picker + White/Amber/UV sliders. */
    void commitColor();

    /** Capability name covering value v on the channel, or empty. */
    QString capabilityNameAt(const QLCChannel *ch, int v) const;

    /** Refresh the gobo/shutter image preview for value v on m_singleChannel
     *  (hides the preview when there's no Picture-preset capability). */
    void updateSinglePreview(int v);

private:
    Doc *m_doc;
    Scene *m_contextScene;
    quint32 m_paletteId;
    bool m_loading;

    QLabel *m_title;
    QLabel *m_warning;
    QStackedWidget *m_stack;
    int m_pageEmpty, m_pageColor, m_pageDimmer, m_pagePanTilt, m_pageBeam, m_pageSingle;

    QColorDialog *m_colorDialog;
    QSlider *m_whiteSlider, *m_amberSlider, *m_uvSlider; //!< extra colour emitters
    QWidget *m_whiteRow, *m_amberRow, *m_uvRow;          //!< hidden when absent
    CapabilityBar *m_dimmerBar; //!< gradient + strobe region marks
    QLabel *m_dimmerValue;
    QLabel *m_dimmerCap;       //!< capability name at current intensity (strobe etc.)
    VCXYPadArea *m_xyPad;
    QSlider *m_beamFocusSlider, *m_beamFrostSlider, *m_beamIrisSlider;
    QLabel *m_beamFocusValue, *m_beamFrostValue, *m_beamIrisValue;
    QSlider *m_singleSlider;
    QLabel *m_singleValue;
    QComboBox *m_singleCombo;  //!< named gobo/shutter capabilities
    QLabel *m_singlePreview;   //!< gobo/shutter image for the current value
    const QLCChannel *m_singleChannel; //!< representative chan for the single page
};

/** @} */

#endif
