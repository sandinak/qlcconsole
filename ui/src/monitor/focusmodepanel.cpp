/*
  Q Light Controller Plus
  focusmodepanel.cpp

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

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardItemModel>
#include <QCloseEvent>
#include <QButtonGroup>
#include <QToolButton>
#include <QComboBox>
#include <QKeyEvent>
#include <QSettings>
#include <QLabel>

#include "controllertestdialog.h"
#include "controllerkeepalive.h"
#include "focuscontrolmap.h"
#include "focusmodepanel.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "mastertimer.h"
#include "aimsolver.h"
#include "monitor.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

QPointer<FocusModePanel> FocusModePanel::s_instance;

namespace
{
const char kInputSetting[] = "focusmode/input";

QString signedDegrees(float v)
{
    return QString::fromUtf8("%1%2\302\260").arg(v >= 0.0f ? "+" : "").arg(double(v), 0, 'f', 2);
}
}

/*****************************************************************************
 * Setup
 *****************************************************************************/

FocusModeSetupDialog::FocusModeSetupDialog(Doc *doc, quint32 sceneId,
                                           const QList<quint32> &selection, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_sceneId(sceneId)
    , m_selection(FocusSession::focusable(doc, selection))
{
    setWindowTitle(tr("Focus Mode"));
    setObjectName(QStringLiteral("focusModeSetup"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#focus-mode"));

    QVBoxLayout *vl = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Align the rig: each head in turn is aimed at its reference "
                                  "and lit, and you nudge its calibration offsets until the "
                                  "light lands there."), this);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    QFormLayout *form = new QFormLayout;
    vl->addLayout(form);

    QList<quint32> all;
    for (Fixture *fx : m_doc->fixtures())
        all << fx->id();
    const int allCount = FocusSession::focusable(m_doc, all).size();

    m_whichCombo = new QComboBox(this);
    m_whichCombo->setObjectName(QStringLiteral("focusWhich"));
    if (!m_selection.isEmpty())
        m_whichCombo->addItem(tr("The selected heads (%1)").arg(m_selection.size()), 0);
    m_whichCombo->addItem(tr("Every moving head on the plot (%1)").arg(allCount), 1);
    form->addRow(tr("Heads:"), m_whichCombo);

    m_orderCombo = new QComboBox(this);
    m_orderCombo->setObjectName(QStringLiteral("focusOrder"));
    m_orderCombo->addItem(tr("By stage position (front to back, house left to right)"),
                          int(FocusSession::ByStagePosition));
    m_orderCombo->addItem(tr("As selected"), int(FocusSession::AsGiven));
    form->addRow(tr("Order:"), m_orderCombo);

    m_firstCombo = new QComboBox(this);
    m_firstCombo->setObjectName(QStringLiteral("focusReference"));
    fillReferences(m_firstCombo, false);
    form->addRow(tr("Aim at:"), m_firstCombo);

    m_secondCombo = new QComboBox(this);
    m_secondCombo->setObjectName(QStringLiteral("focusSecondReference"));
    fillReferences(m_secondCombo, true);
    m_secondCombo->setToolTip(tr("A second place to check the same offsets against (Y or R)"));
    form->addRow(tr("Then check at:"), m_secondCombo);

    m_othersCombo = new QComboBox(this);
    m_othersCombo->setObjectName(QStringLiteral("focusOthers"));
    m_othersCombo->addItem(tr("Dark"), int(FocusSession::OthersDark));
    m_othersCombo->addItem(tr("Dimmed"), int(FocusSession::OthersDim));
    m_othersCombo->addItem(tr("Left as they are"), int(FocusSession::OthersAsTheyAre));
    form->addRow(tr("The other heads:"), m_othersCombo);

    m_levelSpin = new QDoubleSpinBox(this);
    m_levelSpin->setObjectName(QStringLiteral("focusLevel"));
    m_levelSpin->setRange(5, 100);
    m_levelSpin->setDecimals(0);
    m_levelSpin->setSuffix(QStringLiteral(" %"));
    m_levelSpin->setValue(80);
    form->addRow(tr("Light the head at:"), m_levelSpin);

    /* What drives the nudges. The keys work either way; on the keyboard a
       controller lying on the desk is ignored, so it cannot drift a head. */
    m_inputCombo = new QComboBox(this);
    m_inputCombo->setObjectName(QStringLiteral("focusInput"));
    const ProgrammerController::FocusController pad = m_doc->programmer()->focusController();
    m_inputCombo->addItem(pad.present ? tr("Controller \342\200\224 %1").arg(pad.name)
                                      : tr("Controller (none connected)"),
                          int(FocusSession::ControllerInput));
    m_inputCombo->addItem(tr("Keyboard"), int(FocusSession::KeyboardInput));
    if (!pad.present)
    {
        // Not choosable without one.
        if (QStandardItemModel *m = qobject_cast<QStandardItemModel *>(m_inputCombo->model()))
            m->item(0)->setEnabled(false);
    }
    m_inputCombo->setCurrentIndex(
        m_inputCombo->findData(int(FocusModePanel::defaultInput(m_doc))));
    m_inputCombo->setToolTip(tr("Which device nudges the heads. The keyboard shortcuts work "
                                "with either; on the keyboard the controller is ignored."));
    QHBoxLayout *useRow = new QHBoxLayout;
    useRow->addWidget(m_inputCombo, 1);
    /* What the controller actually sends, and what each input does here:
       press everything once and send the log. */
    QPushButton *testBtn = new QPushButton(tr("Controller test\342\200\246"), this);
    testBtn->setObjectName(QStringLiteral("focusControllerTest"));
    testBtn->setAutoDefault(false);
    testBtn->setToolTip(tr("List every input the controller sends as you press it, with what "
                           "it maps to \342\200\224 to check the mapping, or send it to be fixed"));
    connect(testBtn, &QPushButton::clicked, this, [this]() {
        ControllerTestDialog::open(m_doc, this);
    });
    useRow->addWidget(testBtn);
    form->addRow(tr("Use:"), useRow);

    /* Perform is refused by the session itself; in Design, say what is
       running -- Focus mode overrides the heads it uses, nothing else. */
    const int running = m_doc->masterTimer() != nullptr ? m_doc->masterTimer()->runningFunctions() : 0;
    if (running > 0)
    {
        QLabel *warn = new QLabel(tr("%n function(s) running. Focus mode takes over the heads it "
                                     "focuses; everything else keeps playing.", "", running), this);
        warn->setObjectName(QStringLiteral("focusRunningWarning"));
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color: #d08a1c;"));
        vl->addWidget(warn);
    }

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Start"));
    bb->button(QDialogButtonBox::Ok)->setEnabled(m_firstCombo->count() > 0);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

void FocusModeSetupDialog::fillReferences(QComboBox *combo, bool optional)
{
    if (optional)
        combo->addItem(tr("(nothing)"), QVariantList({ int(FocusSession::Reference::None), 0 }));

    const Scene *scene = qobject_cast<const Scene *>(m_doc->function(m_sceneId));
    if (scene != nullptr && !AimSolver::sceneAims(m_doc, scene).isEmpty())
        combo->addItem(tr("The aim of \"%1\"").arg(scene->name()),
                       QVariantList({ int(FocusSession::Reference::SceneAim), m_sceneId }));

    QList<StageTarget *> targets = m_doc->monitorProperties()->stageTargets();
    std::sort(targets.begin(), targets.end(), [](const StageTarget *a, const StageTarget *b) {
        return a->name().localeAwareCompare(b->name()) < 0;
    });
    for (const StageTarget *t : targets)
        combo->addItem(t->isLine() ? tr("%1 (line)").arg(t->name()) : t->name(),
                       QVariantList({ int(FocusSession::Reference::Target), t->id() }));
}

FocusSession::Reference FocusModeSetupDialog::referenceOf(const QComboBox *combo)
{
    FocusSession::Reference ref;
    const QVariantList v = combo->currentData().toList();
    if (v.size() == 2)
    {
        ref.kind = FocusSession::Reference::Kind(v.at(0).toInt());
        ref.id = v.at(1).toUInt();
    }
    return ref;
}

FocusSession::InputDevice FocusModeSetupDialog::inputDevice() const
{
    return FocusSession::InputDevice(m_inputCombo->currentData().toInt());
}

FocusSession::Options FocusModeSetupDialog::options() const
{
    FocusSession::Options o;
    if (m_whichCombo->currentData().toInt() == 0)
        o.fixtures = m_selection;
    else
        for (Fixture *fx : m_doc->fixtures())
            o.fixtures << fx->id();
    o.order = FocusSession::Order(m_orderCombo->currentData().toInt());
    o.first = referenceOf(m_firstCombo);
    o.second = referenceOf(m_secondCombo);
    o.others = FocusSession::Others(m_othersCombo->currentData().toInt());
    o.level = float(m_levelSpin->value() / 100.0);
    return o;
}

/*****************************************************************************
 * Panel
 *****************************************************************************/

FocusModePanel *FocusModePanel::instance()
{
    return s_instance.data();
}

FocusModePanel *FocusModePanel::open(Doc *doc, QWidget *parent, quint32 sceneId,
                                     const QList<quint32> &selection)
{
    if (doc == nullptr || doc->programmer() == nullptr)
        return nullptr;
    if (s_instance != nullptr && doc->programmer()->focusSession()->isActive())
    {
        s_instance->show();
        s_instance->raise();
        s_instance->activateWindow();
        return s_instance.data();
    }

    FocusModeSetupDialog setup(doc, sceneId, selection, parent);
    if (setup.exec() != QDialog::Accepted)
        return nullptr;

    // The choice made here is this machine's from now on.
    rememberInput(setup.inputDevice());
    QString why;
    FocusModePanel *panel = startWith(doc, parent, setup.options(), &why, int(setup.inputDevice()));
    if (panel == nullptr)
        QMessageBox::warning(parent, tr("Focus Mode"), why);
    return panel;
}

FocusSession::InputDevice FocusModePanel::defaultInput(Doc *doc)
{
    QSettings settings;
    if (settings.value(kInputSetting).toString() == QLatin1String("keyboard"))
        return FocusSession::KeyboardInput;
    if (doc != nullptr && doc->programmer() != nullptr
        && doc->programmer()->focusController().present)
        return FocusSession::ControllerInput;
    return FocusSession::KeyboardInput;
}

void FocusModePanel::rememberInput(FocusSession::InputDevice device)
{
    QSettings settings;
    settings.setValue(kInputSetting, device == FocusSession::KeyboardInput
                                     ? QStringLiteral("keyboard") : QStringLiteral("controller"));
}

FocusModePanel *FocusModePanel::startWith(Doc *doc, QWidget *parent,
                                          const FocusSession::Options &options, QString *why,
                                          int input)
{
    /* The rig as it was, so the whole session can be one step on the
       Lighting Studio's undo once it ends. */
    const QByteArray before = Monitor::instance() != nullptr
        ? Monitor::instance()->rigSnapshot() : QByteArray();

    if (!doc->programmer()->focusSession()->start(options, why))
        return nullptr;

    FocusModePanel *panel = new FocusModePanel(doc, parent);
    panel->m_rigBefore = before;
    s_instance = panel;
    panel->setInputDevice(input < 0 ? defaultInput(doc) : FocusSession::InputDevice(input), false);
    panel->refreshController();
    panel->refresh();
    panel->showHighlight(panel->m_session->currentFixture());
    panel->show();
    panel->raise();
    panel->activateWindow();
    return panel;
}

FocusModePanel::FocusModePanel(Doc *doc, QWidget *parent)
    : QWidget(parent, Qt::Tool)
    , m_doc(doc)
    , m_session(doc->programmer()->focusSession())
{
    setAttribute(Qt::WA_DeleteOnClose);
    /* A tool window on a Mac hides whenever the app is not the frontmost
       one: switch to another app and the panel vanished, though Focus mode
       -- and the controller -- were still on. Keep it on screen. */
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    setObjectName(QStringLiteral("focusModePanel"));
    setWindowTitle(tr("Focus Mode"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#focus-mode"));
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *vl = new QVBoxLayout(this);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("focusTitle"));
    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("focusFixture"));
    QFont big = m_nameLabel->font();
    big.setPointSizeF(big.pointSizeF() * 1.6);
    big.setBold(true);
    m_nameLabel->setFont(big);
    m_referenceLabel = new QLabel(this);
    m_referenceLabel->setObjectName(QStringLiteral("focusReferenceName"));
    m_offsetLabel = new QLabel(this);
    m_offsetLabel->setObjectName(QStringLiteral("focusOffsets"));
    QFont mid = m_offsetLabel->font();
    mid.setPointSizeF(mid.pointSizeF() * 1.3);
    m_offsetLabel->setFont(mid);
    m_savedLabel = new QLabel(this);
    m_savedLabel->setObjectName(QStringLiteral("focusSaved"));
    m_stepLabel = new QLabel(this);
    m_stepLabel->setObjectName(QStringLiteral("focusStep"));
    m_progressLabel = new QLabel(this);
    m_progressLabel->setObjectName(QStringLiteral("focusProgress"));

    vl->addWidget(m_titleLabel);
    vl->addWidget(m_nameLabel);
    vl->addWidget(m_referenceLabel);
    vl->addWidget(m_offsetLabel);
    vl->addWidget(m_savedLabel);
    QHBoxLayout *meta = new QHBoxLayout;
    meta->addWidget(m_stepLabel);
    meta->addStretch();
    meta->addWidget(m_progressLabel);
    vl->addLayout(meta);

    /* What drives the nudges, and the map of it: the controller drawn with
       each control labelled by what it does here, or the keys. */
    QHBoxLayout *use = new QHBoxLayout;
    use->addWidget(new QLabel(tr("Use:"), this));
    m_useController = new QToolButton(this);
    m_useController->setObjectName(QStringLiteral("focusUseController"));
    m_useController->setText(tr("Controller"));
    m_useKeyboard = new QToolButton(this);
    m_useKeyboard->setObjectName(QStringLiteral("focusUseKeyboard"));
    m_useKeyboard->setText(tr("Keyboard"));
    QButtonGroup *useGroup = new QButtonGroup(this);
    for (QToolButton *b : { m_useController, m_useKeyboard })
    {
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setAutoRaise(false);
        useGroup->addButton(b);
        use->addWidget(b);
    }
    styleUseButtons();
    m_useKeyboard->setToolTip(tr("Nudge from the keyboard; the controller is ignored"));
    connect(m_useController, &QToolButton::clicked, this, [this]() {
        setInputDevice(FocusSession::ControllerInput);
    });
    connect(m_useKeyboard, &QToolButton::clicked, this, [this]() {
        setInputDevice(FocusSession::KeyboardInput);
    });
    m_deviceLabel = new QLabel(this);
    m_deviceLabel->setObjectName(QStringLiteral("focusDevice"));
    m_deviceLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    use->addWidget(m_deviceLabel, 1);
    vl->addLayout(use);

    m_pausedLabel = new QLabel(this);
    m_pausedLabel->setObjectName(QStringLiteral("focusPausedNote"));
    m_pausedLabel->setWordWrap(true);
    m_pausedLabel->setStyleSheet(QStringLiteral("color: #d08a1c; font-weight: bold;"));
    m_pausedLabel->hide();
    vl->addWidget(m_pausedLabel);

    QHBoxLayout *note = new QHBoxLayout;
    m_noteLabel = new QLabel(this);
    m_noteLabel->setObjectName(QStringLiteral("focusInputNote"));
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setStyleSheet(QStringLiteral("color: #d08a1c;"));
    m_noteLabel->hide();
    note->addWidget(m_noteLabel, 1);
    m_backButton = new QPushButton(tr("Use the controller"), this);
    m_backButton->setObjectName(QStringLiteral("focusControllerBack"));
    m_backButton->setFocusPolicy(Qt::NoFocus);
    m_backButton->hide();
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        setInputDevice(FocusSession::ControllerInput);
    });
    note->addWidget(m_backButton);
    vl->addLayout(note);

    m_map = new FocusControlMap(this);
    m_map->setFocusPolicy(Qt::NoFocus);
    vl->addWidget(m_map, 1);
    // A click on a control does what it would.
    connect(m_map, &FocusControlMap::controlClicked, this, [this](int role) {
        m_session->apply(FocusSession::controlFor(GamepadRole::Role(role)));
    });
    connect(m_map, &FocusControlMap::keyClicked, this, [this](int key) {
        handleKey(key, key == Qt::Key_Z ? Qt::KeyboardModifiers(Qt::ControlModifier)
                                        : Qt::KeyboardModifiers());
    });

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->addStretch();
    QPushButton *exitBtn = new QPushButton(tr("Exit"), this);
    exitBtn->setObjectName(QStringLiteral("focusExit"));
    exitBtn->setFocusPolicy(Qt::NoFocus);
    connect(exitBtn, &QPushButton::clicked, this, [this]() { requestExit(); });
    bottom->addWidget(exitBtn);
    vl->addLayout(bottom);

    ProgrammerController *pc = m_doc->programmer();
    connect(pc, &ProgrammerController::focusControllerChanged,
            this, &FocusModePanel::refreshController);
    connect(pc->controllerKeepAlive(), &ControllerKeepAlive::pausedChanged,
            this, &FocusModePanel::refreshPaused);
    connect(pc, &ProgrammerController::focusControllerInput, this, [this](int role, float value) {
        if (m_session->inputDevice() == FocusSession::ControllerInput)
            m_map->setControlValue(role, value);
    });

    connect(m_session, &FocusSession::currentChanged, this, [this](quint32 fid) {
        showHighlight(fid);
        refresh();
    });
    connect(m_session, &FocusSession::offsetChanged, this, &FocusModePanel::refresh);
    connect(m_session, &FocusSession::referenceChanged, this, &FocusModePanel::refresh);
    connect(m_session, &FocusSession::exitRequested, this, [this]() { requestExit(); });
    // Ended under it -- into Perform, say: close without asking.
    connect(m_session, &FocusSession::stopped, this, [this]() {
        if (!m_finishing)
        {
            m_finishing = true;
            finish();
            close();
        }
    });
}

FocusModePanel::~FocusModePanel()
{
    if (m_session->isActive())
    {
        m_finishing = true;
        m_session->stop();
        finish();
    }
}

void FocusModePanel::refresh()
{
    if (!m_session->isActive())
        return;
    const quint32 fid = m_session->currentFixture();
    const Fixture *fx = m_doc->fixture(fid);
    m_titleLabel->setText(tr("Focus \342\200\224 head %1 of %2")
                          .arg(m_session->index() + 1).arg(m_session->count()));
    m_nameLabel->setText(fx != nullptr ? tr("%1  (ID %2)").arg(fx->name()).arg(fid) : QString());
    m_referenceLabel->setText(tr("Aimed at: %1%2").arg(m_session->referenceName(),
        m_session->usingSecondReference() ? tr("  (second reference)") : QString()));
    const FocusSession::Offset live = m_session->liveOffset(fid);
    const FocusSession::Offset saved = m_session->savedOffset(fid);
    m_offsetLabel->setText(tr("Pan %1    Tilt %2").arg(signedDegrees(live.pan), signedDegrees(live.tilt)));
    if (m_session->isDirty(fid))
    {
        m_savedLabel->setText(tr("Unsaved \342\200\224 saved is pan %1, tilt %2 (A to save, B to revert)")
                              .arg(signedDegrees(saved.pan), signedDegrees(saved.tilt)));
        m_savedLabel->setStyleSheet(QStringLiteral("color: #d08a1c;"));
    }
    else
    {
        m_savedLabel->setText(m_session->isChecked(fid) ? tr("Saved \342\234\223") : tr("As saved"));
        m_savedLabel->setStyleSheet(QStringLiteral("color: #3a9a4a;"));
    }
    m_stepLabel->setText(tr("Step %1\302\260").arg(double(m_session->stepDegrees()), 0, 'g', 3));
    m_progressLabel->setText(tr("%1 of %2 checked").arg(m_session->checkedCount()).arg(m_session->count()));
    m_map->setStepDegrees(m_session->stepDegrees());
}

void FocusModePanel::setInputDevice(FocusSession::InputDevice device, bool remember)
{
    // No controller to have: the keyboard, but not as anyone's choice.
    if (device == FocusSession::ControllerInput
        && !m_doc->programmer()->focusController().present)
    {
        device = FocusSession::KeyboardInput;
        remember = false;
    }
    m_session->setInputDevice(device);
    if (remember)
    {
        rememberInput(device);
        m_fellBack = false;
    }
    if (device == FocusSession::ControllerInput)
        m_fellBack = false;
    if (!m_fellBack)
    {
        m_noteLabel->hide();
        m_backButton->hide();
    }
    m_useController->setChecked(device == FocusSession::ControllerInput);
    m_useKeyboard->setChecked(device == FocusSession::KeyboardInput);
    refreshPaused();
    m_map->clearInput();
    const FocusControlMap::Mode mode = device == FocusSession::KeyboardInput
                                       ? FocusControlMap::KeyboardMap : FocusControlMap::ControllerMap;
    if (mode != m_map->mode())
    {
        m_map->setMode(mode);
        // The keys take less room than the controller.
        if (isVisible())
            resize(width(), qMax(minimumSizeHint().height(), sizeHint().height()));
    }
}

void FocusModePanel::refreshController()
{
    const ProgrammerController::FocusController pad = m_doc->programmer()->focusController();
    m_map->setControls(pad.controls);
    m_useController->setEnabled(pad.present);
    m_useController->setToolTip(pad.present ? tr("Nudge from %1").arg(pad.name)
                                            : tr("No controller Focus mode can use is connected"));
    m_deviceLabel->setText(pad.present ? pad.name : tr("No controller connected"));

    if (!pad.present && m_session->inputDevice() == FocusSession::ControllerInput)
    {
        // Gone mid-session: the keyboard has it now, and nothing drifts.
        m_fellBack = true;
        setInputDevice(FocusSession::KeyboardInput, false);
        m_noteLabel->setText(tr("Controller paused: the controller disconnected. "
                                "Switched to the keyboard."));
        m_noteLabel->show();
        m_backButton->hide();
    }
    else if (pad.present && m_fellBack && m_session->inputDevice() == FocusSession::KeyboardInput)
    {
        m_noteLabel->setText(tr("The controller is back."));
        m_noteLabel->show();
        m_backButton->show();
    }
    else if (!pad.present && m_fellBack)
    {
        m_noteLabel->setText(tr("Controller paused: the controller disconnected. "
                                "Switched to the keyboard."));
        m_backButton->hide();
    }
    refreshPaused();
}

void FocusModePanel::refreshPaused()
{
    ControllerKeepAlive *keep = m_doc->programmer()->controllerKeepAlive();
    /* On the keyboard the controller is not being used, so it is not
       paused; and a disconnect is already said by the note. */
    const bool show = keep->isPaused() && !m_fellBack
                      && m_session->inputDevice() == FocusSession::ControllerInput;
    if (show)
        m_pausedLabel->setText(tr("Controller paused: %1").arg(keep->pauseReason()));
    m_pausedLabel->setVisible(show);
}

void FocusModePanel::showHighlight(quint32 fid)
{
    if (Monitor *mon = Monitor::instance())
        mon->showFocusFixture(fid);
}

bool FocusModePanel::handleKey(int key, Qt::KeyboardModifiers modifiers)
{
    const bool shift = modifiers & Qt::ShiftModifier;
    const float step = shift ? 1.0f : m_session->stepDegrees();
    switch (key)
    {
        case Qt::Key_Left:  m_session->nudge(-step, 0.0f); break;
        case Qt::Key_Right: m_session->nudge(step, 0.0f); break;
        case Qt::Key_Up:    m_session->nudge(0.0f, step); break;
        case Qt::Key_Down:  m_session->nudge(0.0f, -step); break;
        case Qt::Key_BracketRight:
        case Qt::Key_Tab:      m_session->next(); break;
        case Qt::Key_BracketLeft:
        case Qt::Key_Backtab:  m_session->previous(); break;
        case Qt::Key_Return:
        case Qt::Key_Enter:    m_session->save(); break;
        case Qt::Key_Backspace:
        case Qt::Key_Delete:   m_session->revert(); break;
        case Qt::Key_0:        m_session->zero(); break;
        case Qt::Key_R:        m_session->toggleReference(); break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            m_session->setStepDegrees(m_session->stepDegrees() * 10.0f);
            refresh();
            break;
        case Qt::Key_Minus:
            m_session->setStepDegrees(m_session->stepDegrees() / 10.0f);
            refresh();
            break;
        case Qt::Key_Escape:   requestExit(); break;
        case Qt::Key_Z:
            if (!(modifiers & Qt::ControlModifier))
                return false;
            m_session->undoSave();
            break;
        default:
            return false;
    }
    return true;
}

void FocusModePanel::keyPressEvent(QKeyEvent *event)
{
    if (!m_session->isActive())
    {
        QWidget::keyPressEvent(event);
        return;
    }
    // The map lights the key, and the modifiers held with it.
    m_map->setKeyDown(event->key(), true);
    m_map->setKeyDown(Qt::Key_Shift, event->modifiers() & Qt::ShiftModifier);
    m_map->setKeyDown(Qt::Key_Control, event->modifiers() & Qt::ControlModifier);
    if (!handleKey(event->key(), event->modifiers()))
    {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void FocusModePanel::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat())
    {
        m_map->setKeyDown(event->key(), false);
        m_map->setKeyDown(Qt::Key_Shift, event->modifiers() & Qt::ShiftModifier
                                         && event->key() != Qt::Key_Shift);
        m_map->setKeyDown(Qt::Key_Control, event->modifiers() & Qt::ControlModifier
                                           && event->key() != Qt::Key_Control);
    }
    QWidget::keyReleaseEvent(event);
}

bool FocusModePanel::requestExit()
{
    if (m_finishing)
        return true;
    if (m_session->isActive() && m_session->hasUnsaved())
    {
        QMessageBox box(QMessageBox::Question, tr("Focus Mode"),
                        tr("%n head(s) have offsets you have not saved.", "",
                           m_session->unsavedFixtures().size()),
                        QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel, this);
        box.setObjectName(QStringLiteral("focusExitQuestion"));
        box.setInformativeText(tr("Save them, or put those heads back as they were saved?"));
        box.setDefaultButton(QMessageBox::SaveAll);
        const int answer = box.exec();
        if (answer == QMessageBox::Cancel)
            return false;
        if (answer == QMessageBox::SaveAll)
            m_session->saveAll();
    }
    m_finishing = true;
    m_session->stop();
    finish();
    close();
    return true;
}

void FocusModePanel::finish()
{
    showHighlight(0xFFFFFFFF);
    /* Everything this session saved is one step on the Lighting Studio's
       undo (pushRigUndo() adds nothing if the rig is as it was). */
    if (Monitor *mon = Monitor::instance())
        mon->pushRigUndo(m_rigBefore);
    m_rigBefore.clear();
}

void FocusModePanel::styleUseButtons()
{
    /* A segmented choice, the chosen one in the highlight. Set again on a
       theme change: a style sheet's palette() is read when it is set. */
    const QString css = QStringLiteral(
        "QToolButton { background-color: palette(button); color: palette(button-text);"
        " border: 1px solid palette(mid); border-radius: 4px; padding: 3px 10px; }"
        "QToolButton:checked { background-color: palette(highlight);"
        " color: palette(highlighted-text); border-color: palette(highlight); }"
        "QToolButton:disabled { color: palette(mid); }");
    for (QToolButton *b : { m_useController, m_useKeyboard })
    {
        b->setStyleSheet(QString());
        b->setStyleSheet(css);
    }
}

void FocusModePanel::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange)
        styleUseButtons();
    QWidget::changeEvent(event);
}

void FocusModePanel::closeEvent(QCloseEvent *event)
{
    if (!m_finishing && !requestExit())
    {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}
