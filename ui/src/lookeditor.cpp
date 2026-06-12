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
#include <QScrollArea>

#include "lookeditor.h"
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

    // Bottom row: stage target combo
    QHBoxLayout *targetRow = new QHBoxLayout();
    targetRow->setContentsMargins(0, 0, 0, 0);
    QLabel *targetLbl = new QLabel(tr("Stage target:"), pantilt);
    targetLbl->setMinimumWidth(90);
    targetRow->addWidget(targetLbl);
    m_targetCombo = new QComboBox(pantilt);
    m_targetCombo->setToolTip(tr("Associate this palette with a named stage target position"));
    targetRow->addWidget(m_targetCombo, 1);
    ptv->addLayout(targetRow);
    ptv->addStretch(1);

    m_pagePanTilt = m_stack->addWidget(pantilt);
    connect(m_xyPad, SIGNAL(positionChanged(QPointF)),
            this, SLOT(slotPanTiltChanged(QPointF)));
    connect(m_targetCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotTargetChanged(int)));

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

    // Dynamic area (params + input bindings) — scroll in case there are many
    m_effectDynScroll = new QScrollArea(effectPage);
    m_effectDynScroll->setWidgetResizable(true);
    m_effectDynScroll->setFrameShape(QFrame::NoFrame);
    m_effectDynWidget = new QWidget(m_effectDynScroll);
    m_effectDynScroll->setWidget(m_effectDynWidget);
    efv->addWidget(m_effectDynScroll, 1);

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

        // Rebuild target combo from current MonitorProperties (targets may have
        // been added/removed since the last time this palette was shown).
        m_targetCombo->blockSignals(true);
        m_targetCombo->clear();
        m_targetCombo->addItem(tr("(none)"), QVariant(StageTarget::invalidId()));
        if (m_doc->monitorProperties())
        {
            for (StageTarget *t : m_doc->monitorProperties()->stageTargets())
                m_targetCombo->addItem(t->name(), QVariant(t->id()));
        }
        // Pre-select the palette's linked target
        int sel = 0;
        for (int i = 0; i < m_targetCombo->count(); ++i)
        {
            if (m_targetCombo->itemData(i).toUInt() == p->stageTargetId())
            { sel = i; break; }
        }
        m_targetCombo->setCurrentIndex(sel);
        m_targetCombo->blockSignals(false);

        // XY pad is the fallback; disable it when a target is driving position.
        m_xyPad->setEnabled(m_targetCombo->currentData().toUInt() == StageTarget::invalidId());

        m_stack->setCurrentIndex(m_pagePanTilt);
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

void LookEditor::slotTargetChanged(int index)
{
    if (m_loading)
        return;
    QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == NULL || p->type() != QLCPalette::PanTilt)
        return;
    p->setStageTargetId(m_targetCombo->itemData(index).toUInt());
    m_xyPad->setEnabled(m_targetCombo->itemData(index).toUInt() == StageTarget::invalidId());
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
    // Delete the current dynamic widget's children and rebuild.
    delete m_effectDynWidget;
    m_effectDynWidget = new QWidget(m_effectDynScroll);
    m_effectDynScroll->setWidget(m_effectDynWidget);

    QVBoxLayout *dv = new QVBoxLayout(m_effectDynWidget);
    dv->setContentsMargins(0, 4, 0, 4);
    dv->setSpacing(6);

    QLCPalette *p = m_doc->palette(m_paletteId);
    if (!p || p->type() != QLCPalette::Effect)
        return;

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
            QWidget *row = new QWidget(m_effectDynWidget);
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);

            // Slider: stores scaled integer (0..1000 → min..max)
            QSlider *sl = new QSlider(Qt::Horizontal, row);
            sl->setRange(0, 1000);
            double cur = p->effectParamValues().value(pd.name, pd.defaultValue);
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
            QString bindText = bound
                ? tr("Univ %1 Ch %2")
                  .arg(binds.value(id.name).first + 1)
                  .arg(binds.value(id.name).second + 1)
                : tr("(unbound)");

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
    emit paletteChanged(m_paletteId);
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
