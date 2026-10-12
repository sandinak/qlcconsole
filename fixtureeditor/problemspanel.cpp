/*
  Q Light Controller Plus
  problemspanel.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>

#include "problemspanel.h"

using FixtureDefCheck::Issue;

ProblemsPanel::ProblemsPanel(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(2);
    m_summary = new QLabel(this);
    lay->addWidget(m_summary);
    m_list = new QTreeWidget(this);
    m_list->setColumnCount(2);
    m_list->setHeaderLabels(QStringList() << tr("Where") << tr("Problem"));
    m_list->setRootIsDecorated(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(true);
    m_list->setMaximumHeight(150);
    lay->addWidget(m_list);

    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const int i = m_list->indexOfTopLevelItem(item);
        if (i >= 0 && i < m_issues.size())
            emit activated(m_issues.at(i));
    });
    setIssues(QList<Issue>());
}

int ProblemsPanel::errorCount() const
{
    int n = 0;
    foreach (const Issue &i, m_issues)
        if (i.level == Issue::Error)
            ++n;
    return n;
}

void ProblemsPanel::setIssues(const QList<Issue> &issues)
{
    m_issues = issues;
    m_list->clear();
    const int errors = errorCount(), warnings = issues.size() - errors;
    if (issues.isEmpty())
        m_summary->setText(tr("<span style='color:#3aa655;'>No problems found.</span>"));
    else
        m_summary->setText(tr("<b>Problems:</b> "
                              "<span style='color:#e04030;'>%1 error(s)</span>, "
                              "<span style='color:#d08a00;'>%2 warning(s)</span> "
                              "— double-click one to go to it")
                               .arg(errors).arg(warnings));
    m_list->setVisible(!issues.isEmpty());

    foreach (const Issue &i, issues)
    {
        QString where;
        switch (i.section)
        {
        case Issue::General:  where = tr("General"); break;
        case Issue::Physical: where = tr("Physical"); break;
        case Issue::Channels: where = tr("Channels"); break;
        case Issue::Modes:    where = tr("Modes"); break;
        }
        if (!i.target.isEmpty())
            where += QString(" › %1").arg(i.target);
        QTreeWidgetItem *it = new QTreeWidgetItem(m_list, QStringList() << where << i.message);
        const QColor c = (i.level == Issue::Error) ? QColor(224, 64, 48) : QColor(208, 138, 0);
        it->setForeground(0, c);
        it->setText(1, (i.level == Issue::Error ? QStringLiteral("✖ ") : QStringLiteral("⚠ ")) + i.message);
        it->setForeground(1, c);
        it->setToolTip(1, i.message);
    }
}
