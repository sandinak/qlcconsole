/*
  Spike: is a software z-buffer a viable replacement for the rig view's
  painter's-algorithm DrawOp queue?

  Measures three things against the SAME scene:
    1. correctness  -- does the far half of a strip survive without epsilons?
    2. cost         -- ms/frame vs QPainter for a realistic primitive count
    3. what breaks  -- antialiasing and translucency

  The scene reproduces the actual bug: a wide "step face" polygon whose depth
  varies across it, with pixel quads painted ON that face. Under one-depth-per-
  primitive the face's average depth beats every pixel past its midpoint.
*/
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPolygonF>
#include <QElapsedTimer>
#include <QVector>
#include <QVector3D>
#include <QtMath>
#include <QDebug>

static int W = 1400, H = 850;

// ---------------------------------------------------------------- projection
// Same orthographic axonometric as StructureStudioView, same depth convention
// (LARGER == NEARER).
static double azDeg = 28.0, elDeg = 22.0;

static QPointF project(const QVector3D &w)
{
    const double A = qDegreesToRadians(azDeg), E = qDegreesToRadians(elDeg);
    const double cA = qCos(A), sA = qSin(A), cE = qCos(E), sE = qSin(E);
    const double a = w.x() * cA - w.y() * sA;
    const double b = -w.x() * sE * sA - w.y() * sE * cA + w.z() * cE;
    return QPointF(a, b);
}

static double viewDepth(const QVector3D &w)
{
    const double A = qDegreesToRadians(azDeg), E = qDegreesToRadians(elDeg);
    return -(w.x() * -qSin(A) * qCos(E) + w.y() * -qCos(A) * qCos(E) + w.z() * -qSin(E));
}

static double gScale = 90.0;
static QPointF w2s(const QVector3D &w)
{
    const QPointF p = project(w);
    return QPointF(W * 0.5 + p.x() * gScale, H * 0.6 - p.y() * gScale);
}

// ------------------------------------------------------------------ z-buffer
/* Depth across a PLANAR polygon is an affine function of screen x,y -- both
   project() and viewDepth() are linear in world space -- so fitting one plane
   z = Ax + By + C from three vertices is EXACT, not an approximation. That is
   the whole trick: no perspective divide, no per-pixel interpolation error. */
struct ZBuffer
{
    QImage colour;
    QVector<float> depth;

    ZBuffer() : colour(W, H, QImage::Format_RGB32), depth(qsizetype(W) * H, -1e30f)
    {
        colour.fill(QColor(24, 26, 32).rgb());
    }

    void clear()
    {
        colour.fill(QColor(24, 26, 32).rgb());
        depth.fill(-1e30f);
    }

    void fillPoly(const QVector<QPointF> &pts, const QVector<double> &zs, QRgb rgb)
    {
        const int n = pts.size();
        if (n < 3)
            return;

        // Plane fit from the first three non-degenerate vertices.
        double A = 0, B = 0, C = zs[0];
        {
            const double x0 = pts[0].x(), y0 = pts[0].y();
            const double x1 = pts[1].x() - x0, y1 = pts[1].y() - y0;
            const double x2 = pts[2].x() - x0, y2 = pts[2].y() - y0;
            const double det = x1 * y2 - x2 * y1;
            if (qAbs(det) > 1e-9)
            {
                const double d1 = zs[1] - zs[0], d2 = zs[2] - zs[0];
                A = (d1 * y2 - d2 * y1) / det;
                B = (x1 * d2 - x2 * d1) / det;
                C = zs[0] - A * x0 - B * y0;
            }
        }

        double minY = pts[0].y(), maxY = minY;
        for (int i = 1; i < n; ++i)
        {
            minY = qMin(minY, pts[i].y());
            maxY = qMax(maxY, pts[i].y());
        }
        int y0 = qMax(0, int(qFloor(minY)));
        int y1 = qMin(H - 1, int(qCeil(maxY)));

        QVector<double> xs;
        for (int y = y0; y <= y1; ++y)
        {
            const double sy = y + 0.5;
            xs.clear();
            for (int i = 0; i < n; ++i)
            {
                const QPointF &a = pts[i], &b = pts[(i + 1) % n];
                if ((a.y() <= sy && b.y() > sy) || (b.y() <= sy && a.y() > sy))
                    xs << a.x() + (sy - a.y()) / (b.y() - a.y()) * (b.x() - a.x());
            }
            if (xs.size() < 2)
                continue;
            std::sort(xs.begin(), xs.end());
            for (int k = 0; k + 1 < xs.size(); k += 2)
            {
                int xa = qMax(0, int(qCeil(xs[k] - 0.5)));
                int xb = qMin(W - 1, int(qFloor(xs[k + 1] - 0.5)));
                if (xb < xa)
                    continue;
                QRgb *line = reinterpret_cast<QRgb *>(colour.scanLine(y));
                float *zline = depth.data() + y * W;
                double z = A * (xa + 0.5) + B * sy + C;
                for (int x = xa; x <= xb; ++x, z += A)
                {
                    if (float(z) > zline[x])
                    {
                        zline[x] = float(z);
                        line[x] = rgb;
                    }
                }
            }
        }
    }
};

// --------------------------------------------------------------------- scene
struct Prim
{
    QVector<QVector3D> world;
    QRgb rgb;
};

/* One step: a wide face, plus pixel quads painted ON it. Repeated to reach a
   realistic primitive count. */
static void buildScene(QVector<Prim> &out, int steps, int pixels)
{
    for (int s = 0; s < steps; ++s)
    {
        const float x0 = s * 2.5f, x1 = x0 + 2.438f;
        const float y = 0.0f, z0 = 0.0f, z1 = 0.45f;

        Prim face;
        face.world << QVector3D(x0, y, z0) << QVector3D(x1, y, z0)
                   << QVector3D(x1, y, z1) << QVector3D(x0, y, z1);
        face.rgb = qRgb(150, 55, 25);
        out << face;

        for (int p = 0; p < pixels; ++p)
        {
            const float t0 = float(p) / pixels, t1 = float(p + 1) / pixels;
            const float px0 = x0 + (x1 - x0) * t0 + 0.004f;
            const float px1 = x0 + (x1 - x0) * t1 - 0.004f;
            const float pz = 0.20f, ph = 0.03f;
            Prim q;
            /* One millimetre proud of the face -- +Y is toward the eye in this
               projection. This is the hard case: a real 1 mm offset, far
               smaller than the depth the face itself spans across its width. */
            q.world << QVector3D(px0, y + 0.001f, pz)
                    << QVector3D(px1, y + 0.001f, pz)
                    << QVector3D(px1, y + 0.001f, pz + ph)
                    << QVector3D(px0, y + 0.001f, pz + ph);
            q.rgb = qRgb(255, 40, 40);
            out << q;
        }
    }
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    const int STEPS = (argc > 2) ? atoi(argv[2]) : 12;
    const int PIXELS = (argc > 3) ? atoi(argv[3]) : 64;
    gScale = (W * 0.86) / (STEPS * 2.5 * qCos(qDegreesToRadians(azDeg)));
    // (the real rig's angled frame measured 1786 primitives)
    QVector<Prim> scene;
    buildScene(scene, STEPS, PIXELS);
    qInfo() << "primitives:" << scene.size();

    // ---- 1. painter's algorithm, one depth per primitive (what we do today)
    QImage painterImg(W, H, QImage::Format_RGB32);
    QElapsedTimer t;
    const int N = 20;

    struct Op { double depth; QPolygonF poly; QRgb rgb; };
    QVector<Op> ops;
    ops.reserve(scene.size());
    for (const Prim &p : scene)
    {
        Op o;
        double d = 0;
        for (const QVector3D &w : p.world) { o.poly << w2s(w); d += viewDepth(w); }
        o.depth = d / p.world.size();      // ONE depth for the whole polygon
        o.rgb = p.rgb;
        ops << o;
    }

    t.start();
    for (int i = 0; i < N; ++i)
    {
        painterImg.fill(QColor(24, 26, 32).rgb());
        QPainter pnt(&painterImg);
        pnt.setRenderHint(QPainter::Antialiasing, true);
        QVector<Op> sorted = ops;
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const Op &a, const Op &b) { return a.depth < b.depth; });
        pnt.setPen(Qt::NoPen);
        for (const Op &o : sorted) { pnt.setBrush(QColor(o.rgb)); pnt.drawPolygon(o.poly); }
    }
    const double painterMs = t.nsecsElapsed() / 1e6 / N;

    // Same again with antialiasing OFF -- the z-buffer has none, so this is the
    // like-for-like number.
    t.restart();
    for (int i = 0; i < N; ++i)
    {
        painterImg.fill(QColor(24, 26, 32).rgb());
        QPainter pnt(&painterImg);
        pnt.setRenderHint(QPainter::Antialiasing, false);
        QVector<Op> sorted = ops;
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const Op &a, const Op &b) { return a.depth < b.depth; });
        pnt.setPen(Qt::NoPen);
        for (const Op &o : sorted) { pnt.setBrush(QColor(o.rgb)); pnt.drawPolygon(o.poly); }
    }
    const double painterNoAaMs = t.nsecsElapsed() / 1e6 / N;

    // ---- 2. software z-buffer, per-vertex depth
    ZBuffer zb;
    QVector<QVector<QPointF> > zpts;
    QVector<QVector<double> > zzs;
    for (const Prim &p : scene)
    {
        QVector<QPointF> sp; QVector<double> sz;
        for (const QVector3D &w : p.world) { sp << w2s(w); sz << viewDepth(w); }
        zpts << sp; zzs << sz;
    }

    t.restart();
    for (int i = 0; i < N; ++i)
    {
        zb.clear();
        for (int k = 0; k < scene.size(); ++k)
            zb.fillPoly(zpts[k], zzs[k], scene[k].rgb);
    }
    const double zbufMs = t.nsecsElapsed() / 1e6 / N;

    // ---- 3. correctness: how many pixel quads actually survive?
    auto redRuns = [](const QImage &img) {
        int cols = 0;
        for (int x = 0; x < W; ++x)
        {
            bool any = false;
            for (int y = 0; y < H && !any; ++y)
            {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() < 90 && c.blue() < 90) any = true;
            }
            if (any) ++cols;
        }
        return cols;
    };

    qInfo() << "painter, AA off     :" << painterNoAaMs << "ms/frame";
    qInfo() << "painter's algorithm :" << painterMs << "ms/frame,"
            << redRuns(painterImg) << "screen columns showing pixels";
    qInfo() << "software z-buffer   :" << zbufMs << "ms/frame,"
            << redRuns(zb.colour) << "screen columns showing pixels";

    painterImg.save(QString(argv[1]) + "/painter.png");
    zb.colour.save(QString(argv[1]) + "/zbuffer.png");

    /* Antialiasing is the one thing a flat z-buffer gives up. The standard
       answer is to supersample: rasterise at 2x and scale down, which is 4x the
       fill. Cost it rather than assume it. */
    {
        const int w1 = W, h1 = H;
        const double s1 = gScale;
        W *= 2; H *= 2; gScale *= 2.0;
        ZBuffer big;
        QVector<QVector<QPointF> > bp; QVector<QVector<double> > bz;
        for (const Prim &p : scene)
        {
            QVector<QPointF> sp; QVector<double> sz;
            for (const QVector3D &w : p.world) { sp << w2s(w); sz << viewDepth(w); }
            bp << sp; bz << sz;
        }
        QElapsedTimer t2; t2.start();
        QImage out;
        for (int i = 0; i < N; ++i)
        {
            big.clear();
            for (int k = 0; k < scene.size(); ++k)
                big.fillPoly(bp[k], bz[k], scene[k].rgb);
            out = big.colour.scaled(w1, h1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        qInfo() << "z-buffer 2x SSAA    :" << (t2.nsecsElapsed() / 1e6 / N)
                << "ms/frame (antialiased)";
        out.save(QString(argv[1]) + "/zbuffer_ssaa.png");
        W = w1; H = h1; gScale = s1;
    }
    return 0;
}
