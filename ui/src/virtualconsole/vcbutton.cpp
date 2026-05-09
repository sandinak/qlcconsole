/*
  Q Light Controller Plus
  vcbutton.cpp

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#include <QStyleOptionButton>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDialogButtonBox>
#include <QWidgetAction>
#include <QColorDialog>
#include <QImageReader>
#include <QInputDialog>
#include <QFileDialog>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDateTime>
#include <QByteArray>
#include <QSettings>
#include <QPainter>
#include <QString>
#include <QLabel>
#include <QDebug>
#include <QEvent>
#include <QTimer>
#include <QBrush>
#include <QStyle>
#include <QMenu>
#include <QSize>
#include <QPen>

#if defined(WIN32) || defined(Q_OS_WIN)
 #include <QStyleFactory>
#endif

#include "qlcinputsource.h"
#include "qlcchannel.h"
#include "qlcmacros.h"
#include "qlcfile.h"
#include "collection.h"
#include "scene.h"
#include "fixture.h"
#include "fixturegroup.h"

#include "vcbuttonproperties.h"
#include "vcpropertieseditor.h"
#include "virtualconsole.h"
#include "chaseraction.h"
#include "mastertimer.h"
#include "vcsoloframe.h"
#include "vcbutton.h"
#include "function.h"
#include "apputil.h"
#include "chaser.h"
#include "doc.h"

const QSize VCButton::defaultSize(QSize(50, 50));

/*****************************************************************************
 * Initialization
 *****************************************************************************/

VCButton::VCButton(QWidget* parent, Doc* doc) : VCWidget(parent, doc)
    , m_iconPath()
    , m_selectionMode(SelectReplace)
    , m_blackoutFadeOutTime(0)
    , m_startupIntensityEnabled(false)
    , m_startupIntensity(1.0)
    , m_flashOverrides(false)
    , m_flashForceLTP(false)
{
    /* Set the class name "VCButton" as the object name as well */
    setObjectName(VCButton::staticMetaObject.className());

    /* No function is initially attached to the button */
    m_function = Function::invalidId();

    setType(VCWidget::ButtonWidget);
    setCaption(QString());
    setState(Inactive);
    m_action = Action(-1); // avoid use of uninitialized value
    setAction(Toggle);
    setFrameStyle(KVCFrameStyleNone);

    /* Menu actions */
    m_chooseIconAction = new QAction(QIcon(":/image.png"), tr("Choose..."),
                                     this);
    m_chooseIconAction->setShortcut(QKeySequence("SHIFT+C"));

    m_resetIconAction = new QAction(QIcon(":/undo.png"), tr("None"), this);
    m_resetIconAction->setShortcut(QKeySequence("SHIFT+ALT+C"));

    connect(m_chooseIconAction, SIGNAL(triggered(bool)),
            this, SLOT(slotChooseIcon()));
    connect(m_resetIconAction, SIGNAL(triggered(bool)),
            this, SLOT(slotResetIcon()));

    /* Initial size */
    QSettings settings;
    QVariant var = settings.value(SETTINGS_BUTTON_SIZE);
    if (var.isValid() == true)
        resize(var.toSize());
    else
        resize(defaultSize);

    var = settings.value(SETTINGS_BUTTON_STATUSLED);
    if (var.isValid() == true && var.toBool() == true)
        m_ledStyle = true;
    else
        m_ledStyle = false;

    setStyle(AppUtil::saneStyle());

    /* Listen to function removals */
    connect(m_doc, SIGNAL(functionRemoved(quint32)),
            this, SLOT(slotFunctionRemoved(quint32)));
}

VCButton::~VCButton()
{
}

void VCButton::setID(quint32 id)
{
    VCWidget::setID(id);

    if (caption().isEmpty())
        setCaption(tr("Button %1").arg(id));
}

/*****************************************************************************
 * Clipboard
 *****************************************************************************/

VCWidget* VCButton::createCopy(VCWidget* parent) const
{
    Q_ASSERT(parent != NULL);

    VCButton* button = new VCButton(parent, m_doc);
    if (button->copyFrom(this) == false)
    {
        delete button;
        button = NULL;
    }

    return button;
}

bool VCButton::copyFrom(const VCWidget* widget)
{
    const VCButton* button = qobject_cast <const VCButton*> (widget);
    if (button == NULL)
        return false;

    /* Copy button-specific stuff */
    setIconPath(button->iconPath());
    setKeySequence(button->keySequence());
    setFunction(button->function());
    enableStartupIntensity(button->isStartupIntensityEnabled());
    setStartupIntensity(button->startupIntensity());
    setStopAllFadeOutTime(button->stopAllFadeTime());
    setAction(button->action());
    m_state = button->m_state;

    m_flashForceLTP = button->flashForceLTP();
    m_flashOverrides = button->flashOverrides();

    /* Copy common stuff */
    return VCWidget::copyFrom(widget);
}

/*****************************************************************************
 * Properties
 *****************************************************************************/

void VCButton::editProperties()
{
    VCButtonProperties prop(this, m_doc);
    if (prop.exec() == QDialog::Accepted)
        m_doc->setModified();
}

/*****************************************************************************
 * Background color
 *****************************************************************************/

void VCButton::setBackgroundImage(const QString& path)
{
    m_bgPixmap = QPixmap(path);
    m_backgroundImage = path;
    m_doc->setModified();
    update();
}

void VCButton::setBackgroundColor(const QColor& color)
{
    QPalette pal = palette();

    m_hasCustomBackgroundColor = true;
    m_backgroundImage = QString();
    pal.setColor(QPalette::Button, color);
    setPalette(pal);

    m_doc->setModified();
}

void VCButton::resetBackgroundColor()
{
    QColor fg;

    m_hasCustomBackgroundColor = false;
    m_backgroundImage = QString();

    /* Store foreground color */
    if (m_hasCustomForegroundColor == true)
        fg = palette().color(QPalette::ButtonText);

    /* Reset the whole palette to application palette */
    setPalette(QApplication::palette());

    /* Restore foreground color */
    if (fg.isValid() == true)
    {
        QPalette pal = palette();
        pal.setColor(QPalette::ButtonText, fg);
        setPalette(pal);
    }

    m_doc->setModified();
}

QColor VCButton::backgroundColor() const
{
    return palette().color(QPalette::Button);
}

/*****************************************************************************
 * Foreground color
 *****************************************************************************/

void VCButton::setForegroundColor(const QColor& color)
{
    QPalette pal = palette();

    m_hasCustomForegroundColor = true;

    pal.setColor(QPalette::WindowText, color);
    pal.setColor(QPalette::ButtonText, color);
    setPalette(pal);

    m_doc->setModified();
}

void VCButton::resetForegroundColor()
{
    QColor bg;

    m_hasCustomForegroundColor = false;

    /* Store background color */
    if (m_hasCustomBackgroundColor == true)
        bg = palette().color(QPalette::Button);

    /* Reset the whole palette to application palette */
    setPalette(QApplication::palette());

    /* Restore background color */
    if (bg.isValid() == true)
        setBackgroundColor(bg);

    m_doc->setModified();
}

QColor VCButton::foregroundColor() const
{
    return palette().color(QPalette::ButtonText);
}

/*****************************************************************************
 * Button icon
 *****************************************************************************/

QString VCButton::iconPath() const
{
    return m_iconPath;
}

void VCButton::setIconPath(const QString& iconPath)
{
    m_iconPath = iconPath;

    updateIcon();
    m_doc->setModified();
    update();
}

void VCButton::slotChooseIcon()
{
    /* No point coming here if there is no VC */
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc == NULL)
        return;

    QString formats;
    QListIterator <QByteArray> it(QImageReader::supportedImageFormats());
    while (it.hasNext() == true)
        formats += QString("*.%1 ").arg(QString(it.next()).toLower());

    QString path;
    path = QFileDialog::getOpenFileName(this, tr("Select button icon"),
                                        iconPath(), tr("Images (%1)").arg(formats));
    if (path.isEmpty() == false)
    {
        foreach (VCWidget *widget, vc->selectedWidgets())
        {
            VCButton *button = qobject_cast<VCButton*> (widget);
            if (button != NULL)
                button->setIconPath(path);
        }
    }
}

void VCButton::updateIcon()
{
    if (m_action == Blackout)
    {
        m_icon = QIcon(":/blackout.png");
        m_iconSize = QSize(26, 26);
    }
    else if (m_action == StopAll)
    {
        m_icon = QIcon(":/panic.png");
        m_iconSize = QSize(26, 26);
    }
    else if (iconPath().isEmpty() == false)
    {
        m_icon = QIcon(iconPath());
        m_iconSize = QSize(26, 26);
    }
    else
    {
        m_icon = QIcon();
        m_iconSize = QSize(-1, -1);
    }
}

void VCButton::slotResetIcon()
{
    setIconPath(QString());
    update();
}

/*****************************************************************************
 * Function attachment
 *****************************************************************************/

void VCButton::setFunction(quint32 fid)
{
    Function* old = m_doc->function(m_function);
    if (old != NULL)
    {
        /* Get rid of old function connections */
        disconnect(old, SIGNAL(running(quint32)),
                this, SLOT(slotFunctionRunning(quint32)));
        disconnect(old, SIGNAL(stopped(quint32)),
                this, SLOT(slotFunctionStopped(quint32)));
        disconnect(old, SIGNAL(flashing(quint32,bool)),
                this, SLOT(slotFunctionFlashing(quint32,bool)));
    }

    Function* function = m_doc->function(fid);
    if (function != NULL)
    {
        /* Connect to the new function */
        connect(function, SIGNAL(running(quint32)),
                this, SLOT(slotFunctionRunning(quint32)));
        connect(function, SIGNAL(stopped(quint32)),
                this, SLOT(slotFunctionStopped(quint32)));
        connect(function, SIGNAL(flashing(quint32,bool)),
                this, SLOT(slotFunctionFlashing(quint32,bool)));

        m_function = fid;
        setToolTip(function->name());
    }
    else
    {
        /* No function attachment */
        m_function = Function::invalidId();
        setToolTip(QString());
    }
}

quint32 VCButton::function() const
{
    return m_function;
}

void VCButton::adjustFunctionIntensity(Function *f, qreal value)
{
    qreal finalValue = isStartupIntensityEnabled() ? startupIntensity() * value : value;

    VCWidget::adjustFunctionIntensity(f, finalValue);
}

void VCButton::notifyFunctionStarting(quint32 fid, qreal intensity, bool excludeMonitored)
{
    Q_UNUSED(intensity);

    if (mode() == Doc::Design)
        return;

    if (fid == m_function || m_function == Function::invalidId())
        return;

    if (excludeMonitored)
    {
        // stop the controlled Function only if actively started
        // by this Button or if monitoring the startup Function
        if (m_state != Active && m_function != m_doc->startupFunction())
            return;
    }

    if (action() == VCButton::Toggle)
    {
        Function *f = m_doc->function(m_function);
        if (f != NULL)
        {
            f->stop(functionParent());
            resetIntensityOverrideAttribute();
        }
    }
}

void VCButton::slotFunctionRemoved(quint32 fid)
{
    /* Invalidate the button's function if it's the one that was removed */
    if (fid == m_function)
    {
        setFunction(Function::invalidId());
        resetIntensityOverrideAttribute();
    }
}

/*****************************************************************************
 * Button state
 *****************************************************************************/

VCButton::ButtonState VCButton::state() const
{
    return m_state;
}

void VCButton::setState(ButtonState state)
{
    if (state == m_state)
        return;

    m_state = state;

    emit stateChanged(m_state);

    updateFeedback();

    update();
}

void VCButton::updateState()
{
    ButtonState state = Inactive;

    if (m_action == Blackout)
    {
        if (m_doc->inputOutputMap()->blackout())
            state = Active;
    }
    else if (m_action == Toggle)
    {
        Function* function = m_doc->function(m_function);
        if (function != NULL && function->isRunning())
            state = Active;
    }

    if (m_state != state)
        setState(state);
}

/*****************************************************************************
 * Key sequence handler
 *****************************************************************************/

void VCButton::setKeySequence(const QKeySequence& keySequence)
{
    m_keySequence = QKeySequence(keySequence);
}

QKeySequence VCButton::keySequence() const
{
    return m_keySequence;
}

void VCButton::slotKeyPressed(const QKeySequence& keySequence)
{
    if (acceptsInput() == false)
        return;

    if (m_keySequence == keySequence)
        pressFunction();
}

void VCButton::slotKeyReleased(const QKeySequence& keySequence)
{
    if (acceptsInput() == false)
        return;

    if (m_keySequence == keySequence)
        releaseFunction();
}

void VCButton::updateFeedback()
{
    //if (m_state == Monitoring)
    //    return;

    QSharedPointer<QLCInputSource> src = inputSource();
    if (src.isNull() || !src->isValid())
        return;

    // SaveProgrammer / RevertProgrammer LED follows dirty state, not
    // m_state: blink while dirty, off when clean. The blink phase is
    // toggled by the timer in slotProgrammerDirtyBlink().
    if (m_action == SaveProgrammer || m_action == RevertProgrammer)
    {
        const bool dirty = (m_doc != NULL && m_doc->isProgrammerDirty());
        const QLCInputFeedback::FeedbackType type =
            (dirty && m_dirtyBlinkPhase) ? QLCInputFeedback::UpperValue
                                         : QLCInputFeedback::LowerValue;
        sendFeedback(src->feedbackValue(type), src, src->feedbackExtraParams(type));
        return;
    }

    if (m_state == Inactive)
        sendFeedback(src->feedbackValue(QLCInputFeedback::LowerValue), src, src->feedbackExtraParams(QLCInputFeedback::LowerValue));
    else if (m_state == Monitoring)
        sendFeedback(src->feedbackValue(QLCInputFeedback::MonitorValue), src, src->feedbackExtraParams(QLCInputFeedback::MonitorValue));
    else
        sendFeedback(src->feedbackValue(QLCInputFeedback::UpperValue), src, src->feedbackExtraParams(QLCInputFeedback::UpperValue));
}

/*****************************************************************************
 * External input
 *****************************************************************************/

void VCButton::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    /* Don't let input data through in design mode or if disabled */
    if (acceptsInput() == false)
        return;

    if (checkInputSource(universe, (page() << 16) | channel, value, sender()))
    {
        if (m_action == Flash)
        {
            // Keep the button depressed only while the external button is kept down.
            // Raise the button when the external button is raised.
            if (state() == Inactive && value > 0)
                pressFunction();
            else if (state() == Active && value == 0)
                releaseFunction();
        }
        else
        {
            if (value > 0)
            {
                // Only toggle when the external button is pressed.
                pressFunction();
            }
            else
            {
                // Work around the "internal" feedback of some controllers
                // by updating feedback state after button release.
                updateFeedback();
            }
        }
    }
}

/*****************************************************************************
 * Button action
 *****************************************************************************/

void VCButton::setAction(Action action)
{
    // Blackout signal connection
    if (m_action == Blackout && action != Blackout)
        disconnect(m_doc->inputOutputMap(), SIGNAL(blackoutChanged(bool)),
                this, SLOT(slotBlackoutChanged(bool)));
    else if (m_action != Blackout && action == Blackout)
        connect(m_doc->inputOutputMap(), SIGNAL(blackoutChanged(bool)),
                this, SLOT(slotBlackoutChanged(bool)));

    // Programmer selection signal connection
    if (m_action == SelectFixtures && action != SelectFixtures)
        disconnect(m_doc, SIGNAL(programmerSelectionChanged()),
                   this, SLOT(slotProgrammerSelectionChanged()));
    else if (m_action != SelectFixtures && action == SelectFixtures)
        connect(m_doc, SIGNAL(programmerSelectionChanged()),
                this, SLOT(slotProgrammerSelectionChanged()));

    // PadModeSelect → padModeChanged subscription. Functor-style
    // connect (compile-time type-checked) — the old SIGNAL/SLOT
    // string macros silently fail on enum types whose qualifier
    // doesn't normalize identically between signal-side and slot-side.
    if (m_action == PadModeSelect && action != PadModeSelect)
        disconnect(m_doc, &Doc::padModeChanged,
                   this, &VCButton::slotPadModeChanged);
    else if (m_action != PadModeSelect && action == PadModeSelect)
        connect(m_doc, &Doc::padModeChanged,
                this, &VCButton::slotPadModeChanged);

    // FixturePadCell → selection + sub-selection + pad-mode subscriptions.
    // Pad-mode change uses a lambda since the existing slot has a
    // different signature and is also reused for the other two signals.
    if (m_action == FixturePadCell && action != FixturePadCell)
    {
        disconnect(m_doc, &Doc::programmerSelectionChanged,
                   this, &VCButton::slotProgrammerSubSelectionChanged);
        disconnect(m_doc, &Doc::programmerSubSelectionChanged,
                   this, &VCButton::slotProgrammerSubSelectionChanged);
        disconnect(m_doc, &Doc::padModeChanged, this, nullptr);
    }
    else if (m_action != FixturePadCell && action == FixturePadCell)
    {
        connect(m_doc, &Doc::programmerSelectionChanged,
                this, &VCButton::slotProgrammerSubSelectionChanged);
        connect(m_doc, &Doc::programmerSubSelectionChanged,
                this, &VCButton::slotProgrammerSubSelectionChanged);
        connect(m_doc, &Doc::padModeChanged,
                this, [this](Doc::PadMode){ slotProgrammerSubSelectionChanged(); });
    }

    // SaveProgrammer / RevertProgrammer share the same dirty-state
    // subscription + blink machinery — both LEDs follow the dirty
    // flag, both buttons need to flip on/off in sync with it.
    auto wantsDirtyTracking = [](Action a) {
        return a == SaveProgrammer || a == RevertProgrammer;
    };
    const bool wasTracking = wantsDirtyTracking(m_action);
    const bool willTrack   = wantsDirtyTracking(action);

    if (wasTracking && !willTrack)
    {
        disconnect(m_doc, SIGNAL(programmerDirtyChanged(bool)),
                   this, SLOT(slotProgrammerDirtyChanged(bool)));
        if (m_dirtyBlinkTimer != nullptr)
            m_dirtyBlinkTimer->stop();
    }
    else if (!wasTracking && willTrack)
    {
        connect(m_doc, SIGNAL(programmerDirtyChanged(bool)),
                this, SLOT(slotProgrammerDirtyChanged(bool)));
        if (m_dirtyBlinkTimer == nullptr)
        {
            m_dirtyBlinkTimer = new QTimer(this);
            m_dirtyBlinkTimer->setInterval(500);
            connect(m_dirtyBlinkTimer, SIGNAL(timeout()),
                    this, SLOT(slotProgrammerDirtyBlink()));
        }
    }

    // Action update
    m_action = action;
    updateIcon();

    // Update tooltip
    if (m_action == Blackout)
        setToolTip(tr("Toggle Blackout"));
    else if (m_action == StopAll)
        setToolTip(tr("Stop ALL functions!"));
    else if (m_action == SelectFixtures)
        setToolTip(tr("Programmer: %1 selection")
                       .arg(selectionModeToString(m_selectionMode)));
    else if (m_action == SaveProgrammer)
        setToolTip(tr("Save programmer values as a new Scene"));
    else if (m_action == RevertProgrammer)
        setToolTip(tr("Discard programmer edits — restore edited "
                      "scenes to their saved values"));

    if (m_action == SelectFixtures)
        slotProgrammerSelectionChanged();
    if (m_action == SaveProgrammer || m_action == RevertProgrammer)
        slotProgrammerDirtyChanged(m_doc->isProgrammerDirty());
    if (m_action == PadModeSelect)
        slotPadModeChanged(m_doc->padMode());
    if (m_action == FixturePadCell)
        slotProgrammerSubSelectionChanged();
}

VCButton::Action VCButton::action() const
{
    return m_action;
}

QString VCButton::actionToString(VCButton::Action action)
{
    if (action == Flash)
        return QString(KXMLQLCVCButtonActionFlash);
    else if (action == Blackout)
        return QString(KXMLQLCVCButtonActionBlackout);
    else if (action == StopAll)
        return QString(KXMLQLCVCButtonActionStopAll);
    else if (action == SelectFixtures)
        return QString(KXMLQLCVCButtonActionSelect);
    else if (action == SaveProgrammer)
        return QString(KXMLQLCVCButtonActionSaveProgrammer);
    else if (action == RevertProgrammer)
        return QString(KXMLQLCVCButtonActionRevertProgrammer);
    else if (action == PadModeSelect)
        return QString(KXMLQLCVCButtonActionPadModeSelect);
    else if (action == FixturePadCell)
        return QString(KXMLQLCVCButtonActionFixturePadCell);
    else if (action == ChaserStepNext)
        return QString(KXMLQLCVCButtonActionChaserStepNext);
    else if (action == ChaserStepPrev)
        return QString(KXMLQLCVCButtonActionChaserStepPrev);
    else
        return QString(KXMLQLCVCButtonActionToggle);
}

VCButton::Action VCButton::stringToAction(const QString& str)
{
    if (str == KXMLQLCVCButtonActionFlash)
        return Flash;
    else if (str == KXMLQLCVCButtonActionBlackout)
        return Blackout;
    else if (str == KXMLQLCVCButtonActionStopAll)
        return StopAll;
    else if (str == KXMLQLCVCButtonActionSelect)
        return SelectFixtures;
    else if (str == KXMLQLCVCButtonActionSaveProgrammer)
        return SaveProgrammer;
    else if (str == KXMLQLCVCButtonActionRevertProgrammer)
        return RevertProgrammer;
    else if (str == KXMLQLCVCButtonActionPadModeSelect)
        return PadModeSelect;
    else if (str == KXMLQLCVCButtonActionFixturePadCell)
        return FixturePadCell;
    else if (str == KXMLQLCVCButtonActionChaserStepNext)
        return ChaserStepNext;
    else if (str == KXMLQLCVCButtonActionChaserStepPrev)
        return ChaserStepPrev;
    else
        return Toggle;
}

void VCButton::setPadMode(Doc::PadMode mode)
{
    m_padMode = mode;
    if (m_action == PadModeSelect)
        slotPadModeChanged(m_doc != NULL ? m_doc->padMode() : Doc::PadModeOff);
}

void VCButton::setPadCell(int row, int col)
{
    m_padRow = row;
    m_padCol = col;
    if (m_action == FixturePadCell)
        slotProgrammerSubSelectionChanged();
}

quint32 VCButton::resolveFixturePadFixture() const
{
    if (m_doc == NULL || m_padRow < 0 || m_padCol < 0)
        return Function::invalidId();
    quint32 gid = m_doc->activeProgrammerGroup();
    if (gid == Function::invalidId())
        return Function::invalidId();
    FixtureGroup *grp = m_doc->fixtureGroup(gid);
    if (grp == NULL)
        return Function::invalidId();
    const QList<quint32> fixtures = grp->fixtureList();
    const int idx = m_padRow * 8 + m_padCol;
    if (idx < 0 || idx >= fixtures.size())
        return Function::invalidId();
    return fixtures.at(idx);
}

QString VCButton::selectionModeToString(VCButton::SelectionMode mode)
{
    switch (mode)
    {
    case SelectAdd:    return KXMLQLCVCButtonSelectModeAdd;
    case SelectRemove: return KXMLQLCVCButtonSelectModeRemove;
    case SelectToggle: return KXMLQLCVCButtonSelectModeToggle;
    default:           return KXMLQLCVCButtonSelectModeReplace;
    }
}

VCButton::SelectionMode VCButton::stringToSelectionMode(const QString& str)
{
    if (str == KXMLQLCVCButtonSelectModeAdd)    return SelectAdd;
    if (str == KXMLQLCVCButtonSelectModeRemove) return SelectRemove;
    if (str == KXMLQLCVCButtonSelectModeToggle) return SelectToggle;
    return SelectReplace;
}

void VCButton::setSelectionFixtures(const QList<quint32>& fixtureIds)
{
    m_selectionFixtures = fixtureIds;
    if (m_doc != NULL) m_doc->setModified();
    slotProgrammerSelectionChanged();
}

QList<quint32> VCButton::selectionFixtures() const
{
    return m_selectionFixtures;
}

void VCButton::setSelectionGroups(const QList<quint32>& groupIds)
{
    m_selectionGroups = groupIds;
    if (m_doc != NULL) m_doc->setModified();
    slotProgrammerSelectionChanged();
}

QList<quint32> VCButton::selectionGroups() const
{
    return m_selectionGroups;
}

void VCButton::setSelectionMode(VCButton::SelectionMode mode)
{
    if (m_selectionMode == mode)
        return;
    m_selectionMode = mode;
    if (m_doc != NULL) m_doc->setModified();
}

VCButton::SelectionMode VCButton::selectionMode() const
{
    return m_selectionMode;
}

QList<quint32> VCButton::resolveSelectionTargets() const
{
    QList<quint32> ids;
    QSet<quint32> seen;
    for (quint32 fid : m_selectionFixtures)
    {
        if (seen.contains(fid))
            continue;
        ids.append(fid);
        seen.insert(fid);
    }
    Doc *doc = m_doc;
    if (doc != NULL)
    {
        for (quint32 gid : m_selectionGroups)
        {
            FixtureGroup *grp = doc->fixtureGroup(gid);
            if (grp == NULL)
                continue;
            for (quint32 fid : grp->fixtureList())
            {
                if (seen.contains(fid))
                    continue;
                ids.append(fid);
                seen.insert(fid);
            }
        }
    }
    return ids;
}

void VCButton::slotProgrammerSelectionChanged()
{
    if (m_action != SelectFixtures)
        return;
    if (m_doc == NULL)
        return;
    QList<quint32> targets = resolveSelectionTargets();
    bool active = !targets.isEmpty()
                  && m_doc->allInProgrammerSelection(targets);
    ButtonState newState = active ? Active : Inactive;
    if (state() != newState)
    {
        setState(newState);
        // Push the new state to the surface (LED on/off on APC40 etc.).
        updateFeedback();
    }
}

void VCButton::slotProgrammerDirtyChanged(bool dirty)
{
    if (m_action != SaveProgrammer && m_action != RevertProgrammer)
        return;
    if (dirty)
    {
        if (m_dirtyBlinkTimer != nullptr && !m_dirtyBlinkTimer->isActive())
            m_dirtyBlinkTimer->start();
        m_dirtyBlinkPhase = true; // first blink: lit
    }
    else
    {
        if (m_dirtyBlinkTimer != nullptr)
            m_dirtyBlinkTimer->stop();
        m_dirtyBlinkPhase = false;
    }
    updateFeedback();
    update();
}

void VCButton::slotProgrammerDirtyBlink()
{
    if (m_action != SaveProgrammer && m_action != RevertProgrammer)
        return;
    m_dirtyBlinkPhase = !m_dirtyBlinkPhase;
    updateFeedback();
    update();
}

void VCButton::slotPadModeChanged(Doc::PadMode mode)
{
    if (m_action != PadModeSelect)
        return;
    setState(mode == m_padMode ? Active : Inactive);
}

void VCButton::slotProgrammerSubSelectionChanged()
{
    if (m_action != FixturePadCell || m_doc == NULL)
        return;

    // Three visual states for a pad cell in FixtureSelect mode:
    //   Active     — fixture is in the sub-selection (lit, green)
    //   Monitoring — fixture exists at this idx but isn't selected
    //                yet (candidate, orange in VC; same MIDI velocity
    //                as Active by default, can be customized via
    //                input source feedback values for distinct LED
    //                colors on RGB controllers like APC40 mk2)
    //   Inactive   — no fixture at this idx, OR mode != FixtureSelect
    if (m_doc->padMode() != Doc::PadModeFixtureSelect)
    {
        setState(Inactive);
        return;
    }
    const quint32 fid = resolveFixturePadFixture();
    if (fid == Function::invalidId())
    {
        setState(Inactive);
        return;
    }
    setState(m_doc->isInProgrammerSubSelection(fid) ? Active : Monitoring);
}

void VCButton::saveProgrammer()
{
    if (m_doc == NULL)
        return;
    if (!m_doc->isProgrammerDirty())
    {
        // Briefly flash the button to acknowledge the press even though
        // there's nothing to save.
        blink(150);
        return;
    }

    // Two buckets to commit:
    //  - editedSceneIds()   — scenes already mutated in memory; just
    //                          mark Doc modified so the workspace saves.
    //  - programmerValues   — channels with no running-scene owner;
    //                          collect into a brand-new Scene with a
    //                          user-provided name.
    const bool hasEditedScenes = !m_doc->editedSceneIds().isEmpty();
    const bool hasNewValues    = m_doc->hasProgrammerValues();

    if (hasEditedScenes)
        m_doc->setModified();

    // Build dedup-aware row data for the dialog.
    auto categoryLabel = [](Doc::SaveCategory c) -> QString {
        switch (c)
        {
        case Doc::SaveCatPosition:  return tr("Position");
        case Doc::SaveCatColor:     return tr("Color");
        case Doc::SaveCatSpecial:   return tr("Special");
        case Doc::SaveCatIntensity: return tr("Intensity");
        default:                    return tr("Other");
        }
    };

    struct EditedRow {
        quint32 sceneId = Function::invalidId();
        QString sceneName;
        quint32 matchId = Function::invalidId();
        QString matchName;
    };
    QList<EditedRow> editedRows;
    for (quint32 sid : m_doc->editedSceneIds())
    {
        Function *fn = m_doc->function(sid);
        if (fn == NULL)
            continue;
        EditedRow row;
        row.sceneId = sid;
        row.sceneName = fn->name();
        Scene *scene = qobject_cast<Scene*>(fn);
        if (scene != NULL)
        {
            QHash<quint32, QHash<quint32, uchar>> vals;
            for (const SceneValue &sv : scene->values())
                vals[sv.fxi][sv.channel] = sv.value;
            row.matchId = m_doc->findMatchingScene(vals, sid);
            if (row.matchId != Function::invalidId())
            {
                Function *m = m_doc->function(row.matchId);
                if (m != NULL) row.matchName = m->name();
            }
        }
        editedRows.append(row);
    }

    QList<Doc::SaveBucket> buckets;
    if (hasNewValues)
        buckets = m_doc->proposedSaveBuckets();

    struct BucketRow {
        Doc::SaveBucket bucket;
        quint32 matchId = Function::invalidId();
        QString matchName;
    };
    QList<BucketRow> bucketRows;
    for (const Doc::SaveBucket &b : buckets)
    {
        BucketRow row;
        row.bucket = b;
        row.matchId = m_doc->findMatchingScene(b.values);
        if (row.matchId != Function::invalidId())
        {
            Function *m = m_doc->function(row.matchId);
            if (m != NULL) row.matchName = m->name();
        }
        bucketRows.append(row);
    }

    if (editedRows.isEmpty() && bucketRows.isEmpty())
    {
        blink(150);
        return;
    }

    // Build dialog
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Save programmer"));
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QTreeWidget *editedTree = nullptr;
    if (!editedRows.isEmpty())
    {
        layout->addWidget(new QLabel(
            tr("Edited running scenes — your edits already mutated "
               "these. \"Use existing\" reverts the scene to its "
               "pre-edit values and swaps the running collection to "
               "play the matching scene instead:"), &dlg));
        editedTree = new QTreeWidget(&dlg);
        editedTree->setColumnCount(3);
        editedTree->setHeaderLabels(QStringList()
            << tr("Scene") << tr("Match") << tr("Action"));
        editedTree->setRootIsDecorated(false);
        for (const EditedRow &r : editedRows)
        {
            QTreeWidgetItem *it = new QTreeWidgetItem(editedTree);
            it->setText(0, r.sceneName);
            const bool hasMatch = (r.matchId != Function::invalidId());
            it->setText(1, hasMatch ? r.matchName : tr("(no exact match)"));
            QComboBox *combo = new QComboBox(editedTree);
            combo->addItem(tr("Keep edit"));
            if (hasMatch)
            {
                combo->addItem(tr("Use existing — revert + swap in collection"));
                combo->setCurrentIndex(1); // default to "use existing" when matched
            }
            editedTree->setItemWidget(it, 2, combo);
        }
        editedTree->resizeColumnToContents(0);
        editedTree->resizeColumnToContents(1);
        layout->addWidget(editedTree, 1);
    }

    QTreeWidget *bucketTree = nullptr;
    if (!bucketRows.isEmpty())
    {
        layout->addWidget(new QLabel(
            tr("Non-routed values will be saved as one Scene per "
               "(fixture group, category). Edit Name / Path or pick "
               "\"Use existing\" if a matching scene already exists:"),
            &dlg));
        bucketTree = new QTreeWidget(&dlg);
        bucketTree->setColumnCount(6);
        bucketTree->setHeaderLabels(QStringList()
            << tr("Save") << tr("Category") << tr("Group")
            << tr("Name") << tr("Path") << tr("Action"));
        bucketTree->setRootIsDecorated(false);
        for (const BucketRow &r : bucketRows)
        {
            const Doc::SaveBucket &b = r.bucket;
            QTreeWidgetItem *it = new QTreeWidgetItem(bucketTree);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable
                                      | Qt::ItemIsEditable);
            it->setCheckState(0, Qt::Checked);
            it->setText(1, categoryLabel(b.category));
            it->setText(2, b.groupName);
            it->setText(3, b.defaultName);
            it->setText(4, b.defaultPath);
            QComboBox *combo = new QComboBox(bucketTree);
            combo->addItem(tr("Create new"));
            if (r.matchId != Function::invalidId())
            {
                combo->addItem(tr("Use existing: %1").arg(r.matchName));
                combo->setCurrentIndex(1); // default to existing when matched
            }
            bucketTree->setItemWidget(it, 5, combo);
        }
        bucketTree->resizeColumnToContents(0);
        bucketTree->resizeColumnToContents(1);
        bucketTree->resizeColumnToContents(2);
        bucketTree->setColumnWidth(3, 160);
        bucketTree->setColumnWidth(4, 200);
        layout->addWidget(bucketTree, 1);
    }

    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(bb);

    dlg.resize(820, 480);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // ---- Apply choices ----
    const quint32 runningCol = m_doc->singleRunningCollection();

    // Edited scenes: Keep edit (no-op) OR Use existing (revert + swap)
    if (editedTree != nullptr)
    {
        for (int i = 0; i < editedTree->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *it = editedTree->topLevelItem(i);
            QComboBox *combo = qobject_cast<QComboBox*>(
                editedTree->itemWidget(it, 2));
            if (combo == nullptr || combo->currentIndex() != 1)
                continue; // Keep edit
            const EditedRow &r = editedRows.at(i);
            // Revert this scene to its pre-edit snapshot, then swap any
            // running collection's reference to the matching scene.
            m_doc->revertSceneFromSnapshot(r.sceneId);
            for (quint32 cid : m_doc->collectionsContaining(r.sceneId))
            {
                if (cid == runningCol || runningCol == Function::invalidId())
                {
                    m_doc->replaceSceneInCollection(cid, r.sceneId, r.matchId);
                    if (cid == runningCol)
                        break; // stop after the running one
                }
            }
        }
    }

    // Bucket rows: Create new OR Use existing (+ optionally add to running)
    if (bucketTree != nullptr)
    {
        for (int i = 0; i < bucketTree->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *it = bucketTree->topLevelItem(i);
            if (it->checkState(0) != Qt::Checked)
                continue;
            const BucketRow &r = bucketRows.at(i);
            QComboBox *combo = qobject_cast<QComboBox*>(
                bucketTree->itemWidget(it, 5));
            const bool useExisting = (combo != nullptr
                                       && combo->currentIndex() == 1
                                       && r.matchId != Function::invalidId());
            if (useExisting)
            {
                // Don't create a new scene. If a single collection is
                // running and doesn't already reference the match,
                // add it as a child.
                if (runningCol != Function::invalidId())
                {
                    Collection *coll = qobject_cast<Collection*>(
                        m_doc->function(runningCol));
                    if (coll != NULL && !coll->functions().contains(r.matchId))
                    {
                        coll->addFunction(r.matchId);
                        m_doc->setModified();
                    }
                }
            }
            else
            {
                const QString name = it->text(3).trimmed();
                const QString path = it->text(4).trimmed();
                quint32 fid = m_doc->saveBucketAsScene(r.bucket, name, path);
                if (fid == Function::invalidId())
                {
                    QMessageBox::warning(this, tr("Save programmer"),
                        tr("Failed to create scene \"%1\".").arg(name));
                }
            }
        }
    }

    if (!editedRows.isEmpty())
        m_doc->setModified();
    m_doc->clearProgrammerValues();
    blink(250);
}

void VCButton::setStopAllFadeOutTime(int ms)
{
    m_blackoutFadeOutTime = ms;
}

int VCButton::stopAllFadeTime() const
{
    return m_blackoutFadeOutTime;
}

/*****************************************************************************
 * Intensity adjustment
 *****************************************************************************/

void VCButton::enableStartupIntensity(bool enable)
{
    m_startupIntensityEnabled = enable;
}

bool VCButton::isStartupIntensityEnabled() const
{
    return m_startupIntensityEnabled;
}

void VCButton::setStartupIntensity(qreal fraction)
{
    m_startupIntensity = CLAMP(fraction, qreal(0), qreal(1));
}

qreal VCButton::startupIntensity() const
{
    return m_startupIntensity;
}

void VCButton::slotAttributeChanged(int value)
{
#if 0
    ClickAndGoSlider *slider = (ClickAndGoSlider *)sender();
    int idx = slider->property("attrIdx").toInt();

    Function* func = m_doc->function(m_function);
    if (func != NULL)
        func->adjustAttribute((qreal)value / 100, idx);
#else
    Q_UNUSED(value)
#endif
}



/*****************************************************************************
 * Flash Properties
 *****************************************************************************/

bool VCButton::flashOverrides() const
{
    return m_flashOverrides;
}

void VCButton::setFlashOverride(bool shouldOverride)
{
    m_flashOverrides = shouldOverride;
}

bool VCButton::flashForceLTP() const
{
    return m_flashForceLTP;
}

void VCButton::setFlashForceLTP(bool forceLTP)
{
    m_flashForceLTP = forceLTP;
}



/*****************************************************************************
 * Button press / release handlers
 *****************************************************************************/

void VCButton::pressFunction()
{
    /* Don't allow pressing during design mode */
    if (mode() == Doc::Design)
        return;

    Function* f = NULL;
    if (m_action == Toggle)
    {
        f = m_doc->function(m_function);
        if (f == NULL)
            return;

        // if the button is in a SoloFrame and the function is running but was
        // started by a different function (a chaser or collection), turn other
        // functions off and start this one.
        if (state() == Active && !(isChildOfSoloFrame() && f->startedAsChild()))
        {
            f->stop(functionParent());
            resetIntensityOverrideAttribute();
        }
        else
        {
            adjustFunctionIntensity(f, intensity());

            // starting a Chaser is a special case, since it is necessary
            // to use Chaser Actions to properly start the first
            // Chaser step with the right intensity
            if (f->type() == Function::ChaserType || f->type() == Function::SequenceType)
            {
                ChaserAction action;
                action.m_action = ChaserSetStepIndex;
                action.m_stepIndex = 0;
                action.m_masterIntensity = intensity();
                action.m_stepIntensity = 1.0;
                action.m_fadeMode = Chaser::FromFunction;

                Chaser *chaser = qobject_cast<Chaser*>(f);
                chaser->setAction(action);
            }

            f->start(m_doc->masterTimer(), functionParent());
            setState(Active);
            emit functionStarting(m_function);
        }
    }
    else if (m_action == Flash && state() == Inactive)
    {
        f = m_doc->function(m_function);
        if (f != NULL)
        {
            adjustFunctionIntensity(f, intensity());
            f->flash(m_doc->masterTimer(), flashOverrides(), flashForceLTP());
            setState(Active);
        }
    }
    else if (m_action == Blackout)
    {
        m_doc->inputOutputMap()->toggleBlackout();
    }
    else if (m_action == StopAll)
    {
        if (stopAllFadeTime() == 0)
            m_doc->masterTimer()->stopAllFunctions();
        else
            m_doc->masterTimer()->fadeAndStopAll(stopAllFadeTime());
    }
    else if (m_action == SelectFixtures)
    {
        QList<quint32> targets = resolveSelectionTargets();
        switch (m_selectionMode)
        {
        case SelectAdd:
            m_doc->addToProgrammerSelection(targets);
            break;
        case SelectRemove:
            m_doc->removeFromProgrammerSelection(targets);
            break;
        case SelectToggle:
            m_doc->toggleInProgrammerSelection(targets);
            break;
        case SelectReplace:
        default:
            m_doc->setProgrammerSelection(targets);
            break;
        }
    }
    else if (m_action == SaveProgrammer)
    {
        saveProgrammer();
    }
    else if (m_action == RevertProgrammer)
    {
        if (m_doc->isProgrammerDirty())
        {
            m_doc->revertProgrammer();
            blink(250);
        }
        else
        {
            blink(150);
        }
    }
    else if (m_action == PadModeSelect)
    {
        qDebug() << "[VCButton] PadModeSelect press: id=" << id()
                 << "current=" << m_doc->padMode()
                 << "target=" << m_padMode;
        // Toggle: pressing the active mode's button switches pads off;
        // pressing an inactive mode's button enters that mode.
        if (m_doc->padMode() == m_padMode)
            m_doc->setPadMode(Doc::PadModeOff);
        else
            m_doc->setPadMode(m_padMode);
    }
    else if (m_action == FixturePadCell)
    {
        if (m_doc->padMode() != Doc::PadModeFixtureSelect)
        {
            blink(150);    // wrong mode — acknowledge but no-op
            return;
        }
        const quint32 fid = resolveFixturePadFixture();
        if (fid == Function::invalidId())
        {
            blink(150);    // no fixture mapped to this cell
            return;
        }
        m_doc->toggleInProgrammerSubSelection(fid);
        // Visually identify the toggled fixture so the user can see
        // which physical light corresponds to this pad.
        m_doc->flashFixture(fid);
    }
    else if (m_action == ChaserStepNext)
    {
        m_doc->stepCurrentChaser(+1);
        blink(120);
    }
    else if (m_action == ChaserStepPrev)
    {
        m_doc->stepCurrentChaser(-1);
        blink(120);
    }
}

FunctionParent VCButton::functionParent() const
{
    return FunctionParent(FunctionParent::ManualVCWidget, id());
}

void VCButton::releaseFunction()
{
    /* Don't allow operation during design mode */
    if (mode() == Doc::Design)
        return;

    if (m_action == Flash && state() == Active)
    {
        Function* f = m_doc->function(m_function);
        if (f != NULL)
        {
            f->unFlash(m_doc->masterTimer());
            resetIntensityOverrideAttribute();
            setState(Inactive);
        }
    }
}

void VCButton::slotFunctionRunning(quint32 fid)
{
    if (fid == m_function && m_action == Toggle)
    {
        if (state() == Inactive)
            setState(Monitoring);
        emit functionStarting(m_function);
    }
}

void VCButton::slotFunctionStopped(quint32 fid)
{
    if (fid == m_function && m_action == Toggle)
    {
        resetIntensityOverrideAttribute();
        setState(Inactive);
        blink(250);
    }
}

void VCButton::slotFunctionFlashing(quint32 fid, bool state)
{
    // Do not change the state of the button for Blackout or Stop All Functions buttons
    if (m_action != Toggle && m_action != Flash)
        return;

    if (fid != m_function)
        return;

    // if the function was flashed by another button, and the function is still running, keep the button pushed
    Function* f = m_doc->function(m_function);
    if (state == false && m_action == Toggle && f != NULL && f->isRunning())
    {
        return;
    }

    setState(state ? Active : Inactive);
}

void VCButton::blink(int ms)
{
    slotBlink();
    QTimer::singleShot(ms, this, SLOT(slotBlink()));
}

void VCButton::slotBlink()
{
    // This function is called twice with same XOR mask,
    // thus creating a brief opposite-color -- normal-color blink
    QPalette pal = palette();
    QColor color(pal.color(QPalette::Button));
    color.setRgb(color.red()^0xff, color.green()^0xff, color.blue()^0xff);
    pal.setColor(QPalette::Button, color);
    setPalette(pal);
}

void VCButton::slotBlackoutChanged(bool state)
{
    setState(state ? Active : Inactive);
}

bool VCButton::isChildOfSoloFrame() const
{
    QWidget* parent = parentWidget();
    while (parent != NULL)
    {
        if (qobject_cast<VCSoloFrame*>(parent) != NULL)
            return true;
        parent = parent->parentWidget();
    }
    return false;
}

/*****************************************************************************
 * Custom menu
 *****************************************************************************/

QMenu* VCButton::customMenu(QMenu* parentMenu) const
{
    QMenu* menu = new QMenu(parentMenu);
    menu->setTitle(tr("Icon"));
    menu->addAction(m_chooseIconAction);
    menu->addAction(m_resetIconAction);

    return menu;
}

void VCButton::adjustIntensity(qreal val)
{
    if (state() == Active)
    {
        Function* func = m_doc->function(m_function);
        if (func != NULL)
            adjustFunctionIntensity(func, val);
    }

    VCWidget::adjustIntensity(val);
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool VCButton::loadXML(QXmlStreamReader &root)
{
    bool visible = false;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    if (root.name() != KXMLQLCVCButton)
    {
        qWarning() << Q_FUNC_INFO << "Button node not found";
        return false;
    }

    /* Widget commons */
    loadXMLCommon(root);

    /* Icon */
    setIconPath(m_doc->denormalizeComponentPath(root.attributes().value(KXMLQLCVCButtonIcon).toString()));

    /* Children */
    while (root.readNextStartElement())
    {
        //qDebug() << "VC Button tag:" << root.name();
        if (root.name() == KXMLQLCWindowState)
        {
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(x, y, w, h);
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCButtonFunction)
        {
            QString str = root.attributes().value(KXMLQLCVCButtonFunctionID).toString();
            setFunction(str.toUInt());
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCWidgetInput)
        {
            loadXMLInput(root);
        }
        else if (root.name() == KXMLQLCVCButtonAction)
        {
            QXmlStreamAttributes attrs = root.attributes();
            if (attrs.hasAttribute(KXMLQLCVCButtonStopAllFadeTime))
                setStopAllFadeOutTime(attrs.value(KXMLQLCVCButtonStopAllFadeTime).toString().toInt());

            if (attrs.hasAttribute(KXMLQLCVCButtonFlashOverride))
                setFlashOverride(attrs.value(KXMLQLCVCButtonFlashOverride).toInt());

            if (attrs.hasAttribute(KXMLQLCVCButtonFlashForceLTP))
                setFlashForceLTP(attrs.value(KXMLQLCVCButtonFlashForceLTP).toInt());

            if (attrs.hasAttribute(KXMLQLCVCButtonSelectMode))
                setSelectionMode(stringToSelectionMode(
                    attrs.value(KXMLQLCVCButtonSelectMode).toString()));

            if (attrs.hasAttribute(KXMLQLCVCButtonSelectFixtures))
            {
                QList<quint32> fids;
                for (const QString& s : attrs.value(KXMLQLCVCButtonSelectFixtures).toString()
                                             .split(',', Qt::SkipEmptyParts))
                    fids.append(s.toUInt());
                setSelectionFixtures(fids);
            }

            if (attrs.hasAttribute(KXMLQLCVCButtonSelectGroups))
            {
                QList<quint32> gids;
                for (const QString& s : attrs.value(KXMLQLCVCButtonSelectGroups).toString()
                                             .split(',', Qt::SkipEmptyParts))
                    gids.append(s.toUInt());
                setSelectionGroups(gids);
            }

            // Pad-mode + pad-cell coordinates. Persist these so a
            // saved+reloaded workspace doesn't reset PadModeSelect to
            // PadModeOff (no-op on press) or FixturePadCell to (-1,-1)
            // (can't resolve fixture index).
            if (attrs.hasAttribute(KXMLQLCVCButtonAttrPadMode))
            {
                const QString pm = attrs.value(KXMLQLCVCButtonAttrPadMode).toString();
                if (pm.compare(QStringLiteral("FixtureSelect"), Qt::CaseInsensitive) == 0)
                    setPadMode(Doc::PadModeFixtureSelect);
                else if (pm.compare(QStringLiteral("GoboSelect"), Qt::CaseInsensitive) == 0)
                    setPadMode(Doc::PadModeGoboSelect);
                else if (pm.compare(QStringLiteral("ColorPalette"), Qt::CaseInsensitive) == 0)
                    setPadMode(Doc::PadModeColorPalette);
                else
                    setPadMode(Doc::PadModeOff);
            }
            if (attrs.hasAttribute(KXMLQLCVCButtonAttrPadRow)
                && attrs.hasAttribute(KXMLQLCVCButtonAttrPadCol))
            {
                setPadCell(attrs.value(KXMLQLCVCButtonAttrPadRow).toInt(),
                           attrs.value(KXMLQLCVCButtonAttrPadCol).toInt());
            }

            // setAction last so the XML-driven SelectFixtures connection
            // happens after the fixture/group lists are populated.
            setAction(stringToAction(root.readElementText()));
        }
        else if (root.name() == KXMLQLCVCButtonKey)
        {
            setKeySequence(stripKeySequence(QKeySequence(root.readElementText())));
        }
        else if (root.name() == KXMLQLCVCButtonIntensity)
        {
            bool adjust;
            if (root.attributes().value(KXMLQLCVCButtonIntensityAdjust).toString() == KXMLQLCTrue)
                adjust = true;
            else
                adjust = false;
            setStartupIntensity(qreal(root.readElementText().toInt()) / qreal(100));
            enableStartupIntensity(adjust);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown button tag:" << root.name().toString();
            root.skipCurrentElement();
        }
    }

    /* All buttons start raised... */
    setState(Inactive);

    return true;
}

bool VCButton::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    /* VC button entry */
    doc->writeStartElement(KXMLQLCVCButton);

    saveXMLCommon(doc);

    /* Icon */
    doc->writeAttribute(KXMLQLCVCButtonIcon, m_doc->normalizeComponentPath(iconPath()));

    /* Window state */
    saveXMLWindowState(doc);

    /* Appearance */
    saveXMLAppearance(doc);

    /* Function */
    doc->writeStartElement(KXMLQLCVCButtonFunction);
    doc->writeAttribute(KXMLQLCVCButtonFunctionID, QString::number(function()));
    doc->writeEndElement();

    /* Action */
    doc->writeStartElement(KXMLQLCVCButtonAction);

    if (action() == StopAll && stopAllFadeTime() != 0)
    {
        doc->writeAttribute(KXMLQLCVCButtonStopAllFadeTime, QString::number(stopAllFadeTime()));
    }
    else if (action() == Flash)
    {
        doc->writeAttribute(KXMLQLCVCButtonFlashOverride, QString::number(flashOverrides()));
        doc->writeAttribute(KXMLQLCVCButtonFlashForceLTP, QString::number(flashForceLTP()));
    }
    else if (action() == SelectFixtures)
    {
        doc->writeAttribute(KXMLQLCVCButtonSelectMode,
                            selectionModeToString(m_selectionMode));
        if (!m_selectionFixtures.isEmpty())
        {
            QStringList ids;
            for (quint32 fid : m_selectionFixtures)
                ids.append(QString::number(fid));
            doc->writeAttribute(KXMLQLCVCButtonSelectFixtures, ids.join(','));
        }
        if (!m_selectionGroups.isEmpty())
        {
            QStringList ids;
            for (quint32 gid : m_selectionGroups)
                ids.append(QString::number(gid));
            doc->writeAttribute(KXMLQLCVCButtonSelectGroups, ids.join(','));
        }
    }
    else if (action() == PadModeSelect)
    {
        QString pm;
        switch (m_padMode)
        {
        case Doc::PadModeFixtureSelect: pm = QStringLiteral("FixtureSelect"); break;
        case Doc::PadModeGoboSelect:    pm = QStringLiteral("GoboSelect");    break;
        case Doc::PadModeColorPalette:  pm = QStringLiteral("ColorPalette");  break;
        case Doc::PadModeOff:
        default:                        pm = QStringLiteral("Off");           break;
        }
        doc->writeAttribute(KXMLQLCVCButtonAttrPadMode, pm);
    }
    else if (action() == FixturePadCell)
    {
        doc->writeAttribute(KXMLQLCVCButtonAttrPadRow, QString::number(m_padRow));
        doc->writeAttribute(KXMLQLCVCButtonAttrPadCol, QString::number(m_padCol));
    }
    doc->writeCharacters(actionToString(action()));
    doc->writeEndElement();

    /* Key sequence */
    if (m_keySequence.isEmpty() == false)
        doc->writeTextElement(KXMLQLCVCButtonKey, m_keySequence.toString());

    /* Intensity adjustment */
    doc->writeStartElement(KXMLQLCVCButtonIntensity);
    doc->writeAttribute(KXMLQLCVCButtonIntensityAdjust,
                     isStartupIntensityEnabled() ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeCharacters(QString::number(int(startupIntensity() * 100)));
    doc->writeEndElement();

    /* External input */
    saveXMLInput(doc);

    /* End the <Button> tag */
    doc->writeEndElement();

    return true;
}

/*****************************************************************************
 * Event handlers
 *****************************************************************************/

void VCButton::paintEvent(QPaintEvent* e)
{
    QStyleOptionButton option;
    option.initFrom(this);

    /* This should look like a normal button */
    option.features = QStyleOptionButton::None;

    /* Sunken or raised based on state() status */
    if (state() == Inactive)
        option.state = QStyle::State_Raised;
    else
        option.state = QStyle::State_Sunken;

    /* Custom icons are always enabled, to see them in full color also in design mode */
    if (m_action == Toggle || m_action == Flash)
        option.state |= QStyle::State_Enabled;

    /* Icon */
    option.icon = m_icon;
    option.iconSize = m_iconSize;

    /* Paint the button */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    if (m_backgroundImage.isEmpty() == false)
    {
        QRect pxRect = m_bgPixmap.rect();
        // if the pixmap is bigger than the button, then paint a scaled version of it
        // covering the whole button surface
        // if the pixmap is smaller than the button, draw a centered pixmap
        if (pxRect.contains(rect()))
        {
            if (m_ledStyle == true)
                painter.drawPixmap(rect(), m_bgPixmap);
            else
                painter.drawPixmap(3, 3, width() - 6, height() - 6, m_bgPixmap);
        }
        else
        {
            painter.drawPixmap((width() - pxRect.width()) / 2,
                               (height() - pxRect.height()) / 2,
                               m_bgPixmap);
        }
    }

    /* Paint caption with text wrapping */
    if (caption().isEmpty() == false)
    {
        style()->drawItemText(&painter,
                              rect(),
                              Qt::AlignCenter | Qt::TextWordWrap,
                              palette(),
                              (mode() == Doc::Operate),
                              caption());
    }

    /* Flash emblem */
    if (m_action == Flash)
    {
        QIcon icon(":/flash.png");
        painter.drawPixmap(rect().width() - 18, 2,
                           icon.pixmap(QSize(16, 16), QIcon::Normal, QIcon::On));
    }

    /* Dirty highlight on programmer-action buttons: red border + filled
       corner dot for Save (commit), amber for Revert (discard). Pairs
       with the status-bar dirty notice. */
    if (m_doc != NULL && m_doc->isProgrammerDirty()
        && (m_action == SaveProgrammer || m_action == RevertProgrammer))
    {
        const QColor highlight = (m_action == SaveProgrammer)
            ? QColor(230, 0, 0, 255)        // red — commit
            : QColor(245, 165, 0, 255);     // amber — discard
        painter.setPen(QPen(highlight, 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(2, 2, rect().width() - 4, rect().height() - 4, 3, 3);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(highlight));
        painter.drawEllipse(rect().width() - 14, 4, 10, 10);
    }

    if (m_ledStyle == true)
    {
        painter.setPen(QPen(QColor(160, 160, 160, 255), 2));

        if (state() == Active)
        {
            if (m_flashForceLTP || m_flashOverrides)
                painter.setBrush(QBrush(QColor(230, 0, 0, 255)));
            else
                painter.setBrush(QBrush(QColor(0, 230, 0, 255)));
        }
        else if (state() == Monitoring)
            painter.setBrush(QBrush(QColor(255, 170, 0, 255)));
        else
            painter.setBrush(QBrush(QColor(110, 110, 110, 255)));

        int dim = rect().width() / 6;
        if (dim > 14) dim = 14;

        painter.drawEllipse(6, 6, dim, dim);      // Style #1
        //painter.drawRoundedRect(-1, -1, dim, dim, 3, 3);   // Style #2
    }
    else
    {
        // Style #3
        painter.setBrush(Qt::NoBrush);

        if (state() != Inactive)
        {
            int borderWidth = (rect().width() > 80)?3:2;
            painter.setPen(QPen(QColor(20, 20, 20, 255), borderWidth * 2));
            painter.drawRoundedRect(borderWidth, borderWidth,
                                    rect().width() - borderWidth * 2, rect().height() - (borderWidth * 2),
                                    borderWidth + 1,  borderWidth + 1);
            if (state() == Monitoring)
                painter.setPen(QPen(QColor(255, 170, 0, 255), borderWidth));
            else
            {
                if (m_flashForceLTP || m_flashOverrides)
                    painter.setPen(QPen(QColor(230, 0, 0, 255), borderWidth));
                else
                    painter.setPen(QPen(QColor(0, 230, 0, 255), borderWidth));
            }
            painter.drawRoundedRect(borderWidth, borderWidth,
                                    rect().width() - borderWidth * 2, rect().height() - (borderWidth * 2),
                                    borderWidth, borderWidth);
        }
        else
        {
            painter.setPen(QPen(QColor(160, 160, 160, 255), 3));
            painter.drawRoundedRect(1, 1, rect().width() - 2, rect().height() - 2, 3, 3);
        }
    }

    /* Stop painting here */
    painter.end();

    /* Draw a selection frame if appropriate */
    VCWidget::paintEvent(e);
}

void VCButton::mousePressEvent(QMouseEvent* e)
{
    if (mode() == Doc::Design)
        VCWidget::mousePressEvent(e);
    else if (e->button() == Qt::LeftButton)
        pressFunction();
#if 0
    else if (e->button() == Qt::RightButton)
    {
        Function* func = m_doc->function(m_function);
        if (func != NULL)
        {
            QString menuStyle = "QMenu { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #B9D9E8, stop:1 #A4C0CE);"
                            "border: 1px solid black; border-radius: 4px; font:bold; }";
            QMenu *menu = new QMenu();
            menu->setStyleSheet(menuStyle);
            int idx = 0;
            foreach (Attribute attr, func->attributes())
            {
                QString slStyle = "QSlider::groove:horizontal { border: 1px solid #999999; margin: 0; border-radius: 2px;"
                        "height: 15px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #B1B1B1, stop:1 #c4c4c4); }"

                        "QSlider::handle:horizontal {"
                        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f);"
                        "border: 1px solid #5c5c5c; width: 15px; border-radius: 2px; margin: -1px 0; }"

                        "QSlider::sub-page:horizontal { background: #114EA2; border-radius: 2px; }";

                QWidget *entryWidget = new QWidget();
                QHBoxLayout *hbox = new QHBoxLayout(menu);
                hbox->setMargin(3);
                QLabel *label = new QLabel(attr.m_name);
                label->setAlignment(Qt::AlignLeft);
                label->setFixedWidth(100);
                ClickAndGoSlider *slider = new ClickAndGoSlider(menu);
                slider->setOrientation(Qt::Horizontal);
                slider->setSliderStyleSheet(slStyle);
                slider->setFixedSize(QSize(100, 18));
                slider->setMinimum(0);
                slider->setMaximum(100);
                slider->setValue(attr.m_value * 100);
                slider->setProperty("attrIdx", QVariant(idx));
                connect(slider, SIGNAL(valueChanged(int)), this, SLOT(slotAttributeChanged(int)));
                hbox->addWidget(label);
                hbox->addWidget(slider);
                entryWidget->setLayout(hbox);
                QWidgetAction *sliderBoxAction = new QWidgetAction(menu);
                sliderBoxAction->setDefaultWidget(entryWidget);
                menu->addAction(sliderBoxAction);
                idx++;
            }
            menu->exec(QCursor::pos());
        }
    }
#endif
}

void VCButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (mode() == Doc::Design)
        VCWidget::mouseReleaseEvent(e);
    else
        releaseFunction();
}
