/*
  Q Light Controller Plus - qlcconsole
  qxwimporter_test.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef QXWIMPORTER_TEST_H
#define QXWIMPORTER_TEST_H

#include <QObject>

class Doc;

class QxwImporter_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    /** A palette attached to an imported Scene comes across with it. */
    void paletteFollowsScene();
    /** A palette whose id is already taken in the target gets a new one, and
        the Scene that referenced it is rewritten to the new id. */
    void collidingPaletteIsRemapped();
    /** Per-palette fade overrides are keyed by palette id and must be
        remapped alongside the reference itself. */
    void paletteFadeOverrideFollowsRemap();
    /** Palette order is precedence, so it must survive the round trip. */
    void paletteOrderPreserved();

private:
    Doc *m_source;
    Doc *m_target;
};

#endif
