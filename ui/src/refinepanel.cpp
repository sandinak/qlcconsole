/*
  Q Light Controller Plus
  refinepanel.cpp

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QMessageBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QCheckBox>
#include <QLabel>
#include <QFontMetrics>

#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "focuscontrolmap.h"
#include "refinesession.h"
#include "operatorvoice.h"
#include "refinepanel.h"
#include "focusmodepanel.h"
#include "fixture.h"
#include "doc.h"

namespace
{
/* A label that never moves the layout: word-wrapped into a fixed number of
   lines (call after its font is set), ignoring its text's width, and keeping
   its room while hidden -- so a press that changes it never resizes the
   map below or widens the window. Its tooltip has the whole text. (The same
   as FocusModePanel's.) */
void reserveLines(QLabel *label, int lines)
{
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QSizePolicy sp(QSizePolicy::Ignored, QSizePolicy::Fixed);
    sp.setRetainSizeWhenHidden(true);
    label->setSizePolicy(sp);
    label->setFixedHeight(QFontMetrics(label->font()).lineSpacing() * lines
                          + 2 * label->margin() + 2 * label->frameWidth() + 2);
}

/* Keep @p w's room while it is hidden. */
void retainWhenHidden(QWidget *w)
{
    QSizePolicy sp = w->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    w->setSizePolicy(sp);
}
}

QPointer<RefinePanel> RefinePanel::s_instance;

RefinePanel *RefinePanel::instance()
{
    return s_instance.data();
}

RefinePanel *RefinePanel::open(Doc *doc, QWidget *parent, quint32 cueId, quint32 layerId, QString *why)
{
    RefineSession *s = doc->programmer()->refineSession();
    if (s_instance != nullptr && s->isActive() && s->cueId() == cueId)
    {
        s_instance->show();
        s_instance->raise();
        return s_instance.data();
    }
    QString reason;
    if (!s->start(cueId, layerId, &reason))
    {
        if (why != nullptr)
            *why = reason;
        else
            QMessageBox::information(parent, tr("Focus This Cue"), reason);
        return nullptr;
    }
    return showPanel(doc, parent);
}

RefinePanel *RefinePanel::openScene(Doc *doc, QWidget *parent, const QList<quint32> &sceneIds,
                                    quint32 first, quint32 groupId, quint32 fixtureId, QString *why)
{
    RefineSession *s = doc->programmer()->refineSession();
    QString reason;
    if (!s->startScene(sceneIds, first, &reason))
    {
        if (why != nullptr)
            *why = reason;
        else
            QMessageBox::information(parent, tr("Focus This Look"), reason);
        return nullptr;
    }
    // From a row: that group or fixture, as the Show Tree's layer menu does.
    if (fixtureId != 0xFFFFFFFF)
        s->pickFixture(fixtureId);
    else if (groupId != 0xFFFFFFFF)
        s->pickGroup(groupId);
    return showPanel(doc, parent);
}

RefinePanel *RefinePanel::showPanel(Doc *doc, QWidget *parent)
{
    if (s_instance != nullptr)
    {
        s_instance->refresh();
        s_instance->show();
        s_instance->raise();
        return s_instance.data();
    }
    RefinePanel *p = new RefinePanel(doc, parent);
    s_instance = p;
    p->refresh();
    p->show();
    p->raise();
    p->activateWindow();
    return p;
}

RefinePanel::RefinePanel(Doc *doc, QWidget *parent)
    : QWidget(parent, Qt::Tool)
    , m_doc(doc)
    , m_session(doc->programmer()->refineSession())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    setObjectName(QStringLiteral("refinePanel"));
    setWindowTitle(tr("Focus This Cue"));
    setProperty("helpTopic", QStringLiteral("refine.md"));
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *vl = new QVBoxLayout(this);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("refineTitle"));
    m_layer = new QLabel(this);
    m_layer->setObjectName(QStringLiteral("refineLayer"));
    QFont big = m_layer->font();
    big.setPointSizeF(big.pointSizeF() * 1.6);
    big.setBold(true);
    m_layer->setFont(big);
    m_scope = new QLabel(this);
    m_scope->setObjectName(QStringLiteral("refineScope"));
    m_pending = new QLabel(this);
    m_pending->setObjectName(QStringLiteral("refinePending"));
    m_device = new QLabel(this);
    m_device->setStyleSheet(QStringLiteral("color: palette(mid);"));
    /* Each keeps one size whatever it says: a press changes the words,
       never the layout (the map under them grew and shrank). */
    reserveLines(m_title, 2);       // room for the HOLDING prefix
    reserveLines(m_layer, 1);
    reserveLines(m_scope, 2);
    reserveLines(m_pending, 2);
    reserveLines(m_device, 1);
    for (QLabel *l : { m_title, m_layer, m_scope })
        vl->addWidget(l);
    // The same lamp as Focus mode's: blue all kept, yellow here, orange elsewhere.
    {
        QHBoxLayout *lampRow = new QHBoxLayout;
        m_lamp = new FocusStatusLamp(this);
        m_lamp->setObjectName(QStringLiteral("refineStatusLamp"));
        lampRow->addWidget(m_lamp);
        lampRow->addWidget(m_pending, 1);
        vl->addLayout(lampRow);
    }
    // A layer only tracked in (section 6C): where A keeps the change.
    m_toSource = new QCheckBox(this);
    m_toSource->setObjectName(QStringLiteral("refineToSource"));
    m_toSource->setFocusPolicy(Qt::NoFocus);
    m_toSource->setToolTip(doc->programmer()->padText(
        tr("Off (the default): {A} keeps the change in this cue, and the layer becomes "
           "its own here. On: {A} keeps it in the cue its values track from, so it "
           "changes there and in every cue that tracks it. S switches.")));
    retainWhenHidden(m_toSource);
    m_toSource->hide();
    connect(m_toSource, &QCheckBox::toggled, this, [this](bool on) {
        if (on != m_session->keepsToSource())
            m_session->setKeepToSource(on);
    });
    vl->addWidget(m_toSource);
    // Board Q116: a shared palette -- who else uses it, and the second A.
    {
        m_shared = new QWidget(this);
        m_shared->setObjectName(QStringLiteral("refineShared"));
        QHBoxLayout *sh = new QHBoxLayout(m_shared);
        sh->setContentsMargins(0, 0, 0, 0);
        m_sharedLabel = new QLabel(m_shared);
        m_sharedLabel->setObjectName(QStringLiteral("refineSharedText"));
        m_sharedLabel->setWordWrap(true);
        m_sharedLabel->setStyleSheet(QStringLiteral("color: #d08a1c; font-weight: bold;"));
        reserveLines(m_sharedLabel, 2);     // the warning, and who else uses it
        sh->addWidget(m_sharedLabel, 1);
        QPushButton *all = new QPushButton(tr("Change them all"), m_shared);
        all->setObjectName(QStringLiteral("refineSharedConfirm"));
        all->setFocusPolicy(Qt::NoFocus);
        connect(all, &QPushButton::clicked, this, [this]() { m_session->confirmShared(); });
        sh->addWidget(all);
        retainWhenHidden(m_shared);
        m_shared->hide();
        vl->addWidget(m_shared);
    }
    vl->addWidget(m_device);

    m_map = new FocusControlMap(this);
    m_map->setTool(ControllerScheme::RefineTool);
    m_map->setFocusPolicy(Qt::NoFocus);
    vl->addWidget(m_map, 1);
    connect(m_map, &FocusControlMap::controlClicked, this, [this](int role) {
        m_session->apply(ControllerScheme::verbFor(GamepadRole::Role(role)));
    });

    QHBoxLayout *bottom = new QHBoxLayout;
    // Feedback: lights, voice or both (shared with Focus mode).
    bottom->addWidget(new QLabel(tr("Feedback:"), this));
    bottom->addWidget(new OperatorFeedbackCombo(doc->programmer(), this));
    bottom->addWidget(new QLabel(tr("Voice:"), this));
    bottom->addWidget(new VoiceDetailCombo(doc->programmer(), this));
    bottom->addWidget(new QLabel(tr("Waits:"), this));
    bottom->addWidget(new VoiceSettleCombo(this));
    QPushButton *keep = new QPushButton(this);
    keep->setObjectName(QStringLiteral("refineKeep"));
    QPushButton *back = new QPushButton(this);
    back->setObjectName(QStringLiteral("refineRevert"));
    QPushButton *exitBtn = new QPushButton(tr("Exit"), this);
    exitBtn->setObjectName(QStringLiteral("refineExit"));
    for (QPushButton *b : { keep, back, exitBtn })
        b->setFocusPolicy(Qt::NoFocus);
    bottom->addWidget(keep);
    bottom->addWidget(back);
    bottom->addStretch();
    bottom->addWidget(exitBtn);
    vl->addLayout(bottom);
    connect(keep, &QPushButton::clicked, this, [this]() { m_session->keep(); });
    connect(back, &QPushButton::clicked, this, [this]() { m_session->revert(); });
    connect(exitBtn, &QPushButton::clicked, this, [this]() { requestExit(); });

    ProgrammerController *pc = m_doc->programmer();
    auto controls = [this, pc, keep, back]() {
        const ProgrammerController::FocusController pad = pc->focusController();
        // The pad's own names (Q170): "Keep (Cross)" on a PlayStation pad.
        const PadNames::Family family = pad.present ? pad.family : PadNames::Xbox;
        keep->setText(PadNames::expand(tr("Keep ({A})"), family));
        back->setText(PadNames::expand(tr("Back to the cue ({B})"), family));
        m_map->setControls(pad.controls);
        m_map->setFamily(pad.present ? pad.family : PadNames::Xbox);
        m_device->setText(!pc->controllerOwnerName().isEmpty()
                          ? tr("Controller: %1 has it (any other is ignored).").arg(pc->controllerOwnerName())
                          : pad.present ? tr("Controller: %1").arg(pad.name)
                          : pc->isControllerLost()
                            ? tr("Controller lost: the lights are held. The keys work (see Help).")
                            : tr("No controller connected: the keys work (see Help)."));
        m_device->setToolTip(m_device->text());
    };
    controls();
    connect(pc, &ProgrammerController::focusControllerChanged, this, controls);
    // The light bar on the map, as on the pad (Q170).
    m_map->setLightColour(pc->padLight());
    connect(pc, &ProgrammerController::padLightChanged, m_map, &FocusControlMap::setLightColour);
    connect(pc, &ProgrammerController::controllerLost, this, controls);
    connect(pc, &ProgrammerController::holdingChanged, this, &RefinePanel::refresh);
    connect(pc, &ProgrammerController::focusControllerInput, this, [this](int role, float value) {
        m_map->setControlValue(role, value);
    });
    connect(m_session, &RefineSession::changed, this, &RefinePanel::refresh);
    connect(m_session, &RefineSession::exitRequested, this, [this]() { requestExit(); });
    connect(m_session, &RefineSession::stopped, this, [this]() {
        if (!m_finishing)
        {
            m_finishing = true;
            close();
        }
    });
    resize(640, 560);
}

RefinePanel::~RefinePanel()
{
    if (m_session->isActive())
    {
        m_finishing = true;
        m_session->stop();
    }
}

QString RefinePanel::headline() const
{
    return m_title->text() + QLatin1Char('\n') + m_layer->text();
}

QString RefinePanel::scopeLine() const
{
    return m_scope->text();
}

QString RefinePanel::pendingLine() const
{
    return m_pending->text();
}

QString RefinePanel::sharedLine() const
{
    return m_shared->isHidden() ? QString() : m_sharedLabel->text();
}

QString RefinePanel::keepToLine() const
{
    return m_toSource->isHidden() ? QString() : m_toSource->text();
}

void RefinePanel::refresh()
{
    if (!m_session->isActive())
        return;
    const ShowTree::Cue *cue = m_doc->showTree()->cue(m_session->cueId());
    setWindowTitle(m_session->isSceneMode() ? tr("Focus This Look") : tr("Focus This Cue"));
    if (m_session->isSceneMode())
    {
        const Function *f = m_doc->function(m_session->currentLayer());
        m_title->setText(tr("Focus this look \342\200\224 scene %1  \302\267  %2 of %3 (Build)")
                         .arg(f != nullptr ? f->name() : QString())
                         .arg(m_session->layerIndex() + 1).arg(m_session->layers().size()));
    }
    else
        m_title->setText(tr("Focus this cue \342\200\224 cue %1 %2  \302\267  layer %3 of %4")
                         .arg(cue != nullptr ? cue->number : QString(),
                              cue != nullptr ? cue->name : QString())
                         .arg(m_session->layerIndex() + 1).arg(m_session->layers().size()));
    // The freeze chord (Q140): say so where it is seen first.
    if (m_doc->programmer()->isHolding())
        m_title->setText(tr("HOLDING \342\200\224 click both sticks (or H) to go on.  ") + m_title->text());
    m_title->setToolTip(m_title->text());
    m_layer->setText(m_session->wherePhrase(ControllerScheme::VerboseVoice));
    m_layer->setToolTip(m_layer->text());
    QString scope;
    switch (m_session->scope())
    {
        case RefineSession::LayerScope:
            scope = m_session->isSceneMode()
                    ? tr("Scene: edits the scene's own looks (its palettes), for all %1 of its fixtures.")
                      .arg(m_session->layerFixtures().size())
                    : tr("Layer: edits the layer's own look, for all %1 of its fixtures.")
                      .arg(m_session->layerFixtures().size());
            break;
        case RefineSession::GroupScope:
            scope = m_session->isSceneMode()
                    ? tr("Group: an exception for this group (out of the palette, the scene's own values).")
                    : tr("Group: an exception for this group (a child layer).");
            break;
        case RefineSession::FixtureScope:
        {
            const Fixture *fx = m_doc->fixture(m_session->currentFixture());
            scope = (m_session->isSceneMode()
                     ? tr("Fixture: an exception for %1 alone (out of the palette, the scene's own values).")
                     : tr("Fixture: an exception for %1 alone (a child layer)."))
                    .arg(fx != nullptr ? fx->name() : QString());
            break;
        }
    }
    const QString page = m_session->page() == RefineSession::BeamPage ? tr("beam (spot down / flood out, sharper / softer)")
                       : m_session->page() == RefineSession::ColourPage ? tr("colour palettes")
                                                                        : tr("gobo palettes");
    m_scope->setText(m_doc->programmer()->padText(tr("%1  {Y} changes the scope.  D-pad: %2 ({LS} click changes it)."))
                     .arg(scope, page));
    m_scope->setToolTip(m_scope->text());
    if (m_session->isTrackedLayer())
    {
        const ShowTree::Cue *src = m_doc->showTree()->cue(m_session->trackedSource());
        m_toSource->setText(tr("Keep in the cue it tracks from (cue %1), not this one")
                            .arg(src != nullptr ? (src->number.isEmpty() ? src->name : src->number) : QString()));
        QSignalBlocker block(m_toSource);
        m_toSource->setChecked(m_session->keepsToSource());
        m_toSource->show();
    }
    else
        m_toSource->hide();

    if (!m_session->sharedWarning().isEmpty())
    {
        m_sharedLabel->setText(m_session->sharedWarning() + QStringLiteral("\n")
                               + m_session->sharedUsers().join(QStringLiteral(", ")));
        m_shared->show();
    }
    else
        m_shared->hide();
    {
        const bool here = m_session->hasPendingHere();
        const bool elsewhere = m_session->pendingCount() > (here ? 1 : 0);
        m_lamp->setState(elsewhere ? (here ? FocusStatusLamp::BothUnsaved : FocusStatusLamp::OthersUnsaved)
                                   : here ? FocusStatusLamp::ThisUnsaved : FocusStatusLamp::AllSaved);
        m_lamp->setCount(m_session->pendingCount());
    }
    if (!m_session->hasPending())
    {
        m_pending->setText(tr("Nothing pending: the rig shows the cue."));
        m_pending->setStyleSheet(QStringLiteral("color: #0072b2;"));
        return;
    }
    QStringList parts;
    const quint32 fid = m_session->scopeFixtures().value(0, 0xFFFFFFFF);
    float pan = 0.0f, tilt = 0.0f, zoom = 0.0f;
    int focus = 0;
    if (m_session->pendingPosition(fid, pan, tilt))
        parts << tr("pan %1\302\260 tilt %2\302\260").arg(qRound(pan)).arg(qRound(tilt));
    if (m_session->pendingZoom(fid, zoom))
        parts << tr("zoom %1\302\260").arg(double(zoom), 0, 'f', 1);
    if (m_session->pendingFocus(fid, focus))
        parts << tr("focus %1%").arg(qRound(focus * 100.0 / 255.0));
    m_pending->setText(m_doc->programmer()->padText(tr("Pending ({A} keeps it in the cue, {B} goes back): %1"))
                       .arg(parts.isEmpty() ? tr("a palette") : parts.join(QStringLiteral(", "))));
    m_pending->setStyleSheet(QStringLiteral("color: #b08a10;"));
}

bool RefinePanel::requestExit()
{
    if (m_session->hasPending())
    {
        const QMessageBox::StandardButton b = QMessageBox::question(
            this, windowTitle(), tr("Keep what is pending in the cue?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (b == QMessageBox::Cancel)
            return false;
        if (b == QMessageBox::Save)
            m_session->keep();
    }
    m_finishing = true;
    m_session->stop();
    close();
    return true;
}

void RefinePanel::closeEvent(QCloseEvent *event)
{
    if (!m_finishing && m_session->isActive() && !requestExit())
    {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

bool RefinePanel::handleKey(int key, Qt::KeyboardModifiers modifiers)
{
    // The keyboard does what the controller does (Help: refine.md).
    const float step = (modifiers & Qt::ShiftModifier) ? 5.0f : 1.0f;
    using CS = ControllerScheme::Verb;
    // H: the freeze chord's key; while holding, nothing else.
    if (key == Qt::Key_H)
    {
        m_doc->programmer()->toggleHolding();
        return true;
    }
    if (m_doc->programmer()->isHolding())
        return true;
    switch (key)
    {
        case Qt::Key_Left:     m_session->nudgePosition(-step, 0.0f); break;
        case Qt::Key_Right:    m_session->nudgePosition(step, 0.0f); break;
        case Qt::Key_Up:       m_session->nudgePosition(0.0f, step); break;
        case Qt::Key_Down:     m_session->nudgePosition(0.0f, -step); break;
        case Qt::Key_PageDown: m_session->apply(CS::NextGroup); break;
        case Qt::Key_PageUp:   m_session->apply(CS::PreviousGroup); break;
        case Qt::Key_BracketRight:
        case Qt::Key_Tab:      m_session->apply(CS::NextItem); break;
        case Qt::Key_BracketLeft:
        case Qt::Key_Backtab:  m_session->apply(CS::PreviousItem); break;
        case Qt::Key_G:        m_session->apply(CS::CycleScope); break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Shift: keep at the source cue (View + A on the controller).
            if (modifiers & Qt::ShiftModifier)
                m_session->keepAtSource();
            else
                m_session->apply(CS::Keep);
            break;
        case Qt::Key_Backspace:
        case Qt::Key_Delete:   m_session->apply(CS::Revert); break;
        case Qt::Key_0:        m_session->apply(CS::Reset); break;
        case Qt::Key_Z:
            if (!(modifiers & Qt::ControlModifier))
                return false;
            m_session->apply(CS::Undo);
            break;
        case Qt::Key_Escape:   requestExit(); break;
        case Qt::Key_Period:   m_session->apply(CS::ZoomWider); break;
        case Qt::Key_Comma:    m_session->apply(CS::ZoomNarrower); break;
        case Qt::Key_Semicolon: m_session->apply(CS::FocusNearer); break;
        case Qt::Key_Apostrophe: m_session->apply(CS::FocusFarther); break;
        case Qt::Key_P:        m_session->apply(CS::Alternate); break;
        // Both bumpers: the next change not kept.
        case Qt::Key_U:        m_session->nextPending(); break;
        case Qt::Key_W:
            // Say the light; with Shift, the feedback changes (as RS held).
            if (modifiers & Qt::ShiftModifier)
                m_doc->programmer()->cycleOperatorFeedback();
            else
                m_session->apply(CS::WhereAmI);
            break;
        case Qt::Key_S:
            if (!m_session->isTrackedLayer())
                return false;
            m_session->setKeepToSource(!m_session->keepsToSource());
            break;
        default:
            return false;
    }
    refresh();
    return true;
}

void RefinePanel::keyPressEvent(QKeyEvent *event)
{
    if (handleKey(event->key(), event->modifiers()))
    {
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
