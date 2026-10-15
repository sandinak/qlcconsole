/*
  Q Light Controller Plus
  rhiraster.cpp

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

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QMatrix4x4>
#include <QDebug>
#include <QFile>

#include <rhi/qrhi.h>

#include "rhiraster.h"

namespace
{
QShader loadShader(const QString &name)
{
    QFile f(QStringLiteral(":/rig/") + name + QStringLiteral(".qsb"));
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning() << "[RhiRaster] no shader" << name;
        return QShader();
    }
    return QShader::fromSerialized(f.readAll());
}

template <typename T> void release(T *&p)
{
    delete p;
    p = nullptr;
}

const int kVertexSize = int(sizeof(RhiRaster::Vertex));
}

RhiRaster::RhiRaster()
{
}

RhiRaster::~RhiRaster()
{
    releaseTargets();
    release(m_vbuf);
    release(m_ubuf);
    release(m_sampler);
    release(m_rhi);
    release(m_surface);
}

bool RhiRaster::available()
{
    // Asked every time: a test can switch it on for itself.
    const QByteArray env = qgetenv("QLC_RIG_GPU");
    const QString platform = QGuiApplication::platformName();
    if (env == "0")
        return false;
    if (env != "1" && (platform == QLatin1String("offscreen")
                       || platform == QLatin1String("minimal")))
        return false;                   // tests draw on the CPU unless asked
    // The hardware, once.
    static int probed = -1;
    if (probed < 0)
    {
        RhiRaster probe;
        probed = probe.ensureRhi() ? 1 : 0;
    }
    return probed == 1;
}

QString RhiRaster::backendName() const
{
    return m_rhi != nullptr ? QString::fromLatin1(m_rhi->backendName()) : QString();
}

bool RhiRaster::ensureRhi()
{
    if (m_rhi != nullptr)
        return true;
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    QRhiMetalInitParams params;
    m_rhi = QRhi::create(QRhi::Metal, &params);
#elif defined(Q_OS_WIN)
    QRhiD3D11InitParams params;
    m_rhi = QRhi::create(QRhi::D3D11, &params);
#endif
    if (m_rhi == nullptr)
    {
        // OpenGL wherever there is a context to be had (Linux, and any of the
        // above that failed): it needs a surface, even an offscreen one.
        m_surface = QRhiGles2InitParams::newFallbackSurface();
        QRhiGles2InitParams params;
        params.fallbackSurface = m_surface;
        m_rhi = QRhi::create(QRhi::OpenGLES2, &params);
    }
    if (m_rhi == nullptr)
    {
        release(m_surface);
        return false;
    }

    m_ubuf = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 16);
    m_sampler = m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                  QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_ubuf->create() || !m_sampler->create())
    {
        release(m_ubuf);
        release(m_sampler);
        release(m_rhi);
        release(m_surface);
        return false;
    }
    return true;
}

void RhiRaster::releaseTargets()
{
    release(m_solidPs);
    release(m_seePs);
    release(m_depthOnlyPs);
    release(m_lightPs);
    release(m_rollPs);
    release(m_geomSrb);
    release(m_rollSrb);
    release(m_sceneRt);
    release(m_lightRt);
    release(m_rolledRt);
    release(m_sceneRp);
    release(m_lightRp);
    release(m_rolledRp);
    release(m_depth);
    release(m_sceneMs);
    release(m_sceneTex);
    release(m_lightMs);
    release(m_lightTex);
    release(m_rolledTex);
    m_pixels = QSize();
    m_samples = 0;
}

bool RhiRaster::ensureTargets(const QSize &pixels, int samples)
{
    if (pixels == m_pixels && samples == m_samples && m_rollPs != nullptr)
        return true;
    releaseTargets();

    const QRhiTexture::Format lightFormat =
        m_rhi->isTextureFormatSupported(QRhiTexture::RGBA16F) ? QRhiTexture::RGBA16F
                                                              : QRhiTexture::RGBA32F;
    const bool ms = samples > 1;
    const QRhiTexture::Flags rt = QRhiTexture::RenderTarget;
    const QRhiTexture::Flags out = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;

    // The scene: multisampled, resolved into what is read back.
    m_sceneTex = m_rhi->newTexture(QRhiTexture::RGBA8, pixels, 1, out);
    if (ms)
        m_sceneMs = m_rhi->newTexture(QRhiTexture::RGBA8, pixels, samples, rt);
    // The light: the same, in floating point so it can pile up past white.
    m_lightTex = m_rhi->newTexture(lightFormat, pixels, 1, rt);
    if (ms)
        m_lightMs = m_rhi->newTexture(lightFormat, pixels, samples, rt);
    m_rolledTex = m_rhi->newTexture(QRhiTexture::RGBA8, pixels, 1, out);
    m_depth = m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, pixels, samples);
    for (QRhiResource *r : { static_cast<QRhiResource *>(m_sceneTex), static_cast<QRhiResource *>(m_sceneMs),
                             static_cast<QRhiResource *>(m_lightTex), static_cast<QRhiResource *>(m_lightMs),
                             static_cast<QRhiResource *>(m_rolledTex) })
        if (r != nullptr && !static_cast<QRhiTexture *>(r)->create())
            return false;
    if (!m_depth->create())
        return false;

    auto target = [this](QRhiTexture *msTex, QRhiTexture *tex, QRhiRenderBuffer *depth,
                         QRhiTextureRenderTarget **rtOut, QRhiRenderPassDescriptor **rpOut) {
        QRhiColorAttachment att(msTex != nullptr ? msTex : tex);
        if (msTex != nullptr)
            att.setResolveTexture(tex);
        QRhiTextureRenderTargetDescription desc(att);
        if (depth != nullptr)
            desc.setDepthStencilBuffer(depth);
        *rtOut = m_rhi->newTextureRenderTarget(desc);
        *rpOut = (*rtOut)->newCompatibleRenderPassDescriptor();
        (*rtOut)->setRenderPassDescriptor(*rpOut);
        return (*rtOut)->create();
    };
    if (!target(m_sceneMs, m_sceneTex, m_depth, &m_sceneRt, &m_sceneRp)
            || !target(m_lightMs, m_lightTex, m_depth, &m_lightRt, &m_lightRp)
            || !target(nullptr, m_rolledTex, nullptr, &m_rolledRt, &m_rolledRp))
        return false;

    m_geomSrb = m_rhi->newShaderResourceBindings();
    m_geomSrb->setBindings({ QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage, m_ubuf) });
    m_rollSrb = m_rhi->newShaderResourceBindings();
    m_rollSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_ubuf),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                  m_lightTex, m_sampler) });
    if (!m_geomSrb->create() || !m_rollSrb->create())
        return false;

    const QShader vs = loadShader(QStringLiteral("rig.vert"));
    const QShader fs = loadShader(QStringLiteral("rig.frag"));
    const QShader rvs = loadShader(QStringLiteral("rolloff.vert"));
    const QShader rfs = loadShader(QStringLiteral("rolloff.frag"));
    if (!vs.isValid() || !fs.isValid() || !rvs.isValid() || !rfs.isValid())
        return false;

    QRhiVertexInputLayout layout;
    layout.setBindings({ QRhiVertexInputBinding(kVertexSize) });
    layout.setAttributes({ QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0),
                           QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float4,
                                                    3 * sizeof(float)) });

    // Premultiplied "over": the colour is already multiplied by its alpha.
    QRhiGraphicsPipeline::TargetBlend over;
    over.enable = true;
    over.srcColor = QRhiGraphicsPipeline::One;
    over.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    over.srcAlpha = QRhiGraphicsPipeline::One;
    over.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    // Light adds up.
    QRhiGraphicsPipeline::TargetBlend add;
    add.enable = true;
    add.srcColor = QRhiGraphicsPipeline::One;
    add.dstColor = QRhiGraphicsPipeline::One;
    add.srcAlpha = QRhiGraphicsPipeline::One;
    add.dstAlpha = QRhiGraphicsPipeline::One;
    // Depth only: no colour at all.
    QRhiGraphicsPipeline::TargetBlend none;
    none.colorWrite = {};

    auto pipeline = [&](QRhiRenderPassDescriptor *rp, int sampleCount,
                        const QRhiGraphicsPipeline::TargetBlend &blend,
                        bool depthTest, bool depthWrite) {
        QRhiGraphicsPipeline *ps = m_rhi->newGraphicsPipeline();
        ps->setShaderStages({ { QRhiShaderStage::Vertex, vs }, { QRhiShaderStage::Fragment, fs } });
        ps->setVertexInputLayout(layout);
        ps->setShaderResourceBindings(m_geomSrb);
        ps->setRenderPassDescriptor(rp);
        ps->setSampleCount(sampleCount);
        ps->setTargetBlends({ blend });
        ps->setDepthTest(depthTest);
        ps->setDepthWrite(depthWrite);
        ps->setDepthOp(QRhiGraphicsPipeline::Less);   // nearer is smaller here
        if (!ps->create())
            release(ps);
        return ps;
    };
    m_solidPs = pipeline(m_sceneRp, samples, over, true, true);
    m_seePs = pipeline(m_sceneRp, samples, over, true, false);
    m_depthOnlyPs = pipeline(m_lightRp, samples, none, true, true);
    m_lightPs = pipeline(m_lightRp, samples, add, true, false);

    m_rollPs = m_rhi->newGraphicsPipeline();
    m_rollPs->setShaderStages({ { QRhiShaderStage::Vertex, rvs }, { QRhiShaderStage::Fragment, rfs } });
    m_rollPs->setVertexInputLayout(layout);
    m_rollPs->setShaderResourceBindings(m_rollSrb);
    m_rollPs->setRenderPassDescriptor(m_rolledRp);
    if (!m_rollPs->create())
        release(m_rollPs);

    if (m_solidPs == nullptr || m_seePs == nullptr || m_depthOnlyPs == nullptr
            || m_lightPs == nullptr || m_rollPs == nullptr)
        return false;

    m_pixels = pixels;
    m_samples = samples;
    return true;
}

bool RhiRaster::render(const QSize &size, int quality,
                       const QVector<Vertex> &solids,
                       const QVector<Vertex> &seeThrough,
                       const QVector<Vertex> &light)
{
    m_scene = QImage();
    m_light = QImage();
    if (size.isEmpty() || !ensureRhi())
        return false;

    // As many samples as ZRaster's supersampling takes (q x q), as far as the
    // GPU goes.
    int samples = 1;
    const int want = qMin(m_sampleCap, qBound(1, quality, 4) * qBound(1, quality, 4));
    for (int s : m_rhi->supportedSampleCounts())
        if (s <= want && s > samples)
            samples = s;
    if (!ensureTargets(size, samples))
    {
        releaseTargets();
        // Some drivers list sample counts they can't render floating point
        // to (Mesa's OpenGL 2.1): single-sampled from then on, rather than
        // off the GPU altogether.
        if (samples == 1)
            return false;
        m_sampleCap = 1;
        if (!ensureTargets(size, 1))
        {
            releaseTargets();
            return false;
        }
    }

    /* Depth: the view's (larger = nearer) mapped to 0 nearest .. 1 farthest
       over what this frame actually spans, kept off both ends. */
    float zNear = -1e30f, zFar = 1e30f;
    for (const QVector<Vertex> *list : { &solids, &seeThrough, &light })
        for (const Vertex &v : *list)
        {
            zNear = qMax(zNear, v.z);
            zFar = qMin(zFar, v.z);
        }
    const float span = qMax(1e-6f, zNear - zFar);

    const int n = solids.size() + seeThrough.size() + light.size();
    // The roll-off pass's triangle over the whole screen, after the rest.
    static const Vertex screen[3] = { { 0, 0, 0, 0, 0, 0, 0 }, { 2, 0, 0, 0, 0, 0, 0 },
                                      { 0, 2, 0, 0, 0, 0, 0 } };

    const quint32 bytes = quint32((n + 3) * kVertexSize);
    if (m_vbuf == nullptr || m_vbuf->size() < bytes)
    {
        release(m_vbuf);
        m_vbuf = m_rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer,
                                  qMax<quint32>(bytes, 1 << 20));
        if (!m_vbuf->create())
        {
            release(m_vbuf);
            return false;
        }
    }

    QRhiCommandBuffer *cb = nullptr;
    if (m_rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
        return false;

    /* Screen pixels, y down, to the backend's clip space; depth from
       zNear..zFar to 0.01..0.99 (as 0..1, -1..1 in GL's clip space), so the
       vertices go up as they came. */
    QMatrix4x4 m;
    m.setRow(0, QVector4D(2.0f / size.width(), 0.0f, 0.0f, -1.0f));
    m.setRow(1, QVector4D(0.0f, -2.0f / size.height(), 0.0f, 1.0f));
    m.setRow(2, QVector4D(0.0f, 0.0f, -1.96f / span, 1.96f * zNear / span - 0.98f));
    m.setRow(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    const QMatrix4x4 mvp = m_rhi->clipSpaceCorrMatrix() * m;
    const float flip[4] = { m_rhi->isYUpInFramebuffer() ? 0.0f : 1.0f, 0, 0, 0 };

    QRhiResourceUpdateBatch *u = m_rhi->nextResourceUpdateBatch();
    quint32 at = 0;
    for (const QVector<Vertex> *list : { &solids, &seeThrough, &light })
    {
        if (!list->isEmpty())
            u->uploadStaticBuffer(m_vbuf, at, quint32(list->size() * kVertexSize),
                                  list->constData());
        at += quint32(list->size() * kVertexSize);
    }
    u->uploadStaticBuffer(m_vbuf, at, quint32(3 * kVertexSize), screen);
    u->updateDynamicBuffer(m_ubuf, 0, 64, mvp.constData());
    u->updateDynamicBuffer(m_ubuf, 64, 16, flip);

    const QRhiViewport viewport(0, 0, float(size.width()), float(size.height()));
    auto draw = [&](QRhiGraphicsPipeline *ps, int first, int count) {
        if (count <= 0)
            return;
        cb->setGraphicsPipeline(ps);
        cb->setViewport(viewport);
        cb->setShaderResources(m_geomSrb);
        const QRhiCommandBuffer::VertexInput in(m_vbuf, quint32(first * kVertexSize));
        cb->setVertexInput(0, 1, &in);
        cb->draw(quint32(count));
    };
    const int nSolid = solids.size(), nSee = seeThrough.size(), nLight = light.size();

    // 1. The scene: solids, then the see-through surfaces over them.
    cb->beginPass(m_sceneRt, QColor(0, 0, 0, 0), { 1.0f, 0 }, u);
    draw(m_solidPs, 0, nSolid);
    draw(m_seePs, nSolid, nSee);
    cb->endPass();

    // 2. The light: depth from the solids again, then the light against it.
    cb->beginPass(m_lightRt, QColor(0, 0, 0, 0), { 1.0f, 0 });
    if (nLight > 0)
    {
        draw(m_depthOnlyPs, 0, nSolid);
        draw(m_lightPs, nSolid + nSee, nLight);
    }
    cb->endPass();

    // 3. Rolled off, and both read back.
    QRhiResourceUpdateBatch *rb = m_rhi->nextResourceUpdateBatch();
    QRhiReadbackResult sceneBack, lightBack;
    rb->readBackTexture(QRhiReadbackDescription(m_sceneTex), &sceneBack);
    if (nLight > 0)
    {
        cb->beginPass(m_rolledRt, QColor(0, 0, 0, 0), { 1.0f, 0 });
        cb->setGraphicsPipeline(m_rollPs);
        cb->setViewport(viewport);
        cb->setShaderResources(m_rollSrb);
        const QRhiCommandBuffer::VertexInput in(m_vbuf, quint32(n * kVertexSize));
        cb->setVertexInput(0, 1, &in);
        cb->draw(3);
        cb->endPass();
        rb->readBackTexture(QRhiReadbackDescription(m_rolledTex), &lightBack);
    }
    cb->resourceUpdate(rb);
    if (m_rhi->endOffscreenFrame() != QRhi::FrameOpSuccess)
        return false;

    auto toImage = [this](const QRhiReadbackResult &r) {
        if (r.data.isEmpty())
            return QImage();
        QImage img(reinterpret_cast<const uchar *>(r.data.constData()),
                   r.pixelSize.width(), r.pixelSize.height(),
                   QImage::Format_RGBA8888_Premultiplied);
        img = img.copy();                       // own the bytes
        if (m_rhi->isYUpInFramebuffer())         // rows came bottom-up
            img = img.transformed(QTransform::fromScale(1, -1));
        return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    };
    m_scene = toImage(sceneBack);
    if (nLight > 0)
        m_light = toImage(lightBack);
    return !m_scene.isNull();
}
