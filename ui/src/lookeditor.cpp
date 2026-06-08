/*
  Q Light Controller Plus
  lookeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QColorDialog>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QPixmap>
#include <QIcon>
#include <QSet>

#include "lookeditor.h"
#include "virtualconsole/vcxypadarea.h"
#include "capabilitybar.h"
#include "qlcpalette.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

// VC XY pad works in DMX space [0..256); palette pan/tilt are degrees.
static const qreal XY_MAX = 256.0;
static const int PAN_DEG = 540;
static const int TILT_DEG = 270;

LookEditor::LookEditor(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_contextScene(NULL)
    , m_paletteId(QLCPalette::invalidId())
    , m_loading(false)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_title = new QLabel(tr("Look editor"), this);
    root->addWidget(m_title);

    m_warning = new QLabel(this);
    m_warning->setStyleSheet("color: #b00; font-style: italic;");
    m_warning->setWordWrap(true);
    m_warning->hide();
    root->addWidget(m_warning);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack);

    // Empty page
    QWidget *empty = new QWidget(this);
    QVBoxLayout *el = new QVBoxLayout(empty);
    el->addWidget(new QLabel(tr("Select a look above to edit it."), empty));
    m_pageEmpty = m_stack->addWidget(empty);

    // Color page: RGB picker on the LEFT, vertical extra-emitter sliders
    // (White / Amber / UV) on the RIGHT — always visible beside the (tall)
    // colour dialog rather than scrolled off-screen above/below it.
    QWidget *colorPage = new QWidget(this);
    QHBoxLayout *cl = new QHBoxLayout(colorPage);
    cl->setContentsMargins(0, 0, 0, 0);

    m_colorDialog = new QColorDialog(colorPage);
    m_colorDialog->setOptions(QColorDialog::NoButtons
                              | QColorDialog::DontUseNativeDialog);
    m_colorDialog->setWindowFlags(Qt::Widget);
    // Keep the picker compact so the look panel doesn't force the tab (and the
    // window) taller than short screens; it scrolls if the panel is short.
    m_colorDialog->setMinimumHeight(0);
    m_colorDialog->setMaximumHeight(300);
    cl->addWidget(m_colorDialog, 1);

    // One vertical slider (label above) per extra emitter.
    auto makeColorCol = [&](const QString &name, QSlider *&slider) -> QWidget* {
        QWidget *col = new QWidget(colorPage);
        QVBoxLayout *vl = new QVBoxLayout(col);
        vl->setContentsMargins(2, 0, 2, 0);
        vl->addWidget(new QLabel(name, col), 0, Qt::AlignHCenter);
        slider = new QSlider(Qt::Vertical, col);
        slider->setRange(0, 255);
        vl->addWidget(slider, 1, Qt::AlignHCenter);
        connect(slider, SIGNAL(valueChanged(int)), this, SLOT(slotColorExtraChanged()));
        return col;
    };
    QWidget *extras = new QWidget(colorPage);
    QHBoxLayout *xl = new QHBoxLayout(extras);
    xl->setContentsMargins(0, 0, 0, 0);
    m_whiteRow = makeColorCol(tr("W"),  m_whiteSlider);
    m_amberRow = makeColorCol(tr("A"),  m_amberSlider);
    m_uvRow    = makeColorCol(tr("UV"), m_uvSlider);
    xl->addWidget(m_whiteRow);
    xl->addWidget(m_amberRow);
    xl->addWidget(m_uvRow);
    cl->addWidget(extras, 0);

    m_pageColor = m_stack->addWidget(colorPage);
    connect(m_colorDialog, SIGNAL(currentColorChanged(QColor)),
            this, SLOT(slotColorChanged(QColor)));

    // Dimmer page — gradient bar with capability (strobe) region marks.
    QWidget *dimmer = new QWidget(this);
    QVBoxLayout *dv = new QVBoxLayout(dimmer);
    QHBoxLayout *dtop = new QHBoxLayout();
    dv->addLayout(dtop);
    dtop->addWidget(new QLabel(tr("Intensity"), dimmer));
    dtop->addStretch();
    m_dimmerValue = new QLabel("0", dimmer);
    m_dimmerValue->setMinimumWidth(32);
    dtop->addWidget(m_dimmerValue);
    m_dimmerBar = new CapabilityBar(dimmer);
    dv->addWidget(m_dimmerBar);
    // Capability name at the current value on a representative fixture.
    m_dimmerCap = new QLabel(dimmer);
    m_dimmerCap->setStyleSheet("color: #555; font-style: italic;");
    dv->addWidget(m_dimmerCap);
    m_pageDimmer = m_stack->addWidget(dimmer);
    connect(m_dimmerBar, SIGNAL(valueChanged(int)),
            this, SLOT(slotDimmerChanged(int)));

    // Pan/Tilt page (X/Y grid)
    QWidget *pantilt = new QWidget(this);
    // XY pad on the LEFT at a usable size; the right half is left free for
    // future tools.
    QHBoxLayout *pl = new QHBoxLayout(pantilt);
    pl->setContentsMargins(6, 6, 6, 6);
    m_xyPad = new VCXYPadArea(pantilt);
    m_xyPad->setMode(Doc::Operate); // interactive
    m_xyPad->setMinimumSize(200, 200);
    m_xyPad->setMaximumSize(280, 280); // keep the bottom look pane compact
    m_xyPad->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    pl->addWidget(m_xyPad, 0, Qt::AlignLeft | Qt::AlignTop);
    pl->addStretch(1); // reserved right half
    m_pagePanTilt = m_stack->addWidget(pantilt);
    connect(m_xyPad, SIGNAL(positionChanged(QPointF)),
            this, SLOT(slotPanTiltChanged(QPointF)));

    // Single-value page (gobo / shutter / pan / tilt / zoom)
    QWidget *single = new QWidget(this);
    QVBoxLayout *sv = new QVBoxLayout(single);
    // Named capabilities (gobo/shutter) from a representative fixture, with a
    // gobo image preview beside the picker.
    QHBoxLayout *pickRow = new QHBoxLayout();
    sv->addLayout(pickRow);
    m_singlePreview = new QLabel(single);
    m_singlePreview->setFixedSize(48, 48);
    m_singlePreview->setAlignment(Qt::AlignCenter);
    m_singlePreview->setFrameShape(QFrame::StyledPanel);
    m_singlePreview->setVisible(false);
    m_singleChannel = NULL;
    pickRow->addWidget(m_singlePreview);
    m_singleCombo = new QComboBox(single);
    pickRow->addWidget(m_singleCombo, 1);
    QHBoxLayout *sl = new QHBoxLayout();
    sv->addLayout(sl);
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
    connect(m_singleCombo, SIGNAL(activated(int)),
            this, SLOT(slotSingleCapabilityPicked(int)));

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
    QString name = p->name().isEmpty()
                   ? QLCPalette::typeToString(p->type()) : p->name();
    // Prefix the folder path (strip the internal "Palettes/" category).
    QString path = p->path();
    if (path.startsWith(QStringLiteral("Palettes/")))
        path = path.mid(9);
    else if (path == QStringLiteral("Palettes"))
        path.clear();
    if (path.endsWith('/'))
        path.chop(1);
    if (path.isEmpty() == false)
        name = path + "/" + name;

    int uses = 0;
    foreach (Function *fn, m_doc->functions())
    {
        Scene *s = qobject_cast<Scene*>(fn);
        if (s != NULL && s->palettes().contains(paletteId))
            uses++;
    }
    m_title->setText(tr("Editing look: %1   —   used by %n scene(s)", "", uses)
                     .arg(name));

    switch (p->type())
    {
    case QLCPalette::Color:
    {
        const QColor rgb = p->rgbValue();
        m_colorDialog->setCurrentColor(rgb);

        // Extra emitters: load stored wauv, or default White to the additive
        // auto value (min of R,G,B) and Amber/UV to 0.
        const QColor wauv = p->wauvValue();
        const int autoW = qMin(rgb.red(), qMin(rgb.green(), rgb.blue()));
        m_whiteSlider->blockSignals(true); m_amberSlider->blockSignals(true); m_uvSlider->blockSignals(true);
        m_whiteSlider->setValue(wauv.isValid() ? wauv.red()   : autoW);
        m_amberSlider->setValue(wauv.isValid() ? wauv.green() : 0);
        m_uvSlider->setValue   (wauv.isValid() ? wauv.blue()  : 0);
        m_whiteSlider->blockSignals(false); m_amberSlider->blockSignals(false); m_uvSlider->blockSignals(false);

        // White is near-universal on LED fixtures and is the common request,
        // so always offer it; gate the rarer Amber/UV on actual presence (but
        // still show them if we can't determine the targets).
        const bool unknown = (m_contextScene == NULL);
        m_whiteRow->setVisible(true);
        m_amberRow->setVisible(unknown || targetsHaveColour(QLCChannel::Amber));
        m_uvRow->setVisible(unknown || targetsHaveColour(QLCChannel::UV));

        m_stack->setCurrentIndex(m_pageColor);
        break;
    }
    case QLCPalette::Dimmer:
    {
        const QLCChannel *ich = representativeChannel(QLCChannel::Intensity);
        QList<CapabilityBar::Region> regions;
        if (ich != NULL)
            foreach (QLCCapability *cap, ich->capabilities())
                regions.append({ int(cap->min()), int(cap->max()), cap->name() });
        m_dimmerBar->setRegions(regions);
        m_dimmerBar->setValue(p->intValue1());
        m_dimmerValue->setText(QString::number(p->intValue1()));
        m_dimmerCap->setText(capabilityNameAt(ich, p->intValue1()));
        m_stack->setCurrentIndex(m_pageDimmer);
        break;
    }
    case QLCPalette::PanTilt:
    {
        const qreal x = qreal(p->intValue1()) / PAN_DEG * XY_MAX;
        const qreal y = qreal(p->intValue2()) / TILT_DEG * XY_MAX;
        m_xyPad->setPosition(QPointF(x, y));
        m_stack->setCurrentIndex(m_pagePanTilt);
        break;
    }
    default: // Gobo / Shutter / Pan / Tilt / Zoom
    {
        m_singleSlider->setValue(p->intValue1());
        m_singleValue->setText(QString::number(p->intValue1()));

        // Named capabilities for gobo/shutter from a representative fixture.
        int chGroup = -1;
        if (p->type() == QLCPalette::Gobo)         chGroup = QLCChannel::Gobo;
        else if (p->type() == QLCPalette::Shutter) chGroup = QLCChannel::Shutter;

        m_singleCombo->clear();
        const QLCChannel *ch = (chGroup >= 0) ? representativeChannel(chGroup) : NULL;
        m_singleChannel = ch;
        if (ch != NULL)
        {
            int sel = -1, idx = 0;
            foreach (QLCCapability *cap, ch->capabilities())
            {
                // Show a gobo/effect thumbnail next to each named capability.
                QIcon icon;
                if (cap->presetType() == QLCCapability::Picture)
                {
                    const QPixmap px(cap->resource(0).toString());
                    if (px.isNull() == false)
                        icon = QIcon(px);
                }
                m_singleCombo->addItem(icon, cap->name(),
                                       (int(cap->min()) + int(cap->max())) / 2);
                if (p->intValue1() >= cap->min() && p->intValue1() <= cap->max())
                    sel = idx;
                idx++;
            }
            if (sel >= 0)
                m_singleCombo->setCurrentIndex(sel);
            m_singleCombo->setIconSize(QSize(32, 32));
            m_singleCombo->setVisible(true);
        }
        else
        {
            m_singleCombo->setVisible(false);
        }
        updateSinglePreview(p->intValue1());
        m_stack->setCurrentIndex(m_pageSingle);
        break;
    }
    }

    // Warn if this look's type can't be realised on any target fixture.
    m_warning->hide();
    if (m_contextScene != NULL)
    {
        if (p->type() == QLCPalette::Gobo
            && targetsHaveChannelGroup(QLCChannel::Gobo) == false)
        {
            m_warning->setText(tr("⚠ No target fixture has a gobo channel."));
            m_warning->show();
        }
        else if (p->type() == QLCPalette::Shutter
                 && targetsHaveChannelGroup(QLCChannel::Shutter) == false)
        {
            m_warning->setText(tr("⚠ No target fixture has a shutter channel."));
            m_warning->show();
        }
    }

    m_loading = false;
}

void LookEditor::setContextScene(Scene *scene)
{
    m_contextScene = scene;
    // Re-evaluate the applicability warning for the current look.
    if (m_paletteId != QLCPalette::invalidId())
        setPalette(m_paletteId);
}

bool LookEditor::targetsHaveChannelGroup(int group) const
{
    if (m_contextScene == NULL)
        return true; // unknown context -> don't warn

    QSet<quint32> fixtures;
    foreach (quint32 fid, m_contextScene->fixtures())
        fixtures.insert(fid);
    foreach (quint32 gid, m_contextScene->fixtureGroups())
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g != NULL)
            foreach (quint32 fid, g->fixtureList())
                fixtures.insert(fid);
    }

    foreach (quint32 fid, fixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == NULL)
            continue;
        for (quint32 i = 0; i < f->channels(); i++)
        {
            const QLCChannel *ch = f->channel(i);
            if (ch != NULL && int(ch->group()) == group)
                return true;
        }
    }
    return false;
}

void LookEditor::slotColorChanged(const QColor &c)
{
    if (m_loading)
        return;
    // Auto-derive White additively (W = min(R,G,B), RGB kept) on colour change.
    const int autoW = qMin(c.red(), qMin(c.green(), c.blue()));
    m_whiteSlider->blockSignals(true);
    m_whiteSlider->setValue(autoW);
    m_whiteSlider->blockSignals(false);
    commitColor();
}

void LookEditor::slotColorExtraChanged()
{
    if (m_loading)
        return;
    commitColor();
}

void LookEditor::commitColor()
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::Color)
        return;
    const QColor rgb = m_colorDialog->currentColor();
    // wauv encodes White=red, Amber=green, UV=blue (see QLCPalette::Color).
    const QColor wauv(m_whiteSlider->value(), m_amberSlider->value(), m_uvSlider->value());
    p->setValue(QLCPalette::colorToString(rgb, wauv));
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}

bool LookEditor::targetsHaveColour(int primaryColour) const
{
    if (m_contextScene == NULL)
        return false;

    QSet<quint32> fixtures;
    foreach (quint32 fid, m_contextScene->fixtures())
        fixtures.insert(fid);
    foreach (quint32 gid, m_contextScene->fixtureGroups())
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g != NULL)
            foreach (quint32 fid, g->fixtureList())
                fixtures.insert(fid);
    }

    foreach (quint32 fid, fixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f != NULL
            && f->channelNumber(primaryColour, QLCChannel::MSB) != QLCChannel::invalid())
            return true;
    }
    return false;
}

void LookEditor::slotDimmerChanged(int v)
{
    m_dimmerValue->setText(QString::number(v));
    m_dimmerCap->setText(capabilityNameAt(
        representativeChannel(QLCChannel::Intensity), v));
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
    updateSinglePreview(v);
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL)
        return;
    p->setValue(v);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}

void LookEditor::updateSinglePreview(int v)
{
    // Find the Picture-preset capability covering v and show its image.
    QPixmap px;
    if (m_singleChannel != NULL)
    {
        foreach (QLCCapability *cap, m_singleChannel->capabilities())
        {
            if (v >= cap->min() && v <= cap->max())
            {
                if (cap->presetType() == QLCCapability::Picture)
                    px = QPixmap(cap->resource(0).toString());
                break;
            }
        }
    }

    if (px.isNull())
    {
        m_singlePreview->clear();
        m_singlePreview->setVisible(false);
    }
    else
    {
        m_singlePreview->setPixmap(px.scaled(m_singlePreview->size(),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_singlePreview->setVisible(true);
    }
}

void LookEditor::slotSingleCapabilityPicked(int index)
{
    if (index < 0)
        return;
    // Set the slider to the picked capability's mid value; that drives the
    // palette update via slotSingleValueChanged().
    m_singleSlider->setValue(m_singleCombo->itemData(index).toInt());
}

const QLCChannel *LookEditor::representativeChannel(int group) const
{
    if (m_contextScene == NULL)
        return NULL;

    QSet<quint32> fixtures;
    foreach (quint32 fid, m_contextScene->fixtures())
        fixtures.insert(fid);
    foreach (quint32 gid, m_contextScene->fixtureGroups())
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g != NULL)
            foreach (quint32 fid, g->fixtureList())
                fixtures.insert(fid);
    }

    foreach (quint32 fid, fixtures)
    {
        Fixture *f = m_doc->fixture(fid);
        if (f == NULL)
            continue;
        for (quint32 i = 0; i < f->channels(); i++)
        {
            const QLCChannel *ch = f->channel(i);
            if (ch != NULL && int(ch->group()) == group)
                return ch;
        }
    }
    return NULL;
}

QString LookEditor::capabilityNameAt(const QLCChannel *ch, int v) const
{
    if (ch == NULL)
        return QString();
    foreach (QLCCapability *cap, ch->capabilities())
        if (v >= cap->min() && v <= cap->max())
            return cap->name();
    return QString();
}
