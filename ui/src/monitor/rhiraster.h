/*
  Q Light Controller Plus
  rhiraster.h

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

#ifndef RHIRASTER_H
#define RHIRASTER_H

#include <QVector>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QSize>

class QRhi;
class QRhiBuffer;
class QRhiTexture;
class QRhiRenderBuffer;
class QRhiSampler;
class QRhiTextureRenderTarget;
class QRhiRenderPassDescriptor;
class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class QOffscreenSurface;

/** @addtogroup ui_monitor
 * @{
 */

/**
 * The rig view's rasteriser on the GPU (QRhi: Metal, Vulkan, OpenGL,
 * Direct3D): what ZRaster does on the CPU -- solids depth-tested, light
 * added and depth-tested without writing depth, see-through surfaces over
 * the solids back to front -- for the same triangles, with multisampling
 * where ZRaster supersamples.
 *
 * The view still draws everything else with QPainter; this hands back the
 * two images ZRaster would have: the scene on a transparent background,
 * and the light, rolled off and premultiplied for a Screen composite.
 *
 * Triangles come already projected: screen x/y in logical pixels, the
 * view's depth (LARGER = NEARER, as viewDepth() has it) and a colour per
 * corner.
 */
class RhiRaster final
{
public:
    RhiRaster();
    ~RhiRaster();

    /** A triangle corner. */
    struct Vertex
    {
        float x, y, z;          ///< screen px; depth, larger = nearer
        float r, g, b, a;       ///< premultiplied, 0..1
    };

    /** Can this machine draw on the GPU at all? Asks once; false on the
     *  offscreen/minimal platforms unless QLC_RIG_GPU=1. */
    static bool available();

    /**
     * Draws a frame.
     *
     * @param size      final image size, logical pixels
     * @param quality   ZRaster's supersample factor (1..4); the GPU takes
     *                  that many samples per pixel squared, as far as it can
     * @param solids    opaque triangles: depth-tested and depth-writing
     * @param seeThrough translucent triangles, back to front: tested, not
     *                  writing
     * @param light     light: added, tested against the solids only
     * @return false if the GPU could not draw it (fall back to ZRaster)
     */
    bool render(const QSize &size, int quality,
                const QVector<Vertex> &solids,
                const QVector<Vertex> &seeThrough,
                const QVector<Vertex> &light);

    /** The scene, transparent where nothing was drawn. */
    QImage scene() const { return m_scene; }
    /** The light, rolled off; null when there was none. */
    QImage lightImage() const { return m_light; }

    /** The backend in use ("Metal", "OpenGL", ...), for the record. */
    QString backendName() const;

private:
    bool ensureRhi();
    bool ensureTargets(const QSize &pixels, int samples);
    void releaseTargets();

private:
    QRhi *m_rhi = nullptr;
    QOffscreenSurface *m_surface = nullptr;
    QRhiBuffer *m_vbuf = nullptr;
    QRhiBuffer *m_ubuf = nullptr;
    QRhiSampler *m_sampler = nullptr;

    QSize m_pixels;
    int m_samples = 0;
    int m_sampleCap = 16;               ///< 1 once multisampling has failed
    QRhiTexture *m_sceneMs = nullptr, *m_sceneTex = nullptr;
    QRhiTexture *m_lightMs = nullptr, *m_lightTex = nullptr;
    QRhiTexture *m_rolledTex = nullptr;
    QRhiRenderBuffer *m_depth = nullptr;
    QRhiTextureRenderTarget *m_sceneRt = nullptr, *m_lightRt = nullptr, *m_rolledRt = nullptr;
    QRhiRenderPassDescriptor *m_sceneRp = nullptr, *m_lightRp = nullptr, *m_rolledRp = nullptr;
    QRhiShaderResourceBindings *m_geomSrb = nullptr, *m_rollSrb = nullptr;
    QRhiGraphicsPipeline *m_solidPs = nullptr, *m_seePs = nullptr;
    QRhiGraphicsPipeline *m_depthOnlyPs = nullptr, *m_lightPs = nullptr, *m_rollPs = nullptr;

    QImage m_scene;
    QImage m_light;
};

/** @} */

#endif
