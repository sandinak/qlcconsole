/*
  Q Light Controller Plus
  blindeffects.h

  The Blind view's effects (docs/design/LIVE_BLIND.md): what the effects
  of the cue in edit would do -- a dimmer pulsing, a chase, a colour
  change, a mover sweeping -- played off the stage, for the rig views
  only. The same effect runtime the stage uses (EffectInstance), one per
  effect look as EffectScriptRunner makes them, but private: never a
  DMXSource, never in the runner, read by its own clock (advance()).
  Nothing it does reaches a universe.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef BLINDEFFECTS_H
#define BLINDEFFECTS_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

#include "qlcengine_export.h"

class Doc;
class EffectInstance;

class QLC_ENGINE_EXPORT BlindEffects
{
public:
    explicit BlindEffects(Doc *doc);
    ~BlindEffects();

    /** Play the effects of Show Tree cue @p cueId (its scenes' effect
     *  looks). The same cue with the same effects keeps them and their
     *  time, and takes any edit to their settings; another cue, or an
     *  effect added, removed or changed for another, starts them again at
     *  0. An invalid cue, or one with no effect: none. Main thread. */
    void setCue(quint32 cueId);
    /** The same, for a list of scenes (what setCue() plays). */
    void setScenes(const QList<quint32> &sceneIds);
    /** Let every effect go. */
    void clear();

    /** Whether the cue has an effect to play. */
    bool hasEffects() const { return !m_instances.isEmpty(); }
    /** Its clock: ms since the effects started. */
    qint64 clockMs() const { return m_clockMs; }

    /** Move its clock on by @p ms and run every effect once at that time.
     *  Main thread (the scripts' engines). */
    void advance(qint64 ms);

    /** @p levels (fixture id -> one byte per channel, the cue's own) with
     *  the effects' last frame over them, laid as the stage lays them:
     *  layered like EffectScriptRunner::composeWrites(), an intensity the
     *  higher of the two unless the effect owns the channel, the rest the
     *  effect's. */
    QHash<quint32, QByteArray> apply(QHash<quint32, QByteArray> levels) const;

private:
    /** What plays: each scene's effect looks and their scripts. */
    QString signatureOf(const QList<quint32> &sceneIds) const;

    Doc *m_doc;
    QList<EffectInstance *> m_instances;
    QString m_signature;
    qint64 m_clockMs = 0;
};

#endif // BLINDEFFECTS_H
