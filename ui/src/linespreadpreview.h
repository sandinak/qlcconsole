/*
  Q Light Controller Plus
  linespreadpreview.h

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

#ifndef LINESPREADPREVIEW_H
#define LINESPREADPREVIEW_H

#include <QWidget>
#include <QList>

class Doc;
class Scene;
class QLCPalette;

/** @addtogroup ui UI
 * @{
 */

/**
 * Where an Aim look on a line target puts its lights (docs/design/LINE_TARGET_DESIGN.md):
 * the line end to end, and each light as a numbered dot -- 1 is the light
 * nearest the line's start -- at its place along it for the look's Spread,
 * Group and Cross. Lights sharing a point stack. With no scene to say which
 * lights, six stand in.
 */
class LineSpreadPreview final : public QWidget
{
    Q_OBJECT

public:
    explicit LineSpreadPreview(Doc *doc, QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(260, 64); }

    /** Show @p look's lights, the heads of @p scene (may be null). */
    void setLook(const Scene *scene, const QLCPalette *look);

    struct Dot
    {
        int number;          ///< 1 = the light nearest the line's start
        double fraction;     ///< 0..1 along the line
        QString name;        ///< the fixture, when there is one
    };
    /** What it shows, in light order (tests). */
    const QList<Dot> &dots() const { return m_dots; }

    /** Drawn dimmed: the plan it still shows is the last one, not the selection's. */
    void setDimmed(bool dimmed);
    bool isDimmed() const { return m_dimmed; }
    /** Whether the strip offers to open in a window (right-click); the window's own does not. */
    void setPoppable(bool poppable) { m_poppable = poppable; }
    /** Dot and label size at this widget size (1 at the embedded size; tests). */
    double drawScale() const;

signals:
    void popOutRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    Doc *m_doc;
    QList<Dot> m_dots;
    bool m_dimmed = false;
    bool m_poppable = true;
};

/** @} */

#endif
