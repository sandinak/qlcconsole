/*
  Q Light Controller Plus
  effectdesigner.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QAbstractItemModel>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QSettings>
#include <QSplitter>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QPixmap>
#include <QIcon>
#include <algorithm>

#include "effectdesigner.h"
#include "effectbrowser.h"
#include "lookeditor.h"
#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectpresetcache.h"
#include "functionparent.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#define SETTINGS_TRYON  QStringLiteral("effectdesigner/tryOn")
#define SETTINGS_SPLIT  QStringLiteral("effectdesigner/split")

using namespace ShowTree;

namespace
{
    /** The browser's combo roles (lookeditor.cpp / effectbrowser.cpp). */
    const int KindRole = Qt::UserRole + 1;
    const int KindHeader = 0;
}

/****************************************************************************
 * Copying an effect
 ****************************************************************************/

void EffectDesigner::copyEffect(const QLCPalette *from, QLCPalette *to)
{
    if (from == nullptr || to == nullptr || from == to)
        return;
    to->setValues(from->values());
    to->setScriptPath(from->scriptPath());
    to->setPersistent(from->persistent());
    to->setEffectPreset(from->effectPreset());
    to->setWledEffect(from->wledEffect());
    to->setEffectInputBindings(from->effectInputBindings());
    for (const QString &slot : to->effectPaletteBindings().keys())
        to->clearEffectPaletteBinding(slot);
    const QMap<QString, quint32> pb = from->effectPaletteBindings();
    for (auto it = pb.constBegin(); it != pb.constEnd(); ++it)
        to->setEffectPaletteBinding(it.key(), it.value());
    for (const QString &slot : to->effectTargetBindings().keys())
        to->clearEffectTargetBinding(slot);
    const QMap<QString, quint32> tb = from->effectTargetBindings();
    for (auto it = tb.constBegin(); it != tb.constEnd(); ++it)
        to->setEffectTargetBinding(it.key(), it.value());
    to->setEffectParamValues(from->effectParamValues());     // clears the strings
    const QMap<QString, QString> sp = from->effectStringParams();
    for (auto it = sp.constBegin(); it != sp.constEnd(); ++it)
        to->setEffectStringParam(it.key(), it.value());
}

bool EffectDesigner::sameEffect(const QLCPalette *a, const QLCPalette *b)
{
    if (a == nullptr || b == nullptr)
        return a == b;
    return a->scriptPath() == b->scriptPath() && a->effectPreset() == b->effectPreset()
           && a->wledEffect() == b->wledEffect() && a->persistent() == b->persistent()
           && a->effectParamValues() == b->effectParamValues()
           && a->effectStringParams() == b->effectStringParams()
           && a->effectInputBindings() == b->effectInputBindings()
           && a->effectPaletteBindings() == b->effectPaletteBindings()
           && a->effectTargetBindings() == b->effectTargetBindings();
}

/****************************************************************************
 * The designer
 ****************************************************************************/

EffectDesigner::EffectDesigner(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_originalId(QLCPalette::invalidId())
    , m_draftId(QLCPalette::invalidId())
    , m_sceneId(Function::invalidId())
    , m_ownColour(QLCPalette::invalidId())
    , m_ownDimmer(QLCPalette::invalidId())
{
    setObjectName(QStringLiteral("effectDesigner"));
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(6);

    // The header: what it designs, on what, and what's on the rig.
    QHBoxLayout *head = new QHBoxLayout();
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("edTitle"));
    m_title->setTextFormat(Qt::RichText);
    head->addWidget(m_title, 1);

    m_tryOnBtn = new QToolButton(this);
    m_tryOnBtn->setObjectName(QStringLiteral("edTryOn"));
    m_tryOnBtn->setPopupMode(QToolButton::InstantPopup);
    m_tryOnBtn->setToolTip(tr("The groups the bench plays the effect on, to see it on the rig"));
    QMenu *tryMenu = new QMenu(m_tryOnBtn);
    m_tryOnBtn->setMenu(tryMenu);
    connect(tryMenu, &QMenu::aboutToShow, this, &EffectDesigner::fillTryOnMenu);
    head->addWidget(m_tryOnBtn);

    m_contextRow = new QWidget(this);
    {
        QHBoxLayout *c = new QHBoxLayout(m_contextRow);
        c->setContentsMargins(0, 0, 0, 0);
        c->setSpacing(0);
        m_contextBtn = new QPushButton(tr("In context"), m_contextRow);
        m_contextBtn->setObjectName(QStringLiteral("edInContext"));
        m_contextBtn->setToolTip(tr("The rest of the cue plays too, with this layer's effect over it"));
        m_soloBtn = new QPushButton(tr("Solo"), m_contextRow);
        m_soloBtn->setObjectName(QStringLiteral("edSolo"));
        m_soloBtn->setToolTip(tr("Only this layer's heads play: the effect alone"));
        QButtonGroup *bg = new QButtonGroup(m_contextRow);
        for (QPushButton *b : { m_contextBtn, m_soloBtn })
        {
            b->setCheckable(true);
            bg->addButton(b);
            c->addWidget(b);
        }
        m_contextBtn->setChecked(true);
        connect(m_soloBtn, &QPushButton::toggled, this, [this](bool on) {
            if (on != m_solo)
            {
                m_solo = on;
                emit soloChanged(on);
            }
        });
    }
    m_contextRow->hide();
    head->addWidget(m_contextRow);
    v->addLayout(head);

    m_onRig = new QLabel(this);
    m_onRig->setObjectName(QStringLiteral("edOnRig"));
    m_onRig->setStyleSheet(QStringLiteral("color: palette(mid);"));
    v->addWidget(m_onRig);

    // The browser on the left, the settings on the right: at full height.
    QSplitter *split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName(QStringLiteral("edSplit"));
    split->setChildrenCollapsible(false);

    m_look = new LookEditor(doc, split);
    m_look->setObjectName(QStringLiteral("edSettings"));
    m_effectCombo = m_look->findChild<QComboBox *>(QStringLiteral("effectScriptCombo"));
    Q_ASSERT(m_effectCombo != nullptr);
    // The browser is here, at full height: the Look Editor's own picker
    // window would be a second one.
    // Its name, "used by", and Save as Effect are the designer's to say and
    // do (the draft is named when it is kept; Save as preset is below).
    for (const char *name : { "effectChange", "effectSaveAs", "lookName", "lookNameLabel", "lookTitle" })
        if (QWidget *w = m_look->findChild<QWidget *>(QLatin1String(name)))
            w->hide();

    m_browser = new EffectBrowser(doc, m_effectCombo, split);
    m_browser->setObjectName(QStringLiteral("edBrowser"));
    m_browser->setPickWhileBrowsing(true);
    m_browser->setMinimumWidth(340);

    QWidget *right = new QWidget(split);
    QVBoxLayout *rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    m_look->setParent(right);
    rv->addWidget(m_look, 1);

    // The effect's own colour and dimmer (inside that effect only).
    QHBoxLayout *own = new QHBoxLayout();
    QLabel *ownLabel = new QLabel(tr("Its own:"), right);
    ownLabel->setToolTip(tr("A colour or dimmer that belongs to this effect: it applies inside the "
                            "effect only, not to the rest of the layer"));
    own->addWidget(ownLabel);
    m_ownColourCombo = new QComboBox(right);
    m_ownColourCombo->setObjectName(QStringLiteral("edOwnColour"));
    m_ownDimmerCombo = new QComboBox(right);
    m_ownDimmerCombo->setObjectName(QStringLiteral("edOwnDimmer"));
    own->addWidget(new QLabel(tr("Colour"), right));
    own->addWidget(m_ownColourCombo, 1);
    own->addWidget(new QLabel(tr("Dimmer"), right));
    own->addWidget(m_ownDimmerCombo, 1);
    rv->addLayout(own);
    auto ownPicked = [this]() {
        m_ownColour = m_ownColourCombo->currentData().toUInt();
        m_ownDimmer = m_ownDimmerCombo->currentData().toUInt();
        QList<QColor> colours;
        if (const QLCPalette *c = m_doc ? m_doc->palette(m_ownColour) : nullptr)
            colours << c->rgbValue();
        m_browser->setPreviewColours(colours);
        rebuildScene();
        refreshPreview(false);
    };
    connect(m_ownColourCombo, QOverload<int>::of(&QComboBox::activated), this, ownPicked);
    connect(m_ownDimmerCombo, QOverload<int>::of(&QComboBox::activated), this, ownPicked);

    split->addWidget(m_browser);
    split->addWidget(right);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    {
        const QByteArray st = QSettings().value(SETTINGS_SPLIT).toByteArray();
        if (!st.isEmpty())
            split->restoreState(st);
        connect(split, &QSplitter::splitterMoved, this, [split]() {
            QSettings().setValue(SETTINGS_SPLIT, split->saveState());
        });
    }
    v->addWidget(split, 1);

    // The outcomes.
    QHBoxLayout *btns = new QHBoxLayout();
    m_presetBtn = new QPushButton(tr("Save as preset…"), this);
    m_presetBtn->setObjectName(QStringLiteral("edSavePreset"));
    m_presetBtn->setToolTip(tr("Keep this effect and its settings as a preset: a starting point in "
                               "the browser, for this show and others"));
    btns->addWidget(m_presetBtn);
    btns->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("edCancel"));
    m_cancelBtn->setToolTip(tr("Leave without changing anything"));
    btns->addWidget(m_cancelBtn);
    m_updateBtn = new QPushButton(this);
    m_updateBtn->setObjectName(QStringLiteral("edUpdate"));
    btns->addWidget(m_updateBtn);
    m_savePalBtn = new QPushButton(tr("Save as palette…"), this);
    m_savePalBtn->setObjectName(QStringLiteral("edSavePalette"));
    m_savePalBtn->setToolTip(tr("Name it and file it with the show's palettes, to use anywhere"));
    btns->addWidget(m_savePalBtn);
    m_keepBtn = new QPushButton(tr("Keep on this layer"), this);
    m_keepBtn->setObjectName(QStringLiteral("edKeep"));
    m_keepBtn->setToolTip(tr("A one-off for this layer, as a picked value is. "
                             "Make reusable later from its row if you want it elsewhere."));
    m_keepBtn->setDefault(true);
    m_keepBtn->hide();
    btns->addWidget(m_keepBtn);
    v->addLayout(btns);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("edHint"));
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    v->addWidget(m_hint);

    connect(m_presetBtn, &QPushButton::clicked, this, [this]() { saveAsPreset(); });
    connect(m_cancelBtn, &QPushButton::clicked, this, &EffectDesigner::cancel);
    connect(m_updateBtn, &QPushButton::clicked, this, &EffectDesigner::updateOriginal);
    connect(m_savePalBtn, &QPushButton::clicked, this, [this]() { saveAsPalette(); });
    connect(m_keepBtn, &QPushButton::clicked, this, &EffectDesigner::keepOneOff);

    // An edit: the preview follows. A new effect re-takes the scene's
    // instances; a setting is pushed to the running one (as the
    // Programming tab does: recreating it would reset its state).
    connect(m_look, &LookEditor::paletteChanged, this, [this](quint32 pid) {
        if (pid != m_draftId)
            return;
        refreshPreview(false);
        updateHeader();
        updateButtons();
    });
    connect(m_look, &LookEditor::paletteValueChanged, this, [this](quint32 pid) {
        if (pid != m_draftId)
            return;
        refreshPreview(true);
        updateButtons();
    });
    // The Look Editor refills its combo (a preset saved, another palette
    // shown): the browser follows, once.
    connect(m_effectCombo->model(), &QAbstractItemModel::rowsInserted, this, &EffectDesigner::scheduleBrowserReload);
    connect(m_effectCombo->model(), &QAbstractItemModel::modelReset, this, &EffectDesigner::scheduleBrowserReload);

    if (doc != nullptr)
    {
        connect(doc, &Doc::modeChanged, this, [this]() {
            if (m_doc != nullptr && m_doc->mode() == Doc::Design)
                startPreview();
            else
                stopPreview();
            updateHeader();
        });
        // Another show (or the Doc going): what we had goes with the old
        // one -- the Doc deletes it, not us (its timer may be gone).
        connect(doc, &Doc::clearing, this, [this]() {
            m_draftId = QLCPalette::invalidId();
            m_sceneId = Function::invalidId();
            m_previewStarted = false;
            m_originalId = QLCPalette::invalidId();
            m_groups.clear();
            m_fixtures.clear();
            m_tryOn.clear();
            m_base.clear();
            m_cleanAtStart = false;
            m_ownColour = m_ownDimmer = QLCPalette::invalidId();
            m_look->setContextScene(nullptr);
            m_look->setPalette(QLCPalette::invalidId());
            updateHeader();
            updateButtons();
        });
        connect(doc, &Doc::loaded, this, [this]() {
            if (m_tryOn.isEmpty())
                loadTryOn();
            updateHeader();
        });
        connect(doc, &Doc::paletteAdded, this, &EffectDesigner::fillOwnCombos);
        connect(doc, &Doc::paletteRemoved, this, &EffectDesigner::fillOwnCombos);
    }

    loadTryOn();
    fillOwnCombos();
    updateHeader();
    updateButtons();
}

EffectDesigner::~EffectDesigner()
{
    // Leaving without an answer leaves nothing behind.
    if (m_doc != nullptr)
        release(false);
}

void EffectDesigner::loadTryOn()
{
    // The bench's groups, as last time (by name: ids are per show).
    m_tryOn.clear();
    if (m_doc == nullptr)
        return;
    const QStringList names = QSettings().value(SETTINGS_TRYON).toStringList();
    for (FixtureGroup *g : m_doc->fixtureGroups())
        if (names.contains(g->name()))
            m_tryOn << g->id();
    if (m_tryOn.isEmpty() && !m_doc->fixtureGroups().isEmpty())
        m_tryOn << m_doc->fixtureGroups().first()->id();
}

template <typename F> void EffectDesigner::quietly(F f)
{
    // The draft and its preview scene are ours, not the show's: making or
    // dropping them doesn't make the show "modified".
    const bool was = m_doc != nullptr && m_doc->isModified();
    f();
    if (m_doc != nullptr && !was && m_doc->isModified())
        m_doc->resetModified();
}

/****************************************************************************
 * Targets
 ****************************************************************************/

void EffectDesigner::setTargets(const QList<quint32> &groups, const QList<quint32> &fixtures,
                                const QString &label)
{
    m_groups = groups;
    m_fixtures = fixtures;
    m_targetLabel = label;
    rebuildScene();
    refreshPreview(false);
    updateHeader();
}

void EffectDesigner::setTryOnGroups(const QList<quint32> &groups)
{
    m_tryOn = groups;
    QStringList names;
    if (m_doc != nullptr)
        for (quint32 gid : groups)
            if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
                names << g->name();
    QSettings().setValue(SETTINGS_TRYON, names);
    if (isBench())
    {
        rebuildScene();
        refreshPreview(false);
    }
    updateHeader();
}

void EffectDesigner::setBaseLooks(const QList<quint32> &paletteIds)
{
    m_base = paletteIds;
    rebuildScene();
    refreshPreview(false);
}

QList<quint32> EffectDesigner::previewFixtures() const
{
    QList<quint32> out;
    if (m_doc == nullptr)
        return out;
    const QList<quint32> groups = isBench() ? m_tryOn : m_groups;
    for (quint32 gid : groups)
        if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
            for (quint32 f : g->fixtureList())
                if (!out.contains(f))
                    out << f;
    for (quint32 f : m_fixtures)
        if (!out.contains(f) && m_doc->fixture(f) != nullptr)
            out << f;
    return out;
}

void EffectDesigner::fillTryOnMenu()
{
    QMenu *menu = m_tryOnBtn->menu();
    menu->clear();
    if (m_doc == nullptr)
        return;
    QList<FixtureGroup *> groups = m_doc->fixtureGroups();
    std::sort(groups.begin(), groups.end(), [](const FixtureGroup *a, const FixtureGroup *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (FixtureGroup *g : groups)
    {
        QAction *a = menu->addAction(QStringLiteral("%1 (%2)").arg(g->name()).arg(g->fixtureList().count()));
        a->setCheckable(true);
        a->setChecked(m_tryOn.contains(g->id()));
        const quint32 gid = g->id();
        connect(a, &QAction::toggled, this, [this, gid](bool on) {
            QList<quint32> t = m_tryOn;
            if (on && !t.contains(gid))
                t << gid;
            else if (!on)
                t.removeAll(gid);
            setTryOnGroups(t);
        });
    }
    if (groups.isEmpty())
        menu->addAction(tr("No groups in this show"))->setEnabled(false);
}

/****************************************************************************
 * The draft
 ****************************************************************************/

QLCPalette *EffectDesigner::draft() const
{
    return m_doc == nullptr ? nullptr : m_doc->palette(m_draftId);
}

void EffectDesigner::ensureDraft()
{
    if (m_doc == nullptr)
        return;
    quietly([this]() {
        if (m_doc->palette(m_draftId) == nullptr)
        {
            m_cleanAtStart = !m_doc->isModified();
            // A one-off of the tree's: kept out of every palette list, and
            // kept alive by the preview scene that plays it until we say.
            m_draftId = m_doc->showTree()->createInlinePalette(QLCPalette::Effect, QVariantList(),
                                                               tr("New Effect"));
        }
        if (m_doc->function(m_sceneId) == nullptr)
        {
            Scene *s = new Scene(m_doc);
            s->setName(tr("Effect designer (preview)"));
            s->setVisible(false);
            if (m_doc->addFunction(s))
                m_sceneId = s->id();
            else
            {
                delete s;
                m_sceneId = Function::invalidId();
            }
        }
    });
    rebuildScene();
}

void EffectDesigner::rebuildScene()
{
    Scene *s = m_doc == nullptr ? nullptr : qobject_cast<Scene *>(m_doc->function(m_sceneId));
    if (s == nullptr)
        return;
    quietly([this, s]() {
        for (quint32 g : s->fixtureGroups())
            s->removeFixtureGroup(g);
        for (quint32 f : s->fixtures())
            s->removeFixture(f);
        for (quint32 p : s->palettes())
            s->removePalette(p);
        for (quint32 g : isBench() ? m_tryOn : m_groups)
            if (m_doc->fixtureGroup(g) != nullptr)
                s->addFixtureGroup(g);
        if (!isBench())
            for (quint32 f : m_fixtures)
                if (m_doc->fixture(f) != nullptr)
                    s->addFixture(f);
        // The look stack: the base, then the effect with its own looks.
        for (quint32 p : m_base)
            if (m_doc->palette(p) != nullptr && p != m_draftId)
                s->addPalette(p);
        if (m_doc->palette(m_draftId) != nullptr)
            s->addPalette(m_draftId);
        for (quint32 p : ownLooks())
            s->addPalette(p);
    });
    m_look->setContextScene(s);
}

void EffectDesigner::setEffect(quint32 paletteId, const QList<quint32> &own)
{
    if (m_doc == nullptr)
        return;
    ensureDraft();
    QLCPalette *d = draft();
    if (d == nullptr)
        return;
    const QLCPalette *orig = m_doc->palette(paletteId);
    if (orig != nullptr && orig->type() != QLCPalette::Effect)
        orig = nullptr;
    m_originalId = orig == nullptr ? QLCPalette::invalidId() : paletteId;
    quietly([&]() {
        if (orig != nullptr)
        {
            copyEffect(orig, d);
            d->setName(orig->name());
        }
        else
        {
            const QLCPalette blank(QLCPalette::Effect);
            copyEffect(&blank, d);
            d->setName(tr("New Effect"));      // the Look Editor names it after the effect
        }
    });
    m_ownColour = m_ownDimmer = QLCPalette::invalidId();
    for (quint32 p : own)
        if (const QLCPalette *op = m_doc->palette(p))
        {
            if (op->type() == QLCPalette::Color && m_ownColour == QLCPalette::invalidId())
                m_ownColour = p;
            else if (op->type() == QLCPalette::Dimmer && m_ownDimmer == QLCPalette::invalidId())
                m_ownDimmer = p;
        }
    fillOwnCombos();
    rebuildScene();
    // Out with the old instance: this is another effect.
    m_lastSignature.clear();
    quietly([this]() { m_look->setPalette(m_draftId); });
    m_browser->setWledTargets(m_look->lookHasWledTarget());
    m_browser->reload();
    refreshPreview(false);
    updateHeader();
    updateButtons();
}

QList<quint32> EffectDesigner::ownLooks() const
{
    QList<quint32> out;
    if (m_doc == nullptr)
        return out;
    if (m_doc->palette(m_ownColour) != nullptr)
        out << m_ownColour;
    if (m_doc->palette(m_ownDimmer) != nullptr)
        out << m_ownDimmer;
    return out;
}

void EffectDesigner::setOwnLooks(const QList<quint32> &own)
{
    m_ownColour = m_ownDimmer = QLCPalette::invalidId();
    for (quint32 p : own)
        if (const QLCPalette *op = m_doc ? m_doc->palette(p) : nullptr)
        {
            if (op->type() == QLCPalette::Color)
                m_ownColour = p;
            else if (op->type() == QLCPalette::Dimmer)
                m_ownDimmer = p;
        }
    fillOwnCombos();
    rebuildScene();
    refreshPreview(false);
}

void EffectDesigner::fillOwnCombos()
{
    if (m_doc == nullptr)
        return;
    auto fill = [this](QComboBox *combo, QLCPalette::PaletteType type, quint32 current) {
        QSignalBlocker block(combo);
        combo->clear();
        // None of its own: it plays in the layer's colour / dimmer (the
        // bench has none of either).
        combo->addItem(isBench() ? tr("none") : tr("the layer's"), QLCPalette::invalidId());
        QList<QLCPalette *> pals;
        for (QLCPalette *p : m_doc->palettes())
            if (p->type() == type && (!m_doc->showTree()->isInlinePalette(p->id()) || p->id() == current))
                pals << p;
        std::sort(pals.begin(), pals.end(), [](const QLCPalette *a, const QLCPalette *b) {
            return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
        });
        for (QLCPalette *p : pals)
        {
            if (type == QLCPalette::Color)
            {
                QPixmap px(12, 12);
                px.fill(p->rgbValue());
                combo->addItem(QIcon(px), p->name(), p->id());
            }
            else
            {
                combo->addItem(p->name(), p->id());
            }
        }
        const int at = combo->findData(current);
        combo->setCurrentIndex(at < 0 ? 0 : at);
    };
    fill(m_ownColourCombo, QLCPalette::Color, m_ownColour);
    fill(m_ownDimmerCombo, QLCPalette::Dimmer, m_ownDimmer);
}

bool EffectDesigner::pickEffect(const QString &name)
{
    if (draft() == nullptr)
        setEffect(QLCPalette::invalidId());
    for (int i = 0; i < m_effectCombo->count(); ++i)
    {
        if (m_effectCombo->itemData(i, KindRole).toInt() == KindHeader)
            continue;
        if (m_effectCombo->itemData(i).toString() == name || m_effectCombo->itemText(i).trimmed() == name)
        {
            m_effectCombo->setCurrentIndex(i);
            return true;
        }
    }
    return false;
}

QString EffectDesigner::effectName() const
{
    const QLCPalette *d = draft();
    if (d == nullptr)
        return QString();
    if (!d->effectPreset().isEmpty())
        return d->effectPreset();
    if (!d->scriptPath().isEmpty())
    {
        EffectScriptRunner *runner = m_doc->effectScriptRunner();
        if (runner != nullptr)
        {
            const EffectScriptCache::ScriptMeta meta
                = runner->cache()->scriptMeta(EffectScriptCache::portableName(d->scriptPath()));
            if (!meta.displayName.isEmpty())
                return meta.displayName;
        }
        return EffectScriptCache::nameFromPath(d->scriptPath());
    }
    if (!d->wledEffect().isEmpty())
        return tr("WLED %1").arg(d->wledEffect());
    return QString();
}

bool EffectDesigner::isModified() const
{
    const QLCPalette *d = draft();
    if (d == nullptr)
        return false;
    const QLCPalette *o = m_doc->palette(m_originalId);
    if (o == nullptr)
        return !effectName().isEmpty();
    return !sameEffect(d, o);
}

/****************************************************************************
 * Host options
 ****************************************************************************/

void EffectDesigner::setOneOffAllowed(bool allowed)
{
    m_oneOffAllowed = allowed;
    m_keepBtn->setVisible(allowed);
    updateButtons();
}

void EffectDesigner::setContextChoice(bool shown)
{
    m_contextRow->setVisible(shown);
}

void EffectDesigner::setSolo(bool solo)
{
    (solo ? m_soloBtn : m_contextBtn)->setChecked(true);
}

void EffectDesigner::setCancelVisible(bool visible)
{
    m_cancelBtn->setVisible(visible);
}

/****************************************************************************
 * Outcomes
 ****************************************************************************/

quint32 EffectDesigner::keepOneOff()
{
    QLCPalette *d = draft();
    if (d == nullptr || effectName().isEmpty())
        return QLCPalette::invalidId();
    QLCPalette *o = m_doc->palette(m_originalId);
    const QList<quint32> own = ownLooks();
    quint32 result = m_draftId;
    if (o != nullptr && m_doc->showTree()->isInlinePalette(o->id()))
    {
        // The layer's own one-off, edited: it takes the design, no new one.
        copyEffect(d, o);
        o->setName(m_oneOffWhere.isEmpty() ? effectName()
                                           : QStringLiteral("%1 · %2").arg(effectName(), m_oneOffWhere));
        result = o->id();
        m_doc->setModified();
        emit effectChosen(result, own, OneOff);
        release(false);
        return result;
    }
    // Named as the tree names a picked value: "Matrix Fire · 0.0.3 Announcer AT-W-ALL".
    d->setName(m_oneOffWhere.isEmpty() ? effectName()
                                       : QStringLiteral("%1 · %2").arg(effectName(), m_oneOffWhere));
    // The host puts it on the layer while the preview still holds it, so
    // the tree's bake never sees an unused one-off.
    emit effectChosen(result, own, OneOff);
    release(true);
    return result;
}

quint32 EffectDesigner::saveAsPalette(const QString &name, const QString &folder)
{
    QLCPalette *d = draft();
    if (d == nullptr || effectName().isEmpty())
        return QLCPalette::invalidId();
    ShowTreeController *st = m_doc->showTree();
    QString n = name.trimmed();
    if (n.isEmpty())
    {
        // The effect's name, made unique among the show's effects.
        QString suggested = effectName();
        QStringList taken;
        for (const QLCPalette *p : m_doc->palettes())
            if (p->type() == QLCPalette::Effect && !st->isInlinePalette(p->id()))
                taken << p->name();
        for (int i = 2; taken.contains(suggested); ++i)
            suggested = QStringLiteral("%1 %2").arg(effectName()).arg(i);
        bool ok = false;
        n = QInputDialog::getText(this, tr("Save as palette"), tr("Palette name:"), QLineEdit::Normal,
                                  suggested, &ok).trimmed();
        if (!ok || n.isEmpty())
            return QLCPalette::invalidId();
    }
    const QString path = folder.isEmpty() ? st->suggestPaletteFolder(QLCPalette::Effect) : folder;
    const quint32 pid = m_draftId;
    // One undo step in the tree's history (D22): undo makes it a one-off
    // again, which goes once nothing uses it.
    if (!st->makeReusable(pid, n, path))
        return QLCPalette::invalidId();
    const QList<quint32> own = ownLooks();
    emit effectChosen(pid, own, NewPalette);
    release(true);
    return pid;
}

bool EffectDesigner::updateOriginal()
{
    QLCPalette *d = draft();
    QLCPalette *o = m_doc == nullptr ? nullptr : m_doc->palette(m_originalId);
    if (d == nullptr || o == nullptr)
        return false;
    copyEffect(d, o);
    m_doc->setModified();
    // Every scene playing it plays it as now designed.
    if (EffectScriptRunner *runner = m_doc->effectScriptRunner())
    {
        runner->updateEffectParams(o->id());
        for (Function *f : m_doc->functions())
        {
            Scene *s = qobject_cast<Scene *>(f);
            if (s != nullptr && f->id() != m_sceneId && s->palettes().contains(o->id()) && s->isRunning())
            {
                s->resetRuntime();
                runner->syncScene(s->id());
            }
        }
    }
    const QList<quint32> own = ownLooks();
    emit effectChosen(o->id(), own, UpdatedPalette);
    release(false);
    return true;
}

bool EffectDesigner::saveAsPreset(const QString &name)
{
    QLCPalette *d = draft();
    if (d == nullptr || m_doc->effectScriptRunner() == nullptr)
        return false;
    if (d->scriptPath().isEmpty())
    {
        if (name.isEmpty())
            QMessageBox::information(this, tr("Save as preset"),
                                     tr("Pick an effect first, then save it as a preset."));
        return false;
    }
    EffectScriptCache *scache = m_doc->effectScriptRunner()->cache();
    EffectPresetCache *pcache = m_doc->effectScriptRunner()->presetCache();
    QString n = name.trimmed();
    if (n.isEmpty())
    {
        bool ok = false;
        n = QInputDialog::getText(this, tr("Save as preset"), tr("Preset name:"), QLineEdit::Normal,
                                  effectName(), &ok).trimmed();
        if (!ok || n.isEmpty())
            return false;
        if (pcache->contains(n)
            && QMessageBox::question(this, tr("Save as preset"),
                                     tr("A preset named \"%1\" already exists. Replace it?").arg(n),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return false;
    }
    const QString base = EffectScriptCache::portableName(d->scriptPath());
    const EffectScriptCache::ScriptMeta meta = scache->scriptMeta(base);
    EffectPresetCache::Preset preset;
    preset.name = n;
    preset.script = base;
    preset.category = EffectScriptCache::categoryForTypes(meta.fixtureTypes);
    preset.description = meta.description;
    preset.params = d->effectParamValues();
    if (!pcache->savePreset(preset))
    {
        if (name.isEmpty())
            QMessageBox::warning(this, tr("Save as preset"),
                                 tr("Could not write the preset file. Check folder permissions."));
        return false;
    }
    // The draft is that preset now; the browser lists it.
    quietly([&]() { d->setEffectPreset(n); });
    m_look->setPalette(m_draftId);
    updateHeader();
    emit presetSaved(n);
    return true;
}

void EffectDesigner::cancel()
{
    release(false);
    emit cancelled();
}

void EffectDesigner::release(bool keepDraft)
{
    if (m_doc == nullptr)
        return;
    stopPreview();
    m_look->setContextScene(nullptr);
    m_look->setPalette(QLCPalette::invalidId());
    const quint32 scene = m_sceneId;
    const quint32 draftId = m_draftId;
    m_sceneId = Function::invalidId();
    m_draftId = QLCPalette::invalidId();
    m_originalId = QLCPalette::invalidId();
    quietly([&]() {
        if (m_doc->function(scene) != nullptr)
            m_doc->deleteFunction(scene);
        if (!keepDraft && m_doc->palette(draftId) != nullptr && m_doc->showTree()->isInlinePalette(draftId))
            m_doc->deletePalette(draftId);
    });
    // Thrown away: the show is as it was, saved if it was (the draft's
    // edits were never the show's).
    if (!keepDraft && draftId != QLCPalette::invalidId() && m_cleanAtStart && m_doc->isModified())
        m_doc->resetModified();
    m_lastSignature.clear();
    updateHeader();
    updateButtons();
}

/****************************************************************************
 * Preview
 ****************************************************************************/

bool EffectDesigner::isPreviewing() const
{
    // Asked of the timer (it runs it from its next tick).
    return m_previewStarted && m_doc != nullptr && m_doc->function(m_sceneId) != nullptr;
}

void EffectDesigner::startPreview()
{
    if (m_doc == nullptr || m_doc->mode() != Doc::Design || !isVisible())
        return;
    Function *f = m_doc->function(m_sceneId);
    if (f == nullptr || m_previewStarted)
        return;
    f->start(m_doc->masterTimer(), FunctionParent::master());
    m_previewStarted = true;
    m_lastSignature.clear();
    updateHeader();
}

void EffectDesigner::stopPreview()
{
    Function *f = m_doc == nullptr ? nullptr : m_doc->function(m_sceneId);
    if (f != nullptr && m_previewStarted)
        f->stop(FunctionParent::master());
    m_previewStarted = false;
    updateHeader();
}

void EffectDesigner::refreshPreview(bool params)
{
    Scene *s = m_doc == nullptr ? nullptr : qobject_cast<Scene *>(m_doc->function(m_sceneId));
    if (s == nullptr)
        return;
    if (!m_previewStarted)
    {
        startPreview();
        return;
    }
    // No stop/start: that races the timer thread. Re-take the values, and
    // the effect's instance only when the effect itself changed.
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    if (params && runner != nullptr)
        runner->updateEffectParams(m_draftId);
    s->resetRuntime();
    QStringList sig;
    for (quint32 pid : s->palettes())
        if (const QLCPalette *p = m_doc->palette(pid))
            if (p->type() == QLCPalette::Effect)
                sig << QStringLiteral("%1|%2|%3|%4").arg(pid).arg(p->scriptPath(), p->effectPreset(), p->wledEffect());
    sig << QStringLiteral("targets:%1").arg(previewFixtures().count());
    if (sig != m_lastSignature && runner != nullptr)
    {
        m_lastSignature = sig;
        runner->syncScene(s->id());
    }
}

void EffectDesigner::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    startPreview();
}

void EffectDesigner::hideEvent(QHideEvent *e)
{
    stopPreview();
    QWidget::hideEvent(e);
}

QString EffectDesigner::previewText() const
{
    if (m_doc == nullptr || draft() == nullptr)
        return QString();
    const int heads = previewFixtures().count();
    if (m_doc->mode() != Doc::Design)
        return tr("Live: nothing previews (Design mode to see it on the rig)");
    if (heads == 0)
        return isBench() ? tr("Pick groups to Try on, to see it on the rig") : tr("No fixtures to play on");
    if (effectName().isEmpty())
        return tr("Pick an effect: it plays at once on %n fixture(s)", "", heads);
    if (!isPreviewing())
        return tr("Not playing (shows when this is on screen)");
    return tr("▶ On the rig: %1 on %n fixture(s)", "", heads).arg(effectName());
}

/****************************************************************************
 * Header and buttons
 ****************************************************************************/

void EffectDesigner::updateHeader()
{
    QString on;
    if (isBench())
    {
        QStringList names;
        if (m_doc != nullptr)
            for (quint32 gid : m_tryOn)
                if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
                    names << g->name();
        on = tr("the bench");
        m_tryOnBtn->setText(names.isEmpty() ? tr("Try on: (none) ▾")
                                            : tr("Try on: %1 ▾").arg(names.join(QStringLiteral(", "))));
        m_tryOnBtn->show();
    }
    else
    {
        on = m_targetLabel.isEmpty() ? tr("%n fixture(s)", "", previewFixtures().count()) : m_targetLabel;
        m_tryOnBtn->hide();
    }
    const QLCPalette *o = m_doc == nullptr ? nullptr : m_doc->palette(m_originalId);
    QString what = tr("New effect");
    if (o != nullptr)
        what = m_doc->showTree()->isInlinePalette(o->id()) ? tr("This layer's effect") : o->name();
    m_title->setText(tr("<b>Effect designer</b> &nbsp;%1 <span style=\"color:gray\">on %2</span>")
                     .arg(what.toHtmlEscaped(), on.toHtmlEscaped()));
    m_onRig->setText(previewText());
}

void EffectDesigner::updateButtons()
{
    const bool picked = !effectName().isEmpty();
    const QLCPalette *o = m_doc == nullptr ? nullptr : m_doc->palette(m_originalId);
    const bool named = o != nullptr && !m_doc->showTree()->isInlinePalette(o->id());
    m_keepBtn->setEnabled(picked);
    m_savePalBtn->setEnabled(picked);
    m_presetBtn->setEnabled(draft() != nullptr && !draft()->scriptPath().isEmpty());
    m_updateBtn->setVisible(named);
    m_updateBtn->setEnabled(named && isModified());
    if (named)
    {
        m_updateBtn->setText(tr("Update “%1”").arg(o->name()));
        m_updateBtn->setToolTip(tr("Change the palette itself: everywhere it plays plays this"));
    }
    if (m_oneOffAllowed)
        m_hint->setText(tr("Keep on this layer makes a one-off, as a picked value is. Save as palette names it "
                           "and files it for the whole show. Nothing changes until you choose."));
    else if (named)
        m_hint->setText(tr("A copy is being designed: Update changes “%1” everywhere it plays; "
                           "Save as palette makes a new one beside it.").arg(o->name()));
    else
        m_hint->setText(tr("Pick an effect and shape it on the bench. Save as palette keeps it for the show; "
                           "Save as preset keeps it as a starting point for other shows."));
}

void EffectDesigner::scheduleBrowserReload()
{
    if (m_reloadQueued)
        return;
    m_reloadQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_reloadQueued = false;
        m_browser->reload();
    });
}

/****************************************************************************
 * EffectLayerDialog
 ****************************************************************************/

EffectLayerDialog::EffectLayerDialog(Doc *doc, quint32 cueId, quint32 layerId, const Target &target,
                                     const QString &key, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_cueId(cueId)
    , m_layerId(layerId)
    , m_target(target)
    , m_previewedBefore(doc->showTree()->previewedCue())
{
    setObjectName(QStringLiteral("effectLayerDialog"));
    ShowTreeController *st = doc->showTree();
    const CueList *list = st->listOfCue(cueId);
    const Cue *cue = st->cue(cueId);
    const Layer *own = cue == nullptr ? nullptr : cue->layer(layerId);
    if (own != nullptr && m_target.isEmpty())
        m_target = own->target;

    // What the layer plays now: its effect (the row's, else its first),
    // and the rest as the preview's base.
    QList<quint32> base;
    if (list != nullptr)
    {
        const QList<ResolvedCue> rcs = st->resolved(*list);
        const int index = list->indexOf(cueId);
        const ResolvedLayer *rl = index >= 0 && index < rcs.count() ? rcs.at(index).layer(m_target) : nullptr;
        if (rl != nullptr)
        {
            for (auto it = rl->values.constBegin(); it != rl->values.constEnd(); ++it)
            {
                if (it.value().look.none)
                    continue;
                if (isEffectKey(it.key()))
                {
                    const bool wanted = isEffectKey(key) && key != QLatin1String("Effect.")
                                        ? it.key() == key : m_previous.key.isEmpty();
                    if (wanted)
                    {
                        m_previous = it.value().look;
                        m_previousOwn = it.value().origin == Origin::Own;
                    }
                }
                else if (!isLineParamKey(it.key()))
                {
                    base << it.value().look.palettes;
                }
            }
        }
    }

    const QString where = own != nullptr && !own->name.isEmpty() ? own->name : st->targetLabel(m_target);
    const QString cueText = list == nullptr ? QString() : st->cueLabel(*list, list->indexOf(cueId));
    setWindowTitle(tr("Design effect — %1 · %2").arg(cueText, where));

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_designer = new EffectDesigner(doc, this);
    v->addWidget(m_designer);

    QList<quint32> groups = m_target.groups, fixtures = m_target.fixtures;
    for (quint32 set : m_target.sets)
        if (const GroupSet *gs = st->groupSet(set))
        {
            groups << gs->groups;
            fixtures << gs->fixtures;
        }
    m_designer->setTargets(groups, fixtures, tr("%1 — %n fixture(s)", "", st->targetFixtures(m_target).count())
                                                 .arg(st->targetLabel(m_target)));
    m_designer->setBaseLooks(base);
    m_designer->setOneOffAllowed(true);
    m_designer->setOneOffLabel(QStringLiteral("%1 %2").arg(cueText, where).trimmed());
    m_designer->setContextChoice(true);
    m_designer->setCancelVisible(true);
    m_designer->setEffect(m_previous.palettes.value(0, QLCPalette::invalidId()), m_previous.own);

    // In context: the cue plays under it. Solo: only the effect.
    connect(m_designer, &EffectDesigner::soloChanged, this, [this](bool solo) {
        if (solo)
            m_doc->showTree()->stopPreview();
        else if (m_doc->mode() == Doc::Design)
            m_doc->showTree()->previewCue(m_cueId);
    });
    connect(m_designer, &EffectDesigner::effectChosen, this,
            [this](quint32 pid, const QList<quint32> &own, EffectDesigner::Outcome outcome) {
        if (outcome == EffectDesigner::UpdatedPalette && m_previous.palettes.value(0) == pid
            && m_previous.own == own)
        {
            m_applied = true;           // the layer already plays it
            return;
        }
        apply(pid, own);
    });
    connect(m_designer, &EffectDesigner::effectChosen, this, &QDialog::accept, Qt::QueuedConnection);
    connect(m_designer, &EffectDesigner::cancelled, this, &QDialog::reject);
    connect(this, &QDialog::rejected, m_designer, [this]() {
        if (m_designer->draftId() != QLCPalette::invalidId())
            m_designer->cancel();
    });

    if (m_doc->mode() == Doc::Design && m_previewedBefore == ShowTree::invalidId())
        st->previewCue(cueId);          // in context: the cue, under the effect
    resize(1180, 760);
}

EffectLayerDialog::~EffectLayerDialog()
{
    // Back as the tree had it: its preview of the cue it showed.
    ShowTreeController *st = m_doc->showTree();
    if (m_previewedBefore != ShowTree::invalidId())
        st->previewCue(m_previewedBefore);
    else
        st->stopPreview();
}

bool EffectLayerDialog::apply(quint32 paletteId, const QList<quint32> &own)
{
    ShowTreeController *st = m_doc->showTree();
    const QLCPalette *p = m_doc->palette(paletteId);
    if (p == nullptr)
        return false;
    const QString key = effectKey(st->effectKind(p));
    auto set = [this, st](const Look &look) {
        return m_layerId != ShowTree::invalidId() && st->cue(m_cueId) != nullptr
                       && st->cue(m_cueId)->layer(m_layerId) != nullptr
                   ? st->setLayerLook(m_cueId, m_layerId, look)
                   : st->setTargetLook(m_cueId, m_target, look);
    };
    st->beginBatch(tr("Design effect on %1").arg(st->targetLabel(m_target)));
    bool any = false;
    // The effect it had, of another kind: replaced, not played beside it.
    if (!m_previous.key.isEmpty() && m_previous.key != key)
    {
        Look gone;
        gone.key = m_previous.key;
        gone.none = !m_previousOwn;     // tracked in: off from here; its own: cleared
        any |= set(gone);
    }
    Look look;
    look.key = key;
    look.palettes << paletteId;
    look.own = own;
    any |= set(look);
    st->endBatch(any);
    m_applied = any;
    return any;
}

bool EffectLayerDialog::design(Doc *doc, QWidget *parent, quint32 cueId, quint32 layerId,
                               const Target &target, const QString &key)
{
    EffectLayerDialog dlg(doc, cueId, layerId, target, key, parent);
    dlg.exec();
    return dlg.m_applied;
}
