/*
  Q Light Controller Plus
  studiocomponentbrowser.cpp

  Copyright (c) Massimo Callegari

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPixmap>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "studiocomponentbrowser.h"
#include "studiotemplate.h"
#include "doc.h"

static const int kPreviewW = 260;
static const int kPreviewH = 180;

StudioComponentBrowser::StudioComponentBrowser(Doc *doc, const QList<quint32> &selection,
                                               QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_selection(selection)
{
    setWindowTitle(tr("Studio Components"));
    resize(620, 420);

    QHBoxLayout *outer = new QHBoxLayout(this);

    // Left: the component list.
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->addWidget(new QLabel(tr("Components"), this));
    m_list = new QListWidget(this);
    m_list->setMinimumWidth(200);
    leftCol->addWidget(m_list, 1);
    QPushButton *importBtn = new QPushButton(tr("Import…"), this);
    importBtn->setToolTip(tr("Copy an external component .json into the library"));
    leftCol->addWidget(importBtn);
    outer->addLayout(leftCol);

    // Right: preview + details + actions.
    QVBoxLayout *rightCol = new QVBoxLayout;
    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreviewW, kPreviewH);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setAlignment(Qt::AlignCenter);
    rightCol->addWidget(m_preview, 0, Qt::AlignHCenter);

    m_details = new QLabel(this);
    m_details->setWordWrap(true);
    m_details->setMinimumHeight(48);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    rightCol->addWidget(m_details);
    rightCol->addStretch(1);

    if (m_selection.isEmpty())
        rightCol->addWidget(new QLabel(
            tr("Select fixtures on the map to enable stamping."), this));

    QHBoxLayout *actions = new QHBoxLayout;
    m_stampBtn  = new QPushButton(tr("Stamp onto selection"), this);
    m_stampBtn->setDefault(true);
    m_renameBtn = new QPushButton(tr("Rename…"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    actions->addWidget(m_stampBtn);
    actions->addWidget(m_renameBtn);
    actions->addWidget(m_deleteBtn);
    rightCol->addLayout(actions);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    rightCol->addWidget(bb);
    outer->addLayout(rightCol, 1);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &StudioComponentBrowser::selectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { stampCurrent(); });
    connect(m_stampBtn,  &QPushButton::clicked, this, &StudioComponentBrowser::stampCurrent);
    connect(m_renameBtn, &QPushButton::clicked, this, &StudioComponentBrowser::renameCurrent);
    connect(m_deleteBtn, &QPushButton::clicked, this, &StudioComponentBrowser::deleteCurrent);
    connect(importBtn,   &QPushButton::clicked, this, &StudioComponentBrowser::importFile);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reload();
}

void StudioComponentBrowser::reload()
{
    const QString keep = (m_list->currentItem() != nullptr)
        ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();

    m_list->blockSignals(true);
    m_list->clear();
    foreach (const StudioTemplate::Info &i, StudioTemplate::library())
    {
        QListWidgetItem *li = new QListWidgetItem(
            tr("%1  ·  %2 fixt.").arg(i.name).arg(i.roleCount()), m_list);
        li->setData(Qt::UserRole, i.path);
        if (i.path == keep)
            m_list->setCurrentItem(li);
    }
    m_list->blockSignals(false);

    if (m_list->currentItem() == nullptr && m_list->count() > 0)
        m_list->setCurrentRow(0);
    selectionChanged();
}

StudioTemplate::Info StudioComponentBrowser::currentInfo() const
{
    if (m_list->currentItem() == nullptr)
        return StudioTemplate::Info();
    return StudioTemplate::info(m_list->currentItem()->data(Qt::UserRole).toString());
}

void StudioComponentBrowser::selectionChanged()
{
    const StudioTemplate::Info i = currentInfo();
    const bool have = i.isValid();
    const bool canStamp = have && !m_selection.isEmpty();

    m_stampBtn->setEnabled(canStamp);
    m_renameBtn->setEnabled(have);
    m_deleteBtn->setEnabled(have);
    updatePreview(i);

    if (!have)
    {
        m_details->clear();
        m_stampBtn->setToolTip(QString());
        return;
    }
    QString anchor = i.anchorKind.isEmpty() ? tr("free-standing") : i.anchorKind;
    QString txt = tr("<b>%1</b><br>%2 fixtures · anchor: %3")
                      .arg(i.name).arg(i.roleCount()).arg(anchor);
    if (!m_selection.isEmpty())
    {
        txt += tr("<br>Will fill %1 of %2 roles from the %3 selected fixtures.")
                   .arg(qMin(i.roleCount(), m_selection.size()))
                   .arg(i.roleCount()).arg(m_selection.size());
    }
    m_details->setText(txt);
    m_stampBtn->setToolTip(m_selection.isEmpty()
        ? tr("Select fixtures on the map first") : QString());
}

void StudioComponentBrowser::updatePreview(const StudioTemplate::Info &info)
{
    QPixmap pm(kPreviewW, kPreviewH);
    pm.fill(QColor(30, 32, 36));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!info.isValid() || info.locals.isEmpty())
    {
        p.setPen(QColor(120, 125, 135));
        p.drawText(pm.rect(), Qt::AlignCenter,
                   info.isValid() ? tr("(no roles)") : tr("No component selected"));
        p.end();
        m_preview->setPixmap(pm);
        return;
    }

    // Plan (top) view: x → right, y → down. Fit the role cloud into the pixmap.
    float minX = info.locals.first().x(), maxX = minX;
    float minY = info.locals.first().y(), maxY = minY;
    foreach (const QVector3D &l, info.locals)
    {
        minX = qMin(minX, l.x()); maxX = qMax(maxX, l.x());
        minY = qMin(minY, l.y()); maxY = qMax(maxY, l.y());
    }
    const float spanX = qMax(0.2f, maxX - minX);
    const float spanY = qMax(0.2f, maxY - minY);
    const int margin = 22;
    const double sc = qMin((kPreviewW - 2 * margin) / double(spanX),
                           (kPreviewH - 2 * margin) / double(spanY));
    const double cx = (minX + maxX) / 2.0, cy = (minY + maxY) / 2.0;
    auto toPx = [&](const QVector3D &l) {
        return QPointF(kPreviewW / 2.0 + (l.x() - cx) * sc,
                       kPreviewH / 2.0 + (l.y() - cy) * sc);
    };

    p.setPen(QColor(150, 155, 165));
    p.drawText(6, 14, tr("plan"));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(90, 160, 235));
    foreach (const QVector3D &l, info.locals)
        p.drawEllipse(toPx(l), 4.0, 4.0);
    p.end();
    m_preview->setPixmap(pm);
}

void StudioComponentBrowser::stampCurrent()
{
    const StudioTemplate::Info i = currentInfo();
    if (!i.isValid() || m_selection.isEmpty())
        return;

    if (i.roleCount() > m_selection.size())
    {
        const QMessageBox::StandardButton r = QMessageBox::question(this,
            tr("Stamp Component"),
            tr("\"%1\" has %2 roles but %3 fixtures are selected. "
               "Only the first %3 roles will be filled. Continue?")
                .arg(i.name).arg(i.roleCount()).arg(m_selection.size()));
        if (r != QMessageBox::Yes)
            return;
    }

    QString err;
    const quint32 gid = StudioTemplate::stamp(m_doc, i.path, m_selection, &err);
    if (gid == 0)
    {
        QMessageBox::warning(this, tr("Stamp Component"),
                             tr("Could not stamp component: %1").arg(err));
        return;
    }
    emit stamped(gid);
    accept();
}

void StudioComponentBrowser::renameCurrent()
{
    const StudioTemplate::Info i = currentInfo();
    if (!i.isValid())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Component"),
        tr("Name:"), QLineEdit::Normal, i.name, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    QString err;
    if (StudioTemplate::renameInLibrary(i.path, name, &err).isEmpty())
        QMessageBox::warning(this, tr("Rename Component"),
                             tr("Could not rename: %1").arg(err));
    reload();
}

void StudioComponentBrowser::deleteCurrent()
{
    const StudioTemplate::Info i = currentInfo();
    if (!i.isValid())
        return;
    if (QMessageBox::question(this, tr("Delete Component"),
            tr("Delete component \"%1\"? This cannot be undone.").arg(i.name))
            != QMessageBox::Yes)
        return;
    StudioTemplate::removeFile(i.path);
    reload();
}

void StudioComponentBrowser::importFile()
{
    const QString src = QFileDialog::getOpenFileName(this, tr("Import Component"),
        QString(), tr("Fixture Studio Component (*.json)"));
    if (src.isEmpty())
        return;
    // Validate it, then copy into the library under its declared name.
    const StudioTemplate::Info i = StudioTemplate::info(src);
    if (!i.isValid())
    {
        QMessageBox::warning(this, tr("Import Component"),
                             tr("That file is not a Fixture Studio component."));
        return;
    }
    QDir dir(StudioTemplate::libraryPath());
    QString dest = dir.absoluteFilePath(QFileInfo(src).fileName());
    int n = 2;
    while (QFile::exists(dest))
        dest = dir.absoluteFilePath(QString("%1_%2.json")
                   .arg(QFileInfo(src).completeBaseName()).arg(n++));
    if (!QFile::copy(src, dest))
    {
        QMessageBox::warning(this, tr("Import Component"),
                             tr("Could not copy the file into the library."));
        return;
    }
    reload();
}
