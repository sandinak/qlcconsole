/*
  Q Light Controller Plus
  focusmodepanel.h

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

#ifndef FOCUSMODEPANEL_H
#define FOCUSMODEPANEL_H

#include <QPointer>
#include <QDialog>
#include <QWidget>

#include "focussession.h"

class QDoubleSpinBox;
class QPushButton;
class QComboBox;
class QCheckBox;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * What Focus mode starts on: which heads, in what order, aimed at what, and
 * what the others do. FOCUS_MODE_DESIGN.md.
 */
class FocusModeSetupDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @p sceneId: the selected scene, offered as the reference when it has
     *  an Aim look. @p selection: the heads selected now (offered first). */
    FocusModeSetupDialog(Doc *doc, quint32 sceneId, const QList<quint32> &selection,
                         QWidget *parent = nullptr);

    FocusSession::Options options() const;

private:
    void fillReferences(QComboBox *combo, bool optional);
    static FocusSession::Reference referenceOf(const QComboBox *combo);

private:
    Doc *m_doc;
    quint32 m_sceneId;
    QList<quint32> m_selection;
    QComboBox *m_whichCombo;
    QComboBox *m_orderCombo;
    QComboBox *m_firstCombo;
    QComboBox *m_secondCombo;
    QComboBox *m_othersCombo;
    QDoubleSpinBox *m_levelSpin;
};

/**
 * The Focus mode HUD: which head, where it is aimed, its offsets live and
 * saved, the step, progress, the controls -- and the keyboard way to do all
 * of it, so it works without a controller. One at a time; open() raises it.
 */
class FocusModePanel final : public QWidget
{
    Q_OBJECT

public:
    /** Start Focus mode (asking how, through the setup dialog) or raise the
     *  panel if it is running. Returns the panel, or null if it was cancelled
     *  or refused (the reason is shown). */
    static FocusModePanel *open(Doc *doc, QWidget *parent, quint32 sceneId,
                                const QList<quint32> &selection);
    /** Start Focus mode on @p options without asking (open() after its
     *  setup dialog; a test). Null, with @p why, if the session refused. */
    static FocusModePanel *startWith(Doc *doc, QWidget *parent,
                                     const FocusSession::Options &options, QString *why = nullptr);
    /** The running panel, if any. */
    static FocusModePanel *instance();

    ~FocusModePanel() override;

    /** Leave Focus mode, asking first about unsaved offsets. False if the
     *  answer was Cancel. */
    bool requestExit();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    FocusModePanel(Doc *doc, QWidget *parent);
    void refresh();
    void showHighlight(quint32 fid);
    void finish();

private:
    Doc *m_doc;
    FocusSession *m_session;
    QByteArray m_rigBefore;
    bool m_finishing = false;

    QLabel *m_titleLabel;
    QLabel *m_nameLabel;
    QLabel *m_referenceLabel;
    QLabel *m_offsetLabel;
    QLabel *m_savedLabel;
    QLabel *m_stepLabel;
    QLabel *m_progressLabel;
    QPushButton *m_undoButton;
    QPushButton *m_refButton;

    static QPointer<FocusModePanel> s_instance;
};

/** @} */

#endif
