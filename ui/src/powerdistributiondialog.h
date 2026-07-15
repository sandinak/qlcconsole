/*
  Q Light Controller Plus
  powerdistributiondialog.h

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

#ifndef POWERDISTRIBUTIONDIALOG_H
#define POWERDISTRIBUTIONDIALOG_H

#include <QDialog>

class PowerDistributionWidget;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * Standalone window around the workspace power-distribution editor. The editor
 * itself lives in PowerDistributionWidget (also embedded in the Universes page);
 * this dialog is a thin frame so it can be opened from the Programming tab.
 */
class PowerDistributionDialog : public QDialog
{
    Q_OBJECT

public:
    PowerDistributionDialog(Doc *doc, QWidget *parent = NULL);
    ~PowerDistributionDialog();

private:
    Doc *m_doc;
    PowerDistributionWidget *m_widget;
};

/** @} */

#endif
