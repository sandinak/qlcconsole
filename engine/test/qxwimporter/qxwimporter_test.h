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
    void anImportedFixtureNameAlreadyInTheShowIsNumbered();
    /** A palette whose id is already taken in the target gets a new one, and
        the Scene that referenced it is rewritten to the new id. */
    void collidingPaletteIsRemapped();
    /** Per-palette fade overrides are keyed by palette id and must be
        remapped alongside the reference itself. */
    void paletteFadeOverrideFollowsRemap();
    /** Palette order is precedence, so it must survive the round trip. */
    void paletteOrderPreserved();

    /** A fixture's 2D-map placement comes across with it. */
    void mapPlacementFollowsFixture();
    /** Layer/group ids belong to id spaces we don't import, so they must be
        reset rather than left dangling. */
    void mapLayerAndGroupAreReset();
    /** A fixture patched to a power circuit arrives patched. */
    void powerPatchFollowsFixture();
    /** Importing into a workspace that already has the distro reuses it. */
    void powerSourceMatchedByName();

    /** A fixture's 2D layer and group are translated into the target's id
        spaces rather than copied or dropped. */
    void mapLayerAndGroupAreTranslated();
    /** A same-named layer/group in the target is reused, not duplicated. */
    void mapLayerAndGroupMatchedByName();
    /** A nested group brings its ancestry with it. */
    void mapGroupParentChainFollows();

    /** Coverage campaign: references rewritten when ids collide -- a
        collection keeps every member (remaps that chain, 1 to 2 and 2 to
        3, used to drop one), a chaser's steps, an EFX's heads */
    void collectionMembersFollowChainedRemaps();
    void chaserStepsAndEfxHeadsFollowRemaps();

private:
    Doc *m_source;
    Doc *m_target;
};

#endif
