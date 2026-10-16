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
#include <QScrollArea>
#include <QSplitter>
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
#include <algorithm>

#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QShortcut>
#include <QTextBrowser>
#include <QDialog>
#include "lookeditor.h"
#include "programmercontroller.h"
#include "fadetimespin.h"
#include "effectbrowser.h"
#include "qlcfixturedefcache.h"
#include "wledfixture.h"
#include "pathdrawwidget.h"
#include "gradientdirectionwidget.h"
#include "virtualconsole/vcxypadarea.h"
#include "capabilitybar.h"
#include "qlcpalette.h"
#include "monitorproperties.h"
#include "gobolibrary.h"
#include "stagetarget.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
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
#include "qlceventpos.h"
#include "speedmasters.h"
#include "spreadlayer.h"
#include "wledconfigcheck.h"
#include "spreadpreview.h"

// VC XY pad works in DMX space [0..256); palette pan/tilt are degrees.
static const qreal XY_MAX = 256.0;
static const int PAN_DEG = 540;
static const int TILT_DEG = 270;

static const char *PALETTE_DROP_MIME = "application/x-qlcplus-palettes";

// Effect picker combo: each item is a category header, a raw engine script, or
// a preset. The kind is stored at EffectKindRole, the name at Qt::UserRole.
// A WLED item is one of WLED's own effects, run ON WLED fixtures by the device
// (QLCPalette::wledEffect()) and by its port, where there is one, elsewhere.
enum { EffectKindHeader = 0, EffectKindScript = 1, EffectKindPreset = 2, EffectKindWled = 3 };
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
        showPickMenu(qlcEventGlobalPos(ev));
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
    setProperty("helpTopic", QStringLiteral("programming-tab.md#the-inline-look-editor"));
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Name row: "Name:" label + editable QLineEdit
    QHBoxLayout *nameRow = new QHBoxLayout();
    nameRow->setContentsMargins(0, 0, 0, 0);
    QLabel *nameLbl = new QLabel(tr("Name:"), this);
    nameLbl->setMinimumWidth(44);
    nameRow->addWidget(nameLbl);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName("lookName");
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

    m_trialNote = new QLabel(this);
    m_trialNote->setStyleSheet("color: #2a7ab8; font-style: italic;");
    m_trialNote->setWordWrap(true);
    m_trialNote->hide();
    root->addWidget(m_trialNote);

    // Per-look fade override row: "Fade in [ step ] s   out [ step ] s". Each
    // spin's minimum reads "step" — meaning fall back to the chaser step /
    // scene fade for that direction. Any value >= 0 overrides it for this look's
    // channels only, so colour can snap while movers glide (in) and a look can
    // release slower than it punches in (a "pulse": 0 in, slow out).
    m_fadeRow = new QWidget(this);
    QHBoxLayout *fadeLay = new QHBoxLayout(m_fadeRow);
    fadeLay->setContentsMargins(0, 0, 0, 0);
    m_fadeInLabel = new QLabel(tr("Fade in"), m_fadeRow);
    m_fadeInLabel->setToolTip(tr("This look's own fade, for its channels only. 0 snaps; "
                                 "\"scene\" (the lowest value) follows the scene's fade."));
    fadeLay->addWidget(m_fadeInLabel);
    // The one fade-time control (FadeTimeSpin); labels set per look.
    m_fadeInSpin = new FadeTimeSpin(m_fadeRow, tr("scene"));
    connect(m_fadeInSpin, &FadeTimeSpin::msChanged, this, &LookEditor::slotFadeTimeChanged);
    fadeLay->addWidget(m_fadeInSpin);
    fadeLay->addSpacing(8);
    m_fadeOutLabel = new QLabel(tr("out"), m_fadeRow);
    fadeLay->addWidget(m_fadeOutLabel);
    m_fadeOutSpin = new FadeTimeSpin(m_fadeRow, tr("scene"));
    connect(m_fadeOutSpin, &FadeTimeSpin::msChanged, this, &LookEditor::slotFadeTimeChanged);
    fadeLay->addWidget(m_fadeOutSpin);
    fadeLay->addStretch(1);
    root->addWidget(m_fadeRow);
    m_fadeRow->hide();

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("lookPages");
    // QStackedWidget's minimumSizeHint() is the max over ALL pages, not just
    // the current one (a well-known Qt quirk — same one QTabWidget has at
    // the app-window level). The pan/tilt page's 200x200 XY pad
    // (setMinimumSize below) was the widest page, so it silently forced this
    // editor — and everything above it (canvas pane, Programming tab, the
    // whole main window) — to never shrink below that, even on the Color or
    // Dimmer page. Ignored + an explicit floor stops that from leaking out;
    // whichever page IS current still enforces its own real minimum size
    // normally within the space it's given.
    m_stack->setSizePolicy(QSizePolicy::Ignored, m_stack->sizePolicy().verticalPolicy());
    m_stack->setMinimumWidth(200);
    root->addWidget(m_stack);

    // Empty page
    QWidget *empty = new QWidget(this);
    QVBoxLayout *el = new QVBoxLayout(empty);
    /* No label here. The title bar immediately above this already reads
       "Select a look to edit it", and the two said the same thing one line
       apart -- which is how an empty panel ends up carrying more text than a
       full one. */
    el->addStretch();
    m_pageEmpty = m_stack->addWidget(empty);

    // Color page: RGB picker on the LEFT, vertical numbered sliders (R G B,
    // then extra emitters White / Amber / UV) on the RIGHT — always visible
    // beside the (tall) colour dialog rather than scrolled off-screen above
    // or below it. Numbered 1-6 to match PMJOverlay::slotRoleActivated's
    // Level-role fader mapping (fader N controls the Nth slider here), so
    // it's visible on screen which physical fader does what.
    QWidget *colorPage = new QWidget(this);
    QVBoxLayout *cv = new QVBoxLayout(colorPage);
    cv->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *cl = new QHBoxLayout();
    cl->setContentsMargins(0, 0, 0, 0);
    cv->addLayout(cl, 1);

    m_colorDialog = new QColorDialog(colorPage);
    m_colorDialog->setObjectName("lookColor");
    m_colorDialog->setOptions(QColorDialog::NoButtons
                              | QColorDialog::DontUseNativeDialog);
    m_colorDialog->setWindowFlags(Qt::Widget);
    // Keep the picker compact so the look panel doesn't force the tab (and the
    // window) taller than short screens; it scrolls if the panel is short.
    m_colorDialog->setMinimumHeight(0);
    m_colorDialog->setMaximumHeight(300);
    cl->addWidget(m_colorDialog, 1);

    // One vertical slider (numbered label above) per channel.
    auto makeColorCol = [&](int number, const QString &name, QSlider *&slider,
                             const char *slotSignature) -> QWidget* {
        QWidget *col = new QWidget(colorPage);
        QVBoxLayout *vl = new QVBoxLayout(col);
        vl->setContentsMargins(2, 0, 2, 0);
        vl->addWidget(new QLabel(QStringLiteral("%1 %2").arg(number).arg(name), col),
                      0, Qt::AlignHCenter);
        slider = new QSlider(Qt::Vertical, col);
        slider->setRange(0, 255);
        vl->addWidget(slider, 1, Qt::AlignHCenter);
        connect(slider, SIGNAL(valueChanged(int)), this, slotSignature);
        return col;
    };
    QWidget *extras = new QWidget(colorPage);
    QHBoxLayout *xl = new QHBoxLayout(extras);
    xl->setContentsMargins(0, 0, 0, 0);
    QWidget *redRow   = makeColorCol(1, tr("R"),  m_redSlider,   SLOT(slotRgbSliderChanged()));
    QWidget *greenRow = makeColorCol(2, tr("G"),  m_greenSlider, SLOT(slotRgbSliderChanged()));
    QWidget *blueRow  = makeColorCol(3, tr("B"),  m_blueSlider,  SLOT(slotRgbSliderChanged()));
    m_whiteRow = makeColorCol(4, tr("W"),  m_whiteSlider, SLOT(slotColorExtraChanged()));
    m_amberRow = makeColorCol(5, tr("A"),  m_amberSlider, SLOT(slotColorExtraChanged()));
    m_uvRow    = makeColorCol(6, tr("UV"), m_uvSlider,    SLOT(slotColorExtraChanged()));
    xl->addWidget(redRow);
    xl->addWidget(greenRow);
    xl->addWidget(blueRow);
    xl->addWidget(m_whiteRow);
    xl->addWidget(m_amberRow);
    xl->addWidget(m_uvRow);
    cl->addWidget(extras, 0);

    m_colorWarning = new QLabel(colorPage);
    m_colorWarning->setWordWrap(true);
    m_colorWarning->setStyleSheet(QStringLiteral("color: #e0a030;"));
    m_colorWarning->setVisible(false);
    cv->addWidget(m_colorWarning, 0);

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
    m_dimmerBar->setObjectName("lookDimmer");
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
    m_xyPad->setObjectName("lookPanTilt");
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
    QHBoxLayout *sourceRow = new QHBoxLayout();
    sv->addLayout(sourceRow);
    m_singleSourceLabel = new QLabel(tr("Wheel of"), single);
    sourceRow->addWidget(m_singleSourceLabel);
    m_singleSourceCombo = new QComboBox(single);
    m_singleSourceCombo->setToolTip(tr("The look stores one DMX value; each fixture "
                                       "type has its own wheel. Pick the type whose "
                                       "gobos to choose from."));
    sourceRow->addWidget(m_singleSourceCombo, 1);
    m_singleSourceLabel->setVisible(false);
    m_singleSourceCombo->setVisible(false);
    connect(m_singleSourceCombo, SIGNAL(activated(int)),
            this, SLOT(slotSingleSourcePicked(int)));
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
    QLabel *effectLabel = new QLabel(tr("Effect:"), effectPage);
    efScriptRow->addWidget(effectLabel);
    m_effectScriptCombo = new QComboBox(effectPage);
    m_effectScriptCombo->setToolTip(tr("Pick an Effect (or a raw Generator) for this look"));
    /* Cap the popup and force it to scroll. Several styles (macOS among them)
       otherwise draw every item at once, and with categories, generators and
       every preset under each, the list grew taller than the screen -- so the
       ends of it simply could not be reached. "combobox-popup: 0" is the
       documented way to make Qt use a scrolling list view that honours
       maxVisibleItems. */
    m_effectScriptCombo->setMaxVisibleItems(20);
    m_effectScriptCombo->setStyleSheet(QStringLiteral("QComboBox { combobox-popup: 0; }"));
    efScriptRow->addWidget(m_effectScriptCombo, 1);

    /* A flat list is the wrong shape for data that is three levels deep, and no
       amount of scrolling fixes "I know it is a Chase, show me the chases". The
       Browse dialog presents the SAME items as a filterable tree; picking there
       just sets this combo, so the combo stays the single source of truth and
       nothing downstream has to know the dialog exists. */
    QPushButton *browseBtn = new QPushButton(tr("Browse…"), effectPage);
    browseBtn->setToolTip(tr("Pick from a searchable tree of categories, "
                             "generators and presets"));
    connect(browseBtn, &QPushButton::clicked, this, &LookEditor::slotBrowseEffects);
    efScriptRow->addWidget(browseBtn);

    m_newScriptButton = new QPushButton(tr("New script…"), effectPage);
    m_newScriptButton->setToolTip(tr("Create a new effect script from a template and open it in the editor"));
    connect(m_newScriptButton, &QPushButton::clicked, this, &LookEditor::slotNewEffectScript);
    efScriptRow->addWidget(m_newScriptButton);

    m_editScriptButton = new QPushButton(tr("Edit script…"), effectPage);
    m_editScriptButton->setToolTip(tr("Edit this Generator's underlying .js script"));
    connect(m_editScriptButton, &QPushButton::clicked, this, &LookEditor::slotEditEffectScript);
    efScriptRow->addWidget(m_editScriptButton);

    efv->addLayout(efScriptRow);

    /* The picker itself: search, a tab per category, each effect with what it
       does and a strip of lights running it ("a better way than a drop down
       ... too long and too disjointed"). It mirrors the combo above, which
       stays the one source of truth but is no longer shown. */
    m_effectScriptCombo->hide();
    browseBtn->hide();
    effectLabel->hide();
    efScriptRow->insertStretch(0, 1);
    /* What the look runs, in one line over its settings, and a way to change
       it. The browser itself lives in a window of its own: side by side it
       took half the room while working WITH an effect ("can we windowshade
       that ... but we still need to be able to access it"). */
    QHBoxLayout *pickedRow = new QHBoxLayout();
    m_effectPickedLabel = new QLabel(effectPage);
    m_effectPickedLabel->setObjectName(QStringLiteral("effectPicked"));
    m_effectPickedLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    pickedRow->addWidget(m_effectPickedLabel);
    // What it is and how to program it: on hover, and a window on a click
    // ("we can have a [?] button to mouse over or pop a window").
    m_effectInfoButton = new QToolButton(effectPage);
    m_effectInfoButton->setObjectName(QStringLiteral("effectInfo"));
    m_effectInfoButton->setText(QStringLiteral("?"));
    m_effectInfoButton->setAutoRaise(true);
    connect(m_effectInfoButton, &QToolButton::clicked, this, &LookEditor::showEffectDetails);
    pickedRow->addWidget(m_effectInfoButton);
    pickedRow->addStretch(1);
    m_effectChangeButton = new QPushButton(tr("Change effect…"), effectPage);
    m_effectChangeButton->setObjectName(QStringLiteral("effectChange"));
    m_effectChangeButton->setToolTip(tr("Open the effect picker: every one you pick plays at "
                                        "once with its own settings; Cancel puts this one back."));
    connect(m_effectChangeButton, &QPushButton::clicked, this, &LookEditor::openEffectPicker);
    pickedRow->addWidget(m_effectChangeButton);
    efv->addLayout(pickedRow);

    QScrollArea *detailScroll = new QScrollArea(effectPage);
    detailScroll->setObjectName(QStringLiteral("effectDetails"));
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // it fits the width
    QWidget *details = new QWidget(detailScroll);
    QVBoxLayout *detailCol = new QVBoxLayout(details);
    detailCol->setContentsMargins(6, 0, 6, 0);
    detailCol->setSpacing(4);
    detailScroll->setWidget(details);
    efv->addWidget(detailScroll, 1);

    /* The picker: the browser (search, a tab per category, each effect
       running on a strip of lights), Cancel and Done. */
    /* A non-modal dialog: Esc is Cancel and Enter is Done from anywhere in
       it, without depending on the window being the active one. */
    QDialog *pickerDialog = new QDialog(this, Qt::Tool);
    pickerDialog->setModal(false);
    m_effectPicker = pickerDialog;
    m_effectPicker->setObjectName(QStringLiteral("effectPicker"));
    m_effectPicker->setWindowTitle(tr("Pick an effect"));
    m_effectPicker->setAttribute(Qt::WA_DeleteOnClose, false);
    {
        QVBoxLayout *pv = new QVBoxLayout(m_effectPicker);
        /* The list on the left, what the one picked is on the right -- its
           description, what it works with, notes, every setting ("this
           window can be bigger .. and the description on the right side"). */
        QSplitter *ps = new QSplitter(Qt::Horizontal, m_effectPicker);
        ps->setObjectName(QStringLiteral("effectPickerSplit"));
        ps->setChildrenCollapsible(false);
        m_effectBrowser = new EffectBrowser(m_doc, m_effectScriptCombo, ps);
        m_effectBrowser->setPickWhileBrowsing(true);
        m_effectBrowser->setMinimumWidth(380);
        m_effectPickerInfo = new QTextBrowser(ps);
        m_effectPickerInfo->setObjectName(QStringLiteral("effectPickerInfo"));
        m_effectPickerInfo->setMinimumWidth(240);
        m_effectPickerInfo->setFrameShape(QFrame::NoFrame);
        ps->addWidget(m_effectBrowser);
        ps->addWidget(m_effectPickerInfo);
        ps->setStretchFactor(0, 3);
        ps->setStretchFactor(1, 2);
        pv->addWidget(ps, 1);
        QLabel *hint = new QLabel(tr("Each one you pick plays now, with its own settings. "
                                     "Arrow keys step through them."), m_effectPicker);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
        pv->addWidget(hint);
        QDialogButtonBox *pb = new QDialogButtonBox(m_effectPicker);
        QPushButton *cancel = pb->addButton(QDialogButtonBox::Cancel);
        cancel->setObjectName(QStringLiteral("effectPickerCancel"));
        cancel->setToolTip(tr("Put back the effect the look had"));
        QPushButton *done = pb->addButton(QDialogButtonBox::Ok);
        done->setText(tr("Done"));
        done->setObjectName(QStringLiteral("effectPickerDone"));
        connect(cancel, &QPushButton::clicked, this, [this]() { closeEffectPicker(false); });
        connect(done, &QPushButton::clicked, this, [this]() { closeEffectPicker(true); });
        pv->addWidget(pb);
        // Esc (QDialog::reject) puts the look's effect back.
        connect(pickerDialog, &QDialog::rejected, this, [this]() { closeEffectPicker(false); });
        m_effectPicker->resize(860, 600);
        m_effectPicker->setMinimumSize(660, 420);
        const QByteArray geo = QSettings().value(QStringLiteral("lookeditor/effectPickerWide")).toByteArray();
        if (!geo.isEmpty())
            m_effectPicker->restoreGeometry(geo);
        // Closing the window keeps what is picked, like Done.
        m_effectPicker->installEventFilter(this);
    }
    // From here, the details column.
    QVBoxLayout *efvPage = efv;
    efv = detailCol;
    QWidget *effectPageOuter = effectPage;
    effectPage = details;

    // Short description label (one line, shown below combo)
    m_effectDescLabel = new QLabel(effectPage);
    m_effectDescLabel->setWordWrap(true);
    m_effectDescLabel->setStyleSheet("color: palette(mid); font-style: italic;");
    m_effectDescLabel->hide();
    // Not in the settings column: what an effect IS shows in the picker and
    // behind the header's "?" (effectInfoHtml()), not over its settings.

    // Notes label (longer paragraph, shown below description)
    m_effectNotesLabel = new QLabel(effectPage);
    m_effectNotesLabel->setWordWrap(true);
    m_effectNotesLabel->setTextFormat(Qt::PlainText);
    m_effectNotesLabel->hide();

    // Fixture-types label (e.g. "Works with: Moving Head, RGB/LED")
    m_effectTypesLabel = new QLabel(effectPage);
    m_effectTypesLabel->setWordWrap(false);
    m_effectTypesLabel->hide();

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
    // On a row of its own: the column beside the list is narrow.
    QHBoxLayout *efSaveAsRow = new QHBoxLayout();
    efSaveAsRow->addStretch(1);
    efSaveAsRow->addWidget(m_saveAsEffectButton);
    efv->addLayout(efSaveRow);
    efv->addLayout(efSaveAsRow);
    connect(m_saveAsEffectButton, &QPushButton::clicked, this, &LookEditor::slotSaveAsEffect);

    efv->addStretch(1);
    efv = efvPage;
    effectPage = effectPageOuter;

    m_pageEffect = m_stack->addWidget(effectPage);
    connect(m_effectScriptCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotEffectScriptChanged(int)));

    setPalette(QLCPalette::invalidId());
}

LookEditor::~LookEditor()
{
    /* Its children outlive this body: a Name field that still has focus loses
       it as they go and says "edited" -- into an editor already gone (a Qt
       assertion; a crash without one). */
    if (m_nameEdit != nullptr)
        m_nameEdit->disconnect(this);
}

void LookEditor::setTrialNote(const QString &text)
{
    m_trialNote->setText(text);
    m_trialNote->setVisible(!text.isEmpty());
}

QString LookEditor::trialNote() const
{
    return m_trialNote->isHidden() ? QString() : m_trialNote->text();
}

/*********************************************************************
 * The effect picker
 *********************************************************************/

bool LookEditor::lookHasWledTarget() const
{
    if (m_contextScene == nullptr)
        return true;                        // nothing known: list everything
    QList<quint32> fids = m_contextScene->fixtures();
    foreach (quint32 gid, m_contextScene->fixtureGroups())
        if (FixtureGroup *g = m_doc->fixtureGroup(gid))
            fids << g->fixtureList();
    foreach (quint32 fid, fids)
        if (WledConfigCheck::isWled(m_doc->fixture(fid)))
            return true;
    return false;
}

bool LookEditor::effectPickerOpen() const
{
    return m_effectPicker != nullptr && m_effectPicker->isVisible();
}

void LookEditor::openEffectPicker()
{
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (m_effectPicker == nullptr || p == nullptr || p->type() != QLCPalette::Effect)
        return;
    // What to put back on Cancel: everything a pick changes.
    if (!effectPickerOpen())
    {
        m_pickerSnapshot.paletteId = m_paletteId;
        m_pickerSnapshot.name = p->name();
        m_pickerSnapshot.path = p->path();
        m_pickerSnapshot.script = p->scriptPath();
        m_pickerSnapshot.preset = p->effectPreset();
        m_pickerSnapshot.wled = p->wledEffect();
        m_pickerSnapshot.params = p->effectParamValues();
        m_pickerSnapshot.strings = p->effectStringParams();
        m_pickerSnapshot.bindings = p->effectInputBindings();
    }
    m_effectPicker->setWindowTitle(tr("Pick an effect — %1").arg(p->name()));
    if (!m_effectPicker->isVisible()
        && QSettings().value(QStringLiteral("lookeditor/effectPickerWide")).toByteArray().isEmpty())
    {
        // First time: beside the look editor, to its left where there's room.
        const QRect me(mapToGlobal(QPoint(0, 0)), size());
        QPoint at(me.left() - m_effectPicker->width() - 12, me.top() - 200);
        if (at.x() < 0)
            at.setX(me.left() + 20);
        m_effectPicker->move(at.x(), qMax(0, at.y()));
    }
    // WLED effects that run only on a WLED device: offered when there's one.
    m_effectBrowser->setWledTargets(lookHasWledTarget());
    m_effectBrowser->reload();
    m_effectPicker->show();
    m_effectPicker->raise();
    m_effectPicker->activateWindow();
    m_effectBrowser->search()->setFocus();
    /* Wide enough for every tab, whole ("so we can see the tab names at
       least"), whatever the font: a wider one (Linux's, a large system font)
       overflowed the 860 px it opens at. Once laid out, grow the window by
       what the tabs lack, and give it to the browser's side. */
    QTimer::singleShot(0, this, [this]() {
        if (m_effectPicker == nullptr || !m_effectPicker->isVisible())
            return;
        QTabBar *tabs = m_effectBrowser->tabs();
        const int lack = tabs->sizeHint().width() - tabs->width();
        if (lack <= 0)
            return;
        m_effectPicker->resize(m_effectPicker->width() + lack + 8, m_effectPicker->height());
        if (QSplitter *ps = m_effectPicker->findChild<QSplitter *>())
        {
            QList<int> sz = ps->sizes();
            if (sz.size() == 2)
            {
                sz[0] += lack + 8;
                ps->setSizes(sz);
            }
        }
    });
}

void LookEditor::closeEffectPicker(bool keep)
{
    if (m_effectPicker == nullptr)
        return;
    QSettings().setValue(QStringLiteral("lookeditor/effectPickerWide"), m_effectPicker->saveGeometry());
    m_effectPicker->removeEventFilter(this);       // hiding is not the user closing it
    m_effectPicker->hide();
    m_effectPicker->installEventFilter(this);

    QLCPalette *p = m_doc->palette(m_pickerSnapshot.paletteId);
    if (!keep && p != nullptr && p->type() == QLCPalette::Effect
        && (p->scriptPath() != m_pickerSnapshot.script || p->effectPreset() != m_pickerSnapshot.preset
            || p->wledEffect() != m_pickerSnapshot.wled || p->effectParamValues() != m_pickerSnapshot.params))
    {
        p->setName(m_pickerSnapshot.name);
        p->setPath(m_pickerSnapshot.path);
        p->setScriptPath(m_pickerSnapshot.script);
        p->setEffectPreset(m_pickerSnapshot.preset);
        p->setWledEffect(m_pickerSnapshot.wled);
        p->setEffectParamValues(m_pickerSnapshot.params);   // clears the strings too
        for (auto it = m_pickerSnapshot.strings.constBegin(); it != m_pickerSnapshot.strings.constEnd(); ++it)
            p->setEffectStringParam(it.key(), it.value());
        p->setEffectInputBindings(m_pickerSnapshot.bindings);
        m_doc->setModified();
        emit paletteChanged(p->id());
        emit paletteRenamed(p->id());
        if (p->id() == m_paletteId)
            setPalette(m_paletteId);                // the editor shows it again
    }
    m_pickerSnapshot = EffectSnapshot();
}

void LookEditor::showEffectDetails()
{
    if (m_effectDetailsHtml.isEmpty())
        return;
    if (m_effectDetails == nullptr)
    {
        m_effectDetails = new QDialog(this, Qt::Tool);
        m_effectDetails->setObjectName(QStringLiteral("effectDetailsWindow"));
        m_effectDetails->setModal(false);
        m_effectDetails->resize(480, 520);
        QVBoxLayout *v = new QVBoxLayout(m_effectDetails);
        QTextBrowser *text = new QTextBrowser(m_effectDetails);
        text->setObjectName(QStringLiteral("effectDetailsText"));
        text->setOpenExternalLinks(true);
        v->addWidget(text);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, m_effectDetails);
        connect(bb, &QDialogButtonBox::rejected, m_effectDetails, &QDialog::hide);
        v->addWidget(bb);
    }
    m_effectDetails->setWindowTitle(tr("About %1").arg(m_effectScriptCombo->currentText().trimmed()));
    m_effectDetails->findChild<QTextBrowser *>(QStringLiteral("effectDetailsText"))->setHtml(m_effectDetailsHtml);
    m_effectDetails->show();
    m_effectDetails->raise();
}

bool LookEditor::eventFilter(QObject *obj, QEvent *ev)
{
    // The picker's own close button: keep what is picked, as Done does.
    if (obj == m_effectPicker && ev->type() == QEvent::Close)
    {
        closeEffectPicker(true);
        return false;
    }
    return QWidget::eventFilter(obj, ev);
}

void LookEditor::updateEffectHeader()
{
    if (m_effectPickedLabel == nullptr)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    const bool none = p == nullptr || p->type() != QLCPalette::Effect
                      || (p->scriptPath().isEmpty() && p->wledEffect().isEmpty());
    m_effectPickedLabel->setText(none ? tr("No effect yet")
                                      : m_effectScriptCombo->currentText().trimmed());
    m_effectChangeButton->setText(none ? tr("Pick an effect…") : tr("Change effect…"));
    if (none)
    {
        m_effectInfoButton->setVisible(false);
        m_effectDetailsHtml.clear();
        if (m_effectPickerInfo != nullptr)
            m_effectPickerInfo->clear();
    }
}

void LookEditor::setPalette(quint32 paletteId)
{
    // Another look: the picker was for the one before -- it keeps its pick.
    if (effectPickerOpen() && paletteId != m_pickerSnapshot.paletteId)
        closeEffectPicker(true);
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
        emit lookFocusChanged(QLCPalette::invalidId());
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

    // Same "where is it used" as the delete question (ProgrammerController).
    QSet<quint32> usingScenes;
    if (ProgrammerController *pc = m_doc->programmer())
        for (const ProgrammerController::Usage &u : pc->paletteUsage(paletteId))
            if (u.kind == ProgrammerController::Usage::SceneLook)
                usingScenes.insert(u.ownerId);
    const int uses = usingScenes.size();
    QString info = folder.isEmpty() ? QString() : folder + "  •  ";
    info += tr("used by %n scene(s)", "", uses);
    m_title->setText(info);

    switch (p->type())
    {
    case QLCPalette::Color:
    {
        const QColor rgb = p->rgbValue();
        m_colorDialog->setCurrentColor(rgb);
        m_redSlider->blockSignals(true); m_greenSlider->blockSignals(true); m_blueSlider->blockSignals(true);
        m_redSlider->setValue(rgb.red());
        m_greenSlider->setValue(rgb.green());
        m_blueSlider->setValue(rgb.blue());
        m_redSlider->blockSignals(false); m_greenSlider->blockSignals(false); m_blueSlider->blockSignals(false);

        // Extra emitters: load stored wauv, or default White to the additive
        // auto value (min of R,G,B) and Amber/UV to 0.
        const QColor wauv = p->wauvValue();
        const int autoW = qMin(rgb.red(), qMin(rgb.green(), rgb.blue()));
        m_whiteSlider->blockSignals(true); m_amberSlider->blockSignals(true); m_uvSlider->blockSignals(true);
        m_whiteSlider->setValue(wauv.isValid() ? wauv.red()   : autoW);
        m_amberSlider->setValue(wauv.isValid() ? wauv.green() : 0);
        m_uvSlider->setValue   (wauv.isValid() ? wauv.blue()  : 0);
        m_whiteSlider->blockSignals(false); m_amberSlider->blockSignals(false); m_uvSlider->blockSignals(false);
        updateColorWarning();

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
        {
            foreach (QLCCapability *cap, ich->capabilities())
                regions.append({ int(cap->min()), int(cap->max()), cap->name() });
        }
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
        m_xyPad->update();
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

        // Named capabilities for gobo/shutter, from the look's fixtures or,
        // with none that can, from any fixture type in the workspace.
        int chGroup = -1;
        if (p->type() == QLCPalette::Gobo)         chGroup = QLCChannel::Gobo;
        else if (p->type() == QLCPalette::Shutter) chGroup = QLCChannel::Shutter;

        m_singleSources = (chGroup >= 0) ? singleChannelSources(chGroup)
                                         : QList<ChannelSource>();
        m_singleSourceCombo->clear();
        int source = 0;
        for (int i = 0; i < m_singleSources.size(); i++)
        {
            m_singleSourceCombo->addItem(m_singleSources.at(i).label);
            if (m_singleSources.at(i).key == m_singleSourceKey)
                source = i;
        }
        m_singleSourceCombo->setCurrentIndex(source);
        const bool choose = m_singleSources.size() > 1;
        m_singleSourceLabel->setVisible(choose);
        m_singleSourceCombo->setVisible(choose);
        m_singleChannel = m_singleSources.isEmpty() ? NULL
                                                    : m_singleSources.at(source).channel;
        fillSingleCapabilities(p->intValue1());
        updateSinglePreview(p->intValue1());
        m_stack->setCurrentIndex(m_pageSingle);
        setMaximumHeight(340);
        break;
    }
    }

    /* This look's own fades, with a context scene -- by the same rule as
       the canvas (LookFades): an effect has only its release (out), a
       colour feeding an effect has none, anything else both. */
    const bool fadeIn = LookFades::hasFadeIn(m_doc, m_contextScene, m_paletteId);
    const bool fadeOut = LookFades::hasFadeOut(m_doc, m_contextScene, m_paletteId);
    if (m_contextScene != NULL && (fadeIn || fadeOut))
    {
        m_fadeInLabel->setVisible(fadeIn);
        m_fadeInSpin->setVisible(fadeIn);
        m_fadeOutLabel->setVisible(fadeOut);
        m_fadeOutSpin->setVisible(fadeOut);
        m_fadeOutLabel->setText(fadeIn ? tr("out") : tr("Release"));
        m_fadeOutSpin->setSpecialValueText(LookFades::defaultLabel(m_doc, m_paletteId));
        m_fadeInSpin->setMs(m_contextScene->paletteFadeIn(m_paletteId));
        m_fadeOutSpin->setMs(m_contextScene->paletteFadeOut(m_paletteId));
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
    emit lookFocusChanged(paletteId);

    /* A new effect look, nothing picked yet: picking is the first thing to
       do, so the picker opens itself (queued: after the editor is shown). */
    if (p != nullptr && p->type() == QLCPalette::Effect && p->scriptPath().isEmpty()
        && p->wledEffect().isEmpty() && isVisible())
    {
        QPointer<LookEditor> me(this);
        const quint32 id = paletteId;
        QTimer::singleShot(0, this, [me, id]() {
            if (me && me->paletteId() == id && !me->effectPickerOpen())
                me->openEffectPicker();
        });
    }
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
        {
            foreach (quint32 fid, g->fixtureList())
                fixtures.insert(fid);
        }
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
        if (!p->wledEffect().isEmpty()) { autoName = tr("WLED %1").arg(p->wledEffect()); break; }
        const QString path = p->scriptPath();
        if (!path.isEmpty() && m_doc->effectScriptRunner())
        {
            const EffectScriptCache *cache = m_doc->effectScriptRunner()->cache();
            // Its name as listed ("Fire", "Plasma (RGB Matrix)"), else its file's.
            QString n = cache->scriptMeta(EffectScriptCache::portableName(path)).displayName;
            if (n.isEmpty())
                n = cache->nameFromPath(path);
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

    // -1 is the default (the scene's fade; for an effect's release, snap).
    const int inMs  = !m_fadeInSpin->isHidden() ? m_fadeInSpin->ms() : -1;   // an effect has none
    const int outMs = m_fadeOutSpin->ms();
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
    // Keep the numbered R/G/B sliders (fader 1-3 on a mapped control
    // surface) in sync with the dialog's own picker.
    m_redSlider->blockSignals(true); m_greenSlider->blockSignals(true); m_blueSlider->blockSignals(true);
    m_redSlider->setValue(c.red());
    m_greenSlider->setValue(c.green());
    m_blueSlider->setValue(c.blue());
    m_redSlider->blockSignals(false); m_greenSlider->blockSignals(false); m_blueSlider->blockSignals(false);

    // Auto-derive White additively (W = min(R,G,B), RGB kept) on colour change.
    const int autoW = qMin(c.red(), qMin(c.green(), c.blue()));
    m_whiteSlider->blockSignals(true);
    m_whiteSlider->setValue(autoW);
    m_whiteSlider->blockSignals(false);
    commitColor();
}

void LookEditor::slotRgbSliderChanged()
{
    if (m_loading)
        return;
    // Keep the dialog's picker in sync with the numbered sliders (e.g. driven
    // by a control-surface fader) without re-triggering slotColorChanged().
    const QColor c(m_redSlider->value(), m_greenSlider->value(), m_blueSlider->value());
    m_colorDialog->blockSignals(true);
    m_colorDialog->setCurrentColor(c);
    m_colorDialog->blockSignals(false);
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
    const QColor rgb(m_redSlider->value(), m_greenSlider->value(), m_blueSlider->value());
    // wauv encodes White=red, Amber=green, UV=blue (see QLCPalette::Color).
    const QColor wauv(m_whiteSlider->value(), m_amberSlider->value(), m_uvSlider->value());
    p->setValue(QLCPalette::colorToString(rgb, wauv));
    maybeAutoName(p);
    updateColorWarning();
    m_doc->setModified();
    emit paletteChanged(m_paletteId);
}

QString LookEditor::darkColourWarning(const QColor &rgb, const QColor &wauv)
{
    int level = qMax(rgb.red(), qMax(rgb.green(), rgb.blue()));
    if (wauv.isValid())
        level = qMax(level, qMax(wauv.red(), qMax(wauv.green(), wauv.blue())));
    // A fifth of full is dim but deliberate; below that it reads as "off".
    if (level >= 51)
        return QString();
    if (level == 0)
        return tr("This colour is black: fixtures that mix colour (RGB, RGBW) "
                  "will be dark whatever their dimmer is set to.");
    return tr("This colour is at %1% brightness: fixtures that mix colour (RGB, "
              "RGBW) will be nearly dark -- their dimmer scales the colour, it "
              "cannot raise it. Raise the picker's brightness (Val) or the "
              "sliders. Colour-wheel fixtures only take the hue and still shine.")
               .arg(qMax(1, qRound(level * 100.0 / 255.0)));
}

void LookEditor::updateColorWarning()
{
    const QString text = darkColourWarning(
        QColor(m_redSlider->value(), m_greenSlider->value(), m_blueSlider->value()),
        QColor(m_whiteSlider->value(), m_amberSlider->value(), m_uvSlider->value()));
    m_colorWarning->setText(text);
    m_colorWarning->setVisible(!text.isEmpty());
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
        {
            foreach (quint32 fid, g->fixtureList())
                fixtures.insert(fid);
        }
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

QString LookEditor::slotPicture(QLCCapability *cap, QString *swapName) const
{
    if (cap == NULL)
        return QString();
    // The fixtures of the type being browsed, for their own swapped glass.
    MonitorProperties *props = m_doc->monitorProperties();
    for (const ChannelSource &src : m_singleSources)
    {
        if (src.channel != m_singleChannel || props == NULL)
            continue;
        for (quint32 fid : src.fixtures)
        {
            const QString swap = props->goboSwap(fid, src.channelIndex, cap->min());
            const QString path = swap.isEmpty() ? QString() : GoboLibrary::resolve(swap);
            if (path.isEmpty())
                continue;
            if (swapName != NULL)
                *swapName = QFileInfo(swap).completeBaseName();
            return path;
        }
    }
    return cap->presetType() == QLCCapability::Picture ? cap->resource(0).toString() : QString();
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
                const QString picture = slotPicture(cap);
                if (!picture.isEmpty())
                    px = QPixmap(picture);
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

void LookEditor::slotSingleSourcePicked(int index)
{
    if (index < 0 || index >= m_singleSources.size())
        return;
    m_singleSourceKey = m_singleSources.at(index).key;
    m_singleChannel = m_singleSources.at(index).channel;
    fillSingleCapabilities(m_singleSlider->value());
    updateSinglePreview(m_singleSlider->value());
}

void LookEditor::fillSingleCapabilities(int v)
{
    m_singleCombo->clear();
    if (m_singleChannel == NULL)
    {
        m_singleCombo->setVisible(false);
        return;
    }
    int sel = -1, idx = 0;
    foreach (QLCCapability *cap, m_singleChannel->capabilities())
    {
        /* Show a gobo/effect thumbnail next to each named capability -- the
           glass actually fitted: a slot a unit has swapped (Gobo Wheels…)
           shows that picture, and says so. */
        QString swapName;
        const QString picture = slotPicture(cap, &swapName);
        const QString name = swapName.isEmpty() ? cap->name()
                                                : tr("%1 — %2").arg(cap->name(), swapName);
        QIcon icon;
        if (!picture.isEmpty())
        {
            const QPixmap px(picture);
            if (px.isNull() == false)
                icon = QIcon(px);
        }
        m_singleCombo->addItem(icon, name,
                               (int(cap->min()) + int(cap->max())) / 2);
        if (v >= cap->min() && v <= cap->max())
            sel = idx;
        idx++;
    }
    if (sel >= 0)
        m_singleCombo->setCurrentIndex(sel);
    m_singleCombo->setIconSize(QSize(32, 32));
    m_singleCombo->setVisible(true);
}

QList<LookEditor::ChannelSource> LookEditor::singleChannelSources(int group) const
{
    // The look's own targets first; a look with none that can (or no look at
    // all) falls back to the whole workspace.
    QList<quint32> ids;
    if (m_contextScene != NULL)
    {
        QSet<quint32> seen;
        foreach (quint32 fid, m_contextScene->fixtures())
            if (!seen.contains(fid)) { seen.insert(fid); ids << fid; }
        foreach (quint32 gid, m_contextScene->fixtureGroups())
        {
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g == NULL)
                continue;
            foreach (quint32 fid, g->fixtureList())
            {
                if (!seen.contains(fid)) { seen.insert(fid); ids << fid; }
            }
        }
    }

    auto gather = [this, group](const QList<quint32> &fids) {
        QList<ChannelSource> out;
        QHash<QString, int> count;
        foreach (quint32 fid, fids)
        {
            Fixture *f = m_doc->fixture(fid);
            if (f == NULL)
                continue;
            // The channel QLCPalette writes the value to (per fixture mode).
            const quint32 chn = f->fixtureMode() == NULL ? QLCChannel::invalid()
                : f->fixtureMode()->channelNumber(QLCChannel::Group(group), QLCChannel::MSB);
            const QLCChannel *ch = (chn == QLCChannel::invalid()) ? NULL : f->channel(chn);
            if (ch == NULL)
                continue;
            QString key, label;
            if (f->fixtureDef() != NULL)
            {
                label = QString("%1 %2").arg(f->fixtureDef()->manufacturer(),
                                             f->fixtureDef()->model());
                key = label;
                if (f->fixtureMode() != NULL)
                    key += QLatin1Char('/') + f->fixtureMode()->name();
            }
            else
            {
                label = key = f->name();
            }
            if (!count.contains(key))
            {
                ChannelSource src;
                src.key = key;
                src.label = label;
                src.channel = ch;
                src.channelIndex = chn;
                out << src;
            }
            for (ChannelSource &src : out)
                if (src.key == key)
                    src.fixtures << fid;
            count[key]++;
        }
        std::stable_sort(out.begin(), out.end(),
                         [&count](const ChannelSource &a, const ChannelSource &b) {
            return count.value(a.key) > count.value(b.key);
        });
        for (int i = 0; i < out.size(); i++)
        {
            const int n = count.value(out.at(i).key);
            out[i].label = n > 1 ? tr("%1 (%2 fixtures)").arg(out.at(i).label).arg(n)
                                 : out.at(i).label;
        }
        return out;
    };

    QList<ChannelSource> sources = gather(ids);
    if (sources.isEmpty())
    {
        QList<quint32> all;
        foreach (Fixture *f, m_doc->fixtures())
            all << f->id();
        sources = gather(all);
    }
    return sources;
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
        {
            foreach (quint32 fid, g->fixtureList())
                fixtures.insert(fid);
        }
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
    updateEffectHeader();
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
        // The speed choice too: on a master the rate reads "per beat".
        const int speedKey = gp ? int(gp->effectParamValues().value(QStringLiteral("__speedMaster"),
                                                                    SpeedMasters::Inherit))
                                : int(SpeedMasters::Inherit);
        // ...and whether the host spread is on: it hides the script's own.
        const bool spreadKey = gp && SpreadLayer::fromParams(gp->effectParamValues()).isActive();
        const QString buildKey = QString::number(m_paletteId) + QLatin1Char('|') + gScriptPath
                                 + QLatin1Char('|') + QString::number(speedKey)
                                 + QLatin1Char('|') + QString::number(int(spreadKey));
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
        dv->addWidget(new QLabel(tr("(pick an effect from the list to set it up)"), m_effectDynWidget));
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

    /* What the effect IS -- its description, what it works with, its notes,
       which generator does the work, and what each setting means. Shown in
       the picker (where you choose) and behind the header's "?" (hover, or a
       window on a click), not over the settings while working WITH it. */
    {
        const QString desc  = tmpScript.description();
        const QString notes = tmpScript.notes();
        QMap<QString, QString> ftLabels;
        ftLabels["moving"]  = tr("Moving Head");
        ftLabels["rgb"]     = tr("RGB/LED Wash");
        ftLabels["dimmer"]  = tr("Dimmer");
        ftLabels["shutter"] = tr("Strobe/Shutter");
        QStringList human;
        for (const QString &t : tmpScript.fixtureTypes())
            human << ftLabels.value(t, t);
        const QString engine = tmpScript.name().isEmpty()
            ? QFileInfo(scriptPath).completeBaseName() : tmpScript.name();
        const QString preset = p->effectPreset();

        QString summary;
        if (!desc.isEmpty())
            summary += QStringLiteral("<p><i>%1</i></p>").arg(desc.toHtmlEscaped());
        if (!human.isEmpty())
            summary += tr("<p><b>Works with:</b> %1</p>").arg(human.join(", ").toHtmlEscaped());
        const QString generator = preset.isEmpty()
            ? tr("<p>Generator: <b>%1</b></p>").arg(engine.toHtmlEscaped())
            : tr("<p>Effect <b>%1</b> &nbsp;·&nbsp; generator: <b>%2</b></p>")
                  .arg(preset.toHtmlEscaped(), engine.toHtmlEscaped());
        const QString notesHtml = notes.isEmpty() ? QString()
            : QStringLiteral("<p>%1</p>").arg(notes.toHtmlEscaped().replace('\n', QLatin1String("<br>")));

        QString settings;
        for (const EffectScript::ParamDef &pd : tmpScript.paramDefs())
        {
            QString range;
            if (!pd.enumValues.isEmpty())
                range = pd.enumValues.join(QStringLiteral(" / "));
            else if (pd.type == QLatin1String("text"))
                range = tr("words (\"%1\")").arg(pd.defaultText);
            else if (pd.type == QLatin1String("path"))
                range = tr("a drawn path");
            else
                range = tr("%1 to %2, default %3").arg(pd.min).arg(pd.max).arg(pd.defaultValue);
            settings += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td></tr>")
                            .arg(pd.name.toHtmlEscaped(), pd.description.toHtmlEscaped(),
                                 range.toHtmlEscaped());
        }
        if (!settings.isEmpty())
            settings = tr("<p><b>Settings</b></p>") + QStringLiteral(
                "<table cellspacing=\"0\" cellpadding=\"3\">%1</table>").arg(settings);

        const QString name = m_effectScriptCombo->currentText().trimmed();
        m_effectSummaryHtml = summary + notesHtml;
        m_effectDetailsHtml = QStringLiteral("<h3>%1</h3>").arg(name.toHtmlEscaped())
                              + summary + generator + notesHtml + settings;
        m_effectInfoButton->setToolTip(QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped())
                                       + summary + generator
                                       + tr("<p><i>Click for every setting.</i></p>"));
        m_effectInfoButton->setVisible(true);
        if (m_effectPickerInfo != nullptr)
            m_effectPickerInfo->setHtml(m_effectDetailsHtml);
        if (m_effectDetails != nullptr && m_effectDetails->isVisible())
            showEffectDetails();                  // follow a new pick
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

    // On a speed master (its own choice, else its look's), the effect's time
    // counts the master's beats: "speed"/"rate" read as cycles per beat.
    int speedChoice = int(p->effectParamValues().value(QStringLiteral("__speedMaster"),
                                                       SpeedMasters::Inherit));
    const int lookSpeed = m_contextScene != nullptr ? m_contextScene->speedMaster()
                                                    : int(SpeedMasters::Inherit);
    const int effectiveSpeed = speedChoice != SpeedMasters::Inherit ? speedChoice : lookSpeed;
    const bool onMaster = SpeedMasters::isMaster(effectiveSpeed)
                          || effectiveSpeed == SpeedMasters::MainClock;

    // The host spread, when on, stands in for the script's own spread params.
    const SpreadLayer::Settings hostSpread = SpreadLayer::fromParams(p->effectParamValues());
    const QStringList supersededBySpread = tmpScript.takesHostSpread() && hostSpread.isActive()
                                           ? tmpScript.hostSpreadParams() : QStringList();

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
            if (supersededBySpread.contains(pd.name))
                continue;
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
            else if (pd.type == QLatin1String("text"))
            {
                // Words for the effect to show → a line to type them in; taken
                // as they are typed, so the rig shows them as they are.
                QLineEdit *le = new QLineEdit(m_effectDynWidget);
                const QString existing = p->effectStringParams().contains(pd.name)
                                         ? p->effectStringParam(pd.name) : pd.defaultText;
                le->setText(existing);
                le->setToolTip(pd.description);
                le->setObjectName(QStringLiteral("effectText_") + pd.name);
                const QString paramName = pd.name;
                connect(le, &QLineEdit::textChanged,
                        this, [this, paramName](const QString &text) {
                    QLCPalette *pp = m_doc->palette(m_paletteId);
                    if (!pp) return;
                    pp->setEffectStringParam(paramName, text);
                    m_doc->setModified();
                    emit paletteValueChanged(m_paletteId);
                });
                form->addRow(pd.name, le);
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
                const bool perBeat = onMaster && (pd.name == QLatin1String("speed")
                                                  || pd.name == QLatin1String("rate"));
                if (perBeat)
                    sl->setToolTip(tr("Cycles per beat of %1: 1 = one a beat, 0.25 = one a bar.")
                                       .arg(m_doc->speedMasters()->assignmentName(effectiveSpeed)));
                form->addRow(perBeat ? tr("%1 (per beat)").arg(pd.name) : pd.name, row);
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

    // --- Spread (per look) ---
    // How the effect's cycle is spread across its heads, worked out by the host
    // (SPEED_SPREAD_DESIGN.md, S3) for any effect that takes it.
    if (tmpScript.takesHostSpread())
    {
        QLabel *hdr = new QLabel(tr("Spread"), m_effectDynWidget);
        hdr->setStyleSheet("font-weight: bold;");
        dv->addWidget(hdr);
        QWidget *row = new QWidget(m_effectDynWidget);
        row->setObjectName(QStringLiteral("effectSpread"));
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);

        QDoubleSpinBox *amount = new QDoubleSpinBox(row);
        amount->setObjectName(QStringLiteral("spreadAmount"));
        amount->setRange(0.0, 8.0);
        amount->setSingleStep(0.25);
        amount->setDecimals(2);
        amount->setSuffix(tr(" cycles"));
        amount->setSpecialValueText(tr("its own"));
        amount->setValue(hostSpread.amount);
        amount->setToolTip(tr("How far the effect's cycle runs across its heads: 1 = one full "
                              "wave across them, 0.5 = half, 0 = the effect's own spread."));
        QComboBox *axis = new QComboBox(row);
        axis->setObjectName(QStringLiteral("spreadAxis"));
        axis->addItems({ tr("In look order"), tr("Across columns"), tr("Down rows"),
                         tr("Stage left to right"), tr("Front to back"), tr("By height"),
                         tr("Out from the target"), tr("Around the target") });
        axis->setCurrentIndex(int(hostSpread.axis));
        axis->setToolTip(tr("What the heads are put in order by: the look's list, the head "
                            "layout's columns or rows, where they stand on stage, or how far "
                            "out from / which way round the look's Aim target (a ripple, a "
                            "sweep). Heads at the same place share a phase."));
        QComboBox *dir = new QComboBox(row);
        dir->setObjectName(QStringLiteral("spreadDirection"));
        dir->addItems({ tr("Forward"), tr("Reverse"), tr("Centre out"), tr("Ends in") });
        dir->setCurrentIndex(int(hostSpread.direction));
        QSpinBox *wings = new QSpinBox(row);
        wings->setObjectName(QStringLiteral("spreadWings"));
        wings->setRange(1, 8);
        wings->setPrefix(tr("wings "));
        wings->setValue(hostSpread.wings);
        wings->setToolTip(tr("Split the heads into mirrored parts, each running the spread"));
        QSpinBox *blocks = new QSpinBox(row);
        blocks->setObjectName(QStringLiteral("spreadBlocks"));
        blocks->setRange(1, 32);
        blocks->setPrefix(tr("blocks of "));
        blocks->setValue(hostSpread.blocks);
        blocks->setToolTip(tr("Neighbouring heads that share a phase"));
        for (QWidget *w : { (QWidget *)axis, (QWidget *)dir, (QWidget *)wings, (QWidget *)blocks })
            w->setEnabled(hostSpread.isActive());

        auto store = [this, amount, axis, dir, wings, blocks]() {
            if (m_loading) return;
            QLCPalette *pp = m_doc->palette(m_paletteId);
            if (!pp || pp->type() != QLCPalette::Effect) return;
            const bool wasOn = SpreadLayer::fromParams(pp->effectParamValues()).isActive();
            pp->setEffectParamValue(SpreadLayer::paramName("Amount"), amount->value());
            pp->setEffectParamValue(SpreadLayer::paramName("Axis"), axis->currentIndex());
            pp->setEffectParamValue(SpreadLayer::paramName("Direction"), dir->currentIndex());
            pp->setEffectParamValue(SpreadLayer::paramName("Wings"), wings->value());
            pp->setEffectParamValue(SpreadLayer::paramName("Blocks"), blocks->value());
            m_doc->setModified();
            emit paletteValueChanged(m_paletteId);
            // Turned on or off: the script's own spread shows or hides.
            if (wasOn != (amount->value() != 0))
                QTimer::singleShot(0, this, &LookEditor::rebuildEffectDynWidget);
        };
        connect(amount, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [store](double) { store(); });
        connect(axis, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [store](int) { store(); });
        connect(dir, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [store](int) { store(); });
        connect(wings, QOverload<int>::of(&QSpinBox::valueChanged), this, [store](int) { store(); });
        connect(blocks, QOverload<int>::of(&QSpinBox::valueChanged), this, [store](int) { store(); });
        rl->addWidget(amount);
        rl->addWidget(axis);
        rl->addWidget(dir);
        rl->addWidget(wings);
        rl->addWidget(blocks);
        rl->addStretch(1);
        dv->addWidget(row);

        // The spread, seen: the look's heads coloured by phase.
        if (m_contextScene != nullptr)
        {
            SpreadPreview *preview = new SpreadPreview(m_doc, m_contextScene->id(), m_paletteId,
                                                       m_effectDynWidget);
            preview->setVisible(hostSpread.isActive());
            dv->addWidget(preview);
            connect(this, &LookEditor::paletteValueChanged, preview, [preview](quint32) {
                preview->refresh();
            });
            connect(amount, QOverload<double>::of(&QDoubleSpinBox::valueChanged), preview,
                    [preview](double v) { preview->setVisible(v != 0); });
        }
    }

    // --- Speed master (per look) ---
    // What the effect's time runs on (SPEED_SPREAD_DESIGN.md, S2): the look's
    // choice (which a chase step, chase or collection may hand down), its own
    // speed, the main beat clock, or a speed master.
    {
        QWidget *row = new QWidget(m_effectDynWidget);
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 6, 0, 0);
        rl->addWidget(new QLabel(tr("Speed:"), row));
        QComboBox *cb = new QComboBox(row);
        cb->setObjectName(QStringLiteral("effectSpeedMaster"));
        SpeedMasters *sm = m_doc->speedMasters();
        cb->addItem(lookSpeed == SpeedMasters::Inherit
                        ? tr("As the look (own speed unless a chase sets one)")
                        : tr("As the look (%1)").arg(sm->assignmentName(lookSpeed)),
                    int(SpeedMasters::Inherit));
        cb->addItem(sm->assignmentName(SpeedMasters::Free), int(SpeedMasters::Free));
        cb->addItem(sm->assignmentName(SpeedMasters::MainClock), int(SpeedMasters::MainClock));
        for (int i = 0; i < m_doc->speedMasters()->count(); ++i)
            cb->addItem(sm->name(i), SpeedMasters::Master1 + i);
        cb->setCurrentIndex(qMax(0, cb->findData(speedChoice)));
        cb->setToolTip(tr("What this effect's speed follows. On the main clock or a speed "
                          "master its speed counts in cycles per beat and it stays on the beat; "
                          "a master's tap, rate and pause move it."));
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, cb](int) {
            if (m_loading) return;
            QLCPalette *pp = m_doc->palette(m_paletteId);
            if (!pp || pp->type() != QLCPalette::Effect) return;
            pp->setEffectParamValue(QStringLiteral("__speedMaster"), cb->currentData().toInt());
            m_doc->setModified();
            emit paletteValueChanged(m_paletteId);
            // Relabel the speed for beats or seconds (not from inside the
            // combo's own signal: the rebuild deletes it).
            QTimer::singleShot(0, this, &LookEditor::rebuildEffectDynWidget);
        });
        rl->addWidget(cb);
        rl->addStretch(1);
        dv->addWidget(row);
    }

    // --- One-shot length override (per look) ---
    // A oneshot effect runs once over a resolved duration; this pins an explicit
    // length for THIS look (independent per effect). auto (0) = follow the cue /
    // chase step, then the effect's own default.
    if (tmpScript.isOneShot())
    {
        QWidget *row = new QWidget(m_effectDynWidget);
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 6, 0, 0);
        rl->addWidget(new QLabel(tr("One-shot length:"), row));
        QDoubleSpinBox *sp = new QDoubleSpinBox(row);
        sp->setRange(0.0, 120.0);
        sp->setSingleStep(0.1);
        sp->setDecimals(1);
        sp->setSuffix(tr(" s"));
        sp->setSpecialValueText(tr("auto"));   // 0 → follow the cue / default
        sp->setToolTip(tr("How long this one-shot runs, for this look. auto = follow the "
                          "cue / chase step (or the effect's default); set a time to override."));
        sp->setValue(p->effectParamValues().value(QStringLiteral("__durationMs"), 0.0) / 1000.0);
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double s) {
            if (m_loading) return;
            QLCPalette *pp = m_doc->palette(m_paletteId);
            if (!pp || pp->type() != QLCPalette::Effect) return;
            pp->setEffectParamValue(QStringLiteral("__durationMs"), s * 1000.0);  // 0 = auto
            m_doc->setModified();
            emit paletteValueChanged(m_paletteId);
        });
        rl->addWidget(sp);
        rl->addStretch(1);
        dv->addWidget(row);
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
        node.basename = EffectScriptCache::portableName(scache->scriptPath(n));
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

    /* WLED's own effects, where the show has WLED fixtures or the WLED
       definitions are installed: run by the device on WLED fixtures, by their
       port (if any) on everything else. */
    const QStringList wled = wledEffectNames();
    if (!wled.isEmpty())
    {
        addHeader(QStringLiteral("WLED"));
        const QString curWled = p ? p->wledEffect() : QString();
        for (const QString &fx : wled)
        {
            m_effectScriptCombo->addItem(QStringLiteral("  ") + fx, fx);
            const int widx = m_effectScriptCombo->count() - 1;
            m_effectScriptCombo->setItemData(widx, EffectKindWled, EffectKindRole);
            const bool ported = !scache->scriptPathForWled(fx).isEmpty();
            m_effectScriptCombo->setItemData(widx, ported
                ? tr("WLED's %1, run by WLED fixtures themselves; others run its port").arg(fx)
                : tr("WLED's %1, run by WLED fixtures themselves (no port yet: "
                     "other fixtures are left as the look sets them)").arg(fx), Qt::ToolTipRole);
            if (!curWled.isEmpty() && fx.compare(curWled, Qt::CaseInsensitive) == 0)
                selectIdx = widx;
        }
    }

    m_effectScriptCombo->setCurrentIndex(selectIdx);
    if (m_effectBrowser != nullptr)
        m_effectBrowser->reload();
}

QStringList LookEditor::wledEffectNames() const
{
    // A WLED fixture in the show says what its firmware's list is...
    foreach (Fixture *fx, m_doc->fixtures())
        if (WledFixture::runsEffects(fx))
            return WledFixture::effectNames(fx);
    // ...else the installed WLED definition's.
    QLCFixtureDef *def = m_doc->fixtureDefCache()->fixtureDef(QStringLiteral("WLED"),
                                                             QStringLiteral("Lantern"));
    if (def == nullptr)
        def = m_doc->fixtureDefCache()->fixtureDef(QStringLiteral("WLED"), QStringLiteral("Effect Mode"));
    QStringList out;
    if (def != nullptr)
    {
        foreach (QLCChannel *ch, def->channels())
        {
            if (ch->name() == QLatin1String("Effect"))
            {
                foreach (QLCCapability *cap, ch->capabilities())
                    out << cap->name();
            }
        }
    }
    return out;
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

    if (kind == EffectKindWled)
    {
        /* One of WLED's own: WLED fixtures run it (QLCPalette::wledEffect());
           everything else runs its port, or -- with none yet -- the stand-in
           that only gives the look WLED's controls. */
        QString path = scache->scriptPathForWled(name);
        if (path.isEmpty())
            path = scache->scriptPath(QStringLiteral("WLED device effect"));
        p->setScriptPath(path);
        p->setEffectPreset(QString());
        p->setWledEffect(name);
        p->clearEffectParamValues();
        carryOverParams(p, path);
    }
    else if (kind == EffectKindPreset)
    {
        // Stamp the preset: engine script + pinned param values + identity. The
        // preset's own values win; shared params it doesn't pin still carry over.
        const EffectPresetCache::Preset pr =
            m_doc->effectScriptRunner()->presetCache()->preset(name);
        const QString path = scache->scriptPath(pr.script);
        p->setScriptPath(path);
        p->setEffectPreset(pr.name);
        p->setWledEffect(QString());
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
        p->setWledEffect(QString());
        p->clearEffectParamValues();
        carryOverParams(p, path);
    }
    // Wipe old input bindings since the effect changed.
    p->clearEffectInputBindings();
    maybeAutoName(p);


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

    const QString base = EffectScriptCache::portableName(p->scriptPath());
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
    const QString base = EffectScriptCache::portableName(p->scriptPath());
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

/** Pick an effect from a filterable tree instead of a screen-tall dropdown.
 *
 *  Built by walking the combo rather than re-reading the caches: the combo is
 *  already the assembled, sorted, correctly-grouped list, and rebuilding it
 *  here would be a second copy of that logic to keep in step. Each row carries
 *  its combo index, so choosing one is just setCurrentIndex() and every
 *  existing handler fires exactly as it would have.
 */
void LookEditor::slotBrowseEffects()
{
    if (m_effectScriptCombo == nullptr)
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Choose an effect"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QLineEdit *filter = new QLineEdit(&dlg);
    filter->setPlaceholderText(tr("Type to filter…"));
    filter->setClearButtonEnabled(true);
    lay->addWidget(filter);

    QTreeWidget *tree = new QTreeWidget(&dlg);
    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    lay->addWidget(tree, 1);

    const int kIndexRole = Qt::UserRole + 1;

    QTreeWidgetItem *category = nullptr;   // current header
    QTreeWidgetItem *script   = nullptr;   // current generator, presets nest under it
    for (int i = 0; i < m_effectScriptCombo->count(); i++)
    {
        const int kind = m_effectScriptCombo->itemData(i, EffectKindRole).toInt();
        // The combo indents with spaces and bullets for visual nesting; the
        // tree provides real nesting, so strip that decoration back off.
        QString text = m_effectScriptCombo->itemText(i);
        text.remove(QStringLiteral("•"));
        text = text.trimmed();

        QTreeWidgetItem *item = nullptr;
        if (kind == EffectKindHeader)
        {
            category = new QTreeWidgetItem(tree);
            category->setText(0, text);
            QFont f = category->font(0); f.setBold(true); category->setFont(0, f);
            category->setFlags(category->flags() & ~Qt::ItemIsSelectable);
            script = nullptr;
            continue;
        }
        if (kind == EffectKindPreset && script != nullptr)
            item = new QTreeWidgetItem(script);
        else if (category != nullptr)
            { item = new QTreeWidgetItem(category); script = item; }
        else
            { item = new QTreeWidgetItem(tree); script = item; }

        item->setText(0, text);
        item->setToolTip(0, m_effectScriptCombo->itemData(i, Qt::ToolTipRole).toString());
        item->setData(0, kIndexRole, i);
        if (i == m_effectScriptCombo->currentIndex())
        {
            tree->setCurrentItem(item);
            item->setSelected(true);
        }
    }
    tree->expandAll();

    /* Filtering hides non-matching leaves and any category left with nothing
       under it, so the tree collapses down to what was asked for rather than
       leaving empty headings behind. */
    connect(filter, &QLineEdit::textChanged, &dlg, [tree](const QString &needle) {
        for (int c = 0; c < tree->topLevelItemCount(); c++)
        {
            QTreeWidgetItem *cat = tree->topLevelItem(c);
            int visible = 0;
            for (int s = 0; s < cat->childCount(); s++)
            {
                QTreeWidgetItem *sc = cat->child(s);
                int subVisible = 0;
                for (int pr = 0; pr < sc->childCount(); pr++)
                {
                    QTreeWidgetItem *pi = sc->child(pr);
                    const bool m = needle.isEmpty()
                        || pi->text(0).contains(needle, Qt::CaseInsensitive);
                    pi->setHidden(!m);
                    if (m) subVisible++;
                }
                const bool selfMatch = needle.isEmpty()
                    || sc->text(0).contains(needle, Qt::CaseInsensitive);
                sc->setHidden(!selfMatch && subVisible == 0);
                if (!sc->isHidden()) visible++;
            }
            cat->setHidden(visible == 0);
        }
    });

    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(bb);

    // Double-click picks and closes, which is what everyone tries first.
    connect(tree, &QTreeWidget::itemDoubleClicked, &dlg,
            [&dlg](QTreeWidgetItem *it, int) {
        if (it != nullptr && it->data(0, Qt::UserRole + 1).isValid())
            dlg.accept();
    });

    dlg.resize(460, 520);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QTreeWidgetItem *chosen = tree->currentItem();
    if (chosen == nullptr)
        return;
    const QVariant idx = chosen->data(0, kIndexRole);
    if (idx.isValid())
        m_effectScriptCombo->setCurrentIndex(idx.toInt());
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
