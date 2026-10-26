/*
  Q Light Controller Plus
  venuemenu.cpp

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

#include <QActionGroup>
#include <QInputDialog>
#include <QMessageBox>
#include <QListWidget>
#include <QLocale>
#include <QCursor>
#include <QAction>
#include <QTimer>
#include <QMenu>

#include "programmercontroller.h"
#include "venuecorrections.h"
#include "focuscallsdialog.h"
#include "venuerigdialog.h"
#include "focussession.h"
#include "focusmenu.h"
#include "venuemenu.h"
#include "doc.h"

namespace
{
VenueCorrections *venuesOf(Doc *doc)
{
    return doc->programmer()->venueCorrections();
}

/** A venue's name asked for, not modal: @p done gets it (trimmed, not
 *  empty) when it is accepted. */
QInputDialog *askName(QWidget *parent, const QString &objectName, const QString &title,
                      const QString &label, const QString &text,
                      const std::function<void(const QString &)> &done)
{
    QInputDialog *d = new QInputDialog(parent);
    d->setObjectName(objectName);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setProperty("helpTopic", QStringLiteral("venues.md"));
    d->setWindowTitle(title);
    d->setLabelText(label);
    d->setTextValue(text);
    QObject::connect(d, &QInputDialog::textValueSelected, d, [done](const QString &value) {
        const QString name = value.trimmed();
        if (!name.isEmpty())
        {
            done(name);
        }
    });
    d->open();
    return d;
}
}

QString VenueMenu::chipText(Doc *doc, int clashes)
{
    VenueCorrections *vc = venuesOf(doc);
    if (!vc->isActive())
    {
        return tr("Venue: none");
    }
    QString text = vc->needsConfirm() ? tr("Venue: %1?").arg(vc->activeVenue())
                                      : tr("Venue: %1").arg(vc->activeVenue());
    if (clashes < 0)
    {
        clashes = clashCount(doc);
    }
    if (clashes > 0)
    {
        text += clashes == 1 ? tr(" \302\267 1 clash") : tr(" \302\267 %1 clashes").arg(clashes);
    }
    return text;
}

QString VenueMenu::chipToolTip(Doc *doc, int clashes)
{
    VenueCorrections *vc = venuesOf(doc);
    QStringList lines;
    if (!vc->isActive())
    {
        lines << tr("No venue applied: the show's own calibration, rig and patch.");
        if (!vc->venueNames().isEmpty())
        {
            lines << tr("Click to switch to one of this show's venues.");
        }
        else
        {
            lines << tr("Click to make one for the room you are in.");
        }
        return lines.join('\n');
    }
    const QString name = vc->activeVenue();
    lines << tr("%1 is applied: its focus corrections, house rig and patch over the show's own.").arg(name);
    if (vc->needsConfirm())
    {
        const QDateTime when = vc->lastChanged(name);
        lines << tr("Last changed %1: are you still there? Click, then Still at %2?")
                     .arg(when.isValid() ? QLocale().toString(when, QLocale::ShortFormat) : tr("some time ago"),
                          name);
    }
    else if (vc->isConfirmed())
    {
        lines << tr("Confirmed: you are at %1.").arg(name);
    }
    if (clashes < 0)
    {
        clashes = clashCount(doc);
    }
    if (clashes > 0)
    {
        lines << (clashes == 1 ? tr("1 clash with the show's own rig: Clashes lists it.")
                               : tr("%1 clashes with the show's own rig: Clashes lists them.").arg(clashes));
    }
    lines << tr("Click to switch, rename or check it.");
    return lines.join('\n');
}

int VenueMenu::clashCount(Doc *doc)
{
    VenueCorrections *vc = venuesOf(doc);
    if (!vc->isActive())
    {
        return 0;
    }
    return vc->clashes(vc->activeVenue()).size();
}

QString VenueMenu::renameBlocked(Doc *doc)
{
    if (!venuesOf(doc)->isActive())
    {
        return tr("No venue is applied. Switch to the one to rename first.");
    }
    return QString();
}

bool VenueMenu::switchTo(Doc *doc, QWidget *parent, const QString &name)
{
    VenueCorrections *vc = venuesOf(doc);
    if (name == vc->activeVenue())
    {
        return true;
    }
    if (doc->programmer()->focusSession()->isActive())
    {
        QMessageBox::information(parent, tr("Venues"),
                                 tr("Leave Quick focus or Venue focus first: its heads are being worked on."));
        return false;
    }
    if (!VenueRigDialog::confirmClashes(doc, parent, name))
    {
        return false;
    }
    return vc->setActiveVenue(name);
}

QInputDialog *VenueMenu::askNewVenue(Doc *doc, QWidget *parent)
{
    VenueCorrections *vc = venuesOf(doc);
    return askName(parent, QStringLiteral("venueNewName"), tr("New venue"),
                   tr("The room's name (its corrections start from %1):")
                       .arg(vc->lastVenue().isEmpty() ? tr("the show's own") : vc->lastVenue()),
                   VenueCorrections::defaultVenueName(), [doc, parent](const QString &name) {
        VenueCorrections *v = venuesOf(doc);
        if (v->hasVenue(name))
        {
            QMessageBox::information(parent, tr("New venue"),
                                     tr("\342\200\234%1\342\200\235 is already a venue: switch to it "
                                        "with Switch venue.").arg(name));
            return;
        }
        const QString last = v->lastVenue();
        const QString made = last.isEmpty() ? v->addVenue(name) : v->addVenueFrom(name, last);
        if (!made.isEmpty() && !doc->programmer()->focusSession()->isActive())
        {
            v->setActiveVenue(made);
        }
    });
}

QInputDialog *VenueMenu::askRename(Doc *doc, QWidget *parent)
{
    VenueCorrections *vc = venuesOf(doc);
    const QString from = vc->activeVenue();
    if (from.isEmpty())
    {
        return nullptr;
    }
    return askName(parent, QStringLiteral("venueRenameName"), tr("Rename venue"),
                   tr("A new name for %1 (its corrections, house rig and patch go with it):").arg(from),
                   from, [doc, parent, from](const QString &name) {
        if (name == from)
        {
            return;
        }
        if (!venuesOf(doc)->renameVenue(from, name))
        {
            QMessageBox::information(parent, tr("Rename venue"),
                                     tr("\342\200\234%1\342\200\235 is already a venue.").arg(name));
        }
    });
}

void VenueMenu::fill(QMenu *menu, Doc *doc, QWidget *parent)
{
    menu->clear();
    // clear() leaves the submenus (children, not actions): last time's Switch.
    for (QMenu *sub : menu->findChildren<QMenu *>(Qt::FindDirectChildrenOnly))
    {
        delete sub;
    }
    menu->setToolTipsVisible(true);
    menu->setProperty("helpTopic", QStringLiteral("venues.md"));
    VenueCorrections *vc = venuesOf(doc);
    const QString active = vc->activeVenue();
    const QString blocked = FocusMenu::venuesBlocked(doc);

    // The question asked on opening, until it is answered.
    if (vc->needsConfirm())
    {
        QAction *still = menu->addAction(tr("Still at %1?\342\200\246").arg(active), parent, [doc, parent]() {
            FocusCallsDialog::confirmVenue(doc, parent);
        });
        still->setObjectName(QStringLiteral("venueStillAt"));
        still->setToolTip(tr("Its corrections were last changed a while ago: say whether this is still "
                             "the room, or start a new venue"));
        menu->addSeparator();
    }

    QMenu *switchMenu = menu->addMenu(tr("Switch venue"));
    switchMenu->setObjectName(QStringLiteral("venueSwitchMenu"));
    switchMenu->menuAction()->setObjectName(QStringLiteral("venueSwitch"));
    switchMenu->setToolTipsVisible(true);
    {
        QActionGroup *group = new QActionGroup(switchMenu);
        QStringList names = vc->venueNames();
        names.prepend(QString());
        for (const QString &name : names)
        {
            QAction *a = switchMenu->addAction(name.isEmpty() ? tr("None (the show's own calibration)") : name);
            a->setCheckable(true);
            a->setChecked(name == active);
            group->addAction(a);
            QObject::connect(a, &QAction::triggered, parent, [doc, parent, name]() {
                switchTo(doc, parent, name);
            });
        }
        if (names.size() == 1)
        {
            FocusMenu::setAvailable(switchMenu->menuAction(),
                                    tr("No venue yet. Make one with New venue\342\200\246"), QString());
        }
        else
        {
            FocusMenu::setAvailable(switchMenu->menuAction(), blocked,
                                    tr("Put a venue's corrections, house rig and patch on the rig now"));
        }
    }

    QAction *add = menu->addAction(tr("New venue\342\200\246"), parent, [doc, parent]() {
        askNewVenue(doc, parent);
    });
    add->setObjectName(QStringLiteral("venueNew"));
    FocusMenu::setAvailable(add, blocked, tr("Make a venue for the room you are in, and apply it"));

    QAction *rename = menu->addAction(tr("Rename\342\200\246"), parent, [doc, parent]() {
        askRename(doc, parent);
    });
    rename->setObjectName(QStringLiteral("venueRename"));
    FocusMenu::setAvailable(rename, renameBlocked(doc),
                            active.isEmpty() ? QString() : tr("Give %1 another name").arg(active));

    QAction *rig = menu->addAction(tr("Rig and universes\342\200\246"), parent, [doc, parent]() {
        VenueRigDialog::open(doc, parent, venuesOf(doc)->activeVenue());
    });
    rig->setObjectName(QStringLiteral("venueRig"));
    FocusMenu::setAvailable(rig, FocusMenu::venueRigBlocked(doc),
                            tr("This venue's house rig, its outputs for each universe, the channels it keeps, "
                               "and the clash check"));

    menu->addSeparator();
    QAction *focus = menu->addAction(tr("Venue focus\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::startVenueFocus(doc, parent);
    });
    focus->setObjectName(QStringLiteral("venueFocus"));
    FocusMenu::setAvailable(focus, FocusMenu::venueFocusBlocked(doc),
                            active.isEmpty() ? tr("Run the focus calls: name the venue first, then group by "
                                                  "group to each point")
                                             : tr("Run the focus calls for %1, group by group to each point")
                                                   .arg(active));

    QAction *report = menu->addAction(tr("Focus report\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::exportAppliedReport(doc, parent);
    });
    report->setObjectName(QStringLiteral("venueReport"));
    FocusMenu::setAvailable(report, FocusMenu::reportBlocked(doc),
                            tr("A venue's focus, light by light: a PDF or web page to print, or a CSV for "
                               "Lightwright"));

    // Only when there are any: the venue page, its clash list in front.
    const int clashes = clashCount(doc);
    if (clashes > 0)
    {
        menu->addSeparator();
        QAction *list = menu->addAction(tr("Clashes (%1)\342\200\246").arg(clashes), parent, [doc, parent]() {
            if (VenueRigDialog *d = VenueRigDialog::open(doc, parent, venuesOf(doc)->activeVenue()))
            {
                d->clashList()->setFocus();
                if (d->clashList()->count() > 0)
                {
                    d->clashList()->setCurrentRow(0);
                }
            }
        });
        list->setObjectName(QStringLiteral("venueClashes"));
        list->setToolTip(tr("Where %1's house rig and the show's own rig clash: the same addresses, "
                            "channels the room keeps, or the wrong universe").arg(active));
    }
}

/*****************************************************************************
 * The chip
 *****************************************************************************/

VenueChip::VenueChip(Doc *doc, QWidget *parent)
    : QToolButton(parent)
    , m_doc(doc)
    , m_soon(new QTimer(this))
{
    setObjectName(QStringLiteral("footerVenue"));
    setProperty("helpTopic", QStringLiteral("venues.md"));
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    m_soon->setSingleShot(true);
    m_soon->setInterval(250);
    connect(m_soon, &QTimer::timeout, this, &VenueChip::refresh);
    connect(this, &QToolButton::clicked, this, [this]() { openMenu(); });

    VenueCorrections *vc = venuesOf(doc);
    // The venue itself: at once.
    connect(vc, &VenueCorrections::activeVenueChanged, this, &VenueChip::refresh);
    connect(vc, &VenueCorrections::venuesChanged, this, &VenueChip::refresh);
    connect(vc, &VenueCorrections::confirmedChanged, this, &VenueChip::refresh);
    connect(vc, &VenueCorrections::houseRigChanged, this, &VenueChip::refresh);
    // What the clashes come from: once, after a burst (a patch of 40).
    connect(vc, &VenueCorrections::correctionsChanged, this, &VenueChip::refreshSoon);
    connect(doc, &Doc::fixtureAdded, this, &VenueChip::refreshSoon);
    connect(doc, &Doc::fixtureRemoved, this, &VenueChip::refreshSoon);
    connect(doc, &Doc::fixtureChanged, this, &VenueChip::refreshSoon);
    connect(doc, &Doc::fixtureGroupChanged, this, &VenueChip::refreshSoon);
    refresh();
}

void VenueChip::setAlwaysShown(bool on)
{
    m_alwaysShown = on;
    refresh();
}

bool VenueChip::wanted() const
{
    VenueCorrections *vc = venuesOf(m_doc);
    return m_alwaysShown || vc->isActive() || !vc->venueNames().isEmpty();
}

void VenueChip::refreshSoon()
{
    m_soon->start();
}

void VenueChip::refresh()
{
    m_soon->stop();
    m_clashes = VenueMenu::clashCount(m_doc);
    setText(VenueMenu::chipText(m_doc, m_clashes));
    setToolTip(VenueMenu::chipToolTip(m_doc, m_clashes));
    setStyleSheet(m_clashes > 0
        ? QStringLiteral("QToolButton { color:#000000; background:#f5a623; padding:1px 6px; "
                         "border-radius:3px; border:none; }")
        : QStringLiteral("QToolButton { padding:1px 4px; }"));
    setVisible(wanted());
}

QMenu *VenueChip::openMenu()
{
    if (m_menu == nullptr)
    {
        m_menu = new QMenu(this);
        m_menu->setObjectName(QStringLiteral("venueMenu"));
    }
    QWidget *over = window();
    VenueMenu::fill(m_menu, m_doc, over != nullptr ? over : this);
    const QPoint at = isVisible() ? mapToGlobal(QPoint(0, 0)) - QPoint(0, m_menu->sizeHint().height())
                                  : QCursor::pos();
    m_menu->popup(at);
    return m_menu;
}
