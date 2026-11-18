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
#include <QString>

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

#endif
