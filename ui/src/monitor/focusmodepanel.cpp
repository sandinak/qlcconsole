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
#include <QPainter>
#include <QTimer>
#include <QLabel>

#include "controllertestdialog.h"
#include "controllerkeepalive.h"
#include "focuscontrolmap.h"
#include "venuecorrections.h"
#include "operatorvoice.h"
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
/* The names of @p ids, at most @p max, then how many more. */
QString lightNames(const Doc *doc, const QList<quint32> &ids, int max)
{
    QStringList names;
    for (quint32 fid : ids)
    {
        if (names.size() == max)
        {
            names << FocusModePanel::tr("%1 more").arg(ids.size() - max);
            break;
        }
        const Fixture *fx = doc->fixture(fid);
        names << (fx != nullptr ? fx->name() : FocusModePanel::tr("ID %1").arg(fid));
    }
    return names.join(QStringLiteral(", "));
}

QString lightCount(int n)
{
    return n == 1 ? FocusModePanel::tr("1 light") : FocusModePanel::tr("%1 lights").arg(n);
}
}

/*****************************************************************************
 * FocusStatusLamp
 *****************************************************************************/

FocusStatusLamp::FocusStatusLamp(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("focusStatusLamp"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void FocusStatusLamp::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    update();
}

void FocusStatusLamp::setCount(int count)
{
    if (count == m_count)
        return;
    m_count = count;
    update();
}

QString FocusStatusLamp::symbol() const
{
    return (m_flashing || m_state == AllSaved) ? QString::fromUtf8("\u2713") : QStringLiteral("!");
}

void FocusStatusLamp::flash()
{
    m_flashing = true;
    const int serial = ++m_flashSerial;
    update();
    QTimer::singleShot(kFlashMs, this, [this, serial]() {
        // Only the last flash ends it.
        if (serial != m_flashSerial)
            return;
        m_flashing = false;
        update();
    });
}

QColor FocusStatusLamp::ringColor() const
{
    if (m_flashing)
        return savedColor();
    switch (m_state)
    {
        case ThisUnsaved:   return thisColor();
        case OthersUnsaved:
        case BothUnsaved:   return othersColor();
        case AllSaved:      break;
    }
    return savedColor();
}

QColor FocusStatusLamp::centreColor() const
{
    if (m_flashing)
        return savedColor();
    switch (m_state)
    {
        case ThisUnsaved:
        case BothUnsaved:   return thisColor();
        case OthersUnsaved: return othersColor();
        case AllSaved:      break;
    }
    return savedColor();
}

void FocusStatusLamp::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal d = qMin(width(), height()) - 4.0;
    const QRectF outer((width() - d) / 2.0, (height() - d) / 2.0, d, d);
    const QPointF c = outer.center();

    // A dark bezel, so it reads as a lamp on any theme.
    p.setPen(QPen(QColor(20, 20, 22), 2.0));
    p.setBrush(ringColor());
    p.drawEllipse(outer);
    // Two things at once: a red ring round the yellow centre.
    if (ringColor() != centreColor())
    {
        const qreal inner = d * 0.30;
        p.setPen(QPen(QColor(20, 20, 22), 1.5));
        p.setBrush(centreColor());
        p.drawEllipse(c, inner, inner);
    }
    /* Not by colour alone (gap 10): a tick or a "!" in the middle, and how
       many are not saved on the rim. */
    {
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(int(d * 0.42));
        p.setFont(f);
        p.setPen(m_state == ThisUnsaved && !m_flashing ? QColor(20, 20, 22) : QColor(255, 255, 255));
        p.drawText(outer, Qt::AlignCenter, symbol());
        if (m_count > 0 && !m_flashing)
        {
            const qreal r = d * 0.2;
            const QPointF at(outer.right() - r * 0.7, outer.bottom() - r * 0.7);
            p.setPen(QPen(QColor(20, 20, 22), 1.5));
            p.setBrush(QColor(250, 250, 250));
            p.drawEllipse(at, r, r);
            f.setPixelSize(int(r * 1.3));
            p.setFont(f);
            p.setPen(QColor(20, 20, 22));
            p.drawText(QRectF(at.x() - r, at.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(m_count));
        }
    }
    // The glass: a soft highlight up and to the left.
    QRadialGradient gloss(c - QPointF(d * 0.18, d * 0.2), d * 0.45);
    gloss.setColorAt(0.0, QColor(255, 255, 255, m_flashing ? 170 : 110));
    gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(gloss);
    p.drawEllipse(outer.adjusted(2, 2, -2, -2));
}

/*****************************************************************************
 * FocusModePanel
 *****************************************************************************/

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
    // What was selected but cannot be focused: said at Start, not dropped silently.
    for (quint32 fid : selection)
        if (!m_selection.contains(fid) && doc->fixture(fid) != nullptr && !m_unfocusable.contains(fid))
            m_unfocusable << fid;
    setWindowTitle(tr("Quick Focus"));
    setObjectName(QStringLiteral("focusModeSetup"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#quick-focus"));

    QVBoxLayout *vl = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Align the rig: each head in turn is aimed at its reference "
                                  "and lit, and you nudge its calibration offsets until the "
                                  "light lands there. A fixed light (a par, a fixed spot) is "
                                  "lit and you move where it lands, at head height, to where "
                                  "the real one does: that sets its Pan and Tilt."), this);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    QFormLayout *form = new QFormLayout;
    vl->addLayout(form);

    QList<quint32> all;
    for (Fixture *fx : m_doc->fixtures())
        all << fx->id();
    const int allCount = FocusSession::movingHeads(m_doc, all).size();
    const int fixedCount = FocusSession::fixedLights(m_doc, all).size();

    m_whichCombo = new QComboBox(this);
    m_whichCombo->setObjectName(QStringLiteral("focusWhich"));
    // As selected: what is selected on the plot (or in the programmer).
    m_whichCombo->addItem(tr("As selected (%1)").arg(m_selection.size()), 0);
    if (m_selection.isEmpty())
    {
        if (QStandardItemModel *m = qobject_cast<QStandardItemModel *>(m_whichCombo->model()))
            m->item(0)->setEnabled(false);
        m_whichCombo->setItemData(0, m_unfocusable.isEmpty() ? tr("Select fixtures on the plot first")
                                     : tr("None of the selected lights can be aimed: no pan and tilt, "
                                          "and not a fixed light placed on the plot"), Qt::ToolTipRole);
    }
    m_whichCombo->addItem(tr("Every moving head on the plot (%1)").arg(allCount), 1);
    m_whichCombo->addItem(tr("Every fixed light on the plot (%1)").arg(fixedCount), 2);
    // Nothing selected: the moving heads, or the fixed lights when there are none.
    if (m_selection.isEmpty())
        m_whichCombo->setCurrentIndex(allCount == 0 && fixedCount > 0 ? 2 : 1);
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
    /* No stage target (board Q146): the combo says so rather than standing
       blank, and Add a target makes one in the Lighting Studio. */
    m_addTargetBtn = new QPushButton(tr("Add a target\342\200\246"), this);
    m_addTargetBtn->setObjectName(QStringLiteral("focusAddTarget"));
    m_addTargetBtn->setAutoDefault(false);
    m_addTargetBtn->setToolTip(tr("Place a stage target in the Lighting Studio for the heads to aim at"));
    m_addTargetBtn->hide();
    connect(m_addTargetBtn, &QPushButton::clicked, this, [this]() {
        if (!m_addTarget || !m_addTarget())
            return;
        m_firstCombo->clear();
        fillReferences(m_firstCombo, false);
        m_secondCombo->clear();
        fillReferences(m_secondCombo, true);
        updateStart();
    });
    QHBoxLayout *aimRow = new QHBoxLayout;
    aimRow->addWidget(m_firstCombo, 1);
    aimRow->addWidget(m_addTargetBtn);
    form->addRow(tr("Aim at:"), aimRow);

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
        QLabel *warn = new QLabel(tr("%n function(s) running. Quick focus takes over the heads it "
                                     "focuses; everything else keeps playing.", "", running), this);
        warn->setObjectName(QStringLiteral("focusRunningWarning"));
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color: #d08a1c;"));
        vl->addWidget(warn);
    }

    /* Why Start is greyed (board Q146), said where it is greyed. */
    m_reasonLabel = new QLabel(this);
    m_reasonLabel->setObjectName(QStringLiteral("focusStartReason"));
    m_reasonLabel->setWordWrap(true);
    m_reasonLabel->setStyleSheet(QStringLiteral("color: #d08a1c;"));
    m_reasonLabel->hide();
    vl->addWidget(m_reasonLabel);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_startBtn = bb->button(QDialogButtonBox::Ok);
    m_startBtn->setText(tr("Start"));
    m_startBtn->setObjectName(QStringLiteral("focusStart"));
    updateStart();
    connect(m_whichCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateStart(); });
    connect(bb, &QDialogButtonBox::accepted, this, [this]() {
        if (confirmExclusions())
            accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

QList<quint32> FocusModeSetupDialog::unfocusable() const
{
    // Only As selected takes the selection; the others are lights that can be focused.
    return m_whichCombo->currentData().toInt() == 0 ? m_unfocusable : QList<quint32>();
}

QString FocusModeSetupDialog::exclusionText() const
{
    QStringList names;
    for (quint32 fid : unfocusable())
        if (const Fixture *fx = m_doc->fixture(fid))
            names << fx->name();
    return tr("%n selected light(s) cannot be aimed -- no pan and tilt, and not a fixed light placed on "
              "the plot: %1. Leave them out and focus the rest?", "", int(names.size()))
           .arg(names.join(QStringLiteral(", ")));
}

bool FocusModeSetupDialog::confirmExclusions()
{
    if (unfocusable().isEmpty())
        return true;
    QMessageBox box(QMessageBox::Warning, windowTitle(), exclusionText(), QMessageBox::Cancel, this);
    box.setObjectName(QStringLiteral("focusExcludeBox"));
    QPushButton *exclude = box.addButton(tr("Exclude them"), QMessageBox::AcceptRole);
    exclude->setObjectName(QStringLiteral("focusExclude"));
    box.setDefaultButton(exclude);
    box.exec();
    return box.clickedButton() == exclude;
}

void FocusModeSetupDialog::setAddTarget(const std::function<bool()> &addTarget)
{
    m_addTarget = addTarget;
    updateStart();
}

QString FocusModeSetupDialog::startBlocked() const
{
    const QList<quint32> fx = options().fixtures;
    const int which = m_whichCombo->currentData().toInt();
    if (fx.isEmpty())
    {
        if (which == 1)
            return tr("No moving heads are placed on the plot. Place them in the Lighting Studio, "
                      "or choose Fixed lights.");
        if (which == 2)
            return tr("No fixed lights are placed on the plot. Place them in the Lighting Studio, "
                      "or choose moving heads.");
        return tr("Nothing that can be focused is selected. Select lights on the plot, or choose "
                  "every moving head or fixed light.");
    }
    /* Fixed lights need nothing to aim at (they are moved to where they
       land), so they can start with no scene aim or target in the show. */
    const bool allFixed = FocusSession::fixedLights(m_doc, fx).size()
                          == FocusSession::focusable(m_doc, fx).size();
    if (!allFixed && !referenceOf(m_firstCombo).isValid())
        return tr("No stage target to aim at. Add one with Add \342\226\270 Add Target Position.");
    return QString();
}

void FocusModeSetupDialog::updateStart()
{
    const bool noTarget = !referenceOf(m_firstCombo).isValid();
    m_addTargetBtn->setVisible(noTarget && m_addTarget);
    const QString why = startBlocked();
    m_startBtn->setEnabled(why.isEmpty());
    m_startBtn->setToolTip(why);
    m_reasonLabel->setText(why);
    m_reasonLabel->setVisible(!why.isEmpty());
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
    // Never a blank box: say there is nothing to aim at.
    if (combo->count() == 0)
    {
        combo->addItem(tr("No stage target yet"), QVariantList({ int(FocusSession::Reference::None), 0 }));
        combo->setItemData(0, tr("Add one with Add \342\226\270 Add Target Position in the Lighting Studio"), Qt::ToolTipRole);
    }
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
    const int which = m_whichCombo->currentData().toInt();
    if (which == 0)
        o.fixtures = m_selection;
    else
    {
        QList<quint32> all;
        for (Fixture *fx : m_doc->fixtures())
            all << fx->id();
        o.fixtures = (which == 1) ? FocusSession::movingHeads(m_doc, all)
                                  : FocusSession::fixedLights(m_doc, all);
    }
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
                                     const QList<quint32> &selection,
                                     const std::function<bool()> &addTarget)
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
    setup.setAddTarget(addTarget);
    if (setup.exec() != QDialog::Accepted)
        return nullptr;

    // The choice made here is this machine's from now on.
    rememberInput(setup.inputDevice());
    QString why;
    FocusModePanel *panel = startWith(doc, parent, setup.options(), &why, int(setup.inputDevice()));
    if (panel == nullptr)
        QMessageBox::warning(parent, tr("Quick Focus"), why);
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

FocusModePanel *FocusModePanel::attach(Doc *doc, QWidget *parent)
{
    FocusSession *s = doc->programmer()->focusSession();
    if (!s->isActive())
        return nullptr;
    if (s_instance != nullptr)
    {
        s_instance->refresh();
        s_instance->show();
        s_instance->raise();
        return s_instance.data();
    }
    FocusModePanel *panel = new FocusModePanel(doc, parent);
    s_instance = panel;
    panel->setInputDevice(defaultInput(doc), false);
    panel->refreshController();
    panel->refresh();
    panel->showHighlight(s->currentFixture());
    panel->show();
    panel->raise();
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
    setWindowTitle(m_session->isRunningCalls() ? tr("Venue Focus") : tr("Quick Focus"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#quick-focus"));
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *vl = new QVBoxLayout(this);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("focusTitle"));
    m_callLabel = new QLabel(this);
    m_callLabel->setObjectName(QStringLiteral("focusCall"));
    m_callLabel->setWordWrap(true);
    {
        QFont cf = m_callLabel->font();
        cf.setPointSizeF(cf.pointSizeF() * 1.25);
        cf.setBold(true);
        m_callLabel->setFont(cf);
    }
    m_callLabel->hide();
    m_scopeLabel = new QLabel(this);
    m_scopeLabel->setObjectName(QStringLiteral("focusScope"));
    m_scopeLabel->setWordWrap(true);
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
    m_beamLabel = new QLabel(this);
    m_beamLabel->setObjectName(QStringLiteral("focusBeam"));
    m_beamLabel->hide();
    m_savedLabel = new QLabel(this);
    m_savedLabel->setObjectName(QStringLiteral("focusSaved"));
    /* The save status, to be read from the stage: the lamp and a few words. */
    m_lamp = new FocusStatusLamp(this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("focusSaveStatus"));
    m_statusLabel->setWordWrap(true);
    QFont status = m_statusLabel->font();
    status.setPointSizeF(status.pointSizeF() * 1.3);
    status.setBold(true);
    m_statusLabel->setFont(status);
    m_stepLabel = new QLabel(this);
    m_stepLabel->setObjectName(QStringLiteral("focusStep"));
    m_progressLabel = new QLabel(this);
    m_progressLabel->setObjectName(QStringLiteral("focusProgress"));

    vl->addWidget(m_titleLabel);
    vl->addWidget(m_callLabel);
    vl->addWidget(m_nameLabel);
    QHBoxLayout *lampRow = new QHBoxLayout;
    lampRow->addWidget(m_lamp);
    lampRow->addWidget(m_statusLabel, 1);
    vl->addLayout(lampRow);
    vl->addWidget(m_referenceLabel);
    vl->addWidget(m_offsetLabel);
    // What changed since the save, in degrees (Q126), and a ghost on the plot.
    m_deltaLabel = new QLabel(this);
    m_deltaLabel->setObjectName(QStringLiteral("focusDelta"));
    m_deltaLabel->setStyleSheet(QStringLiteral("color: #d08a1c; font-weight: bold;"));
    vl->addWidget(m_deltaLabel);
    /* What was just confirmed -- "Spot 3 kept for Lincoln HS.", "All
       saved, 6 lights." -- shown as the voice says it. */
    QLabel *said = new QLabel(this);
    said->setObjectName(QStringLiteral("focusConfirmed"));
    said->setStyleSheet(QStringLiteral("font-weight: bold;"));
    connect(m_session, &FocusSession::announce, said, [said](const QString &text, int priority) {
        if (priority >= ControllerScheme::ConfirmPriority)
            said->setText(text);
    });
    vl->addWidget(said);
    vl->addWidget(m_beamLabel);
    vl->addWidget(m_savedLabel);
    vl->addWidget(m_scopeLabel);
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
    // Feedback: lights, voice or both (shared with Refine).
    use->addWidget(new QLabel(tr("Feedback:"), this));
    use->addWidget(new OperatorFeedbackCombo(doc->programmer(), this));
    use->addWidget(new QLabel(tr("Voice:"), this));
    use->addWidget(new VoiceDetailCombo(doc->programmer(), this));
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
    connect(pc, &ProgrammerController::holdingChanged, this, &FocusModePanel::refresh);
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
    connect(m_session, &FocusSession::unsavedChanged, this, &FocusModePanel::refreshSaveStatus);
    connect(m_session, &FocusSession::headSaved, m_lamp, &FocusStatusLamp::flash);
    connect(m_session, &FocusSession::referenceChanged, this, &FocusModePanel::refresh);
    connect(m_session, &FocusSession::callChanged, this, &FocusModePanel::refresh);
    connect(m_session, &FocusSession::scopeChanged, this, &FocusModePanel::refresh);
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
    // The beam, where it can change (Q103).
    {
        double deg = 0.0, lo = 0.0, hi = 0.0;
        if (m_session->beamAngle(fid, deg, lo, hi))
        {
            m_beamLabel->setText((m_session->beamKind(fid) == FocusSession::LensSetByHand
                                  ? tr("Beam %1\302\260 (lens %2\342\200\223%3\302\260, set by hand)  "
                                       "D-pad \342\206\221 spot down \342\206\223 flood out, or , .")
                                  : tr("Beam %1\302\260 (zoom %2\342\200\223%3\302\260, while focusing)  "
                                       "D-pad \342\206\221 spot down \342\206\223 flood out, or , ."))
                                 .arg(deg, 0, 'f', 1).arg(lo, 0, 'f', 0).arg(hi, 0, 'f', 0));
            m_beamLabel->show();
        }
        else
            m_beamLabel->hide();
        double focus = 0.0;
        if (m_session->focusLevel(fid, focus))
        {
            m_beamLabel->setText((m_beamLabel->isHidden() ? QString() : m_beamLabel->text() + QStringLiteral("\n"))
                                 + tr("Focus %1%  D-pad \342\206\222 sharper \342\206\220 softer, or ' ;").arg(qRound(focus * 100.0)));
            m_beamLabel->show();
        }
    }
    refreshSaveStatus();
    m_titleLabel->setText(m_session->isRunningCalls()
                          ? tr("Venue focus \342\200\224 fixture %1 of %2")
                            .arg(m_session->index() + 1).arg(m_session->count())
                          : tr("Quick focus \342\200\224 head %1 of %2")
                            .arg(m_session->index() + 1).arg(m_session->count()));
    // The freeze chord (Q140): say so where it is seen first.
    if (m_doc->programmer()->isHolding())
        m_titleLabel->setText(tr("HOLDING \342\200\224 click both sticks (or H) to go on.  ")
                              + m_titleLabel->text());
    // One layout, in this tool's words (ControllerScheme).
    m_map->setTool(m_session->isRunningCalls() ? ControllerScheme::FocusCallsTool
                                               : ControllerScheme::FocusTool);
    if (m_session->isRunningCalls())
    {
        const FocusSession::Call call = m_session->currentCall();
        m_callLabel->setText(tr("Call %1 of %2: %3%4").arg(m_session->callIndex() + 1)
                             .arg(m_session->callCount()).arg(call.name,
                                  call.note.isEmpty() ? QString()
                                                      : QStringLiteral("\n") + call.note));
        m_callLabel->show();
    }
    else
        m_callLabel->hide();
    {
        VenueCorrections *venues = m_doc->programmer()->venueCorrections();
        const QString where = m_session->savesToVenue()
            ? tr("Saves to venue \342\200\234%1\342\200\235 (not the show file)").arg(venues->activeVenue())
            : tr("Saves to the show's own calibration");
        m_scopeLabel->setText(tr("%1 \342\200\224 %2.  Y switches.  %3")
                              .arg(m_session->scope() == FocusSession::GroupScope
                                   ? tr("The whole %1 (%2)").arg(m_session->isRunningCalls()
                                                                 ? tr("call") : tr("list"))
                                     .arg(m_session->order().size())
                                   : tr("One fixture"),
                                   m_session->scope() == FocusSession::GroupScope
                                   ? tr("nudges, saves and reverts all of them")
                                   : tr("bumpers step fixtures"),
                                   where));
    }
    m_nameLabel->setText(fx != nullptr ? tr("%1  (ID %2)").arg(fx->name()).arg(fid) : QString());
    {
        // Q133: its venue correction was skipped -- another fixture type now.
        VenueCorrections *venues = m_doc->programmer()->venueCorrections();
        if (fx != nullptr && venues->needsFocus(fid))
            m_nameLabel->setText(m_nameLabel->text()
                                 + tr("\n! Needs focus: not the fixture focused in %1 (%2)")
                                   .arg(venues->activeVenue(), venues->correctedAs(fid)));
        // Q140: marked dead from the pad (View + X) or D.
        if (fx != nullptr && venues->isDead(fid))
            m_nameLabel->setText(m_nameLabel->text()
                                 + tr("\n\342\234\225 Marked dead in %1 (View + X or D again to clear)")
                                   .arg(venues->activeVenue()));
    }
    m_referenceLabel->setText(tr("Aimed at: %1%2").arg(m_session->referenceName(),
        m_session->isTargetPage()
            ? tr("  \u2014 target page: the stick moves it (A keeps, B puts it back)")
            : m_session->usingSecondReference() ? tr("  (second reference)") : QString()));
    const FocusSession::Offset live = m_session->liveOffset(fid);
    const FocusSession::Offset saved = m_session->savedOffset(fid);
    m_offsetLabel->setText(tr("Pan %1    Tilt %2").arg(signedDegrees(live.pan), signedDegrees(live.tilt)));
    if (m_session->isFixedLight(fid))
    {
        // A fixed light: where it lands, and the aim that gives it.
        const FixtureRigProps rp = m_doc->monitorProperties()->fixtureRigProps(fid);
        m_offsetLabel->setText(tr("Pan %1\302\260 from downstage    Tilt %2\302\260 down")
                               .arg(double(rp.panZeroDir), 0, 'f', 1).arg(double(rp.aimDown), 0, 'f', 1));
        m_savedLabel->setText(m_session->isDirty(fid) ? tr("Unsaved (A or Return to save, B to revert)")
                              : m_session->isChecked(fid) ? tr("Saved \342\234\223") : tr("As it was"));
        m_savedLabel->setStyleSheet(m_session->isDirty(fid) ? QStringLiteral("color: #d08a1c;")
                                                            : QStringLiteral("color: #3a9a4a;"));
        m_stepLabel->setText(tr("Step %1 m").arg(double(m_session->stepDegrees() * FocusSession::metresPerDegree()),
                                                 0, 'g', 3));
        m_progressLabel->setText(tr("%1 of %2 checked").arg(m_session->checkedCount()).arg(m_session->count()));
        m_map->setStepDegrees(m_session->stepDegrees());
        return;
    }
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

QString FocusModePanel::saveStatusText() const
{
    const quint32 cur = m_session->currentFixture();
    QList<quint32> others = m_session->unsavedFixtures();
    const bool here = others.removeAll(cur) > 0;
    if (others.isEmpty())
        return here ? tr("Unsaved: this light") : tr("All saved");
    const QString names = lightNames(m_doc, others, 3);
    if (here)
        return others.size() == 1 ? tr("Unsaved: this light and 1 other (%1)").arg(names)
                                  : tr("Unsaved: this light and %1 others (%2)")
                                    .arg(others.size()).arg(names);
    return others.size() == 1 ? tr("Unsaved: 1 other light (%1)").arg(names)
                              : tr("Unsaved: %1 other lights (%2)").arg(others.size()).arg(names);
}

void FocusModePanel::refreshDelta()
{
    if (!m_session->isActive())
        return;
    const quint32 fid = m_session->currentFixture();
    const QString delta = m_session->deltaText(fid);
    m_deltaLabel->setText(delta.isEmpty() ? QString() : tr("Since the save: %1").arg(delta));
    m_deltaLabel->setVisible(!delta.isEmpty());
    QVector3D ghost;
    Monitor *mon = Monitor::instance();
    if (mon == nullptr)
        return;
    if (m_session->savedGhost(fid, ghost))
        mon->showFocusGhost(fid, ghost, delta);
    else
        mon->showFocusGhost(fid, QVector3D(), QString(), false);
}

QString FocusModePanel::deltaLine() const
{
    return m_deltaLabel->isHidden() ? QString() : m_deltaLabel->text();
}

void FocusModePanel::refreshSaveStatus()
{
    if (!m_session->isActive())
        return;
    const quint32 cur = m_session->currentFixture();
    const QList<quint32> unsaved = m_session->unsavedFixtures();
    const bool here = unsaved.contains(cur);
    const bool elsewhere = unsaved.size() > (here ? 1 : 0);
    const FocusStatusLamp::State state = elsewhere ? (here ? FocusStatusLamp::BothUnsaved
                                                           : FocusStatusLamp::OthersUnsaved)
                                       : here ? FocusStatusLamp::ThisUnsaved
                                              : FocusStatusLamp::AllSaved;
    m_lamp->setState(state);
    m_lamp->setCount(unsaved.size());
    m_statusLabel->setText(saveStatusText());
    // The words in the lamp's colour -- the darker yellow reads on a light theme.
    m_statusLabel->setStyleSheet(elsewhere ? QStringLiteral("color: #c05a00;")
                                 : here ? QStringLiteral("color: #b08a10;")
                                        : QStringLiteral("color: #0072b2;"));
    if (Monitor *mon = Monitor::instance())
        mon->showFocusUnsaved(unsaved);
    refreshDelta();
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
                                            : tr("No controller that can focus is connected"));
    const QString owner = m_doc->programmer()->controllerOwnerName();
    m_deviceLabel->setText(!owner.isEmpty() ? tr("%1 has it (any other is ignored)").arg(owner)
                           : pad.present ? pad.name : tr("No controller connected"));

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
        m_noteLabel->setText(tr("Controller connected: %1 is back.").arg(pad.name));
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
    // H: the freeze chord's key (Q140); while holding, nothing else.
    if (key == Qt::Key_H)
    {
        m_doc->programmer()->toggleHolding();
        return true;
    }
    if (m_doc->programmer()->isHolding())
        return true;
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
        // Shift: as A or B held -- save all; every unsaved light back (Q139).
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (modifiers & Qt::ShiftModifier)
                m_session->saveAllHeld();
            else
                m_session->save();
            break;
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            if (modifiers & Qt::ShiftModifier)
                m_session->revertAllHeld();
            else
                m_session->revert();
            break;
        // Both bumpers: the next light not saved.
        case Qt::Key_U:        m_session->nextUnsaved(); break;
        // View + X on the controller: this light dead (or not) in the venue.
        case Qt::Key_D:        m_session->toggleDead(); break;
        // View + RS on the controller: how many are not saved.
        case Qt::Key_N:
            emit m_doc->programmer()->operatorAnnounce(m_session->unsavedCountPhrase(),
                                                       ControllerScheme::ConfirmPriority);
            break;
        case Qt::Key_0:        m_session->zero(); break;
        // R: the second reference; in focus calls, the target page (Q128).
        case Qt::Key_R:
        case Qt::Key_T:        m_session->apply(FocusSession::ToggleReference); break;
        // Focus calls and the scope (the triggers and Y on the controller).
        case Qt::Key_PageDown: m_session->nextCall(); break;
        case Qt::Key_PageUp:   m_session->previousCall(); break;
        case Qt::Key_G:        m_session->cycleScope(); break;
        case Qt::Key_W:
            // Say the light; with Shift, the feedback changes (as RS held).
            if (modifiers & Qt::ShiftModifier)
                m_doc->programmer()->cycleOperatorFeedback();
            else
                m_session->whereAmI();
            break;
        // The beam (Q103): narrower, wider; ⇧ for 5°.
        case Qt::Key_Comma:
        case Qt::Key_Less:     m_session->nudgeBeam(shift ? -5.0f : -1.0f); refresh(); break;
        case Qt::Key_Period:
        case Qt::Key_Greater:  m_session->nudgeBeam(shift ? 5.0f : 1.0f); refresh(); break;
        // Focus, where it has a focus channel: ; and '.
        case Qt::Key_Semicolon:
        case Qt::Key_Colon:    m_session->nudgeFocus(shift ? -0.1f : -0.02f); refresh(); break;
        case Qt::Key_Apostrophe:
        case Qt::Key_QuoteDbl: m_session->nudgeFocus(shift ? 0.1f : 0.02f); refresh(); break;
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
        const QList<quint32> unsaved = m_session->unsavedFixtures();
        const QString names = lightNames(m_doc, unsaved, 8);
        QMessageBox box(QMessageBox::Question, windowTitle(),
                        unsaved.size() == 1
                            ? tr("%1 has changes you have not saved.").arg(names)
                            : tr("%1 have changes you have not saved: %2.")
                              .arg(lightCount(unsaved.size()), names),
                        QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel, this);
        box.setObjectName(QStringLiteral("focusExitQuestion"));
        box.setInformativeText(tr("Save All keeps them; Discard puts them back as they were saved."));
        box.setDefaultButton(QMessageBox::SaveAll);
        const int answer = box.exec();
        if (answer == QMessageBox::Cancel)
            return false;
        if (answer == QMessageBox::SaveAll)
            m_session->saveAll();
    }
    m_finishing = true;
    m_session->finish();
    finish();
    close();
    return true;
}

void FocusModePanel::finish()
{
    showHighlight(0xFFFFFFFF);
    if (Monitor *mon = Monitor::instance())
        mon->showFocusGhost(0xFFFFFFFF, QVector3D(), QString(), false);
    if (Monitor *mon = Monitor::instance())
        mon->showFocusUnsaved(QList<quint32>());
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
