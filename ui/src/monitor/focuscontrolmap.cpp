/*
  Q Light Controller Plus
  focuscontrolmap.cpp

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

#include <QRegularExpression>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include "focuscontrolmap.h"
#include "focussession.h"

namespace
{
using R = GamepadRole::Role;

/* The controller is drawn in its own coordinates: this box, scaled to fit
   between the two columns of labels. */
const QRectF kPad(240, 36, 320, 290);
/* The keyboard: one box, everything in it (labels too) scaled together. */
const QRectF kKeys(0, 0, 724, 290);

const qreal kStickTravel = 11.0;

/* The usual name of a control, when the profile gives none. */
QString defaultName(int role)
{
    switch (role)
    {
        case R::A: return QStringLiteral("A");
        case R::B: return QStringLiteral("B");
        case R::X: return QStringLiteral("X");
        case R::Y: return QStringLiteral("Y");
        case R::LB: return QStringLiteral("LB");
        case R::RB: return QStringLiteral("RB");
        case R::Back: return QStringLiteral("View");
        case R::Start: return QStringLiteral("Menu");
        case R::LS: return QStringLiteral("LS");
        case R::RS: return QStringLiteral("RS");
        case R::Guide: return QStringLiteral("Guide");
        case R::DpadUp: return QStringLiteral("D-pad up");
        case R::DpadDown: return QStringLiteral("D-pad down");
        case R::DpadLeft: return QStringLiteral("D-pad left");
        case R::DpadRight: return QStringLiteral("D-pad right");
        case R::LeftX: case R::LeftY: return QStringLiteral("Left stick");
        case R::RightX: case R::RightY: return QStringLiteral("Right stick");
        case R::LeftTrigger: return QStringLiteral("LT");
        case R::RightTrigger: return QStringLiteral("RT");
        default: return QString();
    }
}

/* What fits on the control itself: the profile's name when it is short
   ("A", "L1"), a PlayStation shape by its name, else the place's usual
   letters for a shoulder button or the name's initial for a face button. */
QString glyphText(int role, const QString &name)
{
    if (!name.isEmpty() && name.size() <= 2)
        return name;
    QString squashed;
    for (const QChar c : name)
        if (c.isLetterOrNumber())
            squashed += c.toLower();
    if (squashed == QLatin1String("cross"))
        return QString::fromUtf8("\342\234\225");
    if (squashed == QLatin1String("circle"))
        return QString::fromUtf8("\342\227\213");
    if (squashed == QLatin1String("square"))
        return QString::fromUtf8("\342\226\241");
    if (squashed == QLatin1String("triangle"))
        return QString::fromUtf8("\342\226\263");
    switch (role)
    {
        case R::LB: case R::RB: case R::LeftTrigger: case R::RightTrigger:
            return defaultName(role);
        case R::A: case R::B: case R::X: case R::Y:
            return name.isEmpty() ? defaultName(role) : name.left(1).toUpper();
        default:
            return QString();
    }
}

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

/* @p a, @p f of the way to @p b: the shades between text and background,
   which read in every theme where the palette's disabled roles may not. */
QColor blend(const QColor &a, const QColor &b, qreal f)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * f),
                            float(a.greenF() + (b.greenF() - a.greenF()) * f),
                            float(a.blueF() + (b.blueF() - a.blueF()) * f));
}

QString ctrlKeyName()
{
#if defined(Q_OS_MACOS)
    return QString::fromUtf8("\342\214\230");    // the command sign
#else
    return QStringLiteral("Ctrl");
#endif
}
}

FocusControlMap::FocusControlMap(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("focusControlMap"));
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FocusControlMap::setMode(Mode mode)
{
    if (mode == m_mode)
        return;
    m_mode = mode;
    m_hover = -1;
    updateGeometry();
    update();
}

void FocusControlMap::setControls(const QMap<int, QString> &controls)
{
    m_controls = controls;
    update();
}

void FocusControlMap::setStepDegrees(float step)
{
    m_step = step;
    update();
}

void FocusControlMap::setControlValue(int role, float value)
{
    switch (role)
    {
        case R::LeftX:  m_axis[0] = value; break;
        case R::LeftY:  m_axis[1] = value; break;
        case R::RightX: m_axis[2] = value; break;
        case R::RightY: m_axis[3] = value; break;
        default:
            if (value > 0.0f)
                m_lit.insert(role);
            else
                m_lit.remove(role);
            break;
    }
    update();
}

int FocusControlMap::canonicalKey(int key)
{
    switch (key)
    {
        case Qt::Key_Enter:     return Qt::Key_Return;
        case Qt::Key_Delete:    return Qt::Key_Backspace;
        case Qt::Key_Tab:       return Qt::Key_BracketRight;
        case Qt::Key_Backtab:   return Qt::Key_BracketLeft;
        case Qt::Key_Equal:     return Qt::Key_Plus;
        case Qt::Key_Underscore: return Qt::Key_Minus;
        case Qt::Key_T:         return Qt::Key_R;      // the target page, as R
        case Qt::Key_Less:      return Qt::Key_Comma;
        case Qt::Key_Greater:   return Qt::Key_Period;
        case Qt::Key_Colon:     return Qt::Key_Semicolon;
        case Qt::Key_QuoteDbl:  return Qt::Key_Apostrophe;
        default:                return key;
    }
}

void FocusControlMap::setKeyDown(int key, bool down)
{
    key = canonicalKey(key);
    if (down == m_keys.contains(key))
        return;
    if (down)
        m_keys.insert(key);
    else
        m_keys.remove(key);
    update();
}

void FocusControlMap::clearInput()
{
    m_lit.clear();
    m_keys.clear();
    for (float &a : m_axis)
        a = 0.5f;
    update();
}

void FocusControlMap::setTool(ControllerScheme::Tool tool)
{
    if (tool == m_tool)
        return;
    m_tool = tool;
    update();
}

QString FocusControlMap::actionText(int role) const
{
    switch (role)
    {
        case R::LeftX: case R::LeftY:
            return ControllerScheme::stickLabel(m_tool, false);
        case R::RightX: case R::RightY:
            return ControllerScheme::stickLabel(m_tool, true);
        case R::DpadUp:
            // The D-pad's one callout reads its up arm: say all four.
            /* Up is in, down is out; right tightens the focus, left loosens
               (in Refine, LS's own label says the D-pad also pages through
               colour and gobo: here it would crowd the pad off the map). */
            return tr("\342\226\262Spot down \342\226\274Flood out  \342\227\200Softer Sharper\342\226\266");
        case R::DpadDown:
            return tr("Flood out");
        case R::DpadLeft:
            return tr("Softer");
        case R::DpadRight:
            return tr("Sharper");
        default:
            break;
    }
    // Everything else: the one layout, in this tool's words.
    return ControllerScheme::label(m_tool, ControllerScheme::verbFor(GamepadRole::Role(role)));
}

QString FocusControlMap::controlName(int role) const
{
    const QString n = m_controls.value(role);
    return n.isEmpty() ? defaultName(role) : n;
}

bool FocusControlMap::isMapped(int role) const
{
    return m_controls.contains(role);
}

bool FocusControlMap::isLit(int role) const
{
    return m_lit.contains(role);
}

QPointF FocusControlMap::stickDeflection(bool fine) const
{
    const int i = fine ? 2 : 0;
    return QPointF((m_axis[i] - 0.5) * 2.0, (m_axis[i + 1] - 0.5) * 2.0);
}

/* The keys that are a controller verb, so the keyboard map says what the
   controller map says, in the tool's words (ControllerScheme::label()). */
static ControllerScheme::Verb keyVerb(int key)
{
    using V = ControllerScheme::Verb;
    switch (key)
    {
        case Qt::Key_BracketLeft:  return V::PreviousItem;
        case Qt::Key_BracketRight: return V::NextItem;
        case Qt::Key_PageUp:       return V::PreviousGroup;
        case Qt::Key_PageDown:     return V::NextGroup;
        case Qt::Key_G:            return V::CycleScope;
        case Qt::Key_Return:       return V::Keep;
        case Qt::Key_Backspace:    return V::Revert;
        case Qt::Key_0:            return V::Reset;
        case Qt::Key_Control:
        case Qt::Key_Z:            return V::Undo;
        case Qt::Key_Escape:       return V::Exit;
        case Qt::Key_R:            return V::Alternate;
        case Qt::Key_W:            return V::WhereAmI;
        case Qt::Key_Comma:        return V::ZoomNarrower;
        case Qt::Key_Period:       return V::ZoomWider;
        case Qt::Key_Semicolon:    return V::FocusNearer;
        case Qt::Key_Apostrophe:   return V::FocusFarther;
        default:                   return V::NoVerb;
    }
}

QString FocusControlMap::keyAction(int key) const
{
    key = canonicalKey(key);
    const ControllerScheme::Verb verb = keyVerb(key);
    if (verb != ControllerScheme::NoVerb)
        return ControllerScheme::label(m_tool, verb);
    switch (key)
    {
        case Qt::Key_Minus:        return tr("Step \303\26710");
        case Qt::Key_Plus:         return tr("Step \303\22710");
        case Qt::Key_Shift:        return tr("With arrows: 1\302\260");
        case Qt::Key_D:            return tr("Mark dead");
        case Qt::Key_H:            return tr("Hold every light");
        case Qt::Key_U:            return tr("Next unsaved light");
        case Qt::Key_N:            return tr("Say how many are unsaved");
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
            return tr("Nudge %1\302\260").arg(double(m_step), 0, 'g', 3);
        default:
            return QString();
    }
}

QList<int> FocusControlMap::clickable() const
{
    QList<int> ids;
    for (const Hit &h : m_hits)
        if (h.id != -1)
            ids << h.id;
    return ids;
}

bool FocusControlMap::isKeyLit(int key) const
{
    return m_keys.contains(canonicalKey(key));
}

QSize FocusControlMap::sizeHint() const
{
    return m_mode == ControllerMap ? QSize(640, 340) : QSize(700, 280);
}

QSize FocusControlMap::minimumSizeHint() const
{
    return m_mode == ControllerMap ? QSize(520, 280) : QSize(470, 190);
}

void FocusControlMap::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange)
        update();
    QWidget::changeEvent(event);
}

void FocusControlMap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    m_hits.clear();
    if (m_mode == ControllerMap)
        paintController(p);
    else
        paintKeyboard(p);
}

/*****************************************************************************
 * The controller
 *****************************************************************************/

void FocusControlMap::paintController(QPainter &p)
{
    const QPalette &pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor bg = pal.color(QPalette::Window);
    const QColor dim = blend(bg, text, 0.38);       // greyed: not on this controller
    const QColor nameCol = blend(bg, text, 0.62);   // a control's own name
    const QColor hi = pal.color(QPalette::Highlight);
    const QColor hiText = pal.color(QPalette::HighlightedText);
    const QColor bodyFill = blend(pal.color(QPalette::Button), text, 0.07);
    const QColor face = pal.color(QPalette::Base);

    QFont actionFont = font();
    actionFont.setBold(true);
    QFont nameFont = font();
    nameFont.setPointSizeF(font().pointSizeF() * 0.85);
    const QFontMetricsF afm(actionFont), nfm(nameFont);

    /* The labels: each control Focus mode uses, in a column either side,
       with a line to the control. Row is the label's height in the pad's
       coordinates; elbow, when set, is where the line turns. */
    struct Callout
    {
        QList<int> roles;          ///< the control(s) it names
        QPointF anchor;            ///< on the control
        QPointF elbow;             ///< (0,0): straight across
        qreal row;
        bool left;
        QString action;
        QString name;
        bool mapped;
    };
    auto callout = [this](QList<int> roles, QPointF anchor, QPointF elbow, qreal row,
                          bool left, const QString &name = QString()) {
        Callout c;
        c.roles = roles;
        c.anchor = anchor;
        c.elbow = elbow;
        c.row = row;
        c.left = left;
        c.action = actionText(roles.first());
        c.mapped = false;
        for (int r : roles)
            c.mapped = c.mapped || isMapped(r);
        c.name = name.isEmpty() ? controlName(roles.first()) : name;
        if (!c.mapped)
            c.name = tr("%1 (not on this controller)").arg(c.name);
        return c;
    };
    // The D-pad's own name: the profile's for "up", without the direction.
    QString dpadName = m_controls.value(R::DpadUp);
    if (dpadName.isEmpty() || dpadName.compare(QLatin1String("Up"), Qt::CaseInsensitive) == 0)
        dpadName = tr("D-pad");
    else
        dpadName = dpadName.remove(QRegularExpression(QStringLiteral("\\s*up$"),
                                                      QRegularExpression::CaseInsensitiveOption));

    // A stick's name, without the axis ("Left Stick X" is the left stick).
    auto stickName = [this](int xRole, const QString &fallback) {
        QString n = m_controls.value(xRole);
        n.remove(QRegularExpression(QStringLiteral("[\\s_-]*[Xx]$")));
        return n.isEmpty() ? fallback : n;
    };
    const QPointF ls(305, 165), rs(428, 240), dpad(365, 240), abxy(495, 165);
    const QPointF view(372, 150), menu(428, 150), guide(400, 122);
    const qreal fb = 28.0;   // face-button spacing

    QList<Callout> callouts;
    callouts << callout({ R::LeftTrigger }, QPointF(270, 60), QPointF(), 48, true)
             << callout({ R::LB }, QPointF(258, 94), QPointF(), 80, true)
             << callout({ R::Back }, view - QPointF(0, 6), QPointF(372, 116), 116, true)
             << callout({ R::LeftX, R::LeftY }, ls - QPointF(30, 0), QPointF(), 165, true,
                        stickName(R::LeftX, tr("Left stick")))
             << callout({ R::DpadUp, R::DpadDown, R::DpadLeft, R::DpadRight },
                        dpad - QPointF(30, 0), QPointF(), 248, true, dpadName)
             << callout({ R::LS }, ls + QPointF(0, 22), QPointF(305, 206), 206, true)
             << callout({ R::RightTrigger }, QPointF(530, 60), QPointF(), 48, false)
             << callout({ R::RB }, QPointF(542, 94), QPointF(), 80, false)
             << callout({ R::Start }, menu - QPointF(0, 6), QPointF(428, 108), 108, false)
             << callout({ R::Y }, abxy + QPointF(13, -fb), QPointF(), 140, false)
             << callout({ R::B }, abxy + QPointF(fb + 13, 0), QPointF(), 172, false)
             << callout({ R::A }, abxy + QPointF(13, fb), QPointF(), 204, false)
             << callout({ R::X }, abxy + QPointF(-fb, 13), QPointF(abxy.x() - fb, 226), 236, false)
             << callout({ R::RightX, R::RightY }, rs + QPointF(24, 16), QPointF(), 270, false,
                        stickName(R::RightX, tr("Right stick")))
             << callout({ R::RS }, rs + QPointF(0, 22), QPointF(428, 302), 302, false);

    // Room for the label columns, then the pad as large as fits between.
    qreal leftW = 0, rightW = 0;
    for (const Callout &c : callouts)
    {
        const qreal w = qMax(afm.horizontalAdvance(c.action), nfm.horizontalAdvance(c.name));
        (c.left ? leftW : rightW) = qMax(c.left ? leftW : rightW, w);
    }
    const qreal gap = 8, margin = 6;
    const qreal availW = width() - leftW - rightW - 2 * (gap + margin);
    const qreal availH = height() - 2 * margin;
    const qreal s = qBound(0.3, qMin(availW / kPad.width(), availH / kPad.height()), 1.8);
    const qreal total = leftW + gap + kPad.width() * s + gap + rightW;
    const qreal x0 = (width() - total) / 2.0;
    const qreal y0 = (height() - kPad.height() * s) / 2.0;

    QTransform t;
    t.translate(x0 + leftW + gap, y0);
    t.scale(s, s);
    t.translate(-kPad.left(), -kPad.top());

    auto addHit = [this, &t](const QPainterPath &shape, int role, const QString &tip) {
        m_hits << Hit{ t.map(shape), role, tip };
    };
    const QString hoverTip = tr("Click to do it");

    // The body: a slab with two grips.
    QPainterPath body;
    body.addRoundedRect(QRectF(250, 98, 300, 150), 64, 64);
    for (int side = -1; side <= 1; side += 2)
    {
        QPainterPath grip;
        grip.addEllipse(QPointF(0, 0), 46, 68);
        QTransform gt;
        gt.translate(400 + side * 110, 250);
        gt.rotate(-side * 22);
        body = body.united(gt.map(grip));
    }
    body = body.simplified();
    p.setPen(QPen(withAlpha(text, 130), 1.5));
    p.setBrush(bodyFill);
    p.drawPath(t.map(body));

    // The lines, over the body and under the controls.
    p.save();
    for (const Callout &c : callouts)
    {
        if (c.action.isEmpty())
            continue;
        QPen pen(withAlpha(c.mapped ? text : dim, c.mapped ? 120 : 90), 1.0);
        if (!c.mapped)
            pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        const QPointF a = t.map(c.anchor);
        const QPointF end(c.left ? x0 + leftW + gap * 0.5 : x0 + total - rightW - gap * 0.5,
                          t.map(QPointF(0, c.row)).y());
        QPolygonF line;
        line << a;
        if (!c.elbow.isNull())
            line << t.map(c.elbow);
        else
            line << QPointF(a.x() + (c.left ? -6.0 : 6.0) * s, end.y());
        line << end;
        p.drawPolyline(line);
        p.setBrush(c.mapped ? text : dim);
        p.setPen(Qt::NoPen);
        p.drawEllipse(a, 2.0, 2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
    }
    p.restore();

    auto stateColours = [&](int role, bool mapped, QColor &fill, QColor &pen, QColor &label) {
        if (m_lit.contains(role))
        {
            fill = hi;
            pen = hi.darker(130);
            label = hiText;
        }
        else if (mapped)
        {
            fill = face;
            pen = withAlpha(text, m_hover == role ? 230 : 160);
            label = text;
        }
        else
        {
            fill = bodyFill;
            pen = withAlpha(dim, 150);
            label = dim;
        }
    };
    auto drawControl = [&](const QPainterPath &shape, int role, const QString &glyph,
                           const QColor &accent = QColor()) {
        const bool mapped = isMapped(role);
        QColor fill, pen, label;
        stateColours(role, mapped, fill, pen, label);
        p.setBrush(fill);
        p.setPen(QPen(pen, 1.2));
        const QPainterPath mappedShape = t.map(shape);
        p.drawPath(mappedShape);
        if (accent.isValid() && mapped && !m_lit.contains(role))
        {
            p.setPen(QPen(accent, 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawPath(mappedShape);
        }
        if (!glyph.isEmpty())
        {
            QFont f = font();
            f.setBold(true);
            f.setPixelSize(qMax(7, int(qMin(shape.boundingRect().height() * 0.62, 14.0) * s)));
            p.setFont(f);
            p.setPen(label);
            p.drawText(mappedShape.boundingRect(), Qt::AlignCenter, glyph);
        }
        if (!actionText(role).isEmpty())
            addHit(shape, role, QStringLiteral("%1: %2").arg(controlName(role), actionText(role)));
    };

    // Triggers and bumpers.
    for (int side = -1; side <= 1; side += 2)
    {
        const int trig = side < 0 ? R::LeftTrigger : R::RightTrigger;
        const int bump = side < 0 ? R::LB : R::RB;
        QPainterPath tp;
        tp.addRoundedRect(QRectF(400 + side * 107 - 30, 50, 60, 26), 9, 9);
        drawControl(tp, trig, glyphText(trig, m_controls.value(trig)));
        QPainterPath bp;
        bp.addRoundedRect(QRectF(400 + side * 105 - 42, 83, 84, 18), 9, 9);
        drawControl(bp, bump, glyphText(bump, m_controls.value(bump)));
    }

    // View, Guide, Menu.
    {
        QPainterPath vp;
        vp.addRoundedRect(QRectF(view.x() - 11, view.y() - 6, 22, 12), 6, 6);
        drawControl(vp, R::Back, QString());
        QPainterPath mp;
        mp.addRoundedRect(QRectF(menu.x() - 11, menu.y() - 6, 22, 12), 6, 6);
        drawControl(mp, R::Start, QString());
        QPainterPath gp;
        gp.addEllipse(guide, 12, 12);
        drawControl(gp, R::Guide, QString());
    }

    // Face buttons, in the Xbox colours when they carry the Xbox letters.
    const struct { int role; QPointF at; QColor accent; } faces[] = {
        { R::Y, abxy + QPointF(0, -fb), QColor(0xd0, 0xa0, 0x1c) },
        { R::X, abxy + QPointF(-fb, 0), QColor(0x3d, 0x7f, 0xc1) },
        { R::B, abxy + QPointF(fb, 0),  QColor(0xc8, 0x46, 0x3c) },
        { R::A, abxy + QPointF(0, fb),  QColor(0x3a, 0x9a, 0x4a) },
    };
    for (const auto &f : faces)
    {
        QPainterPath fp;
        fp.addEllipse(f.at, 13, 13);
        const QString glyph = glyphText(f.role, m_controls.value(f.role));
        const QString name = m_controls.value(f.role);
        const bool xbox = name.isEmpty() || name.compare(defaultName(f.role), Qt::CaseInsensitive) == 0;
        drawControl(fp, f.role, glyph, xbox ? f.accent : QColor());
    }

    // The D-pad: four arms round a hub.
    {
        const qreal arm = 17, w = 16;
        const struct { int role; QRectF r; } arms[] = {
            { R::DpadUp,    QRectF(dpad.x() - w / 2, dpad.y() - w / 2 - arm, w, arm) },
            { R::DpadDown,  QRectF(dpad.x() - w / 2, dpad.y() + w / 2, w, arm) },
            { R::DpadLeft,  QRectF(dpad.x() - w / 2 - arm, dpad.y() - w / 2, arm, w) },
            { R::DpadRight, QRectF(dpad.x() + w / 2, dpad.y() - w / 2, arm, w) },
        };
        QPainterPath hub;
        hub.addRect(QRectF(dpad.x() - w / 2, dpad.y() - w / 2, w, w));
        const bool any = isMapped(R::DpadUp) || isMapped(R::DpadDown)
                         || isMapped(R::DpadLeft) || isMapped(R::DpadRight);
        p.setBrush(any ? face : bodyFill);
        p.setPen(Qt::NoPen);
        p.drawPath(t.map(hub));
        for (const auto &a : arms)
        {
            QPainterPath ap;
            ap.addRoundedRect(a.r, 3, 3);
            drawControl(ap, a.role, QString());
        }
    }

    // The sticks: a well, and the cap where the stick is.
    auto drawStick = [&](const QPointF &c, int xRole, int yRole, int clickRole, bool fine) {
        const bool mapped = isMapped(xRole) || isMapped(yRole);
        QPainterPath well;
        well.addEllipse(c, 28, 28);
        p.setBrush(mapped ? face.darker(108) : bodyFill);
        p.setPen(QPen(withAlpha(mapped ? text : dim, mapped ? 150 : 130), 1.2));
        p.drawPath(t.map(well));

        const QPointF d = stickDeflection(fine);
        const bool moved = qAbs(d.x()) > 0.1 || qAbs(d.y()) > 0.1;
        const QPointF capAt = c + QPointF(qBound(-1.0, d.x(), 1.0), qBound(-1.0, d.y(), 1.0)) * kStickTravel;
        QPainterPath cap;
        cap.addEllipse(capAt, 17, 17);
        QColor fill, pen, label;
        stateColours(clickRole, mapped, fill, pen, label);
        if (moved && mapped)
        {
            fill = hi;
            pen = hi.darker(130);
        }
        p.setBrush(fill);
        p.setPen(QPen(pen, 1.2));
        p.drawPath(t.map(cap));
        // The grip ring on the cap.
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(withAlpha(moved && mapped ? hiText : (mapped ? text : dim), 70), 1.0));
        QPainterPath ring;
        ring.addEllipse(capAt, 11, 11);
        p.drawPath(t.map(ring));
        if (mapped)
            m_hits << Hit{ t.map(well), -1, tr("%1: %2").arg(controlName(xRole), actionText(xRole)) };
    };
    drawStick(ls, R::LeftX, R::LeftY, R::LS, false);
    drawStick(rs, R::RightX, R::RightY, R::RS, true);

    // The labels.
    for (const Callout &c : callouts)
    {
        if (c.action.isEmpty())
            continue;
        const qreal y = t.map(QPointF(0, c.row)).y();
        const qreal ah = afm.height(), nh = nfm.height();
        const qreal top = y - ah / 2.0;
        QRectF ar, nr;
        if (c.left)
        {
            ar = QRectF(x0, top, leftW, ah);
            nr = QRectF(x0, top + ah, leftW, nh);
        }
        else
        {
            const qreal x = x0 + total - rightW;
            ar = QRectF(x, top, rightW, ah);
            nr = QRectF(x, top + ah, rightW, nh);
        }
        const Qt::Alignment al = (c.left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
        bool lit = false;
        for (int r : c.roles)
            lit = lit || m_lit.contains(r);
        if (c.roles.contains(R::LeftX))
            lit = qAbs(stickDeflection(false).x()) > 0.1 || qAbs(stickDeflection(false).y()) > 0.1;
        if (c.roles.contains(R::RightX))
            lit = qAbs(stickDeflection(true).x()) > 0.1 || qAbs(stickDeflection(true).y()) > 0.1;
        if (lit && c.mapped)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(hi, 70));
            p.drawRoundedRect(ar.united(nr).adjusted(-4, -1, 4, 1), 4, 4);
        }
        p.setFont(actionFont);
        p.setPen(c.mapped ? text : dim);
        p.drawText(ar, al, c.action);
        p.setFont(nameFont);
        p.setPen(c.mapped ? nameCol : dim);
        p.drawText(nr, al, c.name);
        if (c.mapped && c.roles.size() == 1)
        {
            QPainterPath hp;
            hp.addRect(ar.united(nr));
            m_hits << Hit{ hp, c.roles.first(), hoverTip };
        }
    }
}

/*****************************************************************************
 * The keyboard
 *****************************************************************************/

void FocusControlMap::paintKeyboard(QPainter &p)
{
    const QPalette &pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor dim = blend(pal.color(QPalette::Window), text, 0.62);
    const QColor hi = pal.color(QPalette::Highlight);
    const QColor hiText = pal.color(QPalette::HighlightedText);
    const QColor keyFill = blend(pal.color(QPalette::Button), text, 0.07);

    const qreal s = qBound(0.5, qMin((width() - 8) / kKeys.width(), (height() - 8) / kKeys.height()), 1.6);
    QTransform t;
    t.translate((width() - kKeys.width() * s) / 2.0, (height() - kKeys.height() * s) / 2.0);
    t.scale(s, s);
    p.setTransform(t);

    QFont capFont = font();
    capFont.setPixelSize(15);
    capFont.setBold(true);
    QFont smallCapFont = capFont;
    smallCapFont.setPixelSize(12);
    QFont labelFont = font();
    labelFont.setPixelSize(11);
    const QFontMetricsF lfm(labelFont);

    const qreal kh = 38;
    auto key = [&](qreal x, qreal y, qreal w, int k, const QString &cap) {
        const QRectF r(x, y, w, kh);
        const bool lit = isKeyLit(k);
        const bool used = !keyAction(k).isEmpty();
        // A keycap: a shadow edge under the face.
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(text, 50));
        p.drawRoundedRect(r.translated(0, 2), 6, 6);
        p.setBrush(lit ? hi : keyFill);
        p.setPen(QPen(lit ? hi.darker(130) : withAlpha(used ? text : dim, m_hover == k ? 230 : 140), 1.2));
        p.drawRoundedRect(r, 6, 6);
        p.setFont(cap.size() > 2 ? smallCapFont : capFont);
        p.setPen(lit ? hiText : (used ? text : dim));
        p.drawText(r, Qt::AlignCenter, cap);
        if (used)
        {
            QPainterPath hp;
            hp.addRoundedRect(r, 6, 6);
            m_hits << Hit{ t.map(hp), k, keyAction(k) };
        }
        return r;
    };
    /* Under a key (or keys): what it does, wrapped to the room it has. */
    const qreal lh = 3 * lfm.height();
    auto label = [&](const QRectF &under, qreal room, const QString &txt, bool lit) {
        if (txt.isEmpty())
            return;
        const QRectF lr(under.center().x() - room / 2.0, under.bottom() + 4, room, lh);
        p.setFont(labelFont);
        const QRectF used = lfm.boundingRect(lr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, txt);
        if (lit)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(hi, 70));
            p.drawRoundedRect(used.adjusted(-4, -1, 4, 1), 4, 4);
        }
        p.setPen(text);
        p.drawText(lr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, txt);
    };
    const qreal pitch = 70, room = pitch - 4;
    auto single = [&](qreal x, qreal y, qreal w, int k, const QString &cap) {
        const QRectF r = key(x, y, w, k, cap);
        label(r, qMax(room, w + 8), keyAction(k), isKeyLit(k));
        return r;
    };

    // The top row: leave, step through, the second reference, zero, step size, revert.
    const qreal y1 = 6;
    single(6, y1, 52, Qt::Key_Escape, tr("Esc"));
    single(6 + pitch, y1, 42, Qt::Key_BracketLeft, QStringLiteral("["));
    single(6 + 2 * pitch, y1, 42, Qt::Key_BracketRight, QStringLiteral("]"));
    single(6 + 3 * pitch, y1, 42, Qt::Key_R, tr("R / T"));
    single(6 + 4 * pitch, y1, 42, Qt::Key_0, QStringLiteral("0"));
    single(6 + 5 * pitch, y1, 42, Qt::Key_Minus, QString::fromUtf8("\342\210\222"));
    single(6 + 6 * pitch, y1, 42, Qt::Key_Plus, QStringLiteral("+"));
    single(6 + 7 * pitch, y1, 60, Qt::Key_Backspace, QString::fromUtf8("\342\214\253"));

    // The second: the calls and the scope, and the session's own keys.
    const qreal y2 = 94;
    single(6, y2, 54, Qt::Key_PageUp, tr("PgUp"));
    single(6 + pitch, y2, 54, Qt::Key_PageDown, tr("PgDn"));
    single(6 + 2 * pitch, y2, 42, Qt::Key_G, QStringLiteral("G"));
    single(6 + 3 * pitch, y2, 42, Qt::Key_W, QStringLiteral("W"));
    single(6 + 4 * pitch, y2, 42, Qt::Key_U, QStringLiteral("U"));
    single(6 + 5 * pitch, y2, 42, Qt::Key_N, QStringLiteral("N"));
    single(6 + 6 * pitch, y2, 42, Qt::Key_D, QStringLiteral("D"));
    single(6 + 7 * pitch, y2, 42, Qt::Key_H, QStringLiteral("H"));

    // The third: shift, undo, save; the beam and the focus.
    const qreal y3 = 182;
    single(6, y3, 66, Qt::Key_Shift, QString::fromUtf8("\342\207\247"));
    const QRectF ck = key(6 + pitch + 6, y3, 44, Qt::Key_Control, ctrlKeyName());
    const QRectF zz = key(6 + pitch + 54, y3, 40, Qt::Key_Z, QStringLiteral("Z"));
    label(ck.united(zz), 2 * room, keyAction(Qt::Key_Z), isKeyLit(Qt::Key_Control) && isKeyLit(Qt::Key_Z));
    const QRectF ret = key(6 + 3 * pitch - 6, y3, 80, Qt::Key_Return, QString::fromUtf8("\342\217\216"));
    label(ret, qMax(room, 88.0), keyAction(Qt::Key_Return), isKeyLit(Qt::Key_Return));
    single(6 + 4 * pitch + 12, y3, 40, Qt::Key_Comma, QStringLiteral(","));
    single(6 + 5 * pitch + 12, y3, 40, Qt::Key_Period, QStringLiteral("."));
    single(6 + 6 * pitch + 12, y3, 40, Qt::Key_Semicolon, QStringLiteral(";"));
    single(6 + 7 * pitch + 12, y3, 40, Qt::Key_Apostrophe, QStringLiteral("'"));

    // The arrows, as on a keyboard: an inverted T, at the right.
    const qreal ax = 600, aw = 38;
    key(ax + aw + 4, y3 - kh - 4, aw, Qt::Key_Up, QString::fromUtf8("\342\206\221"));
    const QRectF lf = key(ax, y3, aw, Qt::Key_Left, QString::fromUtf8("\342\206\220"));
    const QRectF dn = key(ax + aw + 4, y3, aw, Qt::Key_Down, QString::fromUtf8("\342\206\223"));
    const QRectF rt = key(ax + 2 * (aw + 4), y3, aw, Qt::Key_Right, QString::fromUtf8("\342\206\222"));
    label(lf.united(dn).united(rt), 3 * aw + 8, keyAction(Qt::Key_Up),
          isKeyLit(Qt::Key_Up) || isKeyLit(Qt::Key_Down) || isKeyLit(Qt::Key_Left) || isKeyLit(Qt::Key_Right));

    p.setFont(labelFont);
    p.setPen(dim);
    p.drawText(QRectF(6, kKeys.height() - 18, 600, 16), Qt::AlignLeft | Qt::AlignVCenter,
               tr("The keys work whichever device is chosen."));
    p.resetTransform();
}

/*****************************************************************************
 * Clicking
 *****************************************************************************/

int FocusControlMap::hitAt(const QPointF &pos, QString *tip) const
{
    for (const Hit &h : m_hits)
        if (h.id != -1 && h.shape.contains(pos))
        {
            if (tip != nullptr)
                *tip = h.tip;
            return h.id;
        }
    return -1;
}

void FocusControlMap::mousePressEvent(QMouseEvent *event)
{
    const int id = hitAt(event->position());
    if (id < 0 || event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }
    if (m_mode == ControllerMap)
        emit controlClicked(id);
    else
        emit keyClicked(id);
    event->accept();
}

void FocusControlMap::mouseMoveEvent(QMouseEvent *event)
{
    QString tip;
    const int id = hitAt(event->position(), &tip);
    if (id != m_hover)
    {
        m_hover = id;
        setCursor(id >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        setToolTip(tip);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void FocusControlMap::leaveEvent(QEvent *event)
{
    if (m_hover != -1)
    {
        m_hover = -1;
        update();
    }
    QWidget::leaveEvent(event);
}
