/*
  Q Light Controller Plus
  focusmenu.cpp

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

#include <QAbstractButton>
#include <QActionGroup>
#include <QInputDialog>
#include <QMessageBox>
#include <QAction>
#include <QMenu>

#include "programmercontroller.h"
#include "venuecorrections.h"
#include "focuscallsdialog.h"
#include "venuerigdialog.h"
#include "focussession.h"
#include "focuscalls.h"
#include "focusmenu.h"
#include "fixture.h"
#include "doc.h"

namespace
{
QList<quint32> allFixtures(Doc *doc)
{
    QList<quint32> all;
    for (Fixture *fx : doc->fixtures())
        all << fx->id();
    return all;
}

QList<quint32> callFixtures(Doc *doc)
{
    QList<quint32> fids;
    for (const FocusCall &c : doc->programmer()->focusCalls()->calls())
        for (quint32 fid : FocusCalls::fixturesOf(doc, c))
            if (!fids.contains(fid))
                fids << fid;
    return fids;
}
}

QString FocusMenu::quickFocusBlocked(Doc *doc)
{
    if (FocusSession::focusable(doc, allFixtures(doc)).isEmpty())
        return tr("No lights are placed on the plot. Place them in the Lighting Studio first.");
    return QString();
}

QString FocusMenu::venueFocusBlocked(Doc *doc)
{
    if (doc->programmer()->focusCalls()->count() == 0)
        return tr("No focus calls yet. Add them with Focus \342\226\270 Edit focus calls\342\200\246");
    if (FocusSession::focusable(doc, callFixtures(doc)).isEmpty())
        return tr("None of the focus calls has a light placed on the plot. Place them in the "
                  "Lighting Studio, or change the calls.");
    return QString();
}

QString FocusMenu::practiceBlocked(Doc *doc)
{
    if (doc->programmer()->focusCalls()->count() == 0)
        return tr("No focus calls yet to practise on. Add them with Focus \342\226\270 Edit focus calls\342\200\246");
    if (FocusSession::movingHeads(doc, callFixtures(doc)).isEmpty())
        return tr("No moving head in the focus calls is placed on the plot: a scramble moves "
                  "moving heads.");
    if (doc->programmer()->venueCorrections()->filePath().isEmpty())
        return tr("Save the show first: a practice is kept as a venue in a file beside it.");
    if (doc->programmer()->venueCorrections()->isPracticeActive())
        return tr("A practice is applied already: finish it first.");
    return QString();
}

QString FocusMenu::reportBlocked(Doc *doc)
{
    if (doc->programmer()->venueCorrections()->venueNames().isEmpty())
        return tr("No venue yet: a focus report is a venue's. Make one with Venues \342\226\270 New venue\342\200\246");
    return QString();
}

QString FocusMenu::venuesBlocked(Doc *doc)
{
    if (doc->programmer()->venueCorrections()->filePath().isEmpty())
        return tr("Save the show first: a venue's corrections are kept in a file beside it.");
    return QString();
}

QString FocusMenu::venueRigBlocked(Doc *doc)
{
    if (!doc->programmer()->venueCorrections()->isActive())
        return tr("No venue is applied. Apply one, or make one with New venue\342\200\246");
    return QString();
}

QString FocusMenu::focusCueBlocked(Doc *doc)
{
    if (doc->fixtures().isEmpty())
        return tr("No lights in the show yet to focus. Add fixtures first.");
    return QString();
}

QString FocusMenu::venueFocusCueBlocked(Doc *doc)
{
    if (doc->programmer()->focusCalls()->count() == 0)
        return tr("No focus calls yet for this cue to run. Add them in the Lighting Studio with "
                  "Focus \342\226\270 Edit focus calls\342\200\246");
    return QString();
}

void FocusMenu::setAvailable(QAction *action, const QString &blocked, const QString &tip)
{
    action->setEnabled(blocked.isEmpty());
    action->setToolTip(blocked.isEmpty() ? tip : blocked);
}

void FocusMenu::setAvailable(QAbstractButton *button, const QString &blocked, const QString &tip)
{
    button->setEnabled(blocked.isEmpty());
    button->setToolTip(blocked.isEmpty() ? tip : blocked);
}

void FocusMenu::fill(QMenu *menu, Doc *doc, QWidget *parent, const std::function<void()> &quickFocus)
{
    menu->clear();
    // clear() leaves the submenus (children, not actions): last time's Venues.
    for (QMenu *sub : menu->findChildren<QMenu *>(Qt::FindDirectChildrenOnly))
        delete sub;
    menu->setToolTipsVisible(true);
    VenueCorrections *venues = doc->programmer()->venueCorrections();

    QAction *quick = menu->addAction(tr("Quick focus\342\200\246"), parent, [quickFocus]() {
        if (quickFocus)
            quickFocus();
    });
    quick->setObjectName(QStringLiteral("focusQuick"));
    setAvailable(quick, quickFocusBlocked(doc),
                 tr("Aim each light at its reference in turn and nudge its calibration until it lands "
                    "there; saves into the show (controller: View + Menu)"));

    QAction *venue = menu->addAction(tr("Venue focus\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::startVenueFocus(doc, parent);
    });
    venue->setObjectName(QStringLiteral("focusVenue"));
    setAvailable(venue, venueFocusBlocked(doc),
                 venues->isActive() ? tr("Run the focus calls for %1, group by group to each point; "
                                         "corrections are saved for this venue, not the show")
                                      .arg(venues->activeVenue())
                                    : tr("Run the focus calls: name the venue first, then group by group "
                                         "to each point; corrections are saved for the venue, not the show"));

    QAction *edit = menu->addAction(tr("Edit focus calls\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::run(doc, parent);
    });
    edit->setObjectName(QStringLiteral("focusEditCalls"));
    edit->setToolTip(tr("The show's focus calls, in the order they are run: groups, the point they "
                        "focus to, a note for the crew; and the venue"));

    menu->addSeparator();
    QMenu *venuesMenu = menu->addMenu(tr("Venues"));
    venuesMenu->setObjectName(QStringLiteral("focusVenuesMenu"));
    venuesMenu->menuAction()->setObjectName(QStringLiteral("focusVenues"));
    venuesMenu->setToolTipsVisible(true);
    {
        const QString blocked = venuesBlocked(doc);
        QMenu *apply = venuesMenu->addMenu(tr("Apply"));
        apply->setObjectName(QStringLiteral("focusVenueApplyMenu"));
        apply->menuAction()->setObjectName(QStringLiteral("focusVenueApply"));
        apply->setToolTipsVisible(true);
        QActionGroup *group = new QActionGroup(apply);
        QStringList names = venues->venueNames();
        names.prepend(QString());
        for (const QString &name : names)
        {
            QAction *a = apply->addAction(name.isEmpty() ? tr("None (the show's own calibration)") : name);
            a->setCheckable(true);
            a->setChecked(name == venues->activeVenue());
            group->addAction(a);
            QObject::connect(a, &QAction::triggered, parent, [doc, parent, name]() {
                VenueCorrections *vc = doc->programmer()->venueCorrections();
                if (doc->programmer()->focusSession()->isActive())
                {
                    QMessageBox::information(parent, tr("Venues"),
                                             tr("Leave Quick focus or Venue focus first: its heads are "
                                                "being worked on."));
                    return;
                }
                if (!VenueRigDialog::confirmClashes(doc, parent, name))
                    return;
                vc->setActiveVenue(name);
            });
        }
        if (names.size() == 1)
            setAvailable(apply->menuAction(), tr("No venue yet. Make one with New venue\342\200\246"), QString());
        else
            setAvailable(apply->menuAction(), blocked, tr("Put a venue's corrections on the rig now"));

        QAction *add = venuesMenu->addAction(tr("New venue\342\200\246"), parent, [doc, parent]() {
            VenueCorrections *vc = doc->programmer()->venueCorrections();
            bool ok = false;
            const QString name = QInputDialog::getText(parent, tr("New venue"),
                tr("The room's name (its corrections start from %1):")
                    .arg(vc->lastVenue().isEmpty() ? tr("the show's own") : vc->lastVenue()),
                QLineEdit::Normal, VenueCorrections::defaultVenueName(), &ok).trimmed();
            if (!ok || name.isEmpty())
                return;
            if (vc->hasVenue(name))
            {
                QMessageBox::information(parent, tr("New venue"),
                                         tr("\342\200\234%1\342\200\235 is already a venue: apply it "
                                            "with Venues \342\226\270 Apply.").arg(name));
                return;
            }
            const QString last = vc->lastVenue();
            const QString made = last.isEmpty() ? vc->addVenue(name) : vc->addVenueFrom(name, last);
            if (!made.isEmpty() && !doc->programmer()->focusSession()->isActive())
                vc->setActiveVenue(made);
        });
        add->setObjectName(QStringLiteral("focusVenueNew"));
        setAvailable(add, blocked, tr("Make a venue for the room you are in, and apply it"));

        QAction *rig = venuesMenu->addAction(tr("Rig and universes\342\200\246"), parent, [doc, parent]() {
            VenueRigDialog::open(doc, parent, doc->programmer()->venueCorrections()->activeVenue());
        });
        rig->setObjectName(QStringLiteral("focusVenueRig"));
        setAvailable(rig, venueRigBlocked(doc),
                     tr("This venue's house rig, its outputs for each universe, the channels it keeps, "
                        "and the clash check"));
    }

    menu->addSeparator();
    QAction *practice = menu->addAction(tr("Practice (scramble)\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::practice(doc, parent);
    });
    practice->setObjectName(QStringLiteral("focusPractice"));
    setAvailable(practice, practiceBlocked(doc),
                 tr("Knocks the moving heads in the focus calls off their targets by a random amount; "
                    "you focus them back, scored on degrees off, time and saves. Your real venues are "
                    "never touched."));

    QAction *report = menu->addAction(tr("Focus report\342\200\246"), parent, [doc, parent]() {
        FocusCallsDialog::exportAppliedReport(doc, parent);
    });
    report->setObjectName(QStringLiteral("focusReport"));
    setAvailable(report, reportBlocked(doc),
                 tr("A venue's focus, light by light: a PDF or web page to print, or a CSV for Lightwright"));
}
