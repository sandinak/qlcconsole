/*
  Q Light Controller Plus
  fixtureeditor.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef TEST_FIXTUREEDITOR_H
#define TEST_FIXTUREEDITOR_H

#include <QApplication>
#include <QDialog>
#include <QFormLayout>
#include <QString>
#include <QTabWidget>
#include <QToolButton>
#include <QVariant>

/** The dialog a test drives from a timer: the active modal widget or, since
 *  Fixture Properties is no longer modal (the rig views stay usable beside
 *  it), the open Fixture Properties window. */
inline QWidget *modalOrFixtureEditor()
{
    if (QWidget *m = QApplication::activeModalWidget())
        return m;
    for (QWidget *w : QApplication::topLevelWidgets())
        if (w->isVisible() && qobject_cast<QDialog *>(w) != nullptr
            && w->windowTitle().startsWith(QStringLiteral("Fixture Properties")))
            return w;
    return nullptr;
}

/** Fixture Properties: @p w (a field or a row's label) is shown for this
 *  fixture -- looked at on its tab, Aim & Mount's More unfolded if it is in
 *  it, as a person finds it. Its rows are on tabs, so isVisibleTo(dlg) alone
 *  is false for any row on a tab not shown. */
inline bool fixtureRowShown(QWidget *dlg, QWidget *w)
{
    if (dlg == nullptr || w == nullptr)
        return false;
    if (QTabWidget *tabs = dlg->findChild<QTabWidget *>(QStringLiteral("fixturePropertiesTabs")))
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->widget(i)->isAncestorOf(w))
                tabs->setCurrentIndex(i);
    QWidget *field = w;
    for (QFormLayout *f : dlg->findChildren<QFormLayout *>())
        for (int r = 0; r < f->rowCount(); ++r)
        {
            QLayoutItem *label = f->itemAt(r, QFormLayout::LabelRole);
            QLayoutItem *fieldItem = f->itemAt(r, QFormLayout::FieldRole);
            if (label != nullptr && label->widget() == w && fieldItem != nullptr && fieldItem->widget() != nullptr)
                field = fieldItem->widget();
        }
    QToolButton *more = dlg->findChild<QToolButton *>(QStringLiteral("fixtureAimMore"));
    if (more != nullptr && !more->isChecked())
        for (QWidget *p = field; p != nullptr && p != dlg; p = p->parentWidget())
            if (p->property("fixtureAimShade").toBool())
            {
                more->setChecked(true);
                break;
            }
    return w->isVisibleTo(dlg);
}

#endif
