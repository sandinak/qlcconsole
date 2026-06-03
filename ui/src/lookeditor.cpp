/*
  Q Light Controller Plus
  lookeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QColorDialog>
#include <QSlider>
#include <QLabel>

#include "lookeditor.h"
#include "virtualconsole/vcxypadarea.h"
#include "qlcpalette.h"
#include "doc.h"

// VC XY pad works in DMX space [0..256); palette pan/tilt are degrees.
static const qreal XY_MAX = 256.0;
static const int PAN_DEG = 540;
static const int TILT_DEG = 270;

LookEditor::LookEditor(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_paletteId(QLCPalette::invalidId())
    , m_loading(false)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_title = new QLabel(tr("Look editor"), this);
    root->addWidget(m_title);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack);

    // Empty page
    QWidget *empty = new QWidget(this);
    QVBoxLayout *el = new QVBoxLayout(empty);
    el->addWidget(new QLabel(tr("Select a look above to edit it."), empty));
    m_pageEmpty = m_stack->addWidget(empty);

    // Color page
    m_colorDialog = new QColorDialog(this);
    m_colorDialog->setOptions(QColorDialog::NoButtons
                              | QColorDialog::DontUseNativeDialog);
    m_colorDialog->setWindowFlags(Qt::Widget);
    m_pageColor = m_stack->addWidget(m_colorDialog);
    connect(m_colorDialog, SIGNAL(currentColorChanged(QColor)),
            this, SLOT(slotColorChanged(QColor)));

    // Dimmer page
    QWidget *dimmer = new QWidget(this);
    QHBoxLayout *dl = new QHBoxLayout(dimmer);
    dl->addWidget(new QLabel(tr("Intensity"), dimmer));
    m_dimmerSlider = new QSlider(Qt::Horizontal, dimmer);
    m_dimmerSlider->setRange(0, 255);
    dl->addWidget(m_dimmerSlider, 1);
    m_dimmerValue = new QLabel("0", dimmer);
    m_dimmerValue->setMinimumWidth(32);
    dl->addWidget(m_dimmerValue);
    m_pageDimmer = m_stack->addWidget(dimmer);
    connect(m_dimmerSlider, SIGNAL(valueChanged(int)),
            this, SLOT(slotDimmerChanged(int)));

    // Pan/Tilt page (X/Y grid)
    QWidget *pantilt = new QWidget(this);
    QVBoxLayout *pl = new QVBoxLayout(pantilt);
    m_xyPad = new VCXYPadArea(pantilt);
    m_xyPad->setMode(Doc::Operate); // interactive
    m_xyPad->setMinimumSize(160, 160);
    pl->addWidget(m_xyPad, 1);
    m_pagePanTilt = m_stack->addWidget(pantilt);
    connect(m_xyPad, SIGNAL(positionChanged(QPointF)),
            this, SLOT(slotPanTiltChanged(QPointF)));

    // Single-value page (gobo / shutter / pan / tilt / zoom)
    QWidget *single = new QWidget(this);
    QHBoxLayout *sl = new QHBoxLayout(single);
    sl->addWidget(new QLabel(tr("Value"), single));
    m_singleSlider = new QSlider(Qt::Horizontal, single);
    m_singleSlider->setRange(0, 255);
    sl->addWidget(m_singleSlider, 1);
    m_singleValue = new QLabel("0", single);
    m_singleValue->setMinimumWidth(32);
    sl->addWidget(m_singleValue);
    m_pageSingle = m_stack->addWidget(single);
    connect(m_singleSlider, SIGNAL(valueChanged(int)),
            this, SLOT(slotSingleValueChanged(int)));

    setPalette(QLCPalette::invalidId());
}

LookEditor::~LookEditor()
{
}

void LookEditor::setPalette(quint32 paletteId)
{
    m_paletteId = paletteId;
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == NULL)
    {
        m_title->setText(tr("Look editor"));
        m_stack->setCurrentIndex(m_pageEmpty);
        return;
    }

    m_loading = true;
    const QString name = p->name().isEmpty()
                         ? QLCPalette::typeToString(p->type()) : p->name();
    m_title->setText(tr("Editing look: %1").arg(name));

    switch (p->type())
    {
    case QLCPalette::Color:
        m_colorDialog->setCurrentColor(p->rgbValue());
        m_stack->setCurrentIndex(m_pageColor);
        break;
    case QLCPalette::Dimmer:
        m_dimmerSlider->setValue(p->intValue1());
        m_dimmerValue->setText(QString::number(p->intValue1()));
        m_stack->setCurrentIndex(m_pageDimmer);
        break;
    case QLCPalette::PanTilt:
    {
        const qreal x = qreal(p->intValue1()) / PAN_DEG * XY_MAX;
        const qreal y = qreal(p->intValue2()) / TILT_DEG * XY_MAX;
        m_xyPad->setPosition(QPointF(x, y));
        m_stack->setCurrentIndex(m_pagePanTilt);
        break;
    }
    default: // Gobo / Shutter / Pan / Tilt / Zoom
        m_singleSlider->setValue(p->intValue1());
        m_singleValue->setText(QString::number(p->intValue1()));
        m_stack->setCurrentIndex(m_pageSingle);
        break;
    }
    m_loading = false;
}

void LookEditor::slotColorChanged(const QColor &c)
{
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::Color)
        return;
    p->setValue(c.name());
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}

void LookEditor::slotDimmerChanged(int v)
{
    m_dimmerValue->setText(QString::number(v));
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL)
        return;
    p->setValue(v);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}

void LookEditor::slotPanTiltChanged(const QPointF &pos)
{
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::PanTilt)
        return;
    const int pan = int(pos.x() / XY_MAX * PAN_DEG);
    const int tilt = int(pos.y() / XY_MAX * TILT_DEG);
    p->setValue(pan, tilt);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}

void LookEditor::slotSingleValueChanged(int v)
{
    m_singleValue->setText(QString::number(v));
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL)
        return;
    p->setValue(v);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}
