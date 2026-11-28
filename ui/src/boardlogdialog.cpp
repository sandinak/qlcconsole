/*
  Q Light Controller Plus - qlcconsole
  boardlogdialog.cpp

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

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "boardlogdialog.h"
#include "surfaceboardlog.h"
#include "surfacesetup.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "doc.h"

BoardLogDialog::BoardLogDialog(Doc *doc, quint32 universe, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_universe(universe)
{
    setObjectName(QStringLiteral("boardLog"));
    InputPatch *ip = doc->inputOutputMap()->inputPatch(universe);
    const QString board = ip != nullptr && !ip->profileName().isEmpty() ? ip->profileName()
                                                                         : tr("Universe %1").arg(universe + 1);
    setWindowTitle(tr("Board Log: %1").arg(board));
    QVBoxLayout *lay = new QVBoxLayout(this);
    QLabel *what = new QLabel(tr("What this board sent (IN) and was sent (OUT), newest last, with the app's own "
                                 "steps (--): its init, its profile, the motor fader sent to the Grand Master. "
                                 "The last %1 are kept. To report a problem: do the thing on the board, then "
                                 "Copy and paste it into your report.").arg(SurfaceBoardLog::kSize), this);
    what->setWordWrap(true);
    lay->addWidget(what);
    m_log = new QPlainTextEdit(this);
    m_log->setObjectName(QStringLiteral("boardLogText"));
    m_log->setReadOnly(true);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lay->addWidget(m_log, 1);

    QHBoxLayout *test = new QHBoxLayout();
    m_test = new QPushButton(tr("Lamp Test"), this);
    m_test->setObjectName(QStringLiteral("boardLogLampTest"));
    m_test->setToolTip(tr("Light each of this board's lamps in turn, then all of them, then give them back to the show."));
    test->addWidget(m_test);
    m_step = new QLabel(this);
    m_step->setObjectName(QStringLiteral("boardLogStep"));
    m_step->setWordWrap(true);
    test->addWidget(m_step, 1);
    lay->addLayout(test);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *copy = bb->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copy->setObjectName(QStringLiteral("boardLogCopy"));
    copy->setToolTip(tr("Copy the whole log as text, to paste into a report."));
    QPushButton *clear = bb->addButton(tr("Clear"), QDialogButtonBox::ActionRole);
    clear->setObjectName(QStringLiteral("boardLogClear"));
    connect(copy, &QPushButton::clicked, this, [this]() { QApplication::clipboard()->setText(logText()); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);

    SurfaceSetup *ss = m_doc->surfaceSetup();
    if (ss != nullptr)
    {
        SurfaceBoardLog *log = ss->boardLog();
        // Many messages a second while a fader moves: one repaint a frame.
        QTimer *coalesce = new QTimer(this);
        coalesce->setSingleShot(true);
        coalesce->setInterval(50);
        connect(coalesce, &QTimer::timeout, this, &BoardLogDialog::refresh);
        connect(log, &SurfaceBoardLog::logged, this, [this, coalesce](quint32 u) {
            if (u == m_universe && !coalesce->isActive())
                coalesce->start();
        });
        connect(clear, &QPushButton::clicked, this, [this, log]() { log->clear(m_universe); });
        connect(ss, &SurfaceSetup::lampTestStepped, this, [this](quint32 u, const QString &step) {
            if (u != m_universe)
                return;
            m_step->setText(step);
            m_test->setText(step.isEmpty() ? tr("Lamp Test") : tr("Stop Test"));
        });
        connect(m_test, &QPushButton::clicked, this, [this, ss]() {
            if (ss->lampTestRunning())
                ss->stopLampTest();
            else
                startLampTest();
        });
    }
    resize(820, 520);
    refresh();
}

BoardLogDialog::~BoardLogDialog()
{
    if (SurfaceSetup *ss = m_doc->surfaceSetup())
        ss->stopLampTest();
}

void BoardLogDialog::startLampTest()
{
    SurfaceSetup *ss = m_doc->surfaceSetup();
    QString why;
    if (ss == nullptr || !ss->startLampTest(m_universe, &why))
        m_step->setText(why);
}

QString BoardLogDialog::logText() const
{
    SurfaceSetup *ss = m_doc->surfaceSetup();
    return ss != nullptr ? ss->boardLog()->text(m_universe) : QString();
}

void BoardLogDialog::refresh()
{
    const bool atEnd = m_log->verticalScrollBar()->value() >= m_log->verticalScrollBar()->maximum() - 2;
    m_log->setPlainText(logText());
    if (atEnd)
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}
