/*
  Q Light Controller Plus
  scenetransition.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SCENETRANSITION_H
#define SCENETRANSITION_H

#include <QStringList>
#include <QHash>

class QXmlStreamReader;
class QXmlStreamWriter;
class Fixture;
class Doc;

/** @addtogroup engine_functions Functions
 * @{
 */

/**
 * How a whole look arrives (or leaves): every fixture head reaches it at its
 * own moment -- a wipe across the stage, out from the centre, a random
 * dissolve, along each bar -- instead of everything fading together.
 *
 * Each head has a place in the transition, 0 (first) to 1 (last), from where
 * it is on the stage (TransitionMap). Over @c time ms, the first starts at
 * once and the last at the end; each fades over the @c softness share of the
 * time, so 1 is everything together and a small value is a hard edge.
 */
struct SceneTransition
{
    enum Shape
    {
        None = 0,           ///< the scene's own fade, as always
        Fade,               ///< everything together, over the transition's time
        WipeToStageLeft,    ///< from stage right across to stage left
        WipeToStageRight,
        WipeUp,             ///< from the floor up
        WipeDown,
        WipeUpstage,        ///< from downstage (the front) to the back
        WipeDownstage,
        FromCentre,         ///< out from the middle of the look's fixtures
        ToCentre,           ///< in from the edges
        Dissolve,           ///< heads one by one, in a random order
        AlongBars           ///< each bar drawn on along its length, all at once
    };

    Shape shape = None;
    uint time = 2000;       ///< ms the whole transition takes
    double softness = 0.35; ///< 0..1: each head's own fade, as a share of it

    bool isActive() const { return shape != None && time > 0; }

    /** When a head at @p place (0..1) starts (@p delay ms) and how long it
     *  takes (@p fade ms). */
    void timing(double place, uint &delay, uint &fade) const;
    /** How far through its own fade a head at @p place is, @p elapsed ms
     *  into the transition: 0 (not reached) to 1 (there). */
    double progress(double place, uint elapsed) const;

    static QString shapeToString(Shape shape);
    static Shape stringToShape(const QString &str);
    /** For the UI, in Shape order. */
    static QStringList shapeLabels();

    bool operator==(const SceneTransition &o) const
    { return shape == o.shape && time == o.time && qFuzzyCompare(softness + 1.0, o.softness + 1.0); }
    bool operator!=(const SceneTransition &o) const { return !(*this == o); }

    /** <Transition Kind="In" Shape="WipeUp" Time="2000" Soft="0.35"/>,
     *  written only when active. */
    void saveXML(QXmlStreamWriter *doc, const QString &kind) const;
    /** From a <Transition> element; @p kind gets its Kind. */
    bool loadXML(QXmlStreamReader &root, QString &kind);
};

/**
 * Where each head of a look's fixtures falls in a transition: computed once
 * for the fixtures and the shape, from where they are on the stage
 * (RigHeads::headPositions()).
 */
class TransitionMap
{
public:
    TransitionMap() {}
    TransitionMap(Doc *doc, const QList<quint32> &fixtures, SceneTransition::Shape shape);

    /** Head @p head of fixture @p fid's place, 0..1; @p head < 0 for the
     *  fixture as a whole (the mean of its heads). 0 when unknown. */
    double place(quint32 fid, int head) const;
    /** The place of the head that fixture @p fx's channel @p channel belongs
     *  to -- the fixture as a whole for one in no head (a master dimmer). */
    double placeForChannel(const Fixture *fx, quint32 channel) const;

    bool isEmpty() const { return m_places.isEmpty(); }

private:
    static quint64 key(quint32 fid, int head) { return (quint64(fid) << 16) | quint64(head & 0xFFFF); }
    QHash<quint64, double> m_places;
};

/** @} */

#endif
