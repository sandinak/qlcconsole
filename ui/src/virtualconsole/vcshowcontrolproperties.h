/*
  Q Light Controller Plus
  vcshowcontrolproperties.h

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

#ifndef VCSHOWCONTROLPROPERTIES_H
#define VCSHOWCONTROLPROPERTIES_H

#include <QDialog>

class QComboBox;
class InputSelectionWidget;
class VCShowControl;
class Doc;

/** @addtogroup ui_vc_widgets
 * @{
 */

class VCShowControlProperties final : public QDialog
{
    Q_OBJECT

public:
    VCShowControlProperties(VCShowControl *sc, Doc *doc);
    ~VCShowControlProperties();

public slots:
    void accept() override;

private:
    VCShowControl *m_sc;
    Doc *m_doc;

    QComboBox *m_showCombo;
    InputSelectionWidget *m_playInput;
    InputSelectionWidget *m_stopInput;
    InputSelectionWidget *m_followInput;
    InputSelectionWidget *m_suspendInput;
};

/** @} */

#endif
