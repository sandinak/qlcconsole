/*
  Q Light Controller Plus - Unit test
  blindeffects_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef BLINDEFFECTS_TEST_H
#define BLINDEFFECTS_TEST_H

#include <QObject>
#include <QList>

class Doc;
class Fixture;
class QLCPalette;
class Scene;

class BlindEffects_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A dimmer effect blind: its pulse, frame by frame, on its own clock. */
    void aDimmerEffectPulsesBlind();
    /** A position effect blind: the mover sweeps, the same time the same place. */
    void aPositionEffectMovesBlind();
    /** The stage's frames stay byte for byte what they were while a blind
     *  effect animates, and the stage's effect runner never hears of it. */
    void theStageIsUntouchedWhileBlindEffectsRun();
    /** A Show Tree cue: its effects play; the same cue keeps their time and
     *  takes an edit; a cue with none plays nothing. */
    void aCuesEffectsPlayAndKeepTheirTime();

private:
    Fixture *mover();
    QLCPalette *effect(const QString &script);
    Scene *sceneWith(const QList<quint32> &fixtures, quint32 paletteId);
    /** One stage frame: the timer ticks and universe 0 is written. */
    QByteArray stageFrame();

    Doc *m_doc = nullptr;
    QList<quint32> m_dimmers;
};

#endif
