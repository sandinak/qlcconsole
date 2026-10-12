/*
  Q Light Controller Plus
  gobowheelsdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QIcon>
#include <QSet>

#include "gobowheelsdialog.h"
#include "beamoptics.h"
#include "gobolibrary.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "doc.h"

#define COL_SLOT  0
#define COL_GLASS 1
#define ROLE_CHANNEL (Qt::UserRole)
#define ROLE_MIN     (Qt::UserRole + 1)

namespace
{
    bool isPicture(const QLCCapability *cap)
    {
        return cap != nullptr && cap->presetType() == QLCCapability::Picture;
    }

    QString pictureOf(const QLCCapability *cap)
    {
        return isPicture(cap) ? const_cast<QLCCapability *>(cap)->resource(0).toString() : QString();
    }
}

GoboWheelsDialog::GoboWheelsDialog(Doc *doc, quint32 fixtureId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fid(fixtureId)
{
    Fixture *fx = m_doc->fixture(m_fid);
    setWindowTitle(tr("Gobo Wheels — %1").arg(fx != nullptr ? fx->name() : QString()));
    resize(560, 460);

    pickImage = [](QWidget *parent) {
        return QFileDialog::getOpenFileName(parent, tr("Fit a gobo"),
            GoboLibrary::userDirectory().absolutePath(),
            tr("Gobo pictures (%1)").arg(GoboLibrary::imageFilters().join(' ')));
    };

    if (fx != nullptr)
        m_swaps = m_doc->monitorProperties()->goboSwaps(m_fid);

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("The glass in each slot of this unit. Replace a slot with "
                                  "your own picture — black blocks the light, clear or white "
                                  "lets it through. The fixture definition is not changed."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels(QStringList() << tr("Slot") << tr("Glass"));
    m_tree->setIconSize(QSize(32, 32));
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setSectionResizeMode(COL_SLOT, QHeaderView::Stretch);
    root->addWidget(m_tree, 1);

    QHBoxLayout *row = new QHBoxLayout();
    m_replace = new QPushButton(tr("Replace…"), this);
    m_restore = new QPushButton(tr("Restore"), this);
    QPushButton *folder = new QPushButton(tr("Open Gobo Folder"), this);
    row->addWidget(m_replace);
    row->addWidget(m_restore);
    row->addStretch(1);
    row->addWidget(folder);
    root->addLayout(row);

    int sameType = 0;
    if (fx != nullptr)
    {
        foreach (Fixture *other, m_doc->fixtures())
            if (other->fixtureDef() == fx->fixtureDef() && other->fixtureMode() == fx->fixtureMode())
                ++sameType;
    }
    m_allOfType = new QCheckBox(tr("Fit the same glass in all %1 fixtures of this type").arg(sameType), this);
    m_allOfType->setVisible(sameType > 1);
    root->addWidget(m_allOfType);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &GoboWheelsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &GoboWheelsDialog::reject);
    connect(m_replace, &QPushButton::clicked, this, &GoboWheelsDialog::slotReplace);
    connect(m_restore, &QPushButton::clicked, this, &GoboWheelsDialog::slotRestore);
    connect(folder, &QPushButton::clicked, this, &GoboWheelsDialog::slotOpenFolder);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &GoboWheelsDialog::refreshButtons);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->parent() != nullptr)
            slotReplace();
    });

    rebuild();
    refreshButtons();
}

bool GoboWheelsDialog::hasGoboWheels(const Fixture *fx)
{
    if (fx == nullptr)
        return false;
    for (quint32 i = 0; i < fx->channels(); ++i)
    {
        const QLCChannel *ch = fx->channel(i);
        if (ch == nullptr || ch->group() != QLCChannel::Gobo)
            continue;
        foreach (QLCCapability *cap, ch->capabilities())
            if (isPicture(cap))
                return true;
    }
    return false;
}

QList<const QLCCapability *> GoboWheelsDialog::sameGlass(const QLCChannel *ch, const QLCCapability *cap)
{
    QList<const QLCCapability *> out;
    if (ch == nullptr || cap == nullptr)
        return out;
    out << cap;
    const QString pic = pictureOf(cap);
    if (pic.isEmpty())
        return out;
    foreach (QLCCapability *other, ch->capabilities())
        if (other != cap && pictureOf(other) == pic)
            out << other;
    return out;
}

QString GoboWheelsDialog::effectiveImage(quint32 channel, const QLCCapability *cap) const
{
    const QString swap = m_swaps.value(qMakePair(channel, int(cap->min())));
    if (!swap.isEmpty())
    {
        const QString path = GoboLibrary::resolve(swap);
        if (!path.isEmpty())
            return path;
    }
    return pictureOf(cap);
}

void GoboWheelsDialog::rebuild()
{
    m_tree->clear();
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx == nullptr)
        return;
    for (quint32 i = 0; i < fx->channels(); ++i)
    {
        const QLCChannel *ch = fx->channel(i);
        if (ch == nullptr || ch->group() != QLCChannel::Gobo)
            continue;
        QTreeWidgetItem *wheel = nullptr;
        QSet<const QLCCapability *> shown;
        foreach (QLCCapability *cap, ch->capabilities())
        {
            if (!isPicture(cap) || shown.contains(cap))
                continue;
            const QList<const QLCCapability *> glass = sameGlass(ch, cap);
            for (const QLCCapability *g : glass)
                shown.insert(g);
            if (wheel == nullptr)
            {
                wheel = new QTreeWidgetItem(m_tree);
                wheel->setText(COL_SLOT, ch->name());
                wheel->setFirstColumnSpanned(true);
                wheel->setExpanded(true);
            }
            QTreeWidgetItem *slot = new QTreeWidgetItem(wheel);
            slot->setData(COL_SLOT, ROLE_CHANNEL, i);
            slot->setData(COL_SLOT, ROLE_MIN, int(cap->min()));
            slot->setText(COL_SLOT, glass.size() > 1 ? tr("%1 (and its shake)").arg(cap->name())
                                                     : cap->name());
            const QString swap = m_swaps.value(qMakePair(i, int(cap->min())));
            const QString img = effectiveImage(i, cap);
            slot->setIcon(COL_SLOT, img.isEmpty() ? QIcon() : QIcon(img));
            slot->setText(COL_GLASS, swap.isEmpty() ? tr("as built") : QFileInfo(swap).fileName());
            if (!swap.isEmpty())
            {
                QFont f = slot->font(COL_GLASS);
                f.setBold(true);
                slot->setFont(COL_GLASS, f);
                if (GoboLibrary::resolve(swap).isEmpty())
                    slot->setText(COL_GLASS, tr("%1 (missing)").arg(swap));
            }
        }
    }
}

QTreeWidgetItem *GoboWheelsDialog::currentSlot() const
{
    QTreeWidgetItem *it = m_tree->currentItem();
    return (it != nullptr && it->parent() != nullptr) ? it : nullptr;
}

void GoboWheelsDialog::refreshButtons()
{
    QTreeWidgetItem *slot = currentSlot();
    m_replace->setEnabled(slot != nullptr);
    m_restore->setEnabled(slot != nullptr
        && m_swaps.contains(qMakePair(slot->data(COL_SLOT, ROLE_CHANNEL).toUInt(),
                                      slot->data(COL_SLOT, ROLE_MIN).toInt())));
}

void GoboWheelsDialog::setSlot(QTreeWidgetItem *item, const QString &image)
{
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx == nullptr || item == nullptr)
        return;
    const quint32 chn = item->data(COL_SLOT, ROLE_CHANNEL).toUInt();
    const QLCChannel *ch = fx->channel(chn);
    const QLCCapability *cap = ch != nullptr ? ch->searchCapability(uchar(item->data(COL_SLOT, ROLE_MIN).toInt()))
                                             : nullptr;
    // The slot and its shake: the same piece of glass.
    for (const QLCCapability *g : sameGlass(ch, cap))
    {
        if (image.isEmpty())
            m_swaps.remove(qMakePair(chn, int(g->min())));
        else
            m_swaps.insert(qMakePair(chn, int(g->min())), image);
    }
    const int min = item->data(COL_SLOT, ROLE_MIN).toInt();
    rebuild();
    // Keep the same slot current.
    for (int w = 0; w < m_tree->topLevelItemCount(); ++w)
        for (int s = 0; s < m_tree->topLevelItem(w)->childCount(); ++s)
        {
            QTreeWidgetItem *it = m_tree->topLevelItem(w)->child(s);
            if (it->data(COL_SLOT, ROLE_CHANNEL).toUInt() == chn && it->data(COL_SLOT, ROLE_MIN).toInt() == min)
                m_tree->setCurrentItem(it);
        }
    refreshButtons();
}

void GoboWheelsDialog::slotReplace()
{
    QTreeWidgetItem *slot = currentSlot();
    if (slot == nullptr || !pickImage)
        return;
    const QString path = pickImage(this);
    if (path.isEmpty())
        return;
    const QString name = GoboLibrary::import(path);
    if (name.isEmpty() || BeamOptics::goboMask(GoboLibrary::resolve(name)).isNull())
    {
        QMessageBox::warning(this, tr("Fit a gobo"),
                             tr("“%1” could not be read as a picture.").arg(QFileInfo(path).fileName()));
        return;
    }
    setSlot(slot, name);
}

void GoboWheelsDialog::slotRestore()
{
    setSlot(currentSlot(), QString());
}

void GoboWheelsDialog::slotOpenFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(GoboLibrary::userDirectory().absolutePath()));
}

void GoboWheelsDialog::accept()
{
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx != nullptr)
    {
        MonitorProperties *props = m_doc->monitorProperties();
        QList<quint32> targets;
        if (!m_allOfType->isHidden() && m_allOfType->isChecked())
        {
            foreach (Fixture *other, m_doc->fixtures())
                if (other->fixtureDef() == fx->fixtureDef() && other->fixtureMode() == fx->fixtureMode())
                    targets << other->id();
        }
        else
            targets << m_fid;
        foreach (quint32 t, targets)
        {
            props->clearGoboSwaps(t);
            for (auto it = m_swaps.constBegin(); it != m_swaps.constEnd(); ++it)
                props->setGoboSwap(t, it.key().first, uchar(it.key().second), it.value());
        }
        m_doc->setModified();
    }
    QDialog::accept();
}
