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
class QLineEdit;
class QPushButton;
class Doc;

/** What Make Student Handout writes: a copy of the show for students. */
struct HandoutOptions
{
    /** The copy; a .qlcc (its definitions travel with it) */
    QString path;
    /** The copy's PIN (ShowPin::hash()), or empty for none */
    QString pinHash;
    /** Leave out practice venues and their answer key */
    bool dropPractice = true;
    /** Blank the "checked by" names in the sidecar */
    bool dropNames = true;
    /** Real network output stays held in the copy */
    bool holdOutput = true;
    QString lesson;
    QString link;
};

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

    /** The user pressed Make Student Handout (after the settings were
     *  applied): the caller runs the handout dialog next. */
    bool handoutRequested() const { return m_handoutRequested; }

    /** The user pressed Reset to Handout and gave the PIN (if any) and
     *  confirmed: the dialog closed without applying its other changes, and
     *  the caller puts the show back to its handout state. */
    bool resetRequested() const { return m_resetRequested; }

    /** A Teaching show asks before its patch changes (adding, removing or
     *  re-addressing fixtures): true to go on. Always true for a show that
     *  is not in Teaching mode. @p what is the change in a few words. In test
     *  mode the answer is taken from testAnswers() ("yes" goes on, anything
     *  else or nothing stops) and patchQuestions() counts the questions. */
    static bool confirmPatchChange(Doc *doc, QWidget *parent, const QString &what);
    static int &patchQuestions();

    /** Ask for the handout's options (the file, its PIN, what to leave out);
     *  false on Cancel. */
    static bool askHandoutOptions(Doc *doc, QWidget *parent, HandoutOptions *out);

    /** Test seam: with test mode on, nothing is shown. The answers are
     *  given in turn instead of asking (none left, or an empty one, is
     *  Cancel), and a wrong PIN isn't a message box. Make Student Handout
     *  then takes testHandout() as its options. */
    static HandoutOptions &testHandout();
    static void setTestMode(bool on);
    static QStringList &testAnswers();

public slots:
    void accept() override;

private slots:
    void slotSetPin();
    void slotRemovePin();
    void slotHandout();
    void slotReset();
    void updateEnabled();

private:
    void updatePinText();

private:
    Doc *m_doc;
    QCheckBox *m_openInOperate;
    QCheckBox *m_lockDesign;
    QCheckBox *m_teaching;
    QCheckBox *m_allowOutput;
    QLineEdit *m_lesson;
    QLineEdit *m_link;
    QPushButton *m_handout;
    bool m_handoutRequested = false;
    bool m_resetRequested = false;
    QPushButton *m_reset;
    QLabel *m_pinText;
    QPushButton *m_setPin;
    QPushButton *m_removePin;
    /** What OK writes: the hash now (unchanged), a new one, or none */
    QString m_newPinHash;
};

/** @} */

#endif
