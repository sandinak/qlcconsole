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
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
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
#include "gradientdirectionwidget.h"
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
#include "effectpresetcache.h"
#include "effectscripteditor.h"
#include <QStandardItemModel>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include "inputpatch.h"
#include "inputoutputmap.h"
#include "qlcioplugin.h"

// VC XY pad works in DMX space [0..256); palette pan/tilt are degrees.
static const qreal XY_MAX = 256.0;
static const int PAN_DEG = 540;
static const int TILT_DEG = 270;

static const char *PALETTE_DROP_MIME = "application/x-qlcplus-palettes";

// Effect picker combo: each item is a category header, a raw engine script, or
// a preset. The kind is stored at EffectKindRole, the name at Qt::UserRole.
enum { EffectKindHeader = 0, EffectKindScript = 1, EffectKindPreset = 2 };
static const int EffectKindRole = Qt::UserRole + 1;

// Display order for the category groups in the effect picker.
static const char *kEffectCategoryOrder[] = { "Color", "Dimmer", "Position", "Beam", "Other" };

// Category for a RAW script from its fixtureTypes (shared with the engine cache
// so the picker grouping and the palette-folder pathing never drift apart).
static QString scriptCategory(const QStringList &ft)
{
    return EffectScriptCache::categoryForTypes(ft);
}

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

    // Per-look fade override row: "Fade in [ step ] s   out [ step ] s". Each
    // spin's minimum reads "step" — meaning fall back to the chaser step /
    // scene fade for that direction. Any value >= 0 overrides it for this look's
    // channels only, so colour can snap while movers glide (in) and a look can
    // release slower than it punches in (a "pulse": 0 in, slow out).
    auto makeFadeSpin = [this]() -> QDoubleSpinBox* {
        QDoubleSpinBox *sp = new QDoubleSpinBox(m_fadeRow);
        sp->setDecimals(2);
        sp->setSingleStep(0.1);
        sp->setRange(-0.1, 600.0);         // -0.1 = the special "step" value
        sp->setSpecialValueText(tr("step")); // shown when value == minimum
        sp->setSuffix(tr(" s"));
        connect(sp, SIGNAL(valueChanged(double)), this, SLOT(slotFadeTimeChanged()));
        return sp;
    };
    m_fadeRow = new QWidget(this);
    QHBoxLayout *fadeLay = new QHBoxLayout(m_fadeRow);
    fadeLay->setContentsMargins(0, 0, 0, 0);
    QLabel *fadeLbl = new QLabel(tr("Fade in"), m_fadeRow);
    fadeLbl->setToolTip(tr("Per-look fade time for this look's channels only.\n"
                           "0 = snap instantly; \"step\" (spin below 0) = follow "
                           "the chaser step / scene fade."));
    fadeLay->addWidget(fadeLbl);
    m_fadeInSpin = makeFadeSpin();
    fadeLay->addWidget(m_fadeInSpin);
    fadeLay->addSpacing(8);
    fadeLay->addWidget(new QLabel(tr("out"), m_fadeRow));
    m_fadeOutSpin = makeFadeSpin();
    fadeLay->addWidget(m_fadeOutSpin);
    fadeLay->addStretch(1);
    root->addWidget(m_fadeRow);
    m_fadeRow->hide();

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

    // Strobe page — single 0-100 rate slider; DMX resolution is per-fixture.
    QWidget *strobePage = new QWidget(this);
    QVBoxLayout *stv = new QVBoxLayout(strobePage);
    QHBoxLayout *stop = new QHBoxLayout();
    stv->addLayout(stop);
    stop->addWidget(new QLabel(tr("Strobe rate"), strobePage));
    stop->addStretch();
    m_strobeValue = new QLabel("0%", strobePage);
    m_strobeValue->setMinimumWidth(40);
    stop->addWidget(m_strobeValue);
    m_strobeSlider = new QSlider(Qt::Horizontal, strobePage);
    m_strobeSlider->setRange(0, 100);
    m_strobeSlider->setValue(0);
    stv->addWidget(m_strobeSlider);
    stv->addStretch();
    m_pageStrobe = m_stack->addWidget(strobePage);
    connect(m_strobeSlider, SIGNAL(valueChanged(int)),
            this, SLOT(slotStrobeChanged(int)));

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

    // Effect picker row (Effects grouped by category, each over a Generator)
    QHBoxLayout *efScriptRow = new QHBoxLayout();
    efScriptRow->addWidget(new QLabel(tr("Effect:"), effectPage));
    m_effectScriptCombo = new QComboBox(effectPage);
    m_effectScriptCombo->setToolTip(tr("Pick an Effect (or a raw Generator) for this look"));
    efScriptRow->addWidget(m_effectScriptCombo, 1);

    m_newScriptButton = new QPushButton(tr("New script…"), effectPage);
    m_newScriptButton->setToolTip(tr("Create a new effect script from a template and open it in the editor"));
    connect(m_newScriptButton, &QPushButton::clicked, this, &LookEditor::slotNewEffectScript);
    efScriptRow->addWidget(m_newScriptButton);

    m_editScriptButton = new QPushButton(tr("Edit script…"), effectPage);
    m_editScriptButton->setToolTip(tr("Edit this Generator's underlying .js script"));
    connect(m_editScriptButton, &QPushButton::clicked, this, &LookEditor::slotEditEffectScript);
    efScriptRow->addWidget(m_editScriptButton);

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

    // Keep the script's state across a scene change. Same idea (and the same
    // wording) as "Persist across restarts" on an RGB Matrix — see
    // QLCPalette::persistent().
    m_effectPersistentCb = new QCheckBox(tr("Persist across restarts"), effectPage);
    m_effectPersistentCb->setToolTip(
        tr("Keep this effect running where it left off when the scene using it stops "
           "and another scene with the SAME look starts, instead of restarting it from "
           "its first frame. Use for long, slowly-evolving effects that must survive a "
           "scene change."));
    efv->addWidget(m_effectPersistentCb);
    connect(m_effectPersistentCb, &QCheckBox::clicked, this, [this](bool checked) {
        QLCPalette *p = m_doc->palette(m_paletteId);
        if (p == NULL || p->type() != QLCPalette::Effect)
            return;
        p->setPersistent(checked);
        m_doc->setModified();
    });

    // Save the current Generator + settings as a reusable named Effect.
    QHBoxLayout *efSaveRow = new QHBoxLayout();
    efSaveRow->addStretch(1);
    m_importEffectButton = new QPushButton(tr("Import…"), effectPage);
    m_importEffectButton->setToolTip(
        tr("Import a shared Effect file (.qxfx) — installs its script if you don't have it"));
    efSaveRow->addWidget(m_importEffectButton);
    connect(m_importEffectButton, &QPushButton::clicked, this, &LookEditor::slotImportEffect);

    m_exportEffectButton = new QPushButton(tr("Export…"), effectPage);
    m_exportEffectButton->setToolTip(
        tr("Export this Effect to a portable .qxfx file (script + settings) to share"));
    efSaveRow->addWidget(m_exportEffectButton);
    connect(m_exportEffectButton, &QPushButton::clicked, this, &LookEditor::slotExportEffect);

    m_saveAsEffectButton = new QPushButton(tr("Save as Effect…"), effectPage);
    m_saveAsEffectButton->setToolTip(
        tr("Save this Generator + its current settings as a named Effect you can reuse"));
    efSaveRow->addWidget(m_saveAsEffectButton);
    efv->addLayout(efSaveRow);
    connect(m_saveAsEffectButton, &QPushButton::clicked, this, &LookEditor::slotSaveAsEffect);

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
        if (m_fadeRow) m_fadeRow->hide();
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
        // Populate the picker with PRESETS + raw scripts, grouped by category.
        m_effectScriptCombo->blockSignals(true);
        populateEffectPicker(p);
        m_effectScriptCombo->blockSignals(false);

        rebuildEffectDynWidget();
        m_stack->setCurrentIndex(m_pageEffect);
        setMaximumHeight(QWIDGETSIZE_MAX);
        updateGeometry();
        break;
    }
    case QLCPalette::Strobe:
    {
        int pct = qRound(p->value().toFloat() * 100.0f);
        m_strobeSlider->blockSignals(true);
        m_strobeSlider->setValue(pct);
        m_strobeSlider->blockSignals(false);
        m_strobeValue->setText(QString("%1%").arg(pct));
        m_stack->setCurrentIndex(m_pageStrobe);
        setMaximumHeight(340);
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

    // Per-look fade override: only meaningful with a context scene, and not
    // for Effect palettes (the EffectScriptRunner owns their timing, not the
    // scene's palette-fade path).
    if (m_contextScene != NULL && p->type() != QLCPalette::Effect)
    {
        const int inMs  = m_contextScene->paletteFadeIn(m_paletteId);
        const int outMs = m_contextScene->paletteFadeOut(m_paletteId);
        m_fadeInSpin->blockSignals(true);
        m_fadeOutSpin->blockSignals(true);
        // < 0 override -> the spin's special "step" minimum.
        m_fadeInSpin->setValue(inMs   >= 0 ? inMs  / 1000.0 : m_fadeInSpin->minimum());
        m_fadeOutSpin->setValue(outMs >= 0 ? outMs / 1000.0 : m_fadeOutSpin->minimum());
        m_fadeInSpin->blockSignals(false);
        m_fadeOutSpin->blockSignals(false);
        m_fadeRow->show();
    }
    else
    {
        m_fadeRow->hide();
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
        else if (p->type() == QLCPalette::Strobe
                 && targetsHaveChannelGroup(QLCChannel::Shutter) == false)
        {
            m_warning->setText(tr("⚠ No target fixture has a strobe-capable shutter channel."));
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
    case QLCPalette::Strobe:
    {
        int pct = qRound(p->value().toFloat() * 100.0f);
        if (pct <= 0)       autoName = tr("Strobe Off");
        else if (pct >= 100) autoName = tr("Strobe Max");
        else                 autoName = tr("Strobe %1%").arg(pct);
        break;
    }
    case QLCPalette::Effect:
    {
        // Prefer the preset's name ("Breathe") over the engine basename.
        if (!p->effectPreset().isEmpty()) { autoName = p->effectPreset(); break; }
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
    if (p->name() == autoName) return;      // no change → nothing to reveal
    p->setName(autoName);
    m_nameEdit->blockSignals(true);
    m_nameEdit->setText(autoName);
    m_nameEdit->blockSignals(false);
    emit paletteRenamed(p->id());
}

bool LookEditor::isAutoEffectPath(const QString &path) const
{
    QString p = path;
    if (p.endsWith('/')) p.chop(1);
    if (p.isEmpty() || p == QLatin1String("Palettes/Effect"))
        return true;   // the bare default

    // An auto path looks exactly like Palettes/Effect/<Category>/<Engine>, where
    // the category is one we use and the engine is a real script display name.
    // Anything else (e.g. a user folder "Palettes/Effect/My Faves") is left be.
    const QStringList seg = p.split('/');
    if (seg.size() != 4 || seg[0] != QLatin1String("Palettes")
        || seg[1] != QLatin1String("Effect"))
        return false;
    bool knownCat = false;
    for (const char *c : kEffectCategoryOrder)
        if (seg[2] == QLatin1String(c)) { knownCat = true; break; }
    if (!knownCat)
        return false;
    return m_doc->effectScriptRunner()
        && m_doc->effectScriptRunner()->cache()->scriptNames().contains(seg[3]);
}

void LookEditor::maybeAutoPath(QLCPalette *p, const QString &category, const QString &engine)
{
    if (!p || category.isEmpty() || engine.isEmpty())
        return;
    if (!isAutoEffectPath(p->path()))
        return;   // user organized this palette — don't clobber their folder
    p->setPath(QString("Palettes/Effect/%1/%2/").arg(category, engine));
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

void LookEditor::slotFadeTimeChanged()
{
    if (m_loading || m_contextScene == NULL || m_paletteId == QLCPalette::invalidId())
        return;

    // A value below 0 is the special "step" (fall-back) state.
    const int inMs  = m_fadeInSpin->value()  < 0 ? -1 : int(m_fadeInSpin->value()  * 1000.0 + 0.5);
    const int outMs = m_fadeOutSpin->value() < 0 ? -1 : int(m_fadeOutSpin->value() * 1000.0 + 0.5);
    m_contextScene->setPaletteFade(m_paletteId, inMs, outMs);
    m_doc->setModified();
    // Re-run the preview so the new transition time takes effect, and refresh
    // the look list label (fade indicator).
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

void LookEditor::slotStrobeChanged(int pct)
{
    m_strobeValue->setText(QString("%1%").arg(pct));
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::Strobe)
        return;
    p->setValue(float(pct) / 100.0f);
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
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
    // Skip a redundant rebuild for the SAME palette + script. Selecting a look
    // can emit lookSelected twice (selectionChanged + doubleClicked), which used
    // to build the param panel twice — the first set of sliders was torn down but
    // left ghost pixels painted under the live set (handles appeared to "move" and
    // some snapped back to the original values). A real script change keys a
    // different path, so it still rebuilds.
    {
        QLCPalette *gp = m_doc->palette(m_paletteId);
        QString gScriptPath;
        if (gp && gp->type() == QLCPalette::Effect)
        {
            const QString gName = m_effectScriptCombo->currentData().toString();
            if (!gName.isEmpty() && m_doc->effectScriptRunner())
                gScriptPath = m_doc->effectScriptRunner()->cache()->scriptPath(gName);
        }
        const QString buildKey = QString::number(m_paletteId) + QLatin1Char('|') + gScriptPath;
        qDebug() << "[LE-DIAG] buildKey=" << buildKey
                 << "cached=" << m_effectBuildKey
                 << "hasLayout=" << (m_effectDynWidget->layout() != nullptr);
        if (buildKey == m_effectBuildKey && m_effectDynWidget->layout() != nullptr)
        {
            qDebug() << "[LE-DIAG] early-return (stale key)";
            return;
        }
        m_effectBuildKey = buildKey;
    }

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

    // The Learn-range widgets (if any) were just deleted — abort any capture in
    // progress and drop the dangling pointers before we (maybe) rebuild them.
    if (m_learning)
        stopLearnRange(false);
    m_learnRangeButton = nullptr;
    m_learnRangeStatus = nullptr;

    QVBoxLayout *dv = new QVBoxLayout(m_effectDynWidget);
    dv->setContentsMargins(0, 4, 0, 4);
    dv->setSpacing(6);

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect)
    {
        m_loading = false;
        return;
    }

    // The engine script comes from the PALETTE (a preset stamps it there); the
    // combo's current data may be a preset NAME, which is not a script name.
    QString scriptPath = p->scriptPath();
    qDebug() << "[LE-DIAG] palette scriptPath=" << scriptPath;
    if (!scriptPath.isEmpty() && !QFileInfo::exists(scriptPath)
        && m_doc->effectScriptRunner())
        scriptPath = m_doc->effectScriptRunner()->cache()->scriptPath(
            QFileInfo(scriptPath).completeBaseName());
    qDebug() << "[LE-DIAG] resolved scriptPath=" << scriptPath
             << "exists=" << QFileInfo::exists(scriptPath);

    if (scriptPath.isEmpty())
    {
        qDebug() << "[LE-DIAG] no scriptPath → showing placeholder";
        m_effectDescLabel->hide();
        m_effectNotesLabel->hide();
        m_effectTypesLabel->hide();
        dv->addWidget(new QLabel(tr("(select a generator above to configure it)"), m_effectDynWidget));
        dv->addStretch(1);
        m_loading = false;
        return;
    }

    // Load the script to get meta (params, inputs, palettes)
    EffectScript tmpScript;
    const bool loaded = tmpScript.load(scriptPath);
    qDebug() << "[LE-DIAG] tmpScript.load(" << scriptPath << ") =" << loaded;
    if (!loaded)
    {
        m_effectDescLabel->hide();
        m_effectNotesLabel->hide();
        m_effectTypesLabel->hide();
        dv->addWidget(new QLabel(tr("Error loading generator — check the .js file."), m_effectDynWidget));
        dv->addStretch(1);
        m_effectBuildKey.clear();  // don't cache a failed build — retry on next selection
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

    // Engine/preset banner — always show which script is doing the work.
    {
        const QString engine = tmpScript.name().isEmpty()
            ? QFileInfo(scriptPath).completeBaseName() : tmpScript.name();
        const QString preset = p->effectPreset();
        QLabel *eng = new QLabel(m_effectDynWidget);
        eng->setTextFormat(Qt::RichText);
        if (!preset.isEmpty())
            eng->setText(tr("Effect <b>%1</b> &nbsp;·&nbsp; generator: <b>%2</b>")
                         .arg(preset.toHtmlEscaped(), engine.toHtmlEscaped()));
        else
            eng->setText(tr("Generator: <b>%1</b>").arg(engine.toHtmlEscaped()));
        eng->setStyleSheet("color: palette(mid);");
        dv->addWidget(eng);
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
            else if (pd.type == QLatin1String("direction"))
            {
                // Gradient-direction param → visual compass that previews the
                // look's two colours along the chosen angle.
                GradientDirectionWidget *gdw = new GradientDirectionWidget(m_effectDynWidget);
                gdw->setAngle(cur);

                // Resolve ALL the look's Colour palettes (precedence order) so
                // the preview shows the real multi-stop gradient.
                QList<QColor> stops;
                if (m_contextScene != NULL)
                {
                    foreach (quint32 pid, m_contextScene->palettes())
                    {
                        QLCPalette *lp = m_doc->palette(pid);
                        if (lp != NULL && lp->type() == QLCPalette::Color)
                            stops << lp->colorValue();
                    }
                }
                if (stops.size() >= 2)
                    gdw->setStops(stops);
                else
                    gdw->setColors(QColor(255, 0, 0), QColor(0, 0, 255));

                const QString paramName = pd.name;
                connect(gdw, &GradientDirectionWidget::angleChanged,
                        this, [this, paramName](double deg) {
                    QLCPalette *pp = m_doc->palette(m_paletteId);
                    if (!pp) return;
                    pp->setEffectParamValue(paramName, deg);
                    m_doc->setModified();
                    emit paletteValueChanged(m_paletteId);
                });
                form->addRow(pd.name, gdw);
            }
            else if (pd.name == QLatin1String("midiSource"))
            {
                // MIDI source → a dropdown of the ACTUAL patched input devices by
                // name (not an opaque universe number). Value stored = the 1-based
                // universe the effect reads via data.midi.universes[n]; 0 = any.
                QComboBox *cb = new QComboBox(m_effectDynWidget);
                cb->addItem(tr("Any MIDI source (merged)"), 0);
                InputOutputMap *iom = m_doc->inputOutputMap();
                if (iom != NULL)
                {
                    for (quint32 u = 0; u < iom->universesCount(); u++)
                    {
                        InputPatch *ip = iom->inputPatch(u);
                        if (ip == NULL || ip->inputName().isEmpty())
                            continue;
                        // e.g. "Universe 9 — Launchkey MK4 49"
                        cb->addItem(tr("Universe %1 — %2").arg(u + 1).arg(ip->inputName()),
                                    int(u + 1));
                    }
                }
                const int want = int(cur);
                int sel = cb->findData(want);
                if (sel < 0)   // stored universe no longer patched → keep the number visible
                {
                    if (want > 0)
                        cb->addItem(tr("Universe %1 (not connected)").arg(want), want);
                    sel = qMax(0, cb->findData(want));
                }
                cb->setCurrentIndex(sel);
                cb->setToolTip(pd.description);
                const QString paramName = pd.name;
                connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, [this, cb, paramName](int) {
                    if (m_loading) return;
                    QLCPalette *pp = m_doc->palette(m_paletteId);
                    if (!pp || pp->type() != QLCPalette::Effect) return;
                    pp->setEffectParamValue(paramName, double(cb->currentData().toInt()));
                    m_doc->setModified();
                    emit paletteValueChanged(m_paletteId);
                });
                form->addRow(pd.name, cb);
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
                // Render NON-natively (Qt stylesheet, not QMacStyle). On macOS the
                // native QSlider draws through a single shared NSSlider cell; while
                // one slider is dragged and the panel repaints rapidly, the sibling
                // sliders redraw from that shared cell and show the DRAGGED value —
                // every handle (and fill) tracks the slider being moved even though
                // their own values never change. A stylesheet forces each slider to
                // paint from its own value, eliminating the shared-cell artifact.
                sl->setStyleSheet(
                    "QSlider::groove:horizontal{height:4px;background:#4a4a4a;border-radius:2px;}"
                    "QSlider::sub-page:horizontal{background:#2f7fd1;border-radius:2px;}"
                    "QSlider::add-page:horizontal{background:#4a4a4a;border-radius:2px;}"
                    "QSlider::handle:horizontal{width:13px;margin:-6px 0;border-radius:6px;background:#e0e0e0;}");
                int slVal = (pd.max > pd.min)
                            ? int((cur - pd.min) / (pd.max - pd.min) * 1000) : 0;
                sl->setValue(qBound(0, slVal, 1000));
                sl->setProperty("paramName", pd.name);
                sl->setProperty("paramMin",  pd.min);
                sl->setProperty("paramMax",  pd.max);
                sl->setToolTip(pd.description);
                rl->addWidget(sl, 1);

                QLabel *val = new QLabel(QString::number(cur, 'f', 2), row);
                // FIXED width (not minimum): the displayed number changes width as
                // it's dragged (e.g. "6.90" -> "11.30"); with only a minimum width
                // that resizes the label, which relayouts the row's QFormLayout
                // every tick and repaints the sibling sliders — on macOS those
                // native sliders then render a stale handle (appears to "move",
                // snapping back only on a full repaint). A fixed width removes the
                // per-tick relayout entirely.
                val->setFixedWidth(56);
                val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                sl->setProperty("valueLabel", QVariant::fromValue((QObject*)val));
                rl->addWidget(val);

                connect(sl, SIGNAL(valueChanged(int)), this, SLOT(slotEffectParamChanged(int)));
                form->addRow(pd.name, row);
            }
        }
        dv->addLayout(form);

        // "Learn range" — offered when the script has both note-range params.
        // Plays live: click, play your lowest & highest key, click again to lock.
        // Writes noteLow/noteHigh into the palette (persisted) and switches
        // autoRange to Manual so the learned range is actually used.
        bool hasLo = false, hasHi = false, hasAuto = false;
        for (const EffectScript::ParamDef &pd : pdefs)
        {
            if (pd.name == QLatin1String("noteLow"))  hasLo = true;
            if (pd.name == QLatin1String("noteHigh")) hasHi = true;
            if (pd.name == QLatin1String("autoRange")) hasAuto = true;
        }
        if (hasLo && hasHi)
        {
            m_learnLoParam = QStringLiteral("noteLow");
            m_learnHiParam = QStringLiteral("noteHigh");
            m_learnHasAutoRange = hasAuto;

            QWidget *lrow = new QWidget(m_effectDynWidget);
            QHBoxLayout *ll = new QHBoxLayout(lrow);
            ll->setContentsMargins(0, 2, 0, 0);
            m_learnRangeButton = new QPushButton(tr("Learn range…"), lrow);
            m_learnRangeButton->setToolTip(
                tr("Click, then play your lowest and highest key on the controller; "
                   "click again to lock the range in. Uses the selected MIDI source."));
            connect(m_learnRangeButton, &QPushButton::clicked,
                    this, &LookEditor::slotLearnRangeClicked);
            ll->addWidget(m_learnRangeButton);
            m_learnRangeStatus = new QLabel(lrow);
            m_learnRangeStatus->setStyleSheet("color: palette(mid); font-style: italic;");
            ll->addWidget(m_learnRangeStatus, 1);
            dv->addWidget(lrow);
        }
        else
        {
            m_learnLoParam.clear();
            m_learnHiParam.clear();
        }
    }

    // --- Input bindings ---
    // Hide manual bindings when the script subscribes to the system "joystick"
    // data channel — the programmer controller auto-routes the HID/profile axes.
    const bool systemBound = tmpScript.dataChannelKeys().contains(
                                 QLatin1String("joystick"));
    const QList<EffectScript::InputDef> &idefs = tmpScript.inputDefs();
    if (!idefs.isEmpty() && !systemBound)
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

void LookEditor::populateEffectPicker(QLCPalette *p)
{
    // Reflect the look's persistence flag (blockSignals: this is a programmatic
    // update, not a user click, and the clicked() handler would dirty the Doc).
    if (m_effectPersistentCb != nullptr)
    {
        m_effectPersistentCb->blockSignals(true);
        m_effectPersistentCb->setChecked(p != NULL && p->persistent());
        m_effectPersistentCb->blockSignals(false);
    }

    m_effectScriptCombo->clear();
    m_effectScriptCombo->addItem(tr("(none)"), QString());
    m_effectScriptCombo->setItemData(0, EffectKindScript, EffectKindRole);

    if (!m_doc->effectScriptRunner())
        return;

    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    EffectPresetCache *pcache = m_doc->effectScriptRunner()->presetCache();

    // One engine script + the presets that are backed by it.
    struct ScriptNode {
        QString display;      // effect.name ("Dimmer Phaser")
        QString basename;     // file basename ("dimmer-phaser")
        QString category;
        QString tooltip;
        QList<EffectPresetCache::Preset> presets;
    };
    QList<ScriptNode> nodes;
    QMap<QString, int> byBasename;
    for (const QString &n : scache->scriptNames())
    {
        const EffectScriptCache::ScriptMeta m = scache->scriptMeta(n);
        ScriptNode node;
        node.display  = n;
        node.basename = scache->nameFromPath(scache->scriptPath(n));
        node.category = scriptCategory(m.fixtureTypes);
        node.tooltip  = m.description;
        byBasename.insert(node.basename, nodes.size());
        nodes.append(node);
    }
    // Attach each preset under its engine script.
    for (const EffectPresetCache::Preset &pr : pcache->presets())
        if (byBasename.contains(pr.script))
            nodes[byBasename.value(pr.script)].presets.append(pr);

    // Group script-nodes by category.
    QMap<QString, QList<int>> byCat;
    for (int i = 0; i < nodes.size(); i++)
        byCat[nodes[i].category].append(i);

    QStringList cats;
    for (const char *c : kEffectCategoryOrder)
        if (byCat.contains(QLatin1String(c))) cats << QLatin1String(c);
    for (const QString &c : byCat.keys())
        if (!cats.contains(c)) cats << c;

    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(m_effectScriptCombo->model());
    const QString curPreset = p ? p->effectPreset() : QString();
    const QString curPath   = p ? p->scriptPath()   : QString();
    int selectIdx = 0;

    auto addHeader = [&](const QString &text) {
        m_effectScriptCombo->addItem(text);
        const int hi = m_effectScriptCombo->count() - 1;
        m_effectScriptCombo->setItemData(hi, EffectKindHeader, EffectKindRole);
        if (model && model->item(hi))
        {
            QStandardItem *it = model->item(hi);
            it->setFlags(it->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
            QFont f = it->font(); f.setBold(true); it->setFont(f);
        }
    };

    for (const QString &cat : cats)
    {
        addHeader(cat);                                    // ── Category ──

        QList<int> idxs = byCat.value(cat);
        std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
            return nodes[a].display.compare(nodes[b].display, Qt::CaseInsensitive) < 0; });

        for (int ni : idxs)
        {
            ScriptNode &node = nodes[ni];

            // The engine script — selectable (start from scratch). Indent 1.
            m_effectScriptCombo->addItem(QStringLiteral("  ") + node.display, node.display);
            const int sidx = m_effectScriptCombo->count() - 1;
            m_effectScriptCombo->setItemData(sidx, EffectKindScript, EffectKindRole);
            if (!node.tooltip.isEmpty())
                m_effectScriptCombo->setItemData(sidx, node.tooltip, Qt::ToolTipRole);
            if (curPreset.isEmpty() && selectIdx == 0
                && !curPath.isEmpty() && scache->scriptPath(node.display) == curPath)
                selectIdx = sidx;

            // Its presets — nested under it. Indent 2.
            std::sort(node.presets.begin(), node.presets.end(),
                      [](const EffectPresetCache::Preset &a, const EffectPresetCache::Preset &b) {
                          return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
            for (const EffectPresetCache::Preset &pr : node.presets)
            {
                m_effectScriptCombo->addItem(QStringLiteral("      • ") + pr.name, pr.name);
                const int pidx = m_effectScriptCombo->count() - 1;
                m_effectScriptCombo->setItemData(pidx, EffectKindPreset, EffectKindRole);
                if (!pr.description.isEmpty())
                    m_effectScriptCombo->setItemData(pidx, pr.description, Qt::ToolTipRole);
                if (!curPreset.isEmpty() && pr.name == curPreset)
                    selectIdx = pidx;
            }
        }
    }

    m_effectScriptCombo->setCurrentIndex(selectIdx);
}

void LookEditor::slotEffectScriptChanged(int /*index*/)
{
    if (m_loading)
        return;

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect)
        return;

    if (!m_doc->effectScriptRunner())
        return;

    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    const int     kind = m_effectScriptCombo->currentData(EffectKindRole).toInt();
    const QString name = m_effectScriptCombo->currentData().toString();

    // Remember the outgoing settings so shared params (note range, MIDI source,
    // axis, autoRange, speed…) carry across an effect swap instead of resetting.
    const QMap<QString, double>  prevParams = p->effectParamValues();
    const QMap<QString, QString> prevStr    = p->effectStringParams();

    // Copy the params that ALSO exist in @p newScriptPath (by name or a declared
    // alias), clamped to the new script's range/enum, onto palette @p pp.
    auto carryOverParams = [&](QLCPalette *pp, const QString &newScriptPath) {
        if (newScriptPath.isEmpty())
            return;
        EffectScript ns;
        if (!ns.load(newScriptPath))
            return;
        for (const EffectScript::ParamDef &d : ns.paramDefs())
        {
            // Find the old value under this name or any of its former names.
            bool have = prevParams.contains(d.name);
            double v = have ? prevParams.value(d.name) : 0.0;
            if (!have)
                for (const QString &a : d.aliases)
                    if (prevParams.contains(a)) { v = prevParams.value(a); have = true; break; }
            if (have)
            {
                if (!d.enumValues.isEmpty())
                    v = qBound(0.0, v, double(d.enumValues.size() - 1));
                else if (d.type != QLatin1String("path"))
                    v = qBound(double(d.min), v, double(d.max));
                pp->setEffectParamValue(d.name, v);
            }
            // String/path params (e.g. a drawn path) carry over verbatim by name.
            if (prevStr.contains(d.name))
                pp->setEffectStringParam(d.name, prevStr.value(d.name));
        }
    };

    if (kind == EffectKindPreset)
    {
        // Stamp the preset: engine script + pinned param values + identity. The
        // preset's own values win; shared params it doesn't pin still carry over.
        const EffectPresetCache::Preset pr =
            m_doc->effectScriptRunner()->presetCache()->preset(name);
        const QString path = scache->scriptPath(pr.script);
        p->setScriptPath(path);
        p->setEffectPreset(pr.name);
        p->clearEffectParamValues();
        carryOverParams(p, path);
        for (auto it = pr.params.constBegin(); it != pr.params.constEnd(); ++it)
            p->setEffectParamValue(it.key(), it.value());
    }
    else
    {
        // Raw script (or "(none)"): clear preset identity, then re-apply the
        // shared params the new script also declares.
        const QString path = name.isEmpty() ? QString() : scache->scriptPath(name);
        p->setScriptPath(path);
        p->setEffectPreset(QString());
        p->clearEffectParamValues();
        carryOverParams(p, path);
    }
    // Wipe old input bindings since the effect changed.
    p->clearEffectInputBindings();
    maybeAutoName(p);

    // Organize the palette FOLDER to mirror the picker hierarchy:
    //   Palettes/Effect/<Category>/<Engine>/
    // so browsing effect palettes reads the same as the script picker.
    {
        QString engineBase;
        if (kind == EffectKindPreset)
            engineBase = m_doc->effectScriptRunner()->presetCache()->preset(name).script;
        else if (!name.isEmpty())
            engineBase = scache->nameFromPath(scache->scriptPath(name));
        if (!engineBase.isEmpty())
        {
            const EffectScriptCache::ScriptMeta meta = scache->scriptMeta(engineBase);
            const QString engineDisplay = meta.displayName.isEmpty() ? engineBase : meta.displayName;
            maybeAutoPath(p, scriptCategory(meta.fixtureTypes), engineDisplay);
        }
    }

    m_doc->setModified();
    emit paletteChanged(m_paletteId);

    rebuildEffectDynWidget();
}

void LookEditor::slotSaveAsEffect()
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect || !m_doc->effectScriptRunner())
        return;
    if (p->scriptPath().isEmpty())
    {
        QMessageBox::information(this, tr("Save as Effect"),
            tr("Pick a Generator first, then save it as an Effect."));
        return;
    }

    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    EffectPresetCache *pcache = m_doc->effectScriptRunner()->presetCache();

    // Default the name to the look's current name / preset name.
    const QString suggested = !p->effectPreset().isEmpty() ? p->effectPreset()
                            : (p->name().isEmpty() ? tr("My Effect") : p->name());
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save as Effect"),
        tr("Effect name:"), QLineEdit::Normal, suggested, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    if (pcache->contains(name))
    {
        if (QMessageBox::question(this, tr("Save as Effect"),
                tr("An Effect named \"%1\" already exists. Replace it?").arg(name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    const QString base = scache->nameFromPath(p->scriptPath());
    const EffectScriptCache::ScriptMeta meta = scache->scriptMeta(base);

    EffectPresetCache::Preset preset;
    preset.name        = name;
    preset.script      = base;
    preset.category    = EffectScriptCache::categoryForTypes(meta.fixtureTypes);
    preset.description = meta.description;   // start from the Generator's blurb
    preset.params      = p->effectParamValues();

    if (!pcache->savePreset(preset))
    {
        QMessageBox::warning(this, tr("Save as Effect"),
            tr("Could not write the Effect file. Check folder permissions."));
        return;
    }

    // The look IS this Effect now — adopt its identity and reselect it.
    p->setEffectPreset(name);
    m_doc->setModified();
    m_effectScriptCombo->blockSignals(true);
    populateEffectPicker(p);
    m_effectScriptCombo->blockSignals(false);
    emit paletteChanged(m_paletteId);
}

void LookEditor::slotExportEffect()
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect || !m_doc->effectScriptRunner())
        return;
    if (p->scriptPath().isEmpty())
    {
        QMessageBox::information(this, tr("Export Effect"),
            tr("Pick a Generator first, then export it."));
        return;
    }

    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    const QString base = scache->nameFromPath(p->scriptPath());
    const EffectScriptCache::ScriptMeta meta = scache->scriptMeta(base);

    // Read the script source so the file is portable (a recipient who lacks the
    // script gets it on import).
    QString source;
    QFile sf(scache->scriptPath(base));
    if (sf.open(QIODevice::ReadOnly | QIODevice::Text))
        source = QString::fromUtf8(sf.readAll());

    QJsonObject obj;
    obj["qlcplusEffect"] = 1;
    obj["name"] = p->effectPreset().isEmpty() ? (p->name().isEmpty() ? base : p->name())
                                              : p->effectPreset();
    obj["script"] = base;
    obj["category"] = EffectScriptCache::categoryForTypes(meta.fixtureTypes);
    obj["description"] = meta.description;
    obj["scriptSource"] = source;
    QJsonObject params;
    const QMap<QString, double> pv = p->effectParamValues();
    for (auto it = pv.constBegin(); it != pv.constEnd(); ++it)
        params[it.key()] = it.value();
    obj["params"] = params;
    QJsonObject strs;
    const QMap<QString, QString> sp = p->effectStringParams();
    for (auto it = sp.constBegin(); it != sp.constEnd(); ++it)
        strs[it.key()] = it.value();
    obj["stringParams"] = strs;

    const QString suggested = obj["name"].toString() + ".qxfx";
    QString path = QFileDialog::getSaveFileName(this, tr("Export Effect"),
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(suggested),
        tr("QLC+ Effect (*.qxfx)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".qxfx"))
        path += ".qxfx";

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Export Effect"),
            tr("Could not write \"%1\".").arg(path));
        return;
    }
    out.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    QMessageBox::information(this, tr("Export Effect"),
        tr("Exported to \"%1\".").arg(QFileInfo(path).fileName()));
}

void LookEditor::slotImportEffect()
{
    if (!m_doc->effectScriptRunner())
        return;

    QString path = QFileDialog::getOpenFileName(this, tr("Import Effect"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("QLC+ Effect (*.qxfx);;All files (*)"));
    if (path.isEmpty())
        return;

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Import Effect"), tr("Could not read the file."));
        return;
    }
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
    {
        QMessageBox::warning(this, tr("Import Effect"), tr("Not a valid Effect file."));
        return;
    }
    QJsonObject obj = doc.object();
    // SANITIZE the script basename before it's ever used as a file name — a
    // crafted .qxfx could otherwise set "script" to "../../foo" or an absolute
    // path and make the install write escape the user scripts dir with
    // attacker-controlled content. Mirror the New-script / savePreset sanitizing.
    QString base = obj["script"].toString();
    base.replace(QRegularExpression("[^A-Za-z0-9_-]"), "-");
    const QString name = obj["name"].toString();
    if (base.isEmpty() || name.isEmpty())
    {
        QMessageBox::warning(this, tr("Import Effect"), tr("The file is missing a script or name."));
        return;
    }

    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    EffectPresetCache *pcache = m_doc->effectScriptRunner()->presetCache();

    // Install the embedded script if we don't already have one by that name.
    if (scache->scriptPath(base).isEmpty())
    {
        const QString source = obj["scriptSource"].toString();
        if (source.isEmpty())
        {
            QMessageBox::warning(this, tr("Import Effect"),
                tr("This Effect needs the script \"%1\", which you don't have and the "
                   "file doesn't include.").arg(base));
            return;
        }
        QDir udir = EffectScriptCache::userScriptsDirectory();
        udir.mkpath(".");
        QFile js(udir.filePath(base + ".js"));
        if (!js.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, tr("Import Effect"),
                tr("Could not install the script. Check folder permissions."));
            return;
        }
        js.write(source.toUtf8());
        js.close();
        scache->rescan();
    }

    EffectPresetCache::Preset preset;
    preset.name        = name;
    preset.script      = base;
    preset.category    = obj["category"].toString();
    preset.description = obj["description"].toString();
    const QJsonObject params = obj["params"].toObject();
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        preset.params[it.key()] = it.value().toDouble();

    if (pcache->contains(name) &&
        QMessageBox::question(this, tr("Import Effect"),
            tr("An Effect named \"%1\" already exists. Replace it?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (!pcache->savePreset(preset))
    {
        QMessageBox::warning(this, tr("Import Effect"), tr("Could not save the imported Effect."));
        return;
    }

    // Reflect the new script/preset in the picker.
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p != nullptr && p->type() == QLCPalette::Effect)
    {
        m_effectScriptCombo->blockSignals(true);
        populateEffectPicker(p);
        m_effectScriptCombo->blockSignals(false);
    }
    QMessageBox::information(this, tr("Import Effect"),
        tr("Imported \"%1\". It's now in the Effect picker.").arg(name));
}

void LookEditor::openScriptInEditor(const QString &filePath)
{
    if (filePath.isEmpty())
        return;
    const QString mode = QSettings().value(QStringLiteral("effectscript/editor"),
                                           QStringLiteral("internal")).toString();
    if (mode == QLatin1String("external"))
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        return;
    }
    EffectScriptEditor dlg(filePath, m_doc, this);
    dlg.exec();
    // The editor rescans on save; refresh the picker in case a name changed.
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p != nullptr && p->type() == QLCPalette::Effect)
    {
        m_effectScriptCombo->blockSignals(true);
        populateEffectPicker(p);
        m_effectScriptCombo->blockSignals(false);
    }
}

void LookEditor::slotNewEffectScript()
{
    if (!m_doc->effectScriptRunner())
        return;

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New Effect Script"),
        tr("Script name (letters, numbers, hyphens):"), QLineEdit::Normal,
        tr("my-effect"), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    // Sanitize to a safe basename.
    name.replace(QRegularExpression("[^A-Za-z0-9_-]"), "-");

    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    if (!scache->scriptPath(name).isEmpty())
    {
        QMessageBox::information(this, tr("New Effect Script"),
            tr("A script named \"%1\" already exists.").arg(name));
        return;
    }

    QDir udir = EffectScriptCache::userScriptsDirectory();
    udir.mkpath(".");
    const QString path = udir.filePath(name + ".js");

    // A minimal, commented, valid template the user fills in.
    const QString tmpl = QString(
        "/*\n"
        "  QLC+ Effect Script: %1\n"
        "  Return per-fixture intents each tick (pan/tilt degrees, r/g/b 0-255,\n"
        "  dimmer 0-1). Only the keys you set are written (Override), so the\n"
        "  scene's other palettes fall through on channels you omit.\n"
        "*/\n"
        "(function() {\n"
        "    var effect = new Object;\n"
        "    effect.apiVersion   = 1;\n"
        "    effect.name         = \"%1\";\n"
        "    effect.description  = \"Describe your effect\";\n"
        "    effect.author       = \"You\";\n"
        "    effect.fixtureTypes = [\"moving\"];   // or [\"wash\"], [] = all\n"
        "\n"
        "    effect.parameters = [\n"
        "        { name: \"speed\", description: \"Cycles per second\", min: -3.0, max: 3.0, defaultValue: 0.3 }\n"
        "    ];\n"
        "\n"
        "    effect.tick = function(fixtures, inputs, palettes, params, state) {\n"
        "        var t     = inputs._time !== undefined ? inputs._time : 0;\n"
        "        var speed = params.speed !== undefined ? params.speed : 0.3;\n"
        "        return fixtures.map(function(f, i) {\n"
        "            if (!f.hasPanTilt) return {};\n"
        "            var phase = t * speed * 2 * Math.PI + i;\n"
        "            return {\n"
        "                pan:  (0.5 + 0.2 * Math.sin(phase)) * f.panRange,\n"
        "                tilt: (0.5 + 0.15 * Math.cos(phase)) * f.tiltRange\n"
        "            };\n"
        "        });\n"
        "    };\n"
        "\n"
        "    return effect;\n"
        "})()\n").arg(name);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("New Effect Script"),
            tr("Could not create the script. Check folder permissions."));
        return;
    }
    f.write(tmpl.toUtf8());
    f.close();
    scache->rescan();
    openScriptInEditor(path);
}

void LookEditor::slotEditEffectScript()
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect || p->scriptPath().isEmpty())
    {
        QMessageBox::information(this, tr("Edit Effect Script"),
            tr("Pick a Generator first, then edit its script."));
        return;
    }
    openScriptInEditor(p->scriptPath());
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

// --- Learn range ------------------------------------------------------------
// Human note name using the user's Roland-style convention (C2 = MIDI 48), so
// the readout matches the labels printed on their keyboard.
static QString midiNoteLabel(int note)
{
    static const char *names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    if (note < 0 || note > 127) return QStringLiteral("--");
    return QString::fromLatin1(names[note % 12]) + QString::number(note / 12 - 2);
}

void LookEditor::slotLearnRangeClicked()
{
    if (m_learning)
        stopLearnRange(true);       // second click → lock it in
    else
        startLearnRange();
}

void LookEditor::startLearnRange()
{
    if (m_learnLoParam.isEmpty() || m_learnHiParam.isEmpty())
        return;
    m_learning = true;
    m_learnLo  = 127;
    m_learnHi  = 0;
    if (m_doc->inputOutputMap() != NULL)
        connect(m_doc->inputOutputMap(),
                SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SLOT(slotLearnMidiInput(quint32,quint32,uchar,const QString&)),
                Qt::UniqueConnection);
    if (m_learnRangeButton)
        m_learnRangeButton->setText(tr("Stop && lock"));
    if (m_learnRangeStatus)
        m_learnRangeStatus->setText(tr("Play your lowest, then highest key…"));
}

void LookEditor::stopLearnRange(bool commit)
{
    if (m_doc->inputOutputMap() != NULL)
        disconnect(m_doc->inputOutputMap(),
                   SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                   this, SLOT(slotLearnMidiInput(quint32,quint32,uchar,const QString&)));
    m_learning = false;

    if (m_learnRangeButton)
        m_learnRangeButton->setText(tr("Learn range…"));

    if (!commit || m_learnHi <= m_learnLo)
    {
        if (commit && m_learnRangeStatus)
            m_learnRangeStatus->setText(tr("No range captured — play two keys."));
        return;
    }

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::Effect)
        return;
    p->setEffectParamValue(m_learnLoParam, double(m_learnLo));
    p->setEffectParamValue(m_learnHiParam, double(m_learnHi));
    if (m_learnHasAutoRange)
        p->setEffectParamValue(QStringLiteral("autoRange"), 0.0);   // → Manual
    m_doc->setModified();
    emit paletteValueChanged(m_paletteId);

    // Rebuild the panel so the noteLow/noteHigh sliders (and the autoRange combo)
    // show the locked-in values. Clear the build key or the rebuild early-returns.
    m_effectBuildKey.clear();
    rebuildEffectDynWidget();
    if (m_learnRangeStatus)   // recreated by the rebuild
        m_learnRangeStatus->setText(tr("Locked: %1–%2")
            .arg(midiNoteLabel(m_learnLo)).arg(midiNoteLabel(m_learnHi)));
}

void LookEditor::slotLearnMidiInput(quint32 universe, quint32 channel,
                                    uchar value, const QString &key)
{
    Q_UNUSED(key)
    if (!m_learning || value == 0)
        return;
    // Notes arrive on input channels 128-255 (note = channel - 128).
    if (channel < 128 || channel > 255)
        return;
    // Honour the effect's selected MIDI source: if it targets one universe
    // (midiSource = 1-based), ignore notes from any other device.
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p != NULL)
    {
        const int src = int(p->effectParamValues().value(QStringLiteral("midiSource"), 0.0));
        if (src > 0 && quint32(src) != universe + 1)
            return;
    }
    const int note = int(channel) - 128;
    if (note < m_learnLo) m_learnLo = note;
    if (note > m_learnHi) m_learnHi = note;
    if (m_learnRangeStatus)
        m_learnRangeStatus->setText(tr("Low %1 · High %2")
            .arg(midiNoteLabel(m_learnLo)).arg(midiNoteLabel(m_learnHi)));
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
