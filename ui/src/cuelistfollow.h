/*
  Q Light Controller Plus
  cuelistfollow.h

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

#ifndef CUELISTFOLLOW_H
#define CUELISTFOLLOW_H

#include <QObject>
#include <QPointer>

#include "qlcui_export.h"

class QTreeWidget;
class QTreeWidgetItem;

/**
 * The rule every cue list on screen follows (the Virtual Console cue list,
 * the Run tab's): the active row is always in sight. From the top the
 * highlight moves down until it reaches the middle of the view; from then on
 * the list scrolls so the active row stays in the middle. Near the end the
 * list keeps scrolling over blank space (the scroll range is longer by half
 * a view, no rows are added), so the last cue reaches the middle too.
 *
 * A person scrolling or clicking is not fought: a request that comes while
 * a mouse button is down in the list or its scroll bar, or the wheel was
 * just used, waits for them to finish, and the next follow() re-centres.
 */
class QLC_UI_EXPORT CueListFollow : public QObject
{
    Q_OBJECT

public:
    explicit CueListFollow(QTreeWidget *tree);

    /** Scroll so \a item is in the middle (or as near as the top allows). */
    void follow(QTreeWidgetItem *item);

    /** The vertical scroll bar's extra range, in pixels, now. */
    int padding() const { return m_pad; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool userActive() const;
    void apply();
    void pad(int min, int max);

private:
    QTreeWidget *m_tree;
    QTreeWidgetItem *m_item = nullptr;
    bool m_mouseDown = false;
    bool m_pending = false;     // a follow() that waited for the user
    bool m_touched = false;     // the user scrolled since the last follow()
    qint64 m_wheelAt = 0;
    bool m_padding = false;
    int m_pad = 0;
};

#endif
