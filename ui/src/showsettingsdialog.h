/*
  Q Light Controller Plus
  showsettingsdialog.h

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

#ifndef SHOWSETTINGSDIALOG_H
#define SHOWSETTINGSDIALOG_H

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QLabel;
class QPushButton;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * File ▸ Show Settings: settings saved with the show that aren't about
 * its patch or its cues. Today the Operate ones: open the show in
 * Operate, and a PIN that going back to Design asks for (Teaching mode
 * will add its own here). Changes apply on OK.
 */
class ShowSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ShowSettingsDialog(Doc *doc, QWidget *parent = nullptr);

    /** The PIN the dialog will set on OK: empty for none. */
    bool pinWillBeSet() const { return !m_newPinHash.isEmpty(); }

    /** Ask for a new PIN, twice: the PIN, or empty on Cancel. Says so and
     *  asks again when the two differ or it isn't 4-8 digits. */
    static QString askNewPin(QWidget *parent);
    /** Ask for the PIN @p hash holds. True when it is given; false on
     *  Cancel, or (said) when wrong. */
    static bool askForPin(QWidget *parent, const QString &hash, const QString &why);

    /** Test seam: with test mode on, nothing is shown. The answers are
     *  given in turn instead of asking (none left, or an empty one, is
     *  Cancel), and a wrong PIN isn't a message box. */
    static void setTestMode(bool on);
    static QStringList &testAnswers();

public slots:
    void accept() override;

private slots:
    void slotSetPin();
    void slotRemovePin();

private:
    void updatePinText();

private:
    Doc *m_doc;
    QCheckBox *m_openInOperate;
    QLabel *m_pinText;
    QPushButton *m_setPin;
    QPushButton *m_removePin;
    /** What OK writes: the hash now (unchanged), a new one, or none */
    QString m_newPinHash;
};

/** @} */

#endif
