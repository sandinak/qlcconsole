/*
  Q Light Controller Plus
  scenegrouplooks.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QMessageBox>
#include <QMenu>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QToolTip>

#include <algorithm>
#include <functional>

#include "programmingmanager.h"
#include "speedmasterspanel.h"
#include "speedmasters.h"
#include "scenegrouplooks.h"
#include "fadetimespin.h"
#include "scenetransition.h"
#include <QSpinBox>
#include "functionstreewidget.h"   // palette MIME type
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "fixturegroupsource.h"    // fixture-group / fixture MIME types
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "monitorproperties.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "qlcpalette.h"
#include "aimsolver.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#include "qlceventpos.h"

// Target kinds, stored at Qt::UserRole + 1 on target items.
enum { TargetGroup = 0, TargetFixture = 1, TargetTypeFolder = 2 };
#define TARGET_KIND_ROLE (Qt::UserRole + 1)

// Looks tree columns. Column 0 carries the palette id at Qt::UserRole; the
// In/Out columns carry their fade override in ms at Qt::UserRole (-1 = "step",
// i.e. follow the chaser step / scene fade).
enum { LookColName = 0, LookColIn = 1, LookColOut = 2, LookColCount = 3 };
// Marks a fade cell whose fade doesn't apply, so the delegate keeps it read-only
// and it renders as "—": an Effect's Fade In (its fade-in is the scene envelope),
// and BOTH fades of a colour/dimmer nested under an effect (it feeds the effect's
// gradient — a value, not a painted look — so it has no fade of its own).
static const int LookFadeInertRole = Qt::UserRole + 3;

/* What a fade's default (-1) means for this look: "scene" -- follow the
   scene's fade (or a chaser step's) -- or, for an effect's release, "snap".
   Kept on the cell so the editor and the text agree (LookFades). */
static const int LookFadeDefaultRole = Qt::UserRole + 4;

/** A fade cell's text: "1.5 s", its default's name, or "—" where the look has
 *  no such fade. */
static QString fadeCellText(const QTreeWidgetItem *it, int column)
{
    if (it->data(column, LookFadeInertRole).toBool())
        return QStringLiteral("—");
    return FadeTimeSpin::text(it->data(column, Qt::UserRole).toInt(),
                              it->data(column, LookFadeDefaultRole).toString());
}

/** Editable fade-time cell: the one fade-time control (FadeTimeSpin), its
 *  lowest value the look's default. Only the In/Out columns edit. */
class FadeSpinDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override
    {
        if (index.column() != LookColIn && index.column() != LookColOut)
            return nullptr;
        // Cells whose fade doesn't apply (effect Fade In, or a colour feeding an
        // effect) are read-only.
        if (index.data(LookFadeInertRole).toBool())
            return nullptr;
        return new FadeTimeSpin(parent, index.data(LookFadeDefaultRole).toString());
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (FadeTimeSpin *sp = qobject_cast<FadeTimeSpin *>(editor))
            sp->setMs(index.data(Qt::UserRole).toInt());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        FadeTimeSpin *sp = qobject_cast<FadeTimeSpin *>(editor);
        if (sp == nullptr) return;
        model->setData(index, sp->ms(), Qt::UserRole);
        model->setData(index, FadeTimeSpin::text(sp->ms(), index.data(LookFadeDefaultRole).toString()),
                       Qt::DisplayRole);
    }
};

// Human label + stable bucket key for grouping fixed fixtures by type.
static QString fixtureTypeLabel(const Fixture *f)
{
    if (f == NULL)
        return QString();
    if (f->fixtureDef() != NULL && f->fixtureMode() != NULL)
        return QString("%1 (%2)").arg(f->fixtureDef()->model())
                                 .arg(f->fixtureMode()->name());
    return QObject::tr("Generic %1-channel").arg(f->channels());
}

SceneGroupLooks::SceneGroupLooks(Scene *scene, Doc *doc, QWidget *parent,
                                 bool includeFixtureTargets)
    : QWidget(parent)
    , m_scene(scene)
    , m_doc(doc)
    , m_includeFixtureTargets(includeFixtureTargets)
{
    setAcceptDrops(true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 6, 0, 0);

    /* One button, not three lines of prose.
     *
       This paragraph explained Looks and Targets permanently, at the top of the
       panel, every time a scene was opened -- read once on the first day and
       looked past forever after. Its content now lives in the Programming
       guide, which this reaches; the explanation is still one click away
       without standing in front of the work. */
    QHBoxLayout *helpRow = new QHBoxLayout();

    /* How the whole look arrives and leaves: each fixture at its own moment
       -- a wipe across the stage, from the centre, a dissolve, along each
       bar -- instead of all fading together ("transitions .. fade in ..
       wipe .. from center"). Effects in the look follow it too. */
    if (m_scene != nullptr)
    {
        auto makeShape = [this](const SceneTransition &t) {
            QComboBox *cb = new QComboBox(this);
            cb->addItems(SceneTransition::shapeLabels());
            cb->setCurrentIndex(int(t.shape));
            return cb;
        };
        auto makeTime = [this](const SceneTransition &t) {
            FadeTimeSpin *sp = new FadeTimeSpin(this);
            sp->setMs(int(t.time));
            return sp;
        };
        const SceneTransition tin = m_scene->transitionIn(), tout = m_scene->transitionOut();
        QComboBox *inShape = makeShape(tin);
        FadeTimeSpin *inTime = makeTime(tin);
        QComboBox *outShape = makeShape(tout);
        FadeTimeSpin *outTime = makeTime(tout);
        QSpinBox *edge = new QSpinBox(this);
        edge->setRange(5, 100);
        edge->setSuffix(QStringLiteral("%"));
        edge->setValue(qRound((tin.isActive() ? tin.softness : tout.softness) * 100.0));
        edge->setKeyboardTracking(false);
        inShape->setToolTip(tr("How this look arrives when the scene starts: each fixture when "
                               "the transition reaches it, in place of the scene's fade. "
                               "Effects in the look follow it too."));
        outShape->setToolTip(tr("How it leaves when the scene stops"));
        inTime->setToolTip(tr("How long the transition in takes, start to finish"));
        outTime->setToolTip(tr("How long the transition out takes"));
        edge->setToolTip(tr("How soft its edge is: each fixture's own fade, as a share of "
                            "the time -- 100% all together, low a sharp line"));
        /* The scene's own fade, here where looks fall back to it ("step" /
           "scene") -- it lived only in the Scene Editor's speed dial. A
           transition, when set, stands in for it. */
        auto speedMs = [](uint speed) {
            return (speed == Function::defaultSpeed() || speed == Function::infiniteSpeed())
                ? 0 : int(speed);
        };
        FadeTimeSpin *fadeIn = new FadeTimeSpin(this);
        FadeTimeSpin *fadeOut = new FadeTimeSpin(this);
        fadeIn->setMs(speedMs(m_scene->fadeInSpeed()));
        fadeOut->setMs(speedMs(m_scene->fadeOutSpeed()));
        fadeIn->setObjectName(QStringLiteral("sceneFadeIn"));
        fadeOut->setObjectName(QStringLiteral("sceneFadeOut"));
        fadeIn->setToolTip(tr("The scene's fade in: every look's \"scene\" fade follows it"));
        fadeOut->setToolTip(tr("The scene's fade out"));
        connect(fadeIn, &FadeTimeSpin::msChanged, this, [this](int ms) {
            if (m_scene == nullptr || m_scene->fadeInSpeed() == uint(ms))
                return;
            m_scene->setFadeInSpeed(uint(ms));
            if (m_doc != nullptr)
                m_doc->setModified();
            emit sceneModified();
        });
        connect(fadeOut, &FadeTimeSpin::msChanged, this, [this](int ms) {
            if (m_scene == nullptr || m_scene->fadeOutSpeed() == uint(ms))
                return;
            m_scene->setFadeOutSpeed(uint(ms));
            if (m_doc != nullptr)
                m_doc->setModified();
            emit sceneModified();
        });

        auto apply = [this, inShape, inTime, outShape, outTime, edge, fadeIn, fadeOut]() {
            if (m_scene == nullptr)
                return;
            SceneTransition tin, tout;
            tin.shape = SceneTransition::Shape(inShape->currentIndex());
            tin.time = uint(inTime->ms());
            tin.softness = edge->value() / 100.0;
            tout.shape = SceneTransition::Shape(outShape->currentIndex());
            tout.time = uint(outTime->ms());
            tout.softness = edge->value() / 100.0;
            inTime->setEnabled(tin.shape != SceneTransition::None);
            outTime->setEnabled(tout.shape != SceneTransition::None);
            edge->setEnabled(tin.isActive() || tout.isActive());
            // A transition's time takes the place of the fade.
            fadeIn->setEnabled(!tin.isActive());
            fadeOut->setEnabled(!tout.isActive());
            if (tin == m_scene->transitionIn() && tout == m_scene->transitionOut())
                return;
            m_scene->setTransitionIn(tin);
            m_scene->setTransitionOut(tout);
            if (m_doc != nullptr)
                m_doc->setModified();
            emit sceneModified();
        };
        inTime->setEnabled(tin.shape != SceneTransition::None);
        outTime->setEnabled(tout.shape != SceneTransition::None);
        edge->setEnabled(tin.isActive() || tout.isActive());
        fadeIn->setEnabled(!tin.isActive());
        fadeOut->setEnabled(!tout.isActive());
        connect(inShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [apply](int) { apply(); });
        connect(outShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [apply](int) { apply(); });
        connect(inTime, &FadeTimeSpin::msChanged, this, [apply](int) { apply(); });
        connect(outTime, &FadeTimeSpin::msChanged, this, [apply](int) { apply(); });
        connect(edge, QOverload<int>::of(&QSpinBox::valueChanged), this, [apply](int) { apply(); });
        inShape->setObjectName(QStringLiteral("transitionInShape"));
        outShape->setObjectName(QStringLiteral("transitionOutShape"));
        inTime->setObjectName(QStringLiteral("transitionInTime"));
        edge->setObjectName(QStringLiteral("transitionEdge"));
        helpRow->addWidget(new QLabel(tr("Fade in"), this));
        helpRow->addWidget(fadeIn);
        helpRow->addWidget(new QLabel(tr("out"), this));
        helpRow->addWidget(fadeOut);
        helpRow->addSpacing(12);
        helpRow->addWidget(new QLabel(tr("Transition in:"), this));
        helpRow->addWidget(inShape);
        helpRow->addWidget(inTime);
        helpRow->addSpacing(8);
        helpRow->addWidget(new QLabel(tr("out:"), this));
        helpRow->addWidget(outShape);
        helpRow->addWidget(outTime);
        helpRow->addSpacing(8);
        helpRow->addWidget(new QLabel(tr("Edge:"), this));
        helpRow->addWidget(edge);

        /* What this look's effects run on (SPEED_SPREAD_DESIGN.md, S2): a
           speed master, the main clock, their own speed -- or whatever the
           chase step, chase or collection playing it says. */
        QComboBox *speed = new QComboBox(this);
        speed->setObjectName(QStringLiteral("sceneSpeedMaster"));
        SpeedMastersPanel::fillCombo(speed, m_doc, tr("As played"), m_scene->speedMaster());
        speed->setToolTip(tr("What this look's effects keep time with. \"As played\": whatever "
                             "the chase step, chase or collection playing it says (else their own "
                             "speed). An effect can still choose for itself in the look editor."));
        connect(speed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, speed](int) {
            if (m_scene == nullptr)
                return;
            m_scene->setSpeedMaster(speed->currentData().toInt());
            if (m_doc != nullptr)
                m_doc->setModified();
            emit sceneModified();
        });
        helpRow->addSpacing(8);
        helpRow->addWidget(new QLabel(tr("Speed:"), this));
        helpRow->addWidget(speed);
    }
    helpRow->addStretch();
    QPushButton *guideBtn = new QPushButton(tr("How this works"), this);
    guideBtn->setFlat(true);
    guideBtn->setCursor(Qt::PointingHandCursor);
    connect(guideBtn, &QPushButton::clicked, this,
            [this]() { ProgrammingManager::showGuide(this); });
    helpRow->addWidget(guideBtn);
    root->addLayout(helpRow);

    /* Re-rigging a head changes what it can reach. Re-check when the rig
       changes, but rebuild only if the answer did: this panel is where looks
       are edited, and a rebuild for nothing would pull the rug out. */
    if (m_doc != nullptr && m_doc->monitorProperties() != nullptr)
    {
        connect(m_doc->monitorProperties(), &MonitorProperties::rigPropsChanged, this,
                [this](quint32) { scheduleReachCheck(); });
        // Moving a target is the other half: the same heads, somewhere else.
        connect(m_doc->monitorProperties(), &MonitorProperties::stageTargetsChanged,
                this, &SceneGroupLooks::scheduleReachCheck);
    }

    /* A fixed target renamed anywhere shows its new name here. Deferred, and
       only when the renamed fixture is one of this look's fixed targets. */
    if (m_doc != nullptr)
        connect(m_doc, &Doc::fixtureChanged, this, [this](quint32 fid) {
            if (m_scene == nullptr || !m_includeFixtureTargets
                || !m_scene->fixtures().contains(fid) || m_nameReloadPending)
                return;
            m_nameReloadPending = true;
            QTimer::singleShot(0, this, [this]() {
                m_nameReloadPending = false;
                reload();
            });
        });

    QHBoxLayout *cols = new QHBoxLayout();
    root->addLayout(cols);

    // --- Targets column (groups + optionally fixtures) ---
    QVBoxLayout *targetCol = new QVBoxLayout();
    m_targetsLabel = new QLabel(tr("Targets"), this);
    targetCol->addWidget(m_targetsLabel);
    /* Targets in the inventory -- not patched -- send nothing ("not seeing
       output on farts .. we need a warning in programming view for unpatched
       fixtures"): a group of them looked like any other target. */
    m_unpatchedNote = new QLabel(this);
    m_unpatchedNote->setWordWrap(true);
    m_unpatchedNote->setStyleSheet(QStringLiteral("color: rgb(230,160,40);"));
    m_unpatchedNote->hide();
    targetCol->addWidget(m_unpatchedNote);

    // Declared SCOPE (Scene::LookScope) — what this look is FOR, as a
    // statement of intent. Distinct from the Targets list below it (what it
    // actually paints): a look can be scoped to "SR Truss" while its targets
    // happen to be a superset/subset, e.g. mid-build. Purely organisational
    // metadata today — doesn't affect playback.
    /* Not shown. Sitting right above Targets, it read as a second place to
       say which fixtures the look is on ("what is scope in this context.. I
       set the group and that should be sufficient") while changing nothing
       that lights. Kept, hidden, so a show's saved scope survives. */
    QHBoxLayout *scopeRow = new QHBoxLayout();
    QLabel *scopeLabel = new QLabel(tr("Scope:"), this);
    scopeRow->addWidget(scopeLabel);
    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->setToolTip(tr("What this look is FOR (a statement of intent) — "
        "not what it paints. Whole Stage, or a specific fixture group/zone."));
    scopeRow->addWidget(m_scopeCombo, 1);
    targetCol->addLayout(scopeRow);
    scopeLabel->hide();
    m_scopeCombo->hide();
    connect(m_scopeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotScopeChanged(int)));

    m_targetList = new QTreeWidget(this);
    m_targetList->setHeaderHidden(true);
    m_targetList->setRootIsDecorated(true);
    m_targetList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_targetList->setContextMenuPolicy(Qt::CustomContextMenu);
    targetCol->addWidget(m_targetList);
    QHBoxLayout *targetBtns = new QHBoxLayout();
    m_removeTargetButton = new QPushButton(tr("Remove"), this);
    targetBtns->addWidget(m_removeTargetButton);
    targetBtns->addStretch();
    targetCol->addLayout(targetBtns);
    cols->addLayout(targetCol);

    // --- Looks column ---
    QVBoxLayout *lookCol = new QVBoxLayout();
    lookCol->addWidget(new QLabel(tr("Looks"), this));
    m_lookList = new QTreeWidget(this);
    m_lookList->setColumnCount(LookColCount);
    m_lookList->setHeaderLabels(QStringList() << tr("Look") << tr("Fade In") << tr("Fade Out"));
    if (QTreeWidgetItem *hdr = m_lookList->headerItem())
    {
        const QString fadeTip = tr("Per-look fade time (this look's channels only).\n"
                                   "0 = snap instantly; \"step\" = follow the chaser step / scene fade.\n"
                                   "Double-click a cell to edit; right-click a row to reset to \"step\".");
        hdr->setToolTip(LookColIn,  fadeTip);
        hdr->setToolTip(LookColOut, fadeTip);
    }
    // Tree view: an Effect look is a parent; Colour/Dimmer looks nested UNDER it
    // feed that effect (its gradient/master), while looks at the TOP level light
    // the fixtures directly (static base). Drag a colour onto an effect to feed
    // it, or out to the top level to make it a base.
    m_lookList->setRootIsDecorated(true);
    m_lookList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_lookList->setAllColumnsShowFocus(true);
    m_lookList->header()->setStretchLastSection(false);
    m_lookList->header()->setSectionResizeMode(LookColName, QHeaderView::Stretch);
    m_lookList->header()->setSectionResizeMode(LookColIn, QHeaderView::ResizeToContents);
    m_lookList->header()->setSectionResizeMode(LookColOut, QHeaderView::ResizeToContents);
    // Double-click an In/Out cell to give that look its own fade time.
    m_lookList->setItemDelegate(new FadeSpinDelegate(m_lookList));
    m_lookList->setEditTriggers(QAbstractItemView::DoubleClicked);
    // Order = precedence: looks lower in the list are applied last and win on
    // any channel they share (pan/tilt, colour, dimmer). Reorder by dragging
    // within the list or with the Up/Down buttons.
    m_lookList->setDragDropMode(QAbstractItemView::InternalMove);
    m_lookList->setDefaultDropAction(Qt::MoveAction);
    m_lookList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_lookList->setToolTip(
        tr("Colours/dimmers nested UNDER an Effect feed that effect (its gradient "
           "and master); looks at the top level light the fixtures directly.\n"
           "Drag a colour onto an effect to feed it, or out to the top level to "
           "make it a static base. Order still sets precedence among siblings.\n"
           "Double-click Fade In / Fade Out to set a per-look fade time. "
           "0 = snap instantly; \"step\" = follow the chaser step / scene fade "
           "(spin below 0, or right-click → Reset, to return to \"step\")."));
    lookCol->addWidget(m_lookList);
    QHBoxLayout *lookBtns = new QHBoxLayout();
    m_removeLookButton = new QPushButton(tr("Remove"), this);
    lookBtns->addWidget(m_removeLookButton);
    lookBtns->addStretch();
    m_moveLookUpButton = new QPushButton(QIcon(":/up.png"), QString(), this);
    m_moveLookUpButton->setToolTip(tr("Move look up (applied earlier — lower precedence)"));
    m_moveLookDownButton = new QPushButton(QIcon(":/down.png"), QString(), this);
    m_moveLookDownButton->setToolTip(tr("Move look down (applied later — higher precedence)"));
    lookBtns->addWidget(m_moveLookUpButton);
    lookBtns->addWidget(m_moveLookDownButton);
    lookCol->addLayout(lookBtns);
    cols->addLayout(lookCol);

    connect(m_removeTargetButton, SIGNAL(clicked()), this, SLOT(slotRemoveTarget()));
    connect(m_removeLookButton, SIGNAL(clicked()), this, SLOT(slotRemoveLook()));
    connect(m_moveLookUpButton, SIGNAL(clicked()), this, SLOT(slotMoveLookUp()));
    connect(m_moveLookDownButton, SIGNAL(clicked()), this, SLOT(slotMoveLookDown()));
    // Internal drag-drop reorder: QTreeWidget's internal move implements the drop
    // as take + insert, so the model emits rowsRemoved/rowsInserted, NOT rowsMoved
    // — a rowsMoved connection would silently never fire. Instead filter the
    // list's drop event and sync the order once the move settles. (See eventFilter.)
    m_lookList->viewport()->installEventFilter(this);
    connect(m_lookList, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotLookSelectionChanged()));
    // Double-click the name column (re)loads the look into the editor even if it
    // was already selected; double-click In/Out edits that cell (via delegate).
    connect(m_lookList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotLookDoubleClicked(QTreeWidgetItem*,int)));
    // A committed In/Out edit pushes the per-look fade to the scene.
    connect(m_lookList, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(slotLookFadeEdited(QTreeWidgetItem*,int)));
    connect(m_lookList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotLookContextMenu(QPoint)));
    connect(m_targetList, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotTargetSelectionChanged()));
    connect(m_targetList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotTargetContextMenu(QPoint)));

    reload();
}

SceneGroupLooks::~SceneGroupLooks()
{
}

QString SceneGroupLooks::lookLabel(quint32 paletteId) const
{
    QLCPalette *p = m_doc->palette(paletteId);
    if (p == NULL)
        return tr("(missing palette %1)").arg(paletteId);

    QString name = p->name();
    if (name.isEmpty())
    {
        const QString type = QLCPalette::typeToString(p->type());
        switch (p->type())
        {
        case QLCPalette::Color:
            name = QString("%1 %2").arg(type).arg(p->rgbValue().name()); break;
        case QLCPalette::PanTilt:
            name = QString("%1 P%2 / T%3")
                   .arg(type).arg(p->intValue1()).arg(p->intValue2()); break;
        case QLCPalette::Aim:
            name = type; break;
        default:
            name = QString("%1 %2").arg(type).arg(p->intValue1()); break;
        }
    }

    // Show the folder path too (e.g. "Shutters/Full"), stripping the
    // internal "Palettes/" category prefix.
    QString path = p->path();
    if (path.startsWith(QStringLiteral("Palettes/")))
        path = path.mid(9);
    else if (path == QStringLiteral("Palettes"))
        path.clear();
    if (path.endsWith('/'))
        path.chop(1);
    return path.isEmpty() ? name : path + "/" + name;
}

/** Returns a short "⚠ no look for: X, Y" string for capabilities the fixture
 *  has but the scene's current palette set does not cover.  Empty if all
 *  capabilities are covered (or the fixture has none of note). */
static QString capCoverageHint(Fixture *fxi, const QSet<QLCPalette::PaletteType> &covered)
{
    if (!fxi || !fxi->fixtureMode())
        return QString();

    bool hasRGB = false, hasDimmer = false, hasPT = false, needsShutter = false;
    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        QLCChannel *ch = fxi->fixtureMode()->channel(c);
        if (!ch) continue;
        switch (ch->group())
        {
            case QLCChannel::Pan:
            case QLCChannel::Tilt:
                hasPT = true; break;
            case QLCChannel::Intensity:
                if (ch->colour() == QLCChannel::Red   ||
                    ch->colour() == QLCChannel::Green ||
                    ch->colour() == QLCChannel::Blue)
                    hasRGB = true;
                else
                    hasDimmer = true;
                break;
            case QLCChannel::Colour:
                if (ch->colour() == QLCChannel::Red   ||
                    ch->colour() == QLCChannel::Green ||
                    ch->colour() == QLCChannel::Blue)
                    hasRGB = true;
                break;
            case QLCChannel::Shutter:
            {
                // Only flag shutter as "needs a look" when DMX value 0 defaults
                // to ShutterClose (beam blocked). Fixtures where 0 = ShutterOpen
                // or strobe-off (LED Movinghead-style) are fine without a look.
                QLCCapability *cap = ch->searchCapability(0);
                if (cap && cap->preset() == QLCCapability::ShutterClose)
                    needsShutter = true;
                break;
            }
            default: break;
        }
    }

    QStringList missing;
    if (hasRGB       && !covered.contains(QLCPalette::Color))  missing << QObject::tr("color");
    if (hasDimmer    && !covered.contains(QLCPalette::Dimmer)) missing << QObject::tr("dimmer");
    if (hasPT        && !covered.contains(QLCPalette::PanTilt) &&
                        !covered.contains(QLCPalette::Aim))    missing << QObject::tr("pan/tilt");
    if (needsShutter && !covered.contains(QLCPalette::Shutter))missing << QObject::tr("shutter");

    if (missing.isEmpty())
        return QString();
    return QObject::tr("  ⚠ no look for: %1").arg(missing.join(", "));
}


void SceneGroupLooks::scheduleReachCheck()
{
    if (m_reachCheckPending)
        return;
    m_reachCheckPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_reachCheckPending = false;
        if (m_scene != nullptr && headsThatCannotReach() != m_cannotReach)
            reload();
    });
}

QHash<quint32, QStringList> SceneGroupLooks::headsThatCannotReach() const
{
    QHash<quint32, QStringList> out;
    MonitorProperties *props = m_doc ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr || m_scene == nullptr)
        return out;
    foreach (const AimSolver::SceneAim &a, AimSolver::sceneAims(m_doc, m_scene))
    {
        if (a.solution.reachable())
            continue;
        if (StageTarget *t = props->stageTarget(a.targetId))
            out[a.fixtureId] << t->name();
    }
    return out;
}

void SceneGroupLooks::reload()
{
    // Preserve the effective selection across the rebuild (a type folder is
    // resolved to its fixtures) so adding/removing a target doesn't drop the
    // selection — which would hide the console and make the splitter jump.
    QSet<quint32> selFix, selGrp;
    foreach (QTreeWidgetItem *it, m_targetList->selectedItems())
    {
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture)
            selFix.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetGroup)
            selGrp.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetTypeFolder)
            for (int c = 0; c < it->childCount(); c++)
                selFix.insert(it->child(c)->data(0, Qt::UserRole).toUInt());
    }

    m_targetList->blockSignals(true);
    m_targetList->clear();

    /* Aimed at something they cannot reach. Worse than a missing look: the
       head is not idle, it is stuck at its limit pointing somewhere else. So it
       is red, where the missing-look hint is amber, and it wins the colour. */
    m_cannotReach = headsThatCannotReach();
    const QColor cannotReachColour(220, 60, 50);

    // Which palette types does the current look cover?
    QSet<QLCPalette::PaletteType> coveredTypes;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == NULL)
            continue;
        coveredTypes.insert(p->type());
        /* An effect covers what it drives: its category -- the folder it is
           filed under, else the script's own -- so a colour effect is a look
           for colour ("Effect/Color/sinewave" was flagged "no look for:
           color"). */
        if (p->type() == QLCPalette::Effect)
        {
            QString cat;
            const QString prefix = QStringLiteral("Palettes/Effect/");
            if (p->path().startsWith(prefix))
                cat = p->path().mid(prefix.length()).section('/', 0, 0);
            if (cat.isEmpty() && m_doc->effectScriptRunner() != NULL && !p->scriptPath().isEmpty())
            {
                EffectScriptCache *cache = m_doc->effectScriptRunner()->cache();
                cat = EffectScriptCache::categoryForTypes(
                          cache->scriptMeta(cache->nameFromPath(p->scriptPath())).fixtureTypes);
            }
            if (cat == QLatin1String("Color"))
                coveredTypes.insert(QLCPalette::Color);
            else if (cat == QLatin1String("Dimmer"))
                coveredTypes.insert(QLCPalette::Dimmer);
            else if (cat == QLatin1String("Position"))
                coveredTypes.insert(QLCPalette::PanTilt);
        }
    }

    // Groups = dynamic targets (top level).
    QList<FixtureGroup*> groups;
    foreach (quint32 gid, m_scene->fixtureGroups())
    {
        FixtureGroup *g = m_doc->fixtureGroup(gid);
        if (g != NULL)
            groups.append(g);
    }
    std::sort(groups.begin(), groups.end(),
              [](FixtureGroup *a, FixtureGroup *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });
    foreach (FixtureGroup *g, groups)
    {
        // Aggregate coverage hint across all fixtures in the group.
        QSet<QString> groupMissing;
        foreach (quint32 fid, g->fixtureList())
        {
            const QString hint = capCoverageHint(m_doc->fixture(fid), coveredTypes);
            if (!hint.isEmpty())
            {
                // Extract just the capability names from "  ⚠ no look for: x, y"
                QString caps = hint;
                const int colonIdx = caps.indexOf(':');
                if (colonIdx >= 0)
                    for (const QString &c : caps.mid(colonIdx + 1).trimmed().split(','))
                        groupMissing.insert(c.trimmed());
            }
        }
        QTreeWidgetItem *it = new QTreeWidgetItem(m_targetList);
        QString label = tr("%1  — group").arg(g->name());
        if (!groupMissing.isEmpty())
        {
            QStringList ml = groupMissing.values();
            std::sort(ml.begin(), ml.end());
            label += tr("  ⚠ no look for: %1").arg(ml.join(", "));
            it->setForeground(0, QColor(200, 130, 0));
        }
        int groupShort = 0;
        foreach (quint32 fid, g->fixtureList())
            if (m_cannotReach.contains(fid))
                ++groupShort;
        if (groupShort > 0)
        {
            label += tr("  ⚠ %n can't reach their aim target", "", groupShort);
            it->setForeground(0, cannotReachColour);
        }
        it->setText(0, label);
        it->setData(0, Qt::UserRole, g->id());
        it->setData(0, TARGET_KIND_ROLE, TargetGroup);
    }

    // Fixtures = fixed targets, grouped under per-type folders.
    if (m_includeFixtureTargets)
    {
        QMap<QString, QList<Fixture*> > byType; // type label -> fixtures
        foreach (quint32 fid, m_scene->fixtures())
        {
            Fixture *f = m_doc->fixture(fid);
            if (f != NULL)
                byType[fixtureTypeLabel(f)].append(f);
        }

        QMapIterator<QString, QList<Fixture*> > tit(byType);
        while (tit.hasNext())
        {
            tit.next();
            QList<Fixture*> fxs = tit.value();
            std::sort(fxs.begin(), fxs.end(),
                      [](Fixture *a, Fixture *b) {
                          return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                      });

            QTreeWidgetItem *folder = new QTreeWidgetItem(m_targetList);
            folder->setText(0, tr("%1  (%n)", "", fxs.count()).arg(tit.key()));
            folder->setData(0, TARGET_KIND_ROLE, TargetTypeFolder);
            folder->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            folder->setExpanded(true);
            foreach (Fixture *f, fxs)
            {
                QTreeWidgetItem *it = new QTreeWidgetItem(folder);
                const QString hint = capCoverageHint(f, coveredTypes);
                QString text = hint.isEmpty() ? f->name() : f->name() + hint;
                if (!hint.isEmpty())
                    it->setForeground(0, QColor(200, 130, 0));
                if (m_cannotReach.contains(f->id()))
                {
                    QStringList names = m_cannotReach.value(f->id());
                    names.removeDuplicates();
                    text += tr("  ⚠ can't reach: %1").arg(names.join(", "));
                    it->setForeground(0, cannotReachColour);
                }
                // A staged fixture can be designed against but sends no DMX yet.
                if (!f->isPatched())
                {
                    text += tr("  (unpatched)");
                    it->setToolTip(0, tr("In the inventory: the look is built, but nothing "
                                         "is sent until the fixture is patched"));
                }
                it->setText(0, text);
                it->setData(0, Qt::UserRole, f->id());
                it->setData(0, TARGET_KIND_ROLE, TargetFixture);
            }
        }
    }

    if (m_targetList->topLevelItemCount() == 0)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(m_targetList);
        it->setText(0, tr("(no fixtures — drag groups or fixtures here)"));
        it->setFlags(Qt::NoItemFlags);
    }

    // Restore the previous selection (re-select fixture children / groups),
    // then re-emit once so the bottom panel matches.
    QTreeWidgetItemIterator rit(m_targetList);
    while (*rit != NULL)
    {
        QTreeWidgetItem *it = *rit;
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture && selFix.contains(it->data(0, Qt::UserRole).toUInt()))
            it->setSelected(true);
        else if (kind == TargetGroup && selGrp.contains(it->data(0, Qt::UserRole).toUInt()))
            it->setSelected(true);
        ++rit;
    }
    m_targetList->blockSignals(false);
    slotTargetSelectionChanged();

    // Total distinct fixtures across all targets (fixed + dynamic groups).
    QSet<quint32> allFixtures;
    foreach (quint32 fid, m_scene->fixtures())
        allFixtures.insert(fid);
    foreach (FixtureGroup *g, groups)
        foreach (quint32 fid, g->fixtureList())
            allFixtures.insert(fid);
    QString targetsText = tr("Targets (%n fixture(s))", "", allFixtures.count());
    if (!m_cannotReach.isEmpty())
        targetsText += tr("  —  ⚠ %n can't reach their aim target", "", m_cannotReach.size());
    m_targetsLabel->setText(targetsText);
    m_targetsLabel->setStyleSheet(m_cannotReach.isEmpty()
                                  ? QString() : QStringLiteral("color: rgb(220,60,50);"));

    int unpatched = 0;
    foreach (quint32 fid, allFixtures)
        if (Fixture *f = m_doc->fixture(fid))
            if (!f->isPatched())
                ++unpatched;
    m_unpatchedNote->setVisible(unpatched > 0);
    if (unpatched > 0)
        m_unpatchedNote->setText(unpatched == allFixtures.count()
            ? tr("⚠ None of these fixtures is patched: they send no DMX. The look shows in the "
                 "rig view only. Patch them in the Fixture Manager.")
            : tr("⚠ %1 of these %2 fixtures are not patched: they send no DMX. They show in "
                 "the rig view only. Patch them in the Fixture Manager.")
                  .arg(unpatched).arg(allFixtures.count()));

    // Declared scope: (re)populate from every group in the doc (not just this
    // scene's own targets — scope is a statement of intent, independent of
    // what's actually painted yet), then select whatever the scene has.
    {
        m_scopeCombo->blockSignals(true);
        m_scopeCombo->clear();
        m_scopeCombo->addItem(tr("(not set)"), QVariant(Scene::ScopeUnset));
        m_scopeCombo->addItem(tr("Whole Stage"), QVariant(Scene::ScopeWholeStage));
        int selectIndex = (m_scene->lookScope() == Scene::ScopeWholeStage) ? 1 : 0;
        foreach (FixtureGroup *g, m_doc->fixtureGroups())
        {
            if (g == NULL)
                continue;
            m_scopeCombo->addItem(g->name(), QVariant(Scene::ScopeGroup));
            m_scopeCombo->setItemData(m_scopeCombo->count() - 1, g->id(), Qt::UserRole + 1);
            if (m_scene->lookScope() == Scene::ScopeGroup &&
                m_scene->lookScopeGroupId() == g->id())
                selectIndex = m_scopeCombo->count() - 1;
        }
        m_scopeCombo->setCurrentIndex(selectIndex);
        m_scopeCombo->blockSignals(false);
    }

    // Looks = palettes — preserve the current selection so that moving an
    // effect slider (which triggers reload via slotLookEdited) doesn't
    // deselect the palette and clear the LookEditor.
    quint32 selPid = QLCPalette::invalidId();
    {
        const QList<QTreeWidgetItem*> prevSel = m_lookList->selectedItems();
        if (!prevSel.isEmpty())
            selPid = prevSel.first()->data(LookColName, Qt::UserRole).toUInt();
    }

    m_lookList->blockSignals(true);
    m_lookList->clear();
    // Build the tree from the ordered palette list: each Effect becomes a
    // top-level parent, and any Colour/Dimmer that FOLLOWS it (until the next
    // effect) is nested under it as a child that feeds it. Looks before the first
    // effect stay top-level — the static fixture base. The flat order is the
    // source of truth (see QLCPalette::isEffectScoped); the tree is derived from
    // it and re-derived after every reorder, so nesting == "after this effect".
    QTreeWidgetItem *currentEffectParent = NULL;
    QLCPalette *currentEffect = NULL;
    const QList<quint32> pals = m_scene->palettes();
    foreach (quint32 pid, pals)
    {
        QLCPalette *p = m_doc->palette(pid);
        const bool isEffect = (p != NULL && p->type() == QLCPalette::Effect);

        // Effects, and everything that is the look's own, are top-level; a
        // Colour/Dimmer under an effect nests under it -- the engine's rule
        // (an Aim or Pan/Tilt after an effect is still the look's).
        const bool scoped = !isEffect && currentEffectParent != NULL
                            && QLCPalette::isEffectScoped(m_doc, pals, pid);
        QTreeWidgetItem *it = scoped ? new QTreeWidgetItem(currentEffectParent)
                                     : new QTreeWidgetItem(m_lookList);
        it->setText(LookColName, lookLabel(pid));
        it->setData(LookColName, Qt::UserRole, pid);
        if (p != NULL)
            it->setIcon(LookColName, QIcon(p->iconResource()));

        const bool feedsEffect = scoped;
        // What a nested one does depends on the effect: one that takes
        // colours uses it as its own; one that doesn't (a ballyhoo) has it
        // applied with its output, on the heads it moves.
        const bool takesColours = QLCPalette::effectUsesLookColours(currentEffect);
        it->setToolTip(LookColName,
            isEffect    ? tr("Effect. Colours/dimmers nested under it are its own.")
            : !feedsEffect ? tr("The look's own: lights the fixtures directly, and feeds every "
                                "effect that has no colour or dimmer of its own.")
            : takesColours ? tr("The effect above's own colour/level — not painted on the "
                                "fixtures directly, and not the look's.")
                           : tr("Applied with the effect above, on the heads it drives, while it "
                                "plays — not the look's."));

        // In/Out fade cells (ms at Qt::UserRole; -1 = "step").
        //  • Effect: Fade In is the scene envelope (inert); Fade Out is a real
        //    release time the runner honours (live).
        //  • Colour/Dimmer feeding an effect: BOTH inert — it's a value the effect
        //    reads, not a painted look, so it has no fade of its own.
        //  • Top-level base look: both live.
        // The one rule, shared with the look editor (LookFades).
        const bool inInert  = !LookFades::hasFadeIn(m_doc, m_scene, pid);
        const bool outInert = !LookFades::hasFadeOut(m_doc, m_scene, pid);
        const int inMs  = inInert ? -1 : m_scene->paletteFadeIn(pid);
        const int outMs = m_scene->paletteFadeOut(pid);
        const QString dflt = LookFades::defaultLabel(m_doc, pid);
        it->setData(LookColIn,  Qt::UserRole, inMs);
        it->setData(LookColOut, Qt::UserRole, outMs);
        it->setData(LookColIn,  LookFadeInertRole, inInert);
        it->setData(LookColOut, LookFadeInertRole, outInert);
        it->setData(LookColIn,  LookFadeDefaultRole, tr("scene"));
        it->setData(LookColOut, LookFadeDefaultRole, dflt);
        it->setText(LookColIn,  fadeCellText(it, LookColIn));
        it->setText(LookColOut, fadeCellText(it, LookColOut));
        it->setTextAlignment(LookColIn,  Qt::AlignRight | Qt::AlignVCenter);
        it->setTextAlignment(LookColOut, Qt::AlignRight | Qt::AlignVCenter);
        if (isEffect)
            it->setToolTip(LookColOut,
                tr("Release fade: when this look stops, the effect fades out over "
                   "this time instead of snapping off. \"snap\" = stop at once.\n"
                   "Per-key attack/decay comes from the effect's own parameters "
                   "(e.g. Glow / Release), not this column."));
        else if (feedsEffect)
            it->setToolTip(LookColName, it->toolTip(LookColName) + QLatin1Char(' ')
                                        + tr("It has no fade of its own."));

        // Draggable + (In/Out) editable. Only EFFECT items accept drops, so
        // dropping a colour onto one nests it (feeds the effect); dropping between
        // rows at the top level makes it a base. Leaves never accept children.
        // Editable so a fade cell can be double-clicked (the delegate restricts an
        // effect to its Fade Out cell). Only Effect items accept drops (nesting).
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable
                        | Qt::ItemIsDragEnabled | Qt::ItemIsEditable;
        if (isEffect)
            f |= Qt::ItemIsDropEnabled;
        else
            f &= ~Qt::ItemIsDropEnabled;
        it->setFlags(f);

        if (isEffect)
        {
            currentEffectParent = it;
            currentEffect = p;
        }

        if (selPid != QLCPalette::invalidId() && pid == selPid)
            it->setSelected(true);
    }
    m_lookList->expandAll();
    m_lookList->blockSignals(false);
}

// Routed by MIME: palette -> look, fixture group -> dynamic target,
// fixture -> fixed target.
static bool hasAcceptedFormat(const QMimeData *mime)
{
    return mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType())
        || mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType())
        || mime->hasFormat(FixtureGroupSource::fixtureMimeType());
}

void SceneGroupLooks::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasAcceptedFormat(event->mimeData()))
        event->acceptProposedAction();
}

void SceneGroupLooks::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasAcceptedFormat(event->mimeData()))
        event->acceptProposedAction();
}

/* Where a dropped look actually lands.
 *
 * Scene::addPalette() APPENDS, and in this model the palette ORDER is what
 * decides nesting: a Colour or Dimmer listed after an Effect belongs to that
 * effect rather than painting the scene. So every drop landed at the end, which
 * is after any effect, which means inside it -- whatever the user did, and
 * wherever they aimed. Dropping a colour above the effect in the list put it
 * under the effect anyway.
 *
 * Two rules, in order:
 *   - dropped ON a row, it goes where it was dropped;
 *   - dropped anywhere else, it becomes a BASE look -- before the first effect
 *     -- because "I dragged a colour onto this scene" means colour the scene.
 *     Feeding an effect is the deliberate act, and it has its own gesture
 *     (drop onto the effect), so it should not also be the accident.
 */
void SceneGroupLooks::placeDroppedLooks(const QList<quint32> &pids,
                                        const QPoint &dropPos)
{
    if (m_scene == NULL || pids.isEmpty())
        return;

    QList<quint32> order = m_scene->palettes();

    // Where the user aimed, if they aimed at a row.
    int at = -1;
    const QPoint lp = m_lookList->viewport()->mapFrom(this, dropPos);
    if (QTreeWidgetItem *it = m_lookList->itemAt(lp))
    {
        const quint32 pid = it->data(LookColName, Qt::UserRole).toUInt();
        const int ix = order.indexOf(pid);
        if (ix >= 0)
            at = ix;
    }

    if (at < 0)
    {
        // Before the first effect: a base look.
        at = order.size();
        for (int i = 0; i < order.size(); ++i)
        {
            QLCPalette *p = m_doc->palette(order.at(i));
            if (p != NULL && p->type() == QLCPalette::Effect)
            {
                at = i;
                break;
            }
        }
    }

    // Lift the new ones out and reinsert them together at the chosen point.
    foreach (quint32 pid, pids)
    {
        const int cur = order.indexOf(pid);
        if (cur >= 0)
        {
            order.removeAt(cur);
            if (cur < at)
                --at;
        }
    }
    at = qBound(0, at, order.size());
    for (int i = 0; i < pids.size(); ++i)
        order.insert(at + i, pids.at(i));

    m_scene->reorderPalettes(order);
    m_doc->setModified();
    reload();
    emit sceneModified();
}

void SceneGroupLooks::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (hasAcceptedFormat(mime) == false)
        return;

    // Column precision: a palette (→ Looks) dropped squarely on the Targets
    // column, or a fixture group (→ Targets) dropped squarely on the Looks
    // column, is almost certainly a slip, not intent — the two-column
    // layout visually promises "drag here means this," so silently
    // accepting it anywhere was inconsistent with that. Drops landing
    // between/outside both lists still fall through to the permissive
    // default below (no column is a "clear" wrong answer there).
    const QPoint dropPos = qlcEventPos(event);
    const bool onTargetsCol = m_targetList->geometry().contains(dropPos);
    const bool onLooksCol = m_lookList->geometry().contains(dropPos);
    if (onTargetsCol && mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType())
        && !mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType()))
    {
        QToolTip::showText(mapToGlobal(dropPos),
                            tr("Drop a palette on the Looks list, not Targets."), this);
        return;
    }
    if (onLooksCol && mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType())
        && !mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType()))
    {
        QToolTip::showText(mapToGlobal(dropPos),
                            tr("Drop a fixture group on the Targets list, not Looks."), this);
        return;
    }

    bool changed = false;
    bool palettesAdded = false;

    // If the drop landed on an Effect row in the Looks list, nest any dropped
    // colours/dimmers UNDER that effect (feed it) instead of appending them.
    quint32 dropEffectPid = QLCPalette::invalidId();
    {
        const QPoint lp = m_lookList->viewport()->mapFrom(this, qlcEventPos(event));
        QTreeWidgetItem *it = m_lookList->itemAt(lp);
        while (it != NULL)
        {
            const quint32 pid = it->data(LookColName, Qt::UserRole).toUInt();
            QLCPalette *p = m_doc->palette(pid);
            if (p != NULL && p->type() == QLCPalette::Effect) { dropEffectPid = pid; break; }
            it = it->parent();
        }
    }
    QList<quint32> addedPalettePids;

    if (mime->hasFormat(FunctionsTreeWidget::paletteDragMimeType()))
    {
        QByteArray data = mime->data(FunctionsTreeWidget::paletteDragMimeType());
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 pid = 0;
            stream >> pid;
            if (m_doc->palette(pid) != NULL
                && m_scene->palettes().contains(pid) == false)
            {
                m_scene->addPalette(pid);
                addedPalettePids << pid;
                changed = true;
                palettesAdded = true;
                QLCPalette *ap = m_doc->palette(pid);
                qDebug() << "[DROP] add palette/look" << pid
                            << (ap ? ap->name() : "?")
                            << "type=" << (ap ? (int)ap->type() : -1);
            }
        }
    }

    if (mime->hasFormat(FixtureGroupSource::fixtureGroupMimeType()))
    {
        QByteArray data = mime->data(FixtureGroupSource::fixtureGroupMimeType());
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 gid = 0;
            stream >> gid;
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            if (g != NULL && m_scene->fixtureGroups().contains(gid) == false)
            {
                m_scene->addFixtureGroup(gid);
                changed = true;

                qDebug() << "[DROP] add group" << gid << (g ? g->name() : "?")
                            << "members=" << g->fixtureList()
                            << "| scene fixtures BEFORE=" << m_scene->fixtures()
                            << "groups=" << m_scene->fixtureGroups();

                // The group's applied looks (non-effect palettes) will assert on
                // these fixtures at runtime. Only strip baked values those looks
                // actually replace — stripping a channel no palette covers would
                // leave the fixture dark (nothing else drives it).
                QSet<quint64> covered;
                foreach (quint32 pid, m_scene->palettes())
                {
                    QLCPalette *p = m_doc->palette(pid);
                    if (p == NULL || p->type() == QLCPalette::Effect)
                        continue;
                    const int cOff = QLCPalette::colorSetOffset(m_doc, m_scene->palettes(), pid);
                    foreach (const SceneValue &scv,
                             p->valuesFromFixtureGroups(m_doc, QList<quint32>() << gid, nullptr, cOff))
                        covered.insert((quint64(scv.fxi) << 32) | scv.channel);
                }

                foreach (quint32 fid, g->fixtureList())
                {
                    if (m_scene->fixtures().contains(fid) == false)
                        continue;

                    // Keep any baked value the group's looks don't cover, so the
                    // fixture stays lit; drop only the covered ones.
                    QList<SceneValue> keep;
                    foreach (const SceneValue &scv, m_scene->values())
                    {
                        if (scv.fxi != fid)
                            continue;
                        if (covered.contains((quint64(fid) << 32) | scv.channel))
                            m_scene->unsetValue(fid, scv.channel);
                        else
                            keep << scv;
                    }
                    // Only drop the fixed target if the group's looks fully cover
                    // it; otherwise keep it so its uncovered channels still light.
                    if (keep.isEmpty())
                        m_scene->removeFixture(fid);

                    qDebug() << "[DROP]   fixture" << fid
                                << "coveredByLooks=" << (keep.isEmpty())
                                << "keptBakedValues=" << keep.size();
                }

                qDebug() << "[DROP] scene fixtures AFTER=" << m_scene->fixtures()
                            << "groups=" << m_scene->fixtureGroups();
            }
        }
    }

    if (mime->hasFormat(FixtureGroupSource::fixtureMimeType()))
    {
        QByteArray data = mime->data(FixtureGroupSource::fixtureMimeType());
        QDataStream stream(&data, QIODevice::ReadOnly);
        while (stream.atEnd() == false)
        {
            quint32 fid = 0;
            stream >> fid;
            if (m_doc->fixture(fid) != NULL
                && m_scene->fixtures().contains(fid) == false)
            {
                m_scene->addFixture(fid);
                changed = true;
                qDebug() << "[DROP] add single fixture" << fid
                            << "| scene fixtures now=" << m_scene->fixtures();
            }
        }
    }

    // When a look was applied, let the palettes take over their channels.
    // Deferred so the modal prompt doesn't spin a nested event loop inside the
    // drop event (before it's been accepted / the drag source cleaned up).
    if (palettesAdded)
        QTimer::singleShot(0, this, [this]() { reconcileAfterPaletteApply(); });

    // Nest the just-dropped looks under the effect they were dropped on. This
    // reorders + reloads + emits, so skip the generic refresh below.
    const bool nested = (dropEffectPid != QLCPalette::invalidId() && !addedPalettePids.isEmpty());
    if (nested)
        moveLooksToTarget(addedPalettePids, dropEffectPid);
    else if (addedPalettePids.isEmpty() == false)
    {
        placeDroppedLooks(addedPalettePids, dropPos);
    }
    else if (changed)
    {
        m_doc->setModified();
        reload();
        emit sceneModified();
    }
    event->acceptProposedAction();
}

void SceneGroupLooks::reconcileAfterPaletteApply()
{
    // Channels covered by every currently-applied palette (key = fxi<<32|ch).
    QSet<quint64> covered;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == NULL)
            continue;
        const int cOff = QLCPalette::colorSetOffset(m_doc, m_scene->palettes(), pid);
        foreach (const SceneValue &scv, p->valuesFromFixtureGroups(m_doc, m_scene->fixtureGroups(), nullptr, cOff))
            covered.insert((quint64(scv.fxi) << 32) | scv.channel);
        foreach (const SceneValue &scv, p->valuesFromFixtures(m_doc, m_scene->fixtures(), nullptr, cOff))
            covered.insert((quint64(scv.fxi) << 32) | scv.channel);
    }

    // Strip baked values the palettes now control (palette paramount); gather
    // the rest (baked channels no applied look covers).
    QList<SceneValue> uncovered;
    foreach (const SceneValue &scv, m_scene->values())
    {
        if (covered.contains((quint64(scv.fxi) << 32) | scv.channel))
            m_scene->unsetValue(scv.fxi, scv.channel);
        else
            uncovered << scv;
    }

    if (uncovered.isEmpty() == false)
    {
        // Count the distinct fixtures involved so the total reads sensibly
        // (it's per-fixture channels × fixtures, not just "channels").
        QSet<quint32> fxis;
        foreach (const SceneValue &scv, uncovered)
            fxis.insert(scv.fxi);
        const int ret = QMessageBox::question(this, tr("Apply look"),
            tr("This scene still sets %1 channel value(s) across %2 fixture(s) "
               "that no applied look covers.\n"
               "Clear them so the applied looks fully control this scene?")
               .arg(uncovered.count()).arg(fxis.count()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes)
        {
            foreach (const SceneValue &scv, uncovered)
                m_scene->unsetValue(scv.fxi, scv.channel);
        }
    }

    // Trigger a second runtime reset so the scene fader is rebuilt from the
    // NOW-reconciled value list (baked channels stripped above).  The first
    // resetRuntime fired synchronously in the dropEvent's sceneModified path,
    // before this deferred call ran.
    emit sceneModified();
}

void SceneGroupLooks::slotRemoveTarget()
{
    const QList<QTreeWidgetItem*> sel = m_targetList->selectedItems();
    if (sel.isEmpty())
        return;

    // Resolve the selection to fixture ids + group ids (a type folder removes
    // all its fixtures).
    QSet<quint32> fixIds, grpIds;
    foreach (QTreeWidgetItem *it, sel)
    {
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture)
            fixIds.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetGroup)
            grpIds.insert(it->data(0, Qt::UserRole).toUInt());
        else if (kind == TargetTypeFolder)
            for (int c = 0; c < it->childCount(); c++)
                fixIds.insert(it->child(c)->data(0, Qt::UserRole).toUInt());
    }

    bool changed = false;
    foreach (quint32 id, fixIds)
    {
        // Also clear any baked static values for this fixture, otherwise they
        // are written AFTER (and override) the scene's palette looks.
        Fixture *f = m_doc->fixture(id);
        if (f != NULL)
            for (quint32 i = 0; i < f->channels(); i++)
                m_scene->unsetValue(id, i);
        changed = m_scene->removeFixture(id) || changed;
    }
    foreach (quint32 id, grpIds)
        changed = m_scene->removeFixtureGroup(id) || changed;

    if (changed)
    {
        m_doc->setModified();
        reload();
        emit sceneModified();
    }
}

void SceneGroupLooks::slotTargetContextMenu(const QPoint &pos)
{
    if (m_targetList->selectedItems().isEmpty())
        return;
    QMenu menu(m_targetList);
    QAction *removeAct = menu.addAction(tr("Remove"));
    if (menu.exec(m_targetList->viewport()->mapToGlobal(pos)) == removeAct)
        slotRemoveTarget();
}

void SceneGroupLooks::slotScopeChanged(int index)
{
    if (index < 0)
        return;
    const Scene::LookScope scope =
        static_cast<Scene::LookScope>(m_scopeCombo->itemData(index).toInt());
    const quint32 groupId = (scope == Scene::ScopeGroup)
        ? m_scopeCombo->itemData(index, Qt::UserRole + 1).toUInt()
        : FixtureGroup::invalidId();
    m_scene->setLookScope(scope, groupId);
    m_doc->setModified();
    emit sceneModified();
}

void SceneGroupLooks::slotLookSelectionChanged()
{
    const QList<QTreeWidgetItem*> sel = m_lookList->selectedItems();
    emit lookSelected(sel.isEmpty() ? QLCPalette::invalidId()
                                    : sel.first()->data(LookColName, Qt::UserRole).toUInt());
}

void SceneGroupLooks::slotLookDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (item == NULL)
        return;
    // Double-clicking In/Out opens the fade editor (delegate) — leave it be.
    if (column != LookColName)
        return;
    // Force-load this look into the editor (switches the bottom panel away
    // from the per-fixture channel editor).
    emit lookSelected(item->data(LookColName, Qt::UserRole).toUInt());
}

void SceneGroupLooks::slotLookFadeEdited(QTreeWidgetItem *item, int column)
{
    if (item == NULL || (column != LookColIn && column != LookColOut))
        return;
    const quint32 pid = item->data(LookColName, Qt::UserRole).toUInt();
    if (pid == QLCPalette::invalidId())
        return;
    // Keep the display text in step with the committed value.
    const int inMs  = item->data(LookColIn,  Qt::UserRole).toInt();
    const int outMs = item->data(LookColOut, Qt::UserRole).toInt();
    m_scene->setPaletteFade(pid, inMs, outMs);
    m_doc->setModified();
    emit sceneModified();
}

void SceneGroupLooks::moveLooksToTarget(const QList<quint32> &pids, quint32 effectPid)
{
    if (pids.isEmpty())
        return;

    QList<quint32> order = m_scene->palettes();
    // Pull the moved palettes out, preserving their current relative order.
    QList<quint32> block;
    foreach (quint32 pid, order)
        if (pids.contains(pid))
            block << pid;
    foreach (quint32 pid, block)
        order.removeAll(pid);

    int pos;
    if (effectPid == QLCPalette::invalidId())
    {
        // Base: insert before the first Effect (so isEffectScoped() is false).
        pos = order.size();
        for (int i = 0; i < order.size(); ++i)
        {
            QLCPalette *p = m_doc->palette(order.at(i));
            if (p != NULL && p->type() == QLCPalette::Effect) { pos = i; break; }
        }
    }
    else
    {
        // Feed effect: insert right after it (becomes its first child).
        pos = order.indexOf(effectPid) + 1;
        if (pos == 0)
            pos = order.size();   // effect vanished — fall back to the end
    }
    for (int i = 0; i < block.size(); ++i)
        order.insert(pos + i, block.at(i));

    // reload() unconditionally, even when reorderPalettes() finds the computed
    // order identical to the current one (its own no-op guard): a freshly
    // dropped palette was just appended by addPalette() and has never been
    // RENDERED in the tree at all yet. When "nest right after this effect"
    // happens to land at the same index addPalette()'s append already put it
    // -- the ordinary case of dropping onto your only/last effect -- the
    // reorder is a genuine no-op on the DATA, but the tree still needs its
    // first render showing the palette nested as a child, not floating at
    // the top level where it was drawn before this call ran at all.
    if (m_scene->reorderPalettes(order))
    {
        m_doc->setModified();
        emit sceneModified();
    }
    reload();
}

void SceneGroupLooks::slotLookContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *clicked = m_lookList->itemAt(pos);
    QList<QTreeWidgetItem*> rows = m_lookList->selectedItems();
    if (clicked != NULL && rows.contains(clicked) == false)
        rows = QList<QTreeWidgetItem*>() << clicked;
    // Only looks that carry a fade (non-Effect, editable) can be reset / re-homed.
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](QTreeWidgetItem *it) {
                   return (it->flags() & Qt::ItemIsEditable) == 0;
               }), rows.end());
    if (rows.isEmpty())
        return;

    // Collect the moved palette ids + whether any row is currently nested under
    // an effect (→ can become base) or at the top level (→ can feed an effect).
    QList<quint32> movePids;
    bool anyNested = false, anyBase = false;
    foreach (QTreeWidgetItem *it, rows)
    {
        movePids << it->data(LookColName, Qt::UserRole).toUInt();
        if (it->parent() != NULL) anyNested = true; else anyBase = true;
    }
    // The look's effects, in list order, for the "Feed effect ▸" submenu.
    QList<QPair<quint32,QString>> effects;
    foreach (quint32 pid, m_scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p != NULL && p->type() == QLCPalette::Effect)
            effects << qMakePair(pid, p->name());
    }

    QMenu menu(m_lookList);
    // --- Role: base (fixtures) vs feed an effect ---
    QAction *toBase = NULL;
    QHash<QAction*, quint32> feedActions;
    if (!effects.isEmpty())
    {
        if (anyNested)
            toBase = menu.addAction(tr("Move to fixtures (static base)"));
        if (anyBase || anyNested)
        {
            QMenu *feed = menu.addMenu(tr("Feed effect"));
            for (const auto &e : effects)
            {
                QAction *a = feed->addAction(e.second.isEmpty() ? tr("(effect)") : e.second);
                feedActions.insert(a, e.first);
            }
        }
        menu.addSeparator();
    }
    QAction *rin   = menu.addAction(tr("Reset Fade In to step"));
    QAction *rout  = menu.addAction(tr("Reset Fade Out to step"));
    QAction *rboth = menu.addAction(tr("Reset both to step"));
    QAction *chosen = menu.exec(m_lookList->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (chosen == toBase)
    {
        moveLooksToTarget(movePids, QLCPalette::invalidId());
        return;
    }
    if (feedActions.contains(chosen))
    {
        moveLooksToTarget(movePids, feedActions.value(chosen));
        return;
    }

    const bool resetIn  = (chosen == rin  || chosen == rboth);
    const bool resetOut = (chosen == rout || chosen == rboth);

    m_lookList->blockSignals(true);
    foreach (QTreeWidgetItem *it, rows)
    {
        const quint32 pid = it->data(LookColName, Qt::UserRole).toUInt();
        int inMs  = it->data(LookColIn,  Qt::UserRole).toInt();
        int outMs = it->data(LookColOut, Qt::UserRole).toInt();
        if (resetIn)  inMs  = -1;
        if (resetOut) outMs = -1;
        it->setData(LookColIn,  Qt::UserRole, inMs);
        it->setData(LookColOut, Qt::UserRole, outMs);
        it->setText(LookColIn,  fadeCellText(it, LookColIn));
        it->setText(LookColOut, fadeCellText(it, LookColOut));
        m_scene->setPaletteFade(pid, inMs, outMs);
    }
    m_lookList->blockSignals(false);
    slotLookSelectionChanged();  // refresh bottom look editor fade spinboxes

    m_doc->setModified();
    emit sceneModified();
}

void SceneGroupLooks::slotTargetSelectionChanged()
{
    // Selecting a type folder edits all its fixtures together. Preserve order
    // and de-duplicate (a folder + one of its children could both be selected).
    QList<quint32> fixtures;
    foreach (QTreeWidgetItem *it, m_targetList->selectedItems())
    {
        const int kind = it->data(0, TARGET_KIND_ROLE).toInt();
        if (kind == TargetFixture)
        {
            const quint32 id = it->data(0, Qt::UserRole).toUInt();
            if (fixtures.contains(id) == false)
                fixtures << id;
        }
        else if (kind == TargetTypeFolder)
        {
            for (int c = 0; c < it->childCount(); c++)
            {
                const quint32 id = it->child(c)->data(0, Qt::UserRole).toUInt();
                if (fixtures.contains(id) == false)
                    fixtures << id;
            }
        }
        else if (kind == TargetGroup)
        {
            const quint32 gid = it->data(0, Qt::UserRole).toUInt();
            FixtureGroup *g = m_doc->fixtureGroup(gid);
            qDebug() << "TargetGroup selected: gid=" << gid
                     << "group=" << (g ? g->name() : "(null)")
                     << "fixtures=" << (g ? g->fixtureList() : QList<quint32>());
            if (g != NULL)
            {
                foreach (quint32 fid, g->fixtureList())
                    if (fixtures.contains(fid) == false)
                        fixtures << fid;
            }
        }
    }
    qDebug() << "slotTargetSelectionChanged: emitting" << fixtures.size() << "fixtures" << fixtures;
    emit fixturesSelected(fixtures);
}

void SceneGroupLooks::slotRemoveLook()
{
    const QList<QTreeWidgetItem*> sel = m_lookList->selectedItems();
    if (sel.isEmpty())
        return;
    // Detach from the scene only; leave the palette in the Doc so any
    // other scene referencing it keeps working.
    foreach (QTreeWidgetItem *it, sel)
        m_scene->removePalette(it->data(LookColName, Qt::UserRole).toUInt());
    m_doc->setModified();
    reload();
    emit sceneModified();
}

void SceneGroupLooks::slotMoveLookUp()
{
    moveSelectedLooks(-1);
}

void SceneGroupLooks::slotMoveLookDown()
{
    moveSelectedLooks(+1);
}

void SceneGroupLooks::moveSelectedLooks(int delta)
{
    const QList<QTreeWidgetItem*> sel = m_lookList->selectedItems();
    if (sel.isEmpty() || delta == 0)
        return;

    // Move in the FLAT visual order (== the palette order), so a step can cross a
    // nesting boundary: Up on a colour at the top of an effect's children pops it
    // ABOVE the effect (→ becomes a base on reload); Down on a base colour pushes
    // it under the following effect. reorderPalettes → reload() re-derives the
    // tree, so nesting follows position automatically.
    QList<quint32> order = m_scene->palettes();
    QList<quint32> selPids;
    foreach (QTreeWidgetItem *it, sel)
        selPids << it->data(LookColName, Qt::UserRole).toUInt();

    QList<int> idx;
    foreach (quint32 pid, selPids)
    {
        const int i = order.indexOf(pid);
        if (i >= 0) idx << i;
    }
    if (idx.isEmpty())
        return;
    std::sort(idx.begin(), idx.end());
    // Block-edge guard: don't run off either end.
    if (idx.first() + delta < 0 || idx.last() + delta >= order.size())
        return;
    // Move each item; process the trailing edge first so indices stay valid.
    if (delta > 0)
        std::reverse(idx.begin(), idx.end());
    foreach (int i, idx)
        order.move(i, i + delta);

    if (m_scene->reorderPalettes(order))
    {
        m_doc->setModified();
        emit sceneModified();
        reload();
        // Re-select the moved looks (reload() rebuilt the tree).
        QTreeWidgetItemIterator sit(m_lookList);
        while (*sit)
        {
            if (selPids.contains((*sit)->data(LookColName, Qt::UserRole).toUInt()))
                (*sit)->setSelected(true);
            ++sit;
        }
    }
}

bool SceneGroupLooks::eventFilter(QObject *watched, QEvent *event)
{
    // The look list reorders by internal drag-drop. QTreeWidget performs the drop
    // as a take+insert (rowsRemoved + rowsInserted), so there is no rowsMoved to
    // hook — instead we watch the viewport for the drop itself. Let the view
    // process the drop normally (return false), then sync the resulting order.
    if (watched == m_lookList->viewport() && event->type() == QEvent::Drop)
        slotLooksReordered();
    return QWidget::eventFilter(watched, event);
}

void SceneGroupLooks::slotLooksReordered()
{
    // Fired after an internal drag-drop. Defer the sync so it runs once the drop
    // fully settles (items taken/inserted), not mid-operation.
    QTimer::singleShot(0, this, [this]() { applyLookOrderFromList(); });
}

void SceneGroupLooks::applyLookOrderFromList()
{
    // Depth-first walk: each top-level item, then its children in order. A child
    // (colour nested under an effect) thus lands right after its effect in the
    // flat order, which is exactly what marks it "effect-scoped" (isEffectScoped).
    // reorderPalettes → reload() re-derives the canonical tree from this order, so
    // any transient/invalid nesting (e.g. an effect dropped under an effect) is
    // flattened back automatically.
    QList<quint32> order;
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem *item) {
        order << item->data(LookColName, Qt::UserRole).toUInt();
        for (int c = 0; c < item->childCount(); ++c)
            walk(item->child(c));
    };
    for (int i = 0; i < m_lookList->topLevelItemCount(); ++i)
        walk(m_lookList->topLevelItem(i));

    if (m_scene->reorderPalettes(order))
    {
        m_doc->setModified();
        emit sceneModified();
        // Rebuild the tree from the new flat order so nesting is canonical (a
        // colour dropped between two effects re-homes under the effect above it).
        reload();
    }
}
