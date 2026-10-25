/*
  Q Light Controller Plus - Unit test
  lookstack_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef LOOKSTACK_TEST_H
#define LOOKSTACK_TEST_H

#include <QTemporaryDir>
#include <QObject>
#include <QColor>

class QLCFixtureDef;
class QLCPalette;
class Fixture;
class Doc;

/* How a look's list stacks, end to end: static looks, effects, and what
   belongs to which, on four kinds of fixture, through the scene, the effect
   runner and the faders to DMX. The rules are spelled out in
   docs/features/effects.md ("How a look stacks"). */
class LookStack_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void staticLooksTheLowerWins();
    void aColourEffectTakesTheLooksColour();
    void aColourUnderAnEffectIsThatEffectsOnly();
    void theLowerOfTwoColourEffectsWins();
    void aColourEffectOverridesTheStaticColour();
    void dimmerEffectsScaleAndMultiply();
    void aMovementEffectKeepsTheLooksColourAndDimmer();
    void aColourOrDimmerUnderAMovementEffectIsItsOwn();
    void aDimmerUnderAMovementEffectIsNotTheLooks();
    void anAimOrPanTiltAfterAnEffectIsStillTheLooks();
    void theLowerOfTwoMovementEffectsWins();
    void theLooksOwnColoursFeedEveryEffectWithoutItsOwn();
    void aDimmerLookDimsAFixtureWithNoDimmer();
    void colourOneAndTwoGoToAFixturesTwoColourSets();
    void theSameOnAFixtureGroup();
    /** A look on a group follows the group live: a fixture put in it while
     *  the look plays takes the look on the next tick, without the scene
     *  being touched, and one taken out lets go of it. */
    void aLookOnAGroupFollowsItsMembersLive();
    void aGroupMemberFadesByTheScenesFades();
    /** ...and what the scene holds on that fixture itself (not through the
     *  group) is the scene's still: it stays when the fixture leaves. */
    void aLeavingMembersOwnValuesStay();
    void editingALookWhileItPlaysKeepsTheOrder();
    void aPaletteTriedOnShowsByTheSameRules();
    void theCheckerSaysWhatWontWork();

private:
    /* One look of these palettes, played; the DMX of universe 1 after. */
    QByteArray play(const QList<QLCPalette *> &stack, bool asGroup = false);
    QByteArray tickAndRead();
    class Scene *m_playing = nullptr;
    QLCPalette *colour(const QColor &c);
    QLCPalette *dimmer(int value);
    QLCPalette *panTilt(double pan, double tilt);
    QLCPalette *effect(const QString &file);
    QColor rgbOf(const QByteArray &dmx, const Fixture *fx) const;
    int dimmerOf(const QByteArray &dmx, const Fixture *fx) const;
    int wheelOf(const QByteArray &dmx, const Fixture *fx) const;
    int panOf(const QByteArray &dmx, const Fixture *fx) const;
    int panValue(Fixture *fx, double degrees) const;

    QTemporaryDir m_scripts;
    Doc *m_doc = nullptr;
    Fixture *m_par = nullptr;       //!< R G B, master dimmer
    Fixture *m_batten = nullptr;    //!< R G B W, no dimmer
    Fixture *m_wheel = nullptr;     //!< pan tilt, colour wheel, its head's own dimmer
    Fixture *m_mover = nullptr;     //!< pan tilt, R G B, master dimmer
    Fixture *m_twoSet = nullptr;    //!< two R G B sets in one head, no dimmer
};

#endif
