/*
  Q Light Controller Plus - qlcconsole
  startupwindow.cpp

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
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QEventLoop>
#include <QScrollBar>
#include <QScreen>
#include <QLabel>
#include <QIcon>
#include <QFont>
#include <QFrame>

#include "startupwindow.h"
#include "qlcconfig.h"

StartupWindow::StartupWindow(int totalSteps, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_layout(NULL)
    , m_log(NULL)
    , m_bar(NULL)
    , m_step(0)
{
    setFixedSize(480, 360);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_layout = new QVBoxLayout(this);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *icon = new QLabel(this);
    icon->setPixmap(QIcon(":/qlcconsole.png").pixmap(48, 48));
    header->addWidget(icon);

    QVBoxLayout *titleBox = new QVBoxLayout;
    QLabel *name = new QLabel(QStringLiteral(APPNAME), this);
    QFont nameFont = name->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 4);
    name->setFont(nameFont);
    QLabel *version = new QLabel(QStringLiteral(APPVERSION), this);
    // Every theme in App::applyTheme() sets WindowText explicitly for
    // exactly this reason -- it's guaranteed readable against Window.
    // palette(mid) (shadow/groove tone, never meant for text) was tried
    // here first and came out too faint on more than one theme; a plain,
    // slightly smaller font gives the same "secondary" feel without
    // gambling on contrast.
    QFont versionFont = version->font();
    versionFont.setPointSize(qMax(versionFont.pointSize() - 1, 6));
    version->setFont(versionFont);
    titleBox->addWidget(name);
    titleBox->addWidget(version);
    header->addLayout(titleBox);
    header->addStretch();

    m_layout->addLayout(header);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFrameStyle(QFrame::NoFrame);
    m_layout->addWidget(m_log, 1);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, totalSteps);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_layout->addWidget(m_bar);

    QScreen *scr = QGuiApplication::primaryScreen();
    if (scr != NULL)
    {
        const QRect avail = scr->availableGeometry();
        move(avail.center().x() - width() / 2, avail.center().y() - height() / 2);
    }
}

void StartupWindow::beginStep(const QString &text)
{
    m_step++;
    m_bar->setValue(m_step);
    m_log->appendHtml(QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped()));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    QApplication::processEvents();
}

void StartupWindow::logDetail(const QString &text)
{
    m_log->appendPlainText(QStringLiteral("    %1").arg(text));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    QApplication::processEvents();
}

bool StartupWindow::askYesNo(const QString &question)
{
    logDetail(question);

    QWidget *bar = new QWidget(this);
    QHBoxLayout *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    QLabel *label = new QLabel(question, bar);
    label->setWordWrap(true);
    QPushButton *yes = new QPushButton(tr("Yes"), bar);
    QPushButton *no = new QPushButton(tr("No"), bar);
    row->addWidget(label, 1);
    row->addWidget(yes);
    row->addWidget(no);
    // Above the progress bar, i.e. the last row before it (indices: header
    // layout, log, [this bar], bar) -- count() - 1 is the progress bar.
    m_layout->insertWidget(m_layout->count() - 1, bar);

    bool result = false;
    QEventLoop loop;
    connect(yes, &QPushButton::clicked, &loop, [&result, &loop]() { result = true; loop.quit(); });
    connect(no, &QPushButton::clicked, &loop, [&result, &loop]() { result = false; loop.quit(); });
    loop.exec();

    m_layout->removeWidget(bar);
    delete bar;

    logDetail(result ? tr("-> Yes") : tr("-> No"));

    return result;
}
