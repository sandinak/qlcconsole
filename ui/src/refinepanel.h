/*
  Q Light Controller Plus
  refinepanel.h

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

#ifndef REFINEPANEL_H
#define REFINEPANEL_H

#include <QPointer>
#include <QWidget>

class FocusControlMap;
class RefineSession;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Refine's panel (FOCUS_REFINE_DESIGN.md section 6): the cue, the layer,
 * the scope and what is picked, what is pending, and the controller drawn
 * with Refine's words -- the same layout as Focus mode. Thin: everything it
 * does is RefineSession's.
 */
class RefinePanel final : public QWidget
{
    Q_OBJECT

public:
    /** Refine @p cueId from its layer @p layerId (or raise the panel if it
     *  is running). Null, with the reason shown, if it cannot. */
    static RefinePanel *open(Doc *doc, QWidget *parent, quint32 cueId,
                             quint32 layerId = 0xFFFFFFFF, QString *why = nullptr);
    static RefinePanel *instance();

    ~RefinePanel() override;

    /** Leave, asking about what is not kept. False on Cancel. */
    bool requestExit();

    FocusControlMap *controlMap() const { return m_map; }
    /** The panel's lines, for the tests. */
    QString headline() const;
    QString scopeLine() const;
    QString pendingLine() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    RefinePanel(Doc *doc, QWidget *parent);
    void refresh();
    bool handleKey(int key, Qt::KeyboardModifiers modifiers);

private:
    Doc *m_doc;
    RefineSession *m_session;
    bool m_finishing = false;
    QLabel *m_title;
    QLabel *m_layer;
    QLabel *m_scope;
    QLabel *m_pending;
    QLabel *m_device;
    FocusControlMap *m_map;

    static QPointer<RefinePanel> s_instance;
};

/** @} */

#endif
