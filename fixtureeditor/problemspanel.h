/*
  Q Light Controller Plus
  problemspanel.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PROBLEMSPANEL_H
#define PROBLEMSPANEL_H

#include <QWidget>

#include "fixturedefcheck.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

/**
 * The definition being edited, checked as it is edited
 * (FixtureDefCheck::check()): errors -- clearly wrong or broken -- in red,
 * warnings -- missing data the rig view and effects would have to guess --
 * in amber. Double-click one to go to where it is fixed.
 */
class ProblemsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget *parent = nullptr);

    void setIssues(const QList<FixtureDefCheck::Issue> &issues);
    QList<FixtureDefCheck::Issue> issues() const { return m_issues; }
    int errorCount() const;

signals:
    /** Take the user to where @p issue is fixed. */
    void activated(const FixtureDefCheck::Issue &issue);

private:
    QLabel *m_summary;
    QTreeWidget *m_list;
    QList<FixtureDefCheck::Issue> m_issues;
};

#endif
