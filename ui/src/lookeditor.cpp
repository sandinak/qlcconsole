/*
  Q Light Controller Plus
  lookeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QStackedWidget>
#include <QColorDialog>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QPixmap>
#include <QIcon>
#include <QSet>
#include <QToolButton>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QMenu>
#include <functional>

#include "lookeditor.h"
#include "pathdrawwidget.h"
#include "virtualconsole/vcxypadarea.h"
#include "capabilitybar.h"
#include "qlcpalette.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#include "effectscriptrunner.h"
#include "effectscript.h"
#include "effectscriptcache.h"
#include "inputpatch.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"

// VC XY pad works in DMX space [0..256); palette pan/tilt are degrees.
static const qreal XY_MAX = 256.0;
static const int PAN_DEG = 540;
static const int TILT_DEG = 270;

static const char *PALETTE_DROP_MIME = "application/x-qlcplus-palettes";

/** Drop zone widget for binding an effect palette slot.
 *  Accepts drag-drop from the palette tree AND click-to-pick (left-click
 *  opens a menu of all palettes whose type matches m_slotType).
 *  Right-click → Clear. */
class PaletteDropZone : public QLabel
{
public:
    PaletteDropZone(const QString &slotName, Doc *doc,
                    std::function<void(const QString&, quint32)> onChange,
                    QWidget *parent = nullptr)
        : QLabel(parent), m_slotName(slotName), m_doc(doc)
        , m_onChange(onChange), m_paletteId(QLCPalette::invalidId())
    {
        setAcceptDrops(true);
        setFrameShape(QFrame::StyledPanel);
        setLineWidth(1);
        setMargin(3);
        setMinimumHeight(24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setContextMenuPolicy(Qt::CustomContextMenu);
        connect(this, &QLabel::customContextMenuRequested, [this](const QPoint &pos) {
            QMenu menu(this);
            QAction *clr = menu.addAction(tr("Clear"));
            if (menu.exec(mapToGlobal(pos)) == clr)
                setPaletteId(QLCPalette::invalidId());
        });
        updateDisplay();
    }

    /** Restrict the pick-menu to palettes of this type (QLCPalette type string). */
    void setSlotType(QLCPalette::PaletteType t) { m_slotType = t; }

    // suppressCallback: pass true when called programmatically during widget
    // construction so the onChange lambda isn't fired as if the user changed
    // the binding.
    void setPaletteId(quint32 pid, bool suppressCallback = false)
    {
        m_paletteId = pid;
        updateDisplay();
        if (!suppressCallback && m_onChange) m_onChange(m_slotName, pid);
    }

    quint32 paletteId() const { return m_paletteId; }

protected:
    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton) { QLabel::mousePressEvent(ev); return; }
        showPickMenu(ev->globalPos());
    }

    void dragEnterEvent(QDragEnterEvent *ev) override
    {
        if (ev->mimeData()->hasFormat(QLatin1String(PALETTE_DROP_MIME)))
        {
            setStyleSheet("QLabel { border: 2px solid palette(highlight); padding: 2px; color: palette(text); }");
            ev->acceptProposedAction();
        }
        else ev->ignore();
    }

    void dragLeaveEvent(QDragLeaveEvent *) override { updateDisplay(); }

    void dropEvent(QDropEvent *ev) override
    {
        const QByteArray data = ev->mimeData()->data(QLatin1String(PALETTE_DROP_MIME));
        QDataStream stream(data);
        quint32 pid;
        stream >> pid;
        if (pid != QLCPalette::invalidId())
            setPaletteId(pid);
        ev->acceptProposedAction();
    }

private:
    void showPickMenu(const QPoint &globalPos)
    {
        QMenu menu(this);
        // Show all palettes matching the slot type (or all if type is Undefined)
        bool any = false;
        for (QLCPalette *p : m_doc->palettes())
        {
            if (!p) continue;
            if (m_slotType != QLCPalette::Undefined && p->type() != m_slotType) continue;
            QAction *act = menu.addAction(p->name());
            act->setData(p->id());
            if (p->id() == m_paletteId) { act->setCheckable(true); act->setChecked(true); }
            any = true;
        }
        if (!any)
            menu.addAction(tr("(no palettes available)"))->setEnabled(false);
        if (m_paletteId != QLCPalette::invalidId())
        {
            menu.addSeparator();
            QAction *clr = menu.addAction(tr("Clear"));
            clr->setData(QLCPalette::invalidId());
        }
        QAction *chosen = menu.exec(globalPos);
        if (chosen && chosen->isEnabled())
            setPaletteId(chosen->data().toUInt());
    }

    void updateDisplay()
    {
        if (m_paletteId == QLCPalette::invalidId())
        {
            setText(tr("  click or drag palette here ▾"));
            setStyleSheet("QLabel { color: #888; border: 1px dashed #888; padding: 2px; }");
        }
        else
        {
            QLCPalette *pal = m_doc->palette(m_paletteId);
            setText(pal ? QString("  ") + pal->name() + QString("  ▾")
                        : tr("  (missing palette)"));
            setStyleSheet("QLabel { border: 1px solid #555; padding: 2px; color: palette(text); }");
        }
    }

    QString m_slotName;
    Doc *m_doc;
    std::function<void(const QString&, quint32)> m_onChange;
    quint32 m_paletteId = QLCPalette::invalidId();
    QLCPalette::PaletteType m_slotType = QLCPalette::Undefined;
};

LookEditor::LookEditor(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_contextScene(NULL)
    , m_paletteId(QLCPalette::invalidId())
    , m_loading(false)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Name row: "Name:" label + editable QLineEdit
    QHBoxLayout *nameRow = new QHBoxLayout();
    nameRow->setContentsMargins(0, 0, 0, 0);
    QLabel *nameLbl = new QLabel(tr("Name:"), this);
    nameLbl->setMinimumWidth(44);
    nameRow->addWidget(nameLbl);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("palette name"));
    m_nameEdit->setEnabled(false);
    nameRow->addWidget(m_nameEdit, 1);
    root->addLayout(nameRow);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &LookEditor::slotNameEdited);

    // Info line: "used by N scene(s)" — updated when a palette is selected
    m_title = new QLabel(tr("Look editor"), this);
    m_title->setStyleSheet("color: palette(mid); font-size: small;");
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

    // Pan/Tilt page (X/Y grid + named position presets + stage target)
    QWidget *pantilt = new QWidget(this);
    QVBoxLayout *ptv = new QVBoxLayout(pantilt);
    ptv->setContentsMargins(6, 6, 6, 6);
    ptv->setSpacing(6);

    // Top row: XY pad + preset buttons
    QHBoxLayout *pl = new QHBoxLayout();
    pl->setContentsMargins(0, 0, 0, 0);
    m_xyPad = new VCXYPadArea(pantilt);
    m_xyPad->setMode(Doc::Operate); // interactive
    m_xyPad->setMinimumSize(200, 200);
    m_xyPad->setMaximumSize(280, 280); // keep the bottom look pane compact
    m_xyPad->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    pl->addWidget(m_xyPad, 0, Qt::AlignLeft | Qt::AlignTop);

    // Named position preset buttons (pan°, tilt°)
    // Degrees are in the palette's full-range convention:
    //   Pan  0-540°  (center = 270)
    //   Tilt 0-270°  (center = 135; up = low values, down = high)
    struct { const char *label; int pan; int tilt; } presets[] = {
        { "Home",   270, 135 },   // straight centre
        { "Down",   270, 270 },   // full tilt down
        { "Front",  270, 180 },   // ~45° forward tilt
        { "Left",     0, 135 },   // full pan left
        { "Right",  540, 135 },   // full pan right
    };
    QVBoxLayout *presetCol = new QVBoxLayout();
    presetCol->setSpacing(4);
    for (uint i = 0; i < sizeof(presets) / sizeof(presets[0]); i++)
    {
        QToolButton *btn = new QToolButton(pantilt);
        btn->setText(tr(presets[i].label));
        btn->setMinimumWidth(60);
        int pan = presets[i].pan, tilt = presets[i].tilt;
        connect(btn, &QToolButton::clicked, this, [this, pan, tilt]() {
            const QPointF pt(qreal(pan) / PAN_DEG * XY_MAX,
                             qreal(tilt) / TILT_DEG * XY_MAX);
            m_xyPad->blockSignals(true);
            m_xyPad->setPosition(pt);
            m_xyPad->blockSignals(false);
            m_xyPad->update();
            slotPanTiltChanged(pt);
        });
        presetCol->addWidget(btn);
    }
    presetCol->addStretch(1);
    pl->addLayout(presetCol);
    pl->addStretch(1);
    ptv->addLayout(pl);

    ptv->addStretch(1);

    m_pagePanTilt = m_stack->addWidget(pantilt);
    connect(m_xyPad, SIGNAL(positionChanged(QPointF)),
            this, SLOT(slotPanTiltChanged(QPointF)));

    // Aim page: target picker (no XY pad — position computed from rig geometry)
    QWidget *aim = new QWidget(this);
    QVBoxLayout *av = new QVBoxLayout(aim);
    av->setContentsMargins(6, 6, 6, 6);
    av->setSpacing(6);
    QHBoxLayout *aimRow = new QHBoxLayout();
    aimRow->setContentsMargins(0, 0, 0, 0);
    QLabel *aimLbl = new QLabel(tr("Aim at:"), aim);
    aimLbl->setMinimumWidth(60);
    aimRow->addWidget(aimLbl);
    m_aimTargetCombo = new QComboBox(aim);
    m_aimTargetCombo->setToolTip(tr("Stage target this palette aims at (rig geometry computes per-fixture pan/tilt)"));
    aimRow->addWidget(m_aimTargetCombo, 1);
    av->addLayout(aimRow);
    av->addStretch(1);
    m_pageAim = m_stack->addWidget(aim);
    connect(m_aimTargetCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotAimTargetChanged(int)));

    // Beam page: Focus / Frost / Iris sliders
    QWidget *beam = new QWidget(this);
    QVBoxLayout *bv = new QVBoxLayout(beam);
    bv->setContentsMargins(6, 6, 6, 6);
    auto makeBeamRow = [&](const QString &label, QSlider *&slider, QLabel *&val) {
        QHBoxLayout *row = new QHBoxLayout();
        bv->addLayout(row);
        QLabel *lbl = new QLabel(label, beam);
        lbl->setMinimumWidth(46);
        row->addWidget(lbl);
        slider = new QSlider(Qt::Horizontal, beam);
        slider->setRange(0, 255);
        row->addWidget(slider, 1);
        val = new QLabel("0", beam);
        val->setMinimumWidth(32);
        row->addWidget(val);
    };
    makeBeamRow(tr("Focus"), m_beamFocusSlider, m_beamFocusValue);
    makeBeamRow(tr("Frost"),  m_beamFrostSlider, m_beamFrostValue);
    makeBeamRow(tr("Iris"),   m_beamIrisSlider,  m_beamIrisValue);
    bv->addStretch(1);
    m_pageBeam = m_stack->addWidget(beam);
    connect(m_beamFocusSlider, &QSlider::valueChanged, this, &LookEditor::slotBeamChanged);
    connect(m_beamFrostSlider, &QSlider::valueChanged, this, &LookEditor::slotBeamChanged);
    connect(m_beamIrisSlider,  &QSlider::valueChanged, this, &LookEditor::slotBeamChanged);

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

    // Effect page
    QWidget *effectPage = new QWidget(this);
    QVBoxLayout *efv = new QVBoxLayout(effectPage);
    efv->setContentsMargins(6, 6, 6, 6);
    efv->setSpacing(4);

    // Script selector row
    QHBoxLayout *efScriptRow = new QHBoxLayout();
    efScriptRow->addWidget(new QLabel(tr("Script:"), effectPage));
    m_effectScriptCombo = new QComboBox(effectPage);
    m_effectScriptCombo->setToolTip(tr("Effect script to run for this look"));
    efScriptRow->addWidget(m_effectScriptCombo, 1);
    efv->addLayout(efScriptRow);

    // Short description label (one line, shown below combo)
    m_effectDescLabel = new QLabel(effectPage);
    m_effectDescLabel->setWordWrap(true);
    m_effectDescLabel->setStyleSheet("color: palette(mid); font-style: italic;");
    m_effectDescLabel->hide();
    efv->addWidget(m_effectDescLabel);

    // Notes label (longer paragraph, shown below description)
    m_effectNotesLabel = new QLabel(effectPage);
    m_effectNotesLabel->setWordWrap(true);
    m_effectNotesLabel->setTextFormat(Qt::PlainText);
    m_effectNotesLabel->hide();
    efv->addWidget(m_effectNotesLabel);

    // Fixture-types label (e.g. "Works with: Moving Head, RGB/LED")
    m_effectTypesLabel = new QLabel(effectPage);
    m_effectTypesLabel->setWordWrap(false);
    m_effectTypesLabel->hide();
    efv->addWidget(m_effectTypesLabel);

    // Dynamic area (params + palette/target bindings) — show in full, no scrolling.
    m_effectDynWidget = new QWidget(effectPage);
    efv->addWidget(m_effectDynWidget);
    efv->addStretch(1);

    m_pageEffect = m_stack->addWidget(effectPage);
    connect(m_effectScriptCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotEffectScriptChanged(int)));

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
        m_nameEdit->clear();
        m_nameEdit->setEnabled(false);
        m_title->setText(tr("Select a look to edit it"));
        m_stack->setCurrentIndex(m_pageEmpty);
        setMaximumHeight(340);
        return;
    }

    m_loading = true;

    // Populate the name editor
    m_nameEdit->blockSignals(true);
    m_nameEdit->setText(p->name());
    m_nameEdit->setEnabled(true);
    m_nameEdit->blockSignals(false);

    // Show folder path + usage count as a small info line
    QString folder = p->path();
    if (folder.startsWith(QStringLiteral("Palettes/")))
        folder = folder.mid(9);
    else if (folder == QStringLiteral("Palettes"))
        folder.clear();
    if (folder.endsWith('/'))
        folder.chop(1);

    int uses = 0;
    foreach (Function *fn, m_doc->functions())
    {
        Scene *s = qobject_cast<Scene*>(fn);
        if (s != NULL && s->palettes().contains(paletteId))
            uses++;
    }
    QString info = folder.isEmpty() ? QString() : folder + "  •  ";
    info += tr("used by %n scene(s)", "", uses);
    m_title->setText(info);

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
        setMaximumHeight(340);
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
        setMaximumHeight(340);
        break;
    }
    case QLCPalette::PanTilt:
    {
        const qreal x = qreal(p->intValue1()) / PAN_DEG * XY_MAX;
        const qreal y = qreal(p->intValue2()) / TILT_DEG * XY_MAX;
        m_xyPad->setPosition(QPointF(x, y));
        m_xyPad->setEnabled(true);
        m_stack->setCurrentIndex(m_pagePanTilt);
        setMaximumHeight(340);
        break;
    }
    case QLCPalette::Aim:
    {
        // Rebuild target combo from current MonitorProperties.
        m_aimTargetCombo->blockSignals(true);
        m_aimTargetCombo->clear();
        m_aimTargetCombo->addItem(tr("(none)"), QVariant(StageTarget::invalidId()));
        if (m_doc->monitorProperties())
        {
            for (StageTarget *t : m_doc->monitorProperties()->stageTargets())
                m_aimTargetCombo->addItem(t->name(), QVariant(t->id()));
        }
        // Pre-select the palette's linked target
        int sel = 0;
        for (int i = 0; i < m_aimTargetCombo->count(); ++i)
        {
            if (m_aimTargetCombo->itemData(i).toUInt() == p->stageTargetId())
            { sel = i; break; }
        }
        m_aimTargetCombo->setCurrentIndex(sel);
        m_aimTargetCombo->blockSignals(false);
        m_stack->setCurrentIndex(m_pageAim);
        setMaximumHeight(340);
        break;
    }
    case QLCPalette::Beam:
    {
        QVariantList bv = p->values();
        m_beamFocusSlider->setValue(bv.count() > 0 ? bv.at(0).toInt() : 0);
        m_beamFrostSlider->setValue(bv.count() > 1 ? bv.at(1).toInt() : 0);
        m_beamIrisSlider ->setValue(bv.count() > 2 ? bv.at(2).toInt() : 0);
        m_beamFocusValue->setText(QString::number(m_beamFocusSlider->value()));
        m_beamFrostValue->setText(QString::number(m_beamFrostSlider->value()));
        m_beamIrisValue ->setText(QString::number(m_beamIrisSlider->value()));
        m_stack->setCurrentIndex(m_pageBeam);
        setMaximumHeight(340);
        break;
    }
    case QLCPalette::Effect:
    {
        // Populate script combo from the runner's cache
        m_effectScriptCombo->blockSignals(true);
        m_effectScriptCombo->clear();
        m_effectScriptCombo->addItem(tr("(none)"), QString());
        if (m_doc->effectScriptRunner())
        {
            const EffectScriptCache *cache = m_doc->effectScriptRunner()->cache();
            const QStringList names = cache->scriptNames();
            for (const QString &n : names)
            {
                m_effectScriptCombo->addItem(n, n);
                const QString desc = cache->scriptMeta(n).description;
                if (!desc.isEmpty())
                    m_effectScriptCombo->setItemData(
                        m_effectScriptCombo->count() - 1, desc, Qt::ToolTipRole);
            }
        }
        // Select current script — find the combo entry whose cache path matches
        // the palette's stored path (combo data = display name, not path).
        int scriptIdx = 0;
        const QString curPath = p->scriptPath();
        if (!curPath.isEmpty() && m_doc->effectScriptRunner())
        {
            const EffectScriptCache *cache = m_doc->effectScriptRunner()->cache();
            for (const QString &n : cache->scriptNames())
            {
                if (cache->scriptPath(n) == curPath)
                {
                    for (int i = 0; i < m_effectScriptCombo->count(); ++i)
                    {
                        if (m_effectScriptCombo->itemData(i).toString() == n)
                        { scriptIdx = i; break; }
                    }
                    break;
                }
            }
        }
        m_effectScriptCombo->setCurrentIndex(scriptIdx);
        m_effectScriptCombo->blockSignals(false);

        rebuildEffectDynWidget();
        m_stack->setCurrentIndex(m_pageEffect);
        setMaximumHeight(QWIDGETSIZE_MAX);
        updateGeometry();
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
        setMaximumHeight(340);
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

void LookEditor::maybeAutoName(QLCPalette *p)
{
    if (!p) return;
    const QString cur = p->name().trimmed();
    // Only overwrite factory-default name ("New Color", "New Dimmer", etc.)
    const QString expected = QString("New %1").arg(QLCPalette::typeToString(p->type()));
    if (cur != expected && !cur.isEmpty()) return;

    QString autoName;
    switch (p->type())
    {
    case QLCPalette::Color:
    {
        QColor c = p->rgbValue();
        struct { int r, g, b; const char *name; } table[] = {
            { 255,   0,   0, "Red" },
            {   0, 255,   0, "Green" },
            {   0,   0, 255, "Blue" },
            { 255, 255,   0, "Yellow" },
            { 255, 165,   0, "Orange" },
            { 128,   0, 128, "Purple" },
            { 255,   0, 255, "Magenta" },
            {   0, 255, 255, "Cyan" },
            { 255, 255, 255, "White" },
            { 255, 200, 100, "Warm White" },
            {   0,   0,   0, "Black" },
        };
        int bestDist = INT_MAX;
        const char *bestName = nullptr;
        for (auto &e : table)
        {
            int dr = c.red()   - e.r;
            int dg = c.green() - e.g;
            int db = c.blue()  - e.b;
            int dist = dr*dr + dg*dg + db*db;
            if (dist < bestDist) { bestDist = dist; bestName = e.name; }
        }
        if (bestName) autoName = bestName;
        break;
    }
    case QLCPalette::Dimmer:
    {
        int v = p->intValue1();
        if (v >= 255)     autoName = tr("Full");
        else if (v <= 0)  autoName = tr("Off");
        else              autoName = tr("Intensity %1%").arg(qRound(v * 100.0 / 255.0));
        break;
    }
    case QLCPalette::Effect:
    {
        const QString path = p->scriptPath();
        if (!path.isEmpty() && m_doc->effectScriptRunner())
        {
            const EffectScriptCache *cache = m_doc->effectScriptRunner()->cache();
            const QString n = cache->nameFromPath(path);
            if (!n.isEmpty()) autoName = n;
        }
        break;
    }
    default: return;
    }

    if (autoName.isEmpty()) return;
    p->setName(autoName);
    m_nameEdit->blockSignals(true);
    m_nameEdit->setText(autoName);
    m_nameEdit->blockSignals(false);
}

void LookEditor::slotNameEdited()
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p)
        return;
    const QString newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty() || newName == p->name())
        return;
    p->setName(newName);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
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
    maybeAutoName(p);
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
    maybeAutoName(p);
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

void LookEditor::slotAimTargetChanged(int index)
{
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::Aim)
        return;
    p->setStageTargetId(m_aimTargetCombo->itemData(index).toUInt());
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

void LookEditor::slotBeamChanged(int)
{
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL)
        return;

    int focus = m_beamFocusSlider->value();
    int frost  = m_beamFrostSlider->value();
    int iris   = m_beamIrisSlider->value();
    m_beamFocusValue->setText(QString::number(focus));
    m_beamFrostValue->setText(QString::number(frost));
    m_beamIrisValue ->setText(QString::number(iris));

    QVariantList vals;
    vals << focus << frost << iris;
    p->setValues(vals);
    emit paletteChanged(m_paletteId);
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

// ---------------------------------------------------------------------------
// Effect page
// ---------------------------------------------------------------------------

void LookEditor::rebuildEffectDynWidget()
{
    // Block slider valueChanged → slotEffectParamChanged during the entire
    // rebuild, including any re-entrant emission paths.
    m_loading = true;

    // Clear existing layout and ALL child widgets.  Nested QFormLayouts added
    // via dv->addLayout(pform) leave their row widgets as direct children of
    // m_effectDynWidget; iterating only top-level layout items misses them and
    // they repaint as ghost text on top of the new content.
    if (QLayout *old = m_effectDynWidget->layout())
    {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr)
        {
            if (item->widget()) delete item->widget();
            delete item;
        }
        delete old;
    }
    // Delete any widgets left behind by nested layouts.
    const auto orphans = m_effectDynWidget->findChildren<QWidget*>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *w : orphans) delete w;

    QVBoxLayout *dv = new QVBoxLayout(m_effectDynWidget);
    dv->setContentsMargins(0, 4, 0, 4);
    dv->setSpacing(6);

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect)
    {
        m_loading = false;
        return;
    }

    // Determine the selected script path from the combo
    const QString scriptName = m_effectScriptCombo->currentData().toString();
    QString scriptPath;
    if (!scriptName.isEmpty() && m_doc->effectScriptRunner())
        scriptPath = m_doc->effectScriptRunner()->cache()->scriptPath(scriptName);

    if (scriptPath.isEmpty())
    {
        m_effectDescLabel->hide();
        m_effectNotesLabel->hide();
        m_effectTypesLabel->hide();
        dv->addWidget(new QLabel(tr("(select a script above to configure it)"), m_effectDynWidget));
        dv->addStretch(1);
        m_loading = false;
        return;
    }

    // Load the script to get meta (params, inputs, palettes)
    EffectScript tmpScript;
    if (!tmpScript.load(scriptPath))
    {
        m_effectDescLabel->hide();
        m_effectNotesLabel->hide();
        m_effectTypesLabel->hide();
        dv->addWidget(new QLabel(tr("Error loading script — check the .js file."), m_effectDynWidget));
        dv->addStretch(1);
        m_loading = false;
        return;
    }

    // Populate info labels
    const QString desc  = tmpScript.description();
    const QString notes = tmpScript.notes();
    if (!desc.isEmpty())
    {
        m_effectDescLabel->setText(desc);
        m_effectDescLabel->show();
    }
    else
    {
        m_effectDescLabel->hide();
    }
    if (!notes.isEmpty())
    {
        m_effectNotesLabel->setText(notes);
        m_effectNotesLabel->show();
    }
    else
    {
        m_effectNotesLabel->hide();
    }

    // Fixture types label
    const QStringList ftypes = tmpScript.fixtureTypes();
    if (!ftypes.isEmpty())
    {
        QMap<QString, QString> ftLabels;
        ftLabels["moving"]  = tr("Moving Head");
        ftLabels["rgb"]     = tr("RGB/LED Wash");
        ftLabels["dimmer"]  = tr("Dimmer");
        ftLabels["shutter"] = tr("Strobe/Shutter");
        QStringList human;
        for (const QString &t : ftypes)
            human << ftLabels.value(t, t);
        m_effectTypesLabel->setText(tr("<b>Works with:</b> %1").arg(human.join(", ")));
        m_effectTypesLabel->setTextFormat(Qt::RichText);
        m_effectTypesLabel->show();
    }
    else
    {
        m_effectTypesLabel->hide();
    }

    // --- Palette bindings ---
    const QList<EffectScript::PaletteDef> &paldefs = tmpScript.paletteDefs();
    if (!paldefs.isEmpty())
    {
        QLabel *hdrP = new QLabel(tr("Palette Bindings"), m_effectDynWidget);
        hdrP->setStyleSheet("font-weight: bold;");
        dv->addWidget(hdrP);

        // Explain the fallthrough behaviour so the programmer knows what unbound slots do.
        QLabel *hintP = new QLabel(
            tr("Drag color palettes from the palette list. "
               "Unbound slots fall through to the look's active palette set — "
               "colors are then distributed across fixtures automatically."),
            m_effectDynWidget);
        hintP->setWordWrap(true);
        hintP->setStyleSheet("color: #999; font-size: 10px;");
        dv->addWidget(hintP);

        QFormLayout *pform = new QFormLayout();
        pform->setContentsMargins(0, 0, 0, 0);
        pform->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        const QMap<QString, quint32> &palBinds = p->effectPaletteBindings();

        for (const EffectScript::PaletteDef &pd : paldefs)
        {
            quint32 bound = palBinds.value(pd.name, QLCPalette::invalidId());

            // Slot name + type as the row label; description goes on the tooltip
            const QString rowLabel = pd.name;

            auto *zone = new PaletteDropZone(
                pd.name, m_doc,
                [this](const QString &slot, quint32 pid) {
                    QLCPalette *pal = m_doc->palette(m_paletteId);
                    if (!pal || pal->type() != QLCPalette::Effect) return;
                    if (pid == QLCPalette::invalidId())
                        pal->clearEffectPaletteBinding(slot);
                    else
                        pal->setEffectPaletteBinding(slot, pid);
                    m_doc->setModified();
                    emit paletteValueChanged(m_paletteId);
                },
                m_effectDynWidget);
            zone->setSlotType(QLCPalette::stringToType(pd.type));
            zone->setPaletteId(bound, /*suppressCallback=*/true);
            const QString tip = pd.description.isEmpty()
                                ? tr("Type: %1").arg(pd.type)
                                : QString("%1 (type: %2)").arg(pd.description).arg(pd.type);
            zone->setToolTip(tip);
            pform->addRow(rowLabel, zone);
        }
        dv->addLayout(pform);
    }

    // --- Target bindings ---
    const QList<EffectScript::TargetDef> &tgtdefs = tmpScript.targetDefs();
    if (!tgtdefs.isEmpty())
    {
        const MonitorProperties *mp = m_doc->monitorProperties();
        QLabel *hdrT = new QLabel(tr("Target Bindings"), m_effectDynWidget);
        hdrT->setStyleSheet("font-weight: bold;");
        dv->addWidget(hdrT);

        QFormLayout *tform = new QFormLayout();
        tform->setContentsMargins(0, 0, 0, 0);
        const QMap<QString, quint32> &tgtBinds = p->effectTargetBindings();

        for (const EffectScript::TargetDef &td : tgtdefs)
        {
            QComboBox *cb = new QComboBox(m_effectDynWidget);
            cb->addItem(tr("(none)"), QVariant(StageTarget::invalidId()));

            int selIdx = 0;
            quint32 bound = tgtBinds.value(td.name, StageTarget::invalidId());
            if (mp)
            {
                for (StageTarget *tgt : mp->stageTargets())
                {
                    cb->addItem(tgt->name(), tgt->id());
                    if (tgt->id() == bound) selIdx = cb->count() - 1;
                }
            }
            cb->setCurrentIndex(selIdx);
            cb->setToolTip(td.description);
            cb->setProperty("targetSlot", td.name);
            connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &LookEditor::slotEffectTargetBindingChanged);
            tform->addRow(td.name, cb);
        }
        dv->addLayout(tform);
    }

    // --- Parameters ---
    const QList<EffectScript::ParamDef> &pdefs = tmpScript.paramDefs();
    if (!pdefs.isEmpty())
    {
        QLabel *hdr = new QLabel(tr("Parameters"), m_effectDynWidget);
        hdr->setStyleSheet("font-weight: bold;");
        dv->addWidget(hdr);

        QFormLayout *form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        for (const EffectScript::ParamDef &pd : pdefs)
        {
            double cur = p->effectParamValues().value(pd.name, pd.defaultValue);

            if (pd.type == QLatin1String("path"))
            {
                // XY drawn path param → PathDrawWidget
                PathDrawWidget *pdw = new PathDrawWidget(m_effectDynWidget);
                const QString existing = p->effectStringParam(pd.name);
                if (!existing.isEmpty())
                    pdw->setPath(existing);
                const QString paramName = pd.name;
                connect(pdw, &PathDrawWidget::pathChanged,
                        this, [this, paramName](const QString &json) {
                    QLCPalette *pp = m_doc->palette(m_paletteId);
                    if (!pp) return;
                    pp->setEffectStringParam(paramName, json);
                    m_doc->setModified();
                    emit paletteValueChanged(m_paletteId);
                });
                form->addRow(pd.name, pdw);
            }
            else if (!pd.enumValues.isEmpty())
            {
                // Enum param → dropdown
                QComboBox *cb = new QComboBox(m_effectDynWidget);
                for (const QString &v : pd.enumValues)
                    cb->addItem(v);
                int idx = qBound(0, int(cur), cb->count() - 1);
                cb->setCurrentIndex(idx);
                cb->setToolTip(pd.description);
                cb->setProperty("paramName", pd.name);
                connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &LookEditor::slotEffectEnumParamChanged);
                form->addRow(pd.name, cb);
            }
            else
            {
                // Scalar param → slider
                QWidget *row = new QWidget(m_effectDynWidget);
                QHBoxLayout *rl = new QHBoxLayout(row);
                rl->setContentsMargins(0, 0, 0, 0);

                QSlider *sl = new QSlider(Qt::Horizontal, row);
                sl->setRange(0, 1000);
                int slVal = (pd.max > pd.min)
                            ? int((cur - pd.min) / (pd.max - pd.min) * 1000) : 0;
                sl->setValue(qBound(0, slVal, 1000));
                sl->setProperty("paramName", pd.name);
                sl->setProperty("paramMin",  pd.min);
                sl->setProperty("paramMax",  pd.max);
                sl->setToolTip(pd.description);
                rl->addWidget(sl, 1);

                QLabel *val = new QLabel(QString::number(cur, 'f', 2), row);
                val->setMinimumWidth(40);
                val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                sl->setProperty("valueLabel", QVariant::fromValue((QObject*)val));
                rl->addWidget(val);

                connect(sl, SIGNAL(valueChanged(int)), this, SLOT(slotEffectParamChanged(int)));
                form->addRow(pd.name, row);
            }
        }
        dv->addLayout(form);
    }

    // --- Input bindings ---
    const QList<EffectScript::InputDef> &idefs = tmpScript.inputDefs();
    if (!idefs.isEmpty())
    {
        QLabel *hdr2 = new QLabel(tr("Input bindings"), m_effectDynWidget);
        hdr2->setStyleSheet("font-weight: bold;");
        dv->addWidget(hdr2);
        dv->addWidget(new QLabel(
            tr("Connect a MIDI/OSC/HID input channel to each slot.\n"
               "Click Bind then move the control to capture it."),
            m_effectDynWidget));

        QFormLayout *iform = new QFormLayout();
        iform->setContentsMargins(0, 0, 0, 0);
        const QMap<QString, QPair<quint32,quint32>> &binds = p->effectInputBindings();
        for (const EffectScript::InputDef &id : idefs)
        {
            QWidget *row = new QWidget(m_effectDynWidget);
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);

            const bool bound = binds.contains(id.name);
            QString bindText = tr("(unbound)");
            if (bound)
            {
                const quint32 bUniverse = binds.value(id.name).first;
                const quint32 bChannel  = binds.value(id.name).second;
                bindText = tr("Univ %1 Ch %2").arg(bUniverse + 1).arg(bChannel + 1);
                if (m_doc->inputOutputMap())
                {
                    InputPatch *ip = m_doc->inputOutputMap()->inputPatch(bUniverse);
                    if (ip && ip->plugin())
                    {
                        const QString chanName = ip->plugin()->inputChannelName(ip->input(), bChannel);
                        const QString devName  = ip->inputName();
                        if (!chanName.isEmpty() && !devName.isEmpty())
                            bindText = QString("%1 — %2").arg(devName).arg(chanName);
                        else if (!chanName.isEmpty())
                            bindText = chanName;
                        else if (!devName.isEmpty())
                            bindText = QString("%1  axis %2").arg(devName).arg(bChannel + 1);
                    }
                }
            }

            QLabel *info = new QLabel(bindText, row);
            info->setMinimumWidth(100);
            rl->addWidget(info, 1);

            QToolButton *bindBtn = new QToolButton(row);
            bindBtn->setText(tr("Bind"));
            bindBtn->setProperty("inputSlot",   id.name);
            bindBtn->setProperty("bindLabel",    QVariant::fromValue((QObject*)info));
            bindBtn->setToolTip(id.description);
            rl->addWidget(bindBtn);

            connect(bindBtn, SIGNAL(clicked()), this, SLOT(slotEffectBindInput()));

            iform->addRow(id.name, row);
        }
        dv->addLayout(iform);
    }

    dv->addStretch(1);

    m_effectDynWidget->setFocusPolicy(Qt::ClickFocus);
    m_effectDynWidget->setFocus(Qt::OtherFocusReason);
    m_effectDynWidget->updateGeometry();
    m_loading = false;
}

void LookEditor::slotEffectScriptChanged(int /*index*/)
{
    if (m_loading)
        return;

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect)
        return;

    const QString scriptName = m_effectScriptCombo->currentData().toString();
    if (scriptName.isEmpty())
    {
        p->setScriptPath(QString());
    }
    else if (m_doc->effectScriptRunner())
    {
        const QString path = m_doc->effectScriptRunner()->cache()->scriptPath(scriptName);
        p->setScriptPath(path);
    }
    // Wipe old param/input bindings since script changed
    p->clearEffectInputBindings();
    maybeAutoName(p);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);

    rebuildEffectDynWidget();
}

void LookEditor::slotEffectParamChanged(int value)
{
    if (m_loading)
        return;

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect)
        return;

    QSlider *sl = qobject_cast<QSlider*>(sender());
    if (!sl) return;

    const QString name   = sl->property("paramName").toString();
    const double  pmin   = sl->property("paramMin").toDouble();
    const double  pmax   = sl->property("paramMax").toDouble();
    const double  actual = pmin + (value / 1000.0) * (pmax - pmin);

    p->setEffectParamValue(name, actual);

    // Update the value label
    QLabel *lbl = qobject_cast<QLabel*>(
        sl->property("valueLabel").value<QObject*>());
    if (lbl)
        lbl->setText(QString::number(actual, 'f', 2));

    m_doc->setModified();
    emit paletteValueChanged(m_paletteId);
}

void LookEditor::slotEffectEnumParamChanged(int index)
{
    if (m_loading) return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect) return;
    QComboBox *cb = qobject_cast<QComboBox*>(sender());
    if (!cb) return;
    const QString name = cb->property("paramName").toString();
    p->setEffectParamValue(name, double(index));
    m_doc->setModified();
    emit paletteValueChanged(m_paletteId);
}

void LookEditor::slotEffectPaletteBindingChanged(int /*index*/)
{
    if (m_loading) return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect) return;
    QComboBox *cb = qobject_cast<QComboBox*>(sender());
    if (!cb) return;
    const QString slot = cb->property("paletteSlot").toString();
    quint32 pid = cb->currentData().toUInt();
    if (pid == QLCPalette::invalidId())
        p->clearEffectPaletteBinding(slot);
    else
        p->setEffectPaletteBinding(slot, pid);
    m_doc->setModified();
    emit paletteValueChanged(m_paletteId);
}

void LookEditor::slotEffectTargetBindingChanged(int /*index*/)
{
    if (m_loading) return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect) return;
    QComboBox *cb = qobject_cast<QComboBox*>(sender());
    if (!cb) return;
    const QString slot = cb->property("targetSlot").toString();
    quint32 tid = cb->currentData().toUInt();
    if (tid == StageTarget::invalidId())
        p->clearEffectTargetBinding(slot);
    else
        p->setEffectTargetBinding(slot, tid);
    m_doc->setModified();
    emit paletteValueChanged(m_paletteId);
}

void LookEditor::slotEffectBindInput()
{
    // Minimal implementation: show a message for now.
    // Full input-learn requires subscribing to InputOutputMap events.
    // TODO: implement learn mode like VCWidget::slotInputValueChanged
    QToolButton *btn = qobject_cast<QToolButton*>(sender());
    if (!btn) return;
    btn->setText(tr("(waiting for input…)"));
    // Actual binding stored when input event arrives — wired in full implementation.
}
