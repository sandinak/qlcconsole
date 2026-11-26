/*
  Q Light Controller Plus
  editlivechip.cpp

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

#include <QToolButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QMenu>

#include "editlivechip.h"
#include "showsettingsdialog.h"
#include "showtreecontroller.h"
#include "surfacepages.h"
#include "showoperate.h"
#include "controlmap.h"
#include "editlive.h"
#include "doc.h"

namespace
{
std::function<bool(QWidget *, const QString &)> s_pinAsker;

const char *kOffStyle = "QToolButton { font-weight: bold; padding: 0 6px; border: none; }";
const char *kOnStyle = "QToolButton { color: #ffffff; background: #c62828; font-weight: bold; "
                       "padding: 0 6px; border: none; border-radius: 3px; }";
const char *kLeftStyle = "QToolButton { color: #000000; background: #f5a623; font-weight: bold; "
                         "padding: 0 6px; border: none; border-radius: 3px; }";
}

EditLiveChip::EditLiveChip(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("footerEditLive"));
    setProperty("helpTopic", QStringLiteral("edit-live.md"));
    QHBoxLayout *box = new QHBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(4);
    m_chip = new QToolButton(this);
    m_chip->setObjectName(QStringLiteral("editLiveChip"));
    m_chip->setAutoRaise(true);
    m_chip->setCursor(Qt::PointingHandCursor);
    connect(m_chip, &QToolButton::clicked, this, &EditLiveChip::clicked);
    box->addWidget(m_chip);
    m_storeLeft = new QToolButton(this);
    m_storeLeft->setObjectName(QStringLiteral("editLiveStoreLeft"));
    m_storeLeft->setAutoRaise(true);
    m_storeLeft->setStyleSheet(QLatin1String(kLeftStyle));
    m_storeLeft->hide();
    connect(m_storeLeft, &QToolButton::clicked, this, [this]() {
        EditLive *el = live();
        QString why;
        if (el != nullptr && el->storeLeftBehind(&why))
            emit said(el->lastNote());
        else
            emit said(why);
        refresh();
    });
    box->addWidget(m_storeLeft);
    m_leftTimer = new QTimer(this);
    m_leftTimer->setSingleShot(true);
    m_leftTimer->setInterval(EditLive::leftBehindMs());
    connect(m_leftTimer, &QTimer::timeout, this, &EditLiveChip::refresh);

    if (EditLive *el = live())
    {
        connect(el, &EditLive::changed, this, &EditLiveChip::refresh);
        connect(el, &EditLive::onChanged, this, &EditLiveChip::refresh);
        connect(el, &EditLive::message, this, &EditLiveChip::said);
        connect(el, &EditLive::leftBehind, this, [this](quint32, int) {
            m_leftTimer->start();
            refresh();
        });
    }
    if (ControlMap *map = m_doc->controlMap())
        connect(map->surfacePages(), &SurfacePages::setsChanged, this, &EditLiveChip::refresh);
    connect(m_doc, &Doc::modeChanged, this, &EditLiveChip::refresh);
    refresh();
}

EditLive *EditLiveChip::live() const
{
    return m_doc->controlMap() != nullptr ? m_doc->controlMap()->editLive() : nullptr;
}

void EditLiveChip::setPinAsker(const std::function<bool(QWidget *, const QString &)> &asker)
{
    s_pinAsker = asker;
}

bool EditLiveChip::turnOn(Doc *doc, QWidget *parent, QString *why)
{
    EditLive *el = doc->controlMap() != nullptr ? doc->controlMap()->editLive() : nullptr;
    if (el == nullptr)
        return false;
    if (el->isOn())
        return true;
    // Anyone at this computer, but the show's PIN when it has one (Q339 a).
    ShowOperate *lock = doc->operateSettings();
    if (lock != nullptr && lock->hasPin())
    {
        const QString text = tr("Enter the show's PIN to turn Edit Live on.");
        const bool ok = s_pinAsker ? s_pinAsker(parent, text)
                                   : ShowSettingsDialog::askForPin(parent, lock->pinHash(), text);
        if (!ok)
        {
            if (why != nullptr)
                *why = tr("Edit Live stays off: the PIN was not given.");
            return false;
        }
    }
    return el->setOn(true, why);
}

void EditLiveChip::setChipHeight(int h)
{
    m_chip->setFixedHeight(h);
    m_storeLeft->setFixedHeight(h);
}

void EditLiveChip::refresh()
{
    EditLive *el = live();
    SurfacePages *pages = m_doc->controlMap() != nullptr ? m_doc->controlMap()->surfacePages() : nullptr;
    const bool operate = m_doc->mode() == Doc::Operate;
    const bool on = el != nullptr && el->isOn();
    const bool boards = pages != nullptr && !pages->universes().isEmpty();
    setVisible(on || (operate && boards));
    if (on)
    {
        const int n = el->trims().size();
        m_chip->setText(n == 0 ? tr("EDIT LIVE") : n == 1 ? tr("EDIT LIVE · 1 Trim")
                                                          : tr("EDIT LIVE · %1 Trims").arg(n));
        m_chip->setStyleSheet(QLatin1String(kOnStyle));
        m_chip->setToolTip(tr("Edit Live is on: the board's page controls trim the running cue. "
                              "Click to see the trims, Store or Drop them, or turn it off."));
    }
    else
    {
        m_chip->setText(tr("Edit Live"));
        m_chip->setStyleSheet(QLatin1String(kOffStyle));
        m_chip->setToolTip(tr("Turn on Edit Live: the board's page controls trim the running cue, "
                              "and Store keeps the trims in its looks."));
    }
    const bool left = el != nullptr && el->hasLeftBehind() && m_leftTimer->isActive();
    if (left)
    {
        const QString cue = el->cueText(el->leftBehindCue());
        m_storeLeft->setText(tr("Store to Cue %1").arg(cue));
        m_storeLeft->setToolTip(tr("GO dropped the trims made on cue %1 (%2 settings). Click to store them "
                                   "into the looks that set them in that cue.")
                                    .arg(cue).arg(el->leftBehindCount()));
    }
    m_storeLeft->setVisible(left);
}

QMenu *EditLiveChip::buildMenu()
{
    EditLive *el = live();
    QMenu *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("editLiveMenu"));
    if (el == nullptr)
        return menu;
    const QList<EditLive::Trim> trims = el->trims();
    const QString cue = el->cueText(el->cue());
    if (trims.isEmpty())
        menu->addAction(cue.isEmpty() ? tr("No Cue Running") : tr("No Trims Yet on Cue %1").arg(cue))
            ->setEnabled(false);
    for (const EditLive::Trim &t : trims)
        menu->addAction(el->trimText(t))->setEnabled(false);
    menu->addSeparator();
    QAction *store = menu->addAction(tr("Store to Cue %1").arg(cue));
    store->setObjectName(QStringLiteral("editLiveStore"));
    store->setEnabled(!trims.isEmpty());
    connect(store, &QAction::triggered, this, [this, el]() {
        QString why;
        emit said(el->store(&why) ? el->lastNote() : why);
    });
    QAction *drop = menu->addAction(tr("Drop"));
    drop->setObjectName(QStringLiteral("editLiveDrop"));
    drop->setEnabled(!trims.isEmpty());
    connect(drop, &QAction::triggered, el, &EditLive::drop);
    QAction *off = menu->addAction(tr("Turn Edit Live Off"));
    off->setObjectName(QStringLiteral("editLiveOff"));
    connect(off, &QAction::triggered, el, [el]() { el->setOn(false); });
    return menu;
}

void EditLiveChip::clicked()
{
    EditLive *el = live();
    if (el == nullptr)
        return;
    if (!el->isOn())
    {
        QString why;
        if (!turnOn(m_doc, window(), &why) && !why.isEmpty())
            emit said(why);
        refresh();
        return;
    }
    QMenu *menu = buildMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(m_chip->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu->sizeHint().height()));
}
