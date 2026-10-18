/*
  Q Light Controller Plus
  scenery.h

  Scenery is a SURFACE on stage rather than something to rig on: a cyclorama,
  a back wall, the main drape, legs and borders, a scrim. Light lands on it,
  and (later) it hides what is behind it. The cyclorama is the first kind; the
  rest share the same geometry -- a vertical sheet with a width, a height and a
  bottom trim, standing along a line on the floor, optionally bowed -- and only
  differ in how they look and behave, so they are one object with a kind.

  Coordinate convention (same as the rest of the 2-D Monitor, see stand.h):
    X = stage left (+) / stage right (-)
    Y = downstage (+) / upstage (-)
    Z = height above stage floor (metres, always >= 0)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SCENERY_H
#define SCENERY_H

#include <QObject>
#include <QVector3D>
#include <QPointF>
#include <QColor>
#include <QString>
#include <QList>
#include <QPair>
#include <climits>

class QXmlStreamReader;
class QXmlStreamWriter;
class QLCFixtureMode;
class Fixture;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLScenery QStringLiteral("Scenery")

class Scenery final : public QObject
{
    Q_OBJECT

public:
    /** What it is. The soft goods are cloth (no thickness); a wall and a
     *  back wall are built, and have one. */
    enum Kind
    {
        Cyclorama = 0,
        BackWall,
        MainDrape,
        Leg,
        Border,
        Traveler,
        Scrim,
        Wall,           ///< a low wall, a planter, a fountain's rim: anything built
        Proscenium      ///< the arch that frames the stage: two piers and a header
    };

    /** The line it stands along on the floor. */
    enum Outline
    {
        SheetOutline = 0,   ///< straight, or bowed by bow()
        RoundedU            ///< a U: a half-round front and straight sides running back
    };

    /** Where a curtain that opens and flies is (main drape, traveler). */
    enum DrapeState
    {
        Closed = 0,
        Open,               ///< split, each half gathered at its end
        Flown               ///< taken up out of sight: not in the way of anything
    };

    static quint32 invalidId() { return UINT_MAX; }
    static QString kindName(int kind);
    static QString stateName(int state);
    /** The kinds in the order they are offered. */
    static QList<int> kinds();
    /** The colour a new one of @p kind starts with. */
    static QColor defaultColor(int kind);

    explicit Scenery(quint32 id, QObject *parent = nullptr);

    quint32 id()   const { return m_id; }
    QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    int  kind() const { return m_kind; }
    void setKind(int k) { m_kind = k; }

    /** The middle of the sheet's bottom edge, on the floor plan (metres). */
    float originX() const { return m_originX; }
    void  setOriginX(float x) { m_originX = x; }
    float originY() const { return m_originY; }
    void  setOriginY(float y) { m_originY = y; }

    float width() const { return m_width; }
    void  setWidth(float w) { m_width = (w > 0.1f) ? w : 0.1f; }
    float height() const { return m_height; }
    void  setHeight(float h) { m_height = (h > 0.1f) ? h : 0.1f; }
    /** Height of the bottom edge above the floor (0 for a cyc to the deck;
     *  more for a border or a flown piece). */
    float bottom() const { return m_bottom; }
    void  setBottom(float z) { m_bottom = (z >= 0.0f) ? z : 0.0f; }

    /** Which way the sheet runs: 0 = along X (stage left-right), facing
     *  downstage. Degrees, turning the same way as everything else on the plot. */
    float rotation() const { return m_rotation; }
    void  setRotation(float deg) { m_rotation = deg; }

    /** How far the middle bows AWAY from the audience (metres): a cyc curves
     *  upstage at its centre. 0 is flat; less than 0 bows toward them. */
    float bow() const { return m_bow; }
    void  setBow(float m) { m_bow = m; }

    int  outline() const { return m_outline; }
    void setOutline(int o) { m_outline = (o == RoundedU) ? RoundedU : SheetOutline; }

    /** A U's reach from its open back to the front of its curve (metres).
     *  The front is a half circle as wide as width(); the sides are what
     *  is left. */
    float depth() const { return m_depth; }
    void  setDepth(float d) { m_depth = (d > 0.05f) ? d : 0.05f; }

    /** How thick it is (metres): 0 for cloth. */
    float thickness() const { return m_thickness; }
    void  setThickness(float t) { m_thickness = (t > 0.0f) ? t : 0.0f; }

    /** Closed, open or flown -- for a curtain that does that (hasState()). */
    int  state() const { return m_state; }
    void setState(int st) { m_state = qBound(int(Closed), st, int(Flown)); }
    /** Whether it is a curtain that opens and flies. */
    bool hasState() const { return m_kind == MainDrape || m_kind == Traveler; }

    /*********************************************************************
     * Driven by a fixture
     *********************************************************************/
    /** Where a curtain is between its fixed states: @p open from 0 (closed)
     *  to 1 (open by its opening), @p fly from 0 (in, hanging at its trim)
     *  to 1 (flown out of sight). Closed, Open and Flown are three of these;
     *  a curtain driven by a fixture can be anywhere in between. */
    struct Pose
    {
        double open = 0.0;
        double fly = 0.0;
        bool operator==(const Pose &o) const { return open == o.open && fly == o.fly; }
        bool operator!=(const Pose &o) const { return !(*this == o); }
    };

    /** The pose its fixed state() stands for. */
    Pose staticPose() const;

    /** The curtain fixture that moves it (Fixture::invalidId() when none):
     *  as that fixture's channels move, the curtain opens and flies -- a
     *  scene, a chaser step or a fader moves it, its fade time is how long
     *  the move takes. Only a curtain (hasState()) is ever driven. */
    quint32 driverFixture() const { return m_driver; }
    void setDriverFixture(quint32 fid) { m_driver = fid; }
    bool isDriven() const { return hasState() && m_driver != invalidId(); }

    /** Which channel of a curtain fixture does what: Open (and its fine
     *  byte) and Fly, by name -- "Open", "Open fine", "Fly"; "Position" is
     *  taken for Open too. -1 when it has none. A fixture with no channel
     *  named for opening opens on its first one. */
    enum CurtainChannel { OpenChannel = 0, OpenFineChannel, FlyChannel };
    static int curtainChannel(const QLCFixtureMode *mode, CurtainChannel which);
    /** Where @p values (a fixture's channel values) put a curtain. */
    static Pose poseFromValues(const QLCFixtureMode *mode, const QByteArray &values);
    /** A curtain fixture: one that lights nothing and has a channel to open
     *  a curtain by (the bundled Generic > Stage Curtain). */
    static bool isCurtainFixture(const Fixture *fx);

    /** Cloth: soft goods, drawn as fabric. */
    bool isCloth() const { return !isBuilt(); }
    /** A scrim: seen through, though it still catches the light thrown on it. */
    bool isSeeThrough() const { return m_kind == Scrim; }
    /** Built rather than hung: it has a thickness. */
    bool isBuilt() const { return m_kind == Wall || m_kind == BackWall || m_kind == Proscenium; }

    /** A proscenium's opening: how wide (metres, centred) and how high from
     *  its bottom. The piers either side are what is left of width(); the
     *  header is what is above the opening. An open curtain's opening is the
     *  gap between its halves, each gathered into what is left at its end. */
    float openingWidth() const { return m_openWidth; }
    void  setOpeningWidth(float w) { m_openWidth = (w > 0.1f) ? w : 0.1f; }
    float openingHeight() const { return m_openHeight; }
    void  setOpeningHeight(float h) { m_openHeight = (h > 0.1f) ? h : 0.1f; }

    /** One solid stretch of it: from @p u0 to @p u1 along its line, from
     *  @p z0 to @p z1 high (absolute). */
    struct Piece
    {
        double u0, u1;
        float z0, z1;
    };
    /** What it is made of: one piece for a sheet or a wall, the gathered
     *  ends of an open curtain, nothing when flown, a proscenium's two piers
     *  and its header. */
    QList<Piece> pieces() const;
    /** ...as it is at @p pose: a curtain part open, part flown. */
    QList<Piece> pieces(const Pose &pose) const;

    QColor color() const { return m_color; }
    void   setColor(const QColor &c) { m_color = c; }

    bool locked() const { return m_locked; }
    void setLocked(bool l) { m_locked = l; }
    quint32 layerId() const { return m_layerId; }
    void setLayerId(quint32 id) { m_layerId = id; }
    quint32 groupId() const { return m_groupId; }
    void setGroupId(quint32 id) { m_groupId = id; }

    /*********************************************************************
     * Geometry
     *********************************************************************/
    /** The unit vector along the sheet (from its stage-right end to its
     *  stage-left end at rotation 0) and the one it FACES, on the floor. */
    QPointF along() const;
    QPointF facing() const;

    /** A point on the sheet: @p u in [-1, 1] from one end to the other,
     *  at height @p z (absolute, metres). The bow pulls the middle back; a
     *  U runs from the end of its stage-right side round the front to the
     *  end of its stage-left one. @p offset moves it off the line along
     *  normalAt() -- a wall's faces are at plus and minus half its thickness. */
    QVector3D pointAt(double u, double z, double offset = 0.0) const;

    /** The way the sheet faces at @p u, on the floor (unit): toward the
     *  audience for a flat one, outward round a U. */
    QPointF normalAt(double u) const;

    /** The stretches of the line it actually occupies, as [from, to] in u:
     *  all of it, both gathered ends of an open curtain, or nothing when
     *  it is flown. */
    QList<QPair<double, double> > spans() const;
    QList<QPair<double, double> > spans(const Pose &pose) const;

    /** How many flat panels draw its whole line: 1 straight, more curved. */
    int segments() const;

    /** The sheet's line on the floor, @p segments + 1 points end to end. */
    QList<QPointF> baseLine(int segments = 16) const;

    /** Where a ray from @p origin along @p dir first meets the sheet, as a
     *  distance along @p dir; false when it misses (or only grazes past
     *  @p maxT). The bowed sheet is taken as @p segments flat panels (more
     *  when it is a U); a wall is met on either face or on its top, and an
     *  open curtain only where it hangs.
     *  @p normal, when given, receives the (horizontal, unit) normal of the
     *  panel hit, turned to face the ray -- the surface a pool lies on.
     *  @p pose, when given, is where a curtain is instead of its state(). */
    bool rayHit(const QVector3D &origin, const QVector3D &dir, double &t,
                double maxT = 1e9, int segments = 16, QVector3D *normal = nullptr,
                const Pose *pose = nullptr) const;

    bool loadXML(QXmlStreamReader &root);
    bool saveXML(QXmlStreamWriter *doc) const;

private:
    quint32 m_id;
    QString m_name;
    int     m_kind     = Cyclorama;
    float   m_originX  = 0.0f;
    float   m_originY  = 0.0f;
    float   m_width    = 10.0f;
    float   m_height   = 6.0f;
    float   m_bottom   = 0.0f;
    float   m_rotation = 0.0f;
    float   m_bow      = 0.0f;
    int     m_outline  = SheetOutline;
    float   m_depth    = 1.0f;
    float   m_thickness = 0.0f;
    int     m_state    = Closed;
    float   m_openWidth = 8.0f;
    float   m_openHeight = 5.0f;
    QColor  m_color;
    bool    m_locked   = false;
    quint32 m_layerId  = 0;
    quint32 m_groupId  = 0;
    quint32 m_driver   = UINT_MAX;
};

/** @} */

#endif
