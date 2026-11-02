/*
  Q Light Controller Plus
  workbenchencoders.cpp

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

/* The Workbench under an encoder bank (EncoderBank, docs/features/encoder-banks.md):
   the controller's turns edit what is selected, on the path a hand takes.
   A look in the Look Editor is edited by the editor itself (its own undo
   step); the channels of selected fixtures or groups, or of a Show Tree
   layer's scene, are edited here as one step on the shared history, merged
   for a gesture. */

#include <QSharedPointer>
#include <QStackedWidget>
#include <QScrollArea>
#include <QTreeWidget>
#include <QTimer>
#include <QLabel>
#include <climits>

#include "workbench.h"
#include "workbenchstack.h"
#include "encoderattributes.h"
#include "fixtureconsole.h"
#include "programmingoutline.h"
#include "encoderbank.h"
#include "controlmap.h"
#include "paletteops.h"
#include "lookeditor.h"
#include "qlcpalette.h"
#include "scenevalue.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

Workbench::EncoderTarget Workbench::encoderTarget() const
{
    EncoderTarget t;
    if (!isVisible())
        return t;
    if (currentPage() == LookPage && m_lookEditor->paletteId() != QLCPalette::invalidId())
    {
        t.look = m_lookEditor;
        return t;
    }
    Scene *s = qobject_cast<Scene *>(m_doc->function(m_currentScene));
    if (s != nullptr && currentPage() == TargetPage)
    {
        t.scene = s;
        t.fixtures = m_selectedFixtures;
        return t;
    }
    if (s == nullptr && m_stack != nullptr && m_stackLayer >= 0 && m_stackLayer < m_stack->layers().size())
    {
        Scene *layer = qobject_cast<Scene *>(m_doc->function(m_stack->layerScene(m_stackLayer)));
        if (layer != nullptr)
        {
            t.scene = layer;
            t.fixtures = m_selectedFixtures;
        }
    }
    return t;
}

QString Workbench::encoderLine() const
{
    return m_encoderLine->text();
}

void Workbench::connectEncoders()
{
    ControlMap *map = m_doc->controlMap();
    if (map == nullptr)
        return;
    EncoderBank *enc = map->encoderBank();
    connect(enc, &EncoderBank::adjusted, this, &Workbench::encoderAdjusted);
    connect(enc, &EncoderBank::homed, this, &Workbench::encoderHomed);
    connect(enc, &EncoderBank::refused, this, &Workbench::encoderMessage);
    connect(enc, &EncoderBank::bankChanged, this, [this](int) {
        m_encoderMessage.clear();
        scheduleEncoderRings();
    });
    connect(enc, &EncoderBank::ringsWanted, this, &Workbench::scheduleEncoderRings);
    connect(enc, &EncoderBank::deviceChanged, this, &Workbench::scheduleEncoderRings);

    // Another selection, another page, or the values moved (an undo).
    connect(this, &Workbench::fixturesSelected, this, [this]() { scheduleEncoderRings(); });
    connect(this, &Workbench::currentPaletteIdChanged, this, [this]() { scheduleEncoderRings(); });
    connect(m_rows, &QTreeWidget::itemSelectionChanged, this, [this]() { scheduleEncoderRings(); });
    connect(m_page, &QStackedWidget::currentChanged, this, [this]() { scheduleEncoderRings(); });
    if (PaletteOps *ops = m_doc->paletteOps())
        connect(ops, &PaletteOps::changed, this, [this]() { scheduleEncoderRings(); });
}

void Workbench::scheduleEncoderRings()
{
    if (m_encoderRingsQueued)
        return;
    m_encoderRingsQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_encoderRingsQueued = false;
        refreshEncoderRings();
    });
}

void Workbench::encoderMessage(const QString &text)
{
    m_encoderMessage = text;
    refreshEncoderRings();
}

void Workbench::refreshEncoderRings()
{
    EncoderBank *enc = m_doc->controlMap() != nullptr ? m_doc->controlMap()->encoderBank() : nullptr;
    if (enc == nullptr)
        return;
    const bool on = enc->isEnabled();
    m_encoderLine->setVisible(on);
    if (!on)
    {
        m_encoderLine->clear();
        return;
    }
    const int bank = enc->bank();
    QString line = tr("Encoders — %1").arg(EncoderBank::bankName(bank));

    const EncoderTarget t = encoderTarget();
    if (!t.isValid())
    {
        enc->clearRings();
        if (m_encoderMessage.isEmpty())
            line += tr(" · select a fixture, a group or a look to turn");
    }
    else
    {
        QStringList names;
        for (int slot = 0; slot < EncoderBank::slotsPerBank(); ++slot)
        {
            float norm = 0.0f;
            bool has = false;
            if (slot < EncoderBank::slotCount(bank))
                has = t.look != nullptr
                          ? t.look->encoderValue(bank, slot, &norm)
                          : EncoderAttributes::read(m_doc, t.scene, t.fixtures, bank, slot, &norm);
            enc->setRing(slot, has ? norm : -1.0f);
            if (has)
                names << EncoderBank::slotName(bank, slot);
        }
        if (m_encoderMessage.isEmpty())
            line += names.isEmpty() ? tr(" · nothing here to turn") : QStringLiteral(" · ") + names.join(QStringLiteral(", "));
    }
    if (!m_encoderMessage.isEmpty())
        line += QStringLiteral(" · ") + m_encoderMessage;
    m_encoderLine->setText(line);
}

void Workbench::recordEncoderStep(Scene *scene, const QSharedPointer<Scene> &before, const QString &what,
                                  int bank, int slot, bool home)
{
    PaletteOps *ops = m_doc->paletteOps();
    if (ops == nullptr || before.isNull())
        return;
    // A gesture on one encoder (turns within a moment of each other) is one
    // step; a tap on its push is a step of its own.
    ops->recordSceneStep(home ? tr("home %1 in %2").arg(what, scene->name())
                              : tr("turn %1 in %2").arg(what, scene->name()),
                         { before },
                         QStringLiteral("%1:%2:%3:%4").arg(home ? QStringLiteral("encoderhome")
                                                                : QStringLiteral("encoder"))
                             .arg(scene->id()).arg(bank).arg(slot));
}

void Workbench::encoderAdjusted(int bank, int slot, int detents, bool fine)
{
    if (!isVisible())
        return;
    const EncoderTarget t = encoderTarget();
    if (!t.isValid())
    {
        encoderMessage(tr("nothing selected to turn"));
        return;
    }
    const QString what = EncoderBank::slotName(bank, slot);
    bool ok = false;
    if (t.look != nullptr)
    {
        ok = t.look->encoderAdjust(bank, slot, detents, fine);
        if (!ok)
        {
            encoderMessage(tr("this look has no %1").arg(what.toLower()));
            return;
        }
    }
    else
    {
        if (!EncoderAttributes::available(m_doc, t.fixtures, bank, slot))
        {
            encoderMessage(tr("the selection has no %1").arg(what.toLower()));
            return;
        }
        PaletteOps *ops = m_doc->paletteOps();
        const QSharedPointer<Scene> before = ops != nullptr ? ops->sceneSnapshot(t.scene->id())
                                                            : QSharedPointer<Scene>();
        ok = EncoderAttributes::adjust(m_doc, t.scene, t.fixtures, bank, slot, detents, fine);
        if (!ok)
        {
            encoderMessage(tr("a look drives the %1 — edit the look").arg(what.toLower()));
            return;
        }
        afterEncoderEdit(t, what, before, bank, slot);
    }
    float norm = 0.0f;
    const bool has = t.look != nullptr ? t.look->encoderValue(bank, slot, &norm)
                                       : EncoderAttributes::read(m_doc, t.scene, t.fixtures, bank, slot, &norm);
    m_encoderMessage = has ? tr("%1 %2%").arg(what).arg(int(norm * 100.0f + 0.5f)) : what;
    refreshEncoderRings();
}

void Workbench::afterEncoderEdit(const EncoderTarget &t, const QString &what,
                                 const QSharedPointer<Scene> &before, int bank, int slot, bool home)
{
    m_doc->setModified();
    if (t.scene->isRunning())
        t.scene->resetRuntime();
    refreshPreview();
    // The channel console shows the first fixture's values as they now are.
    if (t.scene->id() == m_currentScene && !t.fixtures.isEmpty() && m_fixtureScroll->isVisible())
    {
        const quint32 lead = t.fixtures.first();
        m_fixtureConsole->blockSignals(true);
        for (const SceneValue &scv : t.scene->values())
            if (scv.fxi == lead)
                m_fixtureConsole->setSceneValue(scv);
        m_fixtureConsole->blockSignals(false);
    }
    recordEncoderStep(t.scene, before, what, bank, slot, home);
    m_doc->notifyFunctionChanged(t.scene->id());
}

void Workbench::encoderHomed(int bank, int slot)
{
    if (!isVisible())
        return;
    const EncoderTarget t = encoderTarget();
    if (!t.isValid())
    {
        encoderMessage(tr("nothing selected to turn"));
        return;
    }
    const QString what = EncoderBank::slotName(bank, slot);
    if (t.look != nullptr)
    {
        if (!t.look->encoderHome(bank, slot))
        {
            encoderMessage(tr("this look has no %1").arg(what.toLower()));
            return;
        }
    }
    else
    {
        PaletteOps *ops = m_doc->paletteOps();
        const QSharedPointer<Scene> before = ops != nullptr ? ops->sceneSnapshot(t.scene->id())
                                                            : QSharedPointer<Scene>();
        if (!EncoderAttributes::home(m_doc, t.scene, t.fixtures, bank, slot))
        {
            encoderMessage(tr("the selection has no %1, or a look drives it").arg(what.toLower()));
            return;
        }
        afterEncoderEdit(t, what, before, bank, slot, true);
    }
    m_encoderMessage = tr("%1 home").arg(what);
    refreshEncoderRings();
}
