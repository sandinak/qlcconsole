/*
  Q Light Controller Plus
  capturemanager.h

  Copyright (c) Heikki Junnila

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

#ifndef CAPTUREMANAGER_H
#define CAPTUREMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>

class Doc;
class Scene;

/**
 * CaptureManager records DMX-level overrides driven by VC widgets while
 * the show is running, so the user can fold those edits back into the
 * scenes that originally asserted those channels.
 *
 * Lifecycle:
 *   - setCapturing(true) starts recording overrides; widgets call
 *     recordOverride() when their level-mode output changes.
 *   - Caller invokes buildPlan() to compute, per running Scene, the LTP
 *     subset of captured channels along with conflict and chaser flags.
 *   - applyInPlace() mutates the targeted Scenes; saveAsNew() clones them
 *     under user-supplied names and rewires the running Collection.
 */
class CaptureManager : public QObject
{
    Q_OBJECT

public:
    /** A single captured channel override. */
    struct Override
    {
        quint32 fxi;
        quint32 channel;
        uchar value;
        QString sourceName;
    };

    /** Per-Scene apply plan computed from current overrides. */
    struct ScenePlan
    {
        Scene* scene;
        QList<Override> overrides;
        QSet<QPair<quint32, quint32> > conflicts;
        QSet<QPair<quint32, quint32> > chaserDriven;
    };

    explicit CaptureManager(Doc* doc, QObject* parent = nullptr);

    bool isCapturing() const;
    void setCapturing(bool on);

    /** Called by VC widgets while capturing. */
    void recordOverride(quint32 fxi, quint32 channel, uchar value,
                        const QString& sourceName);

    /** Discard recorded overrides without applying. */
    void clear();

    /** Number of distinct (fxi, channel) overrides currently held. */
    int overrideCount() const;

    /**
     * Build the per-Scene apply plan against the currently running
     * collection-of-scenes context. Picks LTP per channel from running
     * scenes; flags conflicts where multiple running scenes touch a
     * captured channel; flags channels that are currently being driven
     * by a Chaser's active step (excluded from apply by default).
     */
    QList<ScenePlan> buildPlan() const;

    /** Mutate Scenes in place per plan. Marks Doc modified. */
    void applyInPlace(const QList<ScenePlan>& plan);

    /**
     * Clone Scenes per plan, applying overrides to the clones, rename
     * each clone using the supplied map (keyed by original Scene id),
     * place the clones in the same folder as the originals, and rewire
     * any running Collection that contained the original Scene to point
     * at the clone instead.
     */
    void saveAsNew(const QList<ScenePlan>& plan,
                   const QHash<quint32, QString>& newNamesById);

signals:
    void capturingChanged(bool on);
    void overrideRecorded(quint32 fxi, quint32 channel, uchar value);
    void changesApplied();

private slots:
    void slotFunctionStarted(quint32 id);
    void slotFunctionStopped(quint32 id);

private:
    Doc* m_doc;
    bool m_capturing;

    QHash<QPair<quint32, quint32>, Override> m_overrides;

    QList<quint32> m_runningOrder;
    QSet<quint32> m_running;
};

#endif
