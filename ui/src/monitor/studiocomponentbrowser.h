/*
  Q Light Controller Plus
  studiocomponentbrowser.h

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

#ifndef STUDIOCOMPONENTBROWSER_H
#define STUDIOCOMPONENTBROWSER_H

#include <QDialog>
#include <QList>

#include "studiotemplate.h"

class QListWidget;
class QLabel;
class QPushButton;
class Doc;

/** Fixture Studio — browsable component library.
 *
 *  Lists the saved studio components (StudioTemplate JSON files in the shared
 *  library folder) with a plan-view thumbnail and details, so a component can be
 *  stamped onto the current fixture selection without hunting for a file path.
 *  Also manages the library: rename, delete, and import a one-off .json.
 *
 *  Stamping binds the component's roles, in order, to @p selection (passed at
 *  construction) and emits stamped(groupId) so the host can open + refresh it.
 */
class StudioComponentBrowser : public QDialog
{
    Q_OBJECT

public:
    StudioComponentBrowser(Doc *doc, const QList<quint32> &selection,
                           QWidget *parent = nullptr);

signals:
    /** A component was stamped onto the selection → a new studio group id. */
    void stamped(quint32 groupId);

private slots:
    void reload();                 ///< re-read the library folder into the list
    void selectionChanged();       ///< refresh preview + button enablement
    void stampCurrent();           ///< stamp the highlighted component onto the selection
    void renameCurrent();
    void deleteCurrent();
    void importFile();             ///< copy an external .json into the library

private:
    StudioTemplate::Info currentInfo() const;   ///< the highlighted component (invalid if none)
    void updatePreview(const StudioTemplate::Info &info);

private:
    Doc            *m_doc;
    QList<quint32>  m_selection;   ///< fixtures a stamp will bind to

    QListWidget    *m_list = nullptr;
    QLabel         *m_preview = nullptr;
    QLabel         *m_details = nullptr;
    QPushButton    *m_stampBtn = nullptr;
    QPushButton    *m_renameBtn = nullptr;
    QPushButton    *m_deleteBtn = nullptr;
};

#endif // STUDIOCOMPONENTBROWSER_H
