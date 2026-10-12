/*
  Q Light Controller Plus
  repairdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QSplitter>
#include <QLabel>
#include <QTimer>

#include "repairdialog.h"

RepairDialog::RepairDialog(const QString &path, const QByteArray &xml, QWidget *parent)
    : QDialog(parent)
    , m_path(path)
    , m_settingText(false)
{
    setWindowTitle(tr("Repair %1").arg(QFileInfo(path).fileName()));
    QVBoxLayout *lay = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr("This definition cannot be read whole as it is. Apply a "
                                  "repair below, or edit the file by hand -- it is checked "
                                  "again as you go."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    /* What could not have been meant any other way ("\>" for "/>") is
       fixed before it is shown -- said so, and undoable. */
    m_autoBar = new QWidget(this);
    QHBoxLayout *autoLay = new QHBoxLayout(m_autoBar);
    autoLay->setContentsMargins(0, 0, 0, 0);
    m_autoNote = new QLabel(m_autoBar);
    m_autoNote->setWordWrap(true);
    autoLay->addWidget(m_autoNote, 1);
    QPushButton *undo = new QPushButton(tr("Undo"), m_autoBar);
    autoLay->addWidget(undo);
    lay->addWidget(m_autoBar);
    connect(undo, &QPushButton::clicked, this, [this]() {
        m_settingText = true;
        m_text->setPlainText(QString::fromUtf8(m_original));
        m_settingText = false;
        m_autoBar->hide();
        recheck();
    });

    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    m_problem->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_problem);

    QSplitter *split = new QSplitter(Qt::Vertical, this);
    m_text = new QPlainTextEdit(split);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Menlo"));
    mono.setStyleHint(QFont::Monospace);
    m_text->setFont(mono);
    QWidget *fixBox = new QWidget(split);
    QVBoxLayout *fixLay = new QVBoxLayout(fixBox);
    fixLay->setContentsMargins(0, 0, 0, 0);
    fixLay->addWidget(new QLabel(tr("Proposed repairs (each one tried first):"), fixBox));
    m_fixes = new QListWidget(fixBox);
    fixLay->addWidget(m_fixes);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_apply = box->addButton(tr("Apply Repair"), QDialogButtonBox::ActionRole);
    m_open = box->addButton(tr("Open in Editor"), QDialogButtonBox::AcceptRole);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_apply, &QPushButton::clicked, this, [this]() { applyRepair(m_fixes->currentRow()); });
    connect(m_fixes, &QListWidget::itemDoubleClicked, this, [this]() { applyRepair(m_fixes->currentRow()); });
    connect(m_fixes, &QListWidget::currentRowChanged, this, [this](int row) {
        m_apply->setEnabled(row >= 0);
        // Show where it would change.
        if (row >= 0 && row < m_repairs.size())
        {
            const QTextBlock b = m_text->document()->findBlockByNumber(m_repairs.at(row).line - 1);
            if (b.isValid())
            {
                QTextCursor c(b);
                m_text->setTextCursor(c);
                m_text->centerCursor();
            }
        }
    });
    lay->addWidget(box);

    // Edited by hand: checked again a moment after typing stops.
    m_recheck = new QTimer(this);
    m_recheck->setSingleShot(true);
    m_recheck->setInterval(400);
    connect(m_recheck, &QTimer::timeout, this, &RepairDialog::recheck);
    connect(m_text, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_settingText)
            m_recheck->start();
    });

    m_original = xml;
    QStringList applied;
    const QByteArray fixed = FixtureDefCheck::autoRepair(xml, &applied);
    m_autoNote->setText(tr("<span style='color:#3aa655;font-weight:bold;'>Fixed automatically:</span> %1")
                            .arg(applied.join(QStringLiteral("; ")).toHtmlEscaped()));
    m_autoBar->setVisible(!applied.isEmpty());

    m_settingText = true;
    m_text->setPlainText(QString::fromUtf8(fixed));
    m_settingText = false;
    recheck();
    resize(820, 620);
}

QByteArray RepairDialog::xml() const
{
    return m_text->toPlainText().toUtf8();
}

void RepairDialog::applyRepair(int index)
{
    if (index < 0 || index >= m_repairs.size())
        return;
    const QByteArray fixed = m_repairs.at(index).fixed;
    m_settingText = true;
    m_text->setPlainText(QString::fromUtf8(fixed));
    m_settingText = false;
    recheck();
}

void RepairDialog::recheck()
{
    const QByteArray current = xml();
    FixtureDefCheck::ParseProblem p;
    const bool broken = FixtureDefCheck::diagnose(current, &p);

    QList<QTextEdit::ExtraSelection> marks;
    if (broken)
    {
        // Red when it will not load; amber when it loads but loses part of itself.
        const QString colour = p.loads ? QStringLiteral("#d08a00") : QStringLiteral("#e04030");
        m_problem->setText(p.line > 0
            ? tr("<span style='color:%1;font-weight:bold;'>Line %2%3:</span> %4")
                  .arg(colour).arg(p.line)
                  .arg(p.column > 0 ? tr(", column %1").arg(p.column) : QString())
                  .arg(p.message.toHtmlEscaped())
            : tr("<span style='color:%1;font-weight:bold;'>%2</span>").arg(colour, p.message.toHtmlEscaped()));
        if (p.line > 0)
        {
            QTextEdit::ExtraSelection mark;
            mark.format.setBackground(QColor(224, 64, 48, 70));
            mark.format.setProperty(QTextFormat::FullWidthSelection, true);
            const QTextBlock b = m_text->document()->findBlockByNumber(p.line - 1);
            if (b.isValid())
            {
                mark.cursor = QTextCursor(b);
                marks << mark;
                m_text->setTextCursor(mark.cursor);
                m_text->centerCursor();
            }
        }
    }
    else
    {
        m_problem->setText(tr("<span style='color:#3aa655;font-weight:bold;'>It reads now.</span> "
                              "Open it in the editor to look it over and save."));
    }
    m_text->setExtraSelections(marks);

    m_repairs = broken ? FixtureDefCheck::repairs(current) : QList<FixtureDefCheck::Repair>();
    m_fixes->clear();
    foreach (const FixtureDefCheck::Repair &rp, m_repairs)
    {
        QListWidgetItem *it = new QListWidgetItem(
            tr("%1%2\n    line %3:  %4   →   %5")
                .arg(rp.description, rp.complete ? tr("  (it then reads)") : QString())
                .arg(rp.line).arg(rp.before.left(70), rp.after.left(70)),
            m_fixes);
        if (rp.complete)
        {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
    }
    if (broken && m_repairs.isEmpty())
        new QListWidgetItem(tr("No repair to propose for this one -- edit the line by hand."), m_fixes);
    if (!m_repairs.isEmpty())
        m_fixes->setCurrentRow(0);
    m_apply->setEnabled(!m_repairs.isEmpty());
    // A file that loads -- whole, or losing part of itself -- can be opened.
    m_open->setEnabled(!broken || p.loads);
    m_open->setText(broken && p.loads ? tr("Open Anyway") : tr("Open in Editor"));
    if (!broken)
        m_open->setDefault(true);
}
