/*
  Q Light Controller Plus
  beamocclusion.h

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

/*
  Line of sight: can a head's beam get to the point it is aimed at, or is
  something on stage in the way?

  The stage's solid features -- drapes and travelers as they hang now (a
  closed panel blocks, the open gap does not, a flown one is out of the way),
  walls and the proscenium, platform and riser sides and decks, towers, and
  trusses / pipes / stands thick enough to matter -- are turned once into flat
  panels and rods, each with a bounding box, and kept until the stage changes.
  A query is then plain arithmetic over that list, cheap enough to run for
  every head on every tick.

  The model is immutable once built and shared by pointer, so the DMX thread
  (the follow spot, an Aim look dimming) and the GUI (reach warnings, aim
  traces, the rig views) can all hold one at once; MonitorProperties keeps the
  current one and rebuilds it, under a lock, when the stage's geometry changes
  (see MonitorProperties::occlusionModel()).

  Coordinates as everywhere else: X stage left (+), Y downstage (+), Z up,
  metres.

  A live drape (a curtain driven open and closed by a fixture channel) needs
  nothing here: the model is built from Scenery::pieces(), and the stage
  signature is taken from the panels it yields, so whatever moves the pieces
  -- the drape's state today, a live pose later -- rebuilds the model on the
  next query.
*/

#ifndef BEAMOCCLUSION_H
#define BEAMOCCLUSION_H

#include <QSharedPointer>
#include <QVector3D>
#include <QVector>
#include <QString>
#include <QMutex>
#include <climits>

class MonitorProperties;

namespace BeamOcclusion
{
    /** What can stand in a beam's way. Flags: MonitorProperties keeps which
     *  of these count (occlusionKinds()). */
    enum Kind
    {
        Drapes    = 1 << 0,    ///< cyc, main drape, traveler, legs, borders (not a scrim)
        Walls     = 1 << 1,    ///< back wall, a wall, the proscenium
        Platforms = 1 << 2,    ///< platforms, risers and steps: their sides and solid decks
        Towers    = 1 << 3,    ///< towers
        Trusses   = 1 << 4,    ///< truss runs (and truss towers), thicker than minSize
        Pipes     = 1 << 5,    ///< booms and pipes, thicker than minSize
        Stands    = 1 << 6     ///< stand poles, thicker than minSize
    };

    /** Every kind, in the order they are offered. */
    QList<int> kinds();
    /** The kinds that count unless the stage says otherwise: everything --
     *  a pipe or a stand pole is thinner than the default minimum size, so
     *  it does not get in the way until that is lowered. */
    int defaultKinds();
    /** Thinner than this (metres) a truss, pipe or stand does not block: a
     *  beam is wider than a 48 mm pipe and lights round it. */
    float defaultMinSize();
    /** A kind's name, for a menu or a warning. */
    QString kindName(int kind);

    /** Something left out of a query: the structure a head is mounted on
     *  cannot block it. */
    struct Exclude
    {
        int kind;
        quint32 id;
    };

    /** What a beam meets on the way to its point. */
    struct Hit
    {
        bool blocked = false;
        int kind = 0;                 ///< the blocker's Kind, when blocked
        quint32 id = UINT_MAX;        ///< the blocker's own id (scenery, platform, ...)
        QString name;                 ///< what to call it: "Main drape", "Riser 2"
        float fraction = 1.0f;        ///< 0..1 of the way from the head where it stops
        float distance = 0.0f;        ///< metres from the head to where it stops
        /** How close the beam passes the nearest solid feature it goes by, in
         *  metres -- positive: clear by that much; negative: blocked, that far
         *  in from the blocker's edge. Capped at clearanceReach(). */
        float clearance = 0.0f;
        QVector3D point;              ///< where it stops (the aim point when clear)
    };

    /** Clearance is not looked for further out than this (metres). */
    float clearanceReach();

    class Model
    {
    public:
        /** The beam from @p from (a head's beam origin) to @p to (its aim
         *  point). The first stretch at the head (its own yoke and clamp) and
         *  the last few centimetres (the deck a target stands on) are not
         *  tested. */
        Hit segment(const QVector3D &from, const QVector3D &to,
                    const QVector<Exclude> &exclude = QVector<Exclude>()) const;
        /** A beam fired from @p origin along unit @p dir, up to @p maxT
         *  metres: the first solid feature it meets, for a view drawing it. */
        Hit ray(const QVector3D &origin, const QVector3D &dir, float maxT,
                const QVector<Exclude> &exclude = QVector<Exclude>()) const;

        int blockerCount() const { return m_blockers.size(); }
        bool isEmpty() const { return m_blockers.isEmpty(); }
        quint64 signature() const { return m_signature; }

        /* Building (MonitorProperties does this; tests may too). */
        struct Panel        ///< c + a*e1 + b*e2, a and b in 0..1; e1 and e2 square
        {
            QVector3D c, e1, e2, n;
            float l1sq, l2sq;
            int blocker;
        };
        struct Rod          ///< a cylinder round p0..p1
        {
            QVector3D p0, p1;
            float r;
            int blocker;
        };
        struct Blocker
        {
            int kind;
            quint32 id;
            QString name;
            QVector3D lo, hi;           ///< bounding box
            int firstPanel, panels, firstRod, rods;
        };
        void beginBlocker(int kind, quint32 id, const QString &name);
        void addPanel(const QVector3D &c, const QVector3D &e1, const QVector3D &e2);
        void addRod(const QVector3D &p0, const QVector3D &p1, float r);
        void endBlocker();
        void finish();

    private:
        Hit test(const QVector3D &a, const QVector3D &b, float t0, float t1,
                 const QVector<Exclude> &exclude) const;

        QVector<Blocker> m_blockers;
        QVector<Panel> m_panels;
        QVector<Rod> m_rods;
        quint64 m_signature = 0;
    };

    typedef QSharedPointer<const Model> ModelPtr;

    /** Build the model of @p mp's stage: the features of @p kinds, trusses,
     *  pipes and stands only when at least @p minSize thick. */
    ModelPtr build(const MonitorProperties *mp, int kinds, float minSize);

    /** What head @p fixtureId is mounted on -- its truss (and the truss that
     *  carries that), pipe, tower, the structure its hanger is on, the deck
     *  or riser it is fixed to, or the platform a free head stands on. */
    QVector<Exclude> ownMounts(const MonitorProperties *mp, quint32 fixtureId);

    /** The current model for a stage, rebuilt only when the stage changed.
     *  Thread-safe. */
    class Cache
    {
    public:
        ModelPtr get(const MonitorProperties *mp, int kinds, float minSize);
        void clear();

    private:
        QMutex m_mutex;
        ModelPtr m_model;
        quint64 m_signature = 0;
    };
}

#endif
