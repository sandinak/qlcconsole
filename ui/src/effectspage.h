/*
  Q Light Controller Plus
  effectspage.h

  Every effect of the show in one place (docs/design/WORKFLOW_DESIGN.md W4) -- effect
  palettes in their folders, and the classic effects (EFX and RGB Matrix
  functions) marked as such -- beside the effect designer on the Try on
  bench. + New makes an effect palette, an EFX or an RGB Matrix. It was the
  Window ▸ Effects window; Build ▸ Effects is effects' one home now (Q68)
  and the app no longer opens it. Kept for the effect designer's tests.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTSPAGE_H
#define EFFECTSPAGE_H

#include <QPointer>
#include <QWidget>

class QStackedWidget;
class QTreeWidgetItem;
class QTreeWidget;
class QToolButton;
class QLineEdit;
class QLabel;
class EffectDesigner;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class EffectsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit EffectsPage(Doc *doc, QWidget *parent = nullptr);
    ~EffectsPage() override;

    EffectDesigner *designer() const { return m_designer; }
    QTreeWidget *navigator() const { return m_nav; }

    /** Rows of the navigator as text, indented by depth (tests). */
    QStringList navigatorText() const;

    /** Show an effect palette in the designer (a copy, on the bench). */
    bool selectPalette(quint32 paletteId);
    /** Show an EFX / RGB Matrix in its own editor. */
    bool selectFunction(quint32 functionId);
    /** A new effect palette in the designer. */
    void newEffect();
    /** A new EFX / RGB Matrix on the Try on groups, opened. Returns its id. */
    quint32 newEfx();
    quint32 newRgbMatrix();

    /** The function whose editor is shown (invalid: the designer is). */
    quint32 editedFunction() const { return m_function; }

private:
    void rebuild();
    void scheduleRebuild();
    void applyFilter();
    void itemActivated(QTreeWidgetItem *item);
    void showEditor(QWidget *editor, const QString &note);
    void closeEditor();

private:
    QPointer<Doc> m_doc;
    QLineEdit *m_filter;
    QToolButton *m_newBtn;
    QTreeWidget *m_nav;
    QStackedWidget *m_stack;
    EffectDesigner *m_designer;
    QWidget *m_editorPage;
    QLabel *m_editorNote;
    QWidget *m_editor = nullptr;
    quint32 m_function;
    quint32 m_selectAfterRebuild;
    bool m_rebuildQueued = false;
};

/** @} */

#endif
