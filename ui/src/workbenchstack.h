/*
  Q Light Controller Plus
  workbenchstack.h

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

#ifndef WORKBENCHSTACK_H
#define WORKBENCHSTACK_H

#include <QStringList>
#include <QObject>
#include <QList>
#include <functional>

#include "grouphead.h"
#include "function.h"

class QMimeData;
class QWidget;
class QPoint;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * What the Workbench has loaded: "a stack of things that play together"
 * (docs/design/WORKFLOW_DESIGN.md §3.4). The Workbench shows every back end the same
 * way -- a header, one row per layer, an editor for the selected row, a
 * live preview -- and asks the back end what is there and to make each
 * edit, one step on the shared history.
 *
 * FunctionStack (below) edits real functions: a scene, a collection's
 * members. W3's TreeCueStack will edit a Show Tree cue through its controller.
 */
class WorkbenchStack : public QObject
{
    Q_OBJECT

public:
    /** One row at the top of the Workbench. */
    struct Layer
    {
        enum Kind
        {
            SceneLayer,     //!< a scene: opens up into its targets and looks
            FunctionLayer,  //!< played as it is (▣): an EFX, RGB Matrix, chaser...
            CueLayer        //!< W3: a Show Tree cue's layer, one row per attribute
        };
        /** W3: one attribute row of a CueLayer ("Colour  Blue"). */
        struct Attribute
        {
            QString key;            //!< ShowTree key ("Colour", "Effect.Color")
            QString label;          //!< "Colour"
            QString value;          //!< "Blue + Red", "off"
            QString origin;         //!< "" its own; "from 0.0.2", "cue look"...
            bool inherited = false; //!< not set on this layer here: grey
            quint32 palette = 0xFFFFFFFF;   //!< the value's (first) palette
        };
        Kind kind = FunctionLayer;
        quint32 functionId = Function::invalidId();
        QString label;      //!< the row's name ("" the function's)
        QString badge;      //!< the badge column ("also in 2", "EFX ▣")
        QString tip;        //!< the hover
        /** W3 (TreeCueStack): carried into the cue (tracked, included, base),
         *  not its own -- a grey row; a value set on it makes it own. */
        bool tracked = false;
        QList<Attribute> attributes;
        quint32 cueId = 0xFFFFFFFF;     //!< the cue it is a layer of
        quint32 layerId = 0xFFFFFFFF;   //!< its id there (invalid: carried in)
        QString targetSignature;        //!< ShowTree::Target::signature()
    };

    explicit WorkbenchStack(Doc *doc, QObject *parent = nullptr);
    virtual ~WorkbenchStack();

    Doc *doc() const { return m_doc; }

    /** Who it is, for ↩ and for telling two loads apart ("f:12"). */
    virtual QString key() const = 0;
    /** The same thing loaded afresh (for ↩), owned by @a parent; null when
     *  it is gone. */
    virtual WorkbenchStack *reopen(QObject *parent) const = 0;
    virtual QString title() const = 0;
    /** The colour the title is shown in ("#rrggbb"; "" the theme's): a
     *  Show Tree cue's text colour. */
    virtual QString titleColour() const { return QString(); }
    /** "Scene", "Collection"... (the header's tag). */
    virtual QString kindText() const = 0;
    /** Where it is ("Show/0-Preshow"; "" none). */
    virtual QString where() const = 0;
    /** The header's detail lines: what it holds, where it is used, what
     *  fires it, what is wrong with it. */
    virtual QStringList infoLines() const = 0;

    /** The function that plays in Design (invalid: nothing). */
    virtual quint32 previewFunction() const = 0;
    /** The function loaded (invalid for a back end that isn't one). */
    virtual quint32 rootFunction() const { return Function::invalidId(); }

    virtual QList<Layer> layers() const = 0;

    /** Edited somewhere else (a cue list's collection, D13): why; "" no. */
    virtual QString readOnlyReason() const { return QString(); }

    virtual bool canAddLayers() const { return false; }
    virtual bool canRemoveLayers() const { return false; }
    virtual bool canReorderLayers() const { return false; }
    /** What adding @a functionIds would do, or "!why not". */
    virtual QString addPlan(const QList<quint32> &functionIds) const;

    /** Add @a functionIds as layers before layer @a at (-1: the end). One
     *  step. */
    virtual bool addLayers(const QList<quint32> &functionIds, int at);
    /** Take layers @a indexes out (the functions stay). One step. */
    virtual bool removeLayers(const QList<int> &indexes);
    /** Move layer @a from to place @a to. One step. */
    virtual bool moveLayer(int from, int to);
    /** Make a new function of @a type -- an empty scene, an EFX on
     *  @a heads, an RGB Matrix on group @a group -- and add it after layer
     *  @a after (-1: the end). One step. Its id, or invalid. */
    virtual quint32 addNew(Function::Type type, int after, const QList<GroupHead> &heads,
                           quint32 group);

    /************************************************************************
     * W3: what a Show Tree cue (TreeCueStack) needs from the Workbench. The
     * defaults: none of it.
     ************************************************************************/
    /** Drops on the rows are the back end's (all of them, any MIME). */
    virtual bool takesDrops() const { return false; }
    /** What dropping @a mime on layer @a layer (-1: below the rows) would
     *  do; "!why" refused, "" nothing. */
    virtual QString dropPlanOn(int layer, const QMimeData *mime, bool separate) const;
    /** Drop it: one step. */
    virtual bool dropOn(int layer, const QMimeData *mime, bool separate);
    /** The context menu of layer @a layer (or its attribute row
     *  @a attribute, -1: the layer). False: the Workbench's own. */
    virtual bool showMenu(int layer, int attribute, const QPoint &globalPos);
    /** ⌫ on attribute row @a attribute of layer @a layer: one step. */
    virtual bool clearAttribute(int layer, int attribute);
    /** A page of the back end's own for the editor pane (layer @a layer,
     *  -1: the whole), lent: handed back with returnPane(). Null: none. */
    virtual QWidget *lendPane(int layer, int attribute);
    virtual void returnPane(QWidget *pane);
    /** The fixtures layer @a layer lights (the 2D view highlights them). */
    virtual QList<quint32> layerFixtures(int layer) const;
    /** The layer that lights fixture @a fixtureId (-1: none). */
    virtual int layerOfFixture(quint32 fixtureId) const;
    /** The scene layer @a layer plays (a Look Editor's context). */
    virtual quint32 layerScene(int layer) const;
    /** The back end plays what is loaded itself (the Show Tree's preview),
     *  not previewFunction() under the Workbench. */
    virtual bool playsItself() const { return false; }
    virtual void startOwnPreview() {}
    virtual void stopOwnPreview() {}
    /** "▶ On the rig: …" while it plays (empty: the Workbench's words). */
    virtual QString ownPreviewText() const { return QString(); }
    /** RC8: its own preview is held by a running show (a GO took the
     *  stage): the Workbench offers Play instead of Stop. */
    virtual bool ownPreviewHeld() const { return false; }
    /** Play over the running show: the run's hold lets go. */
    virtual void releaseRunHold() {}
    /** In context (all of it) or Solo (only the selected layer). */
    virtual bool canSolo() const { return false; }
    virtual bool solo() const { return false; }
    virtual void setSolo(bool solo) { Q_UNUSED(solo) }
    /** The layer selected now (Solo plays it; -1: none). */
    virtual void setFocusLayer(int layer) { Q_UNUSED(layer) }

    /************************************************************************
     * W2: chasers, shared members
     ************************************************************************/
public:
    /** A widget of the back end's own under the header (a chaser's step
     *  walker), owned by @a parent; null: none. */
    virtual QWidget *createHeaderWidget(QWidget *parent);
    /** Layer @a index is shared (it is used elsewhere too): split it off as
     *  a copy of its own, in its place. One step. The copy's id, or invalid. */
    virtual quint32 makeOwnCopy(int index);
    /** Whether layer @a index can be split off (makeOwnCopy()). */
    virtual bool canMakeOwnCopy(int index) const;

signals:
    /** Something the header or the rows show changed. */
    void changed();
    /** The layers are another set now (a chaser walked to another step):
     *  show them afresh, play what now previews. */
    void layersReplaced();

protected:
    Doc *m_doc;
};

/**
 * The FunctionStack back end: a scene (one layer), a collection (one layer
 * per member: a scene opens up, anything else plays as it is), a chaser
 * (walked one step at a time, W2: the step's function opened up as layers,
 * a collection step's members editable as a collection's are), or any
 * other function (one row, its stock editor).
 */
class FunctionStack final : public WorkbenchStack
{
    Q_OBJECT

public:
    FunctionStack(Doc *doc, quint32 functionId, QObject *parent = nullptr);

    quint32 functionId() const { return m_fid; }
    Function *function() const;

    QString key() const override;
    WorkbenchStack *reopen(QObject *parent) const override;
    QString title() const override;
    QString kindText() const override;
    QString where() const override;
    QStringList infoLines() const override;
    quint32 previewFunction() const override;
    quint32 rootFunction() const override { return m_fid; }
    QList<Layer> layers() const override;
    QString readOnlyReason() const override;

    bool canAddLayers() const override;
    bool canRemoveLayers() const override;
    bool canReorderLayers() const override;
    QString addPlan(const QList<quint32> &functionIds) const override;
    bool addLayers(const QList<quint32> &functionIds, int at) override;
    bool removeLayers(const QList<int> &indexes) override;
    bool moveLayer(int from, int to) override;
    quint32 addNew(Function::Type type, int after, const QList<GroupHead> &heads,
                   quint32 group) override;

    /** Where function @a fid is used, but here: "0.0.3-Announcer",
     *  "Full Show (step 2)" -- for the "also in" badge. */
    static QStringList alsoIn(Doc *doc, quint32 fid, quint32 here);
    /** A collection, chaser or show that holds @a fid, at any depth (so
     *  adding @a fid to @a container would loop). */
    static bool contains(Doc *doc, quint32 container, quint32 fid);

    /************************************************************************
     * W2: a chaser, walked step by step
     ************************************************************************/
public:
    QWidget *createHeaderWidget(QWidget *parent) override;
    quint32 makeOwnCopy(int index) override;
    bool canMakeOwnCopy(int index) const override;

    /** The chaser loaded (null: something else is). */
    class Chaser *chaser() const;
    /** The step walked to (0 for anything but a chaser). */
    int step() const { return m_step; }
    int stepCount() const;
    /** Walk to step @a index (clamped): its layers come up. False: no
     *  chaser, or already there. */
    bool setStep(int index);
    /** The function of the step walked to (invalid: none). */
    quint32 stepFunction() const;

    /** Change chaser step @a index with @a edit, one step on the history
     *  named @a label. */
    bool editStep(int index, const QString &label, const std::function<void(class ChaserStep &)> &edit);
    /** Insert @a functionIds as steps of the chaser loaded before step
     *  @a at (-1: the end). One step. */
    bool insertSteps(const QList<quint32> &functionIds, int at);
    bool removeStep(int index);
    bool moveStep(int from, int to);
    /** ⇧⌘N: a new empty scene, filed with the chaser, as a step after the
     *  step walked to (the first, when it has none) -- one step -- walked
     *  to. The scene's id (invalid: no chaser, or read-only). */
    quint32 addNewStep();
    /** The step walked to plays a shared function: split it off (Make
     *  this step's own copy). One step. The copy's id, or invalid. */
    quint32 makeStepOwnCopy();

    /** What adding @a functionIds as steps of chaser @a chaserId would do,
     *  or "!why not". */
    static QString stepsPlan(Doc *doc, quint32 chaserId, const QList<quint32> &functionIds);
    /** Add @a functionIds as steps of chaser @a chaserId before step @a at
     *  (-1: the end), timed as the Chaser editor adds them. One step. */
    static bool addSteps(Doc *doc, quint32 chaserId, const QList<quint32> &functionIds, int at = -1);
    /** Chaser from selection: a new chaser with @a functionIds as its steps,
     *  in that order, filed in @a folder. One step. Its id, or invalid. */
    static quint32 makeChaser(Doc *doc, const QList<quint32> &functionIds, const QString &folder);
    /** Split member @a index of collection @a containerId (or step @a index
     *  of a chaser) off as a copy of its own, in its place. One step. */
    static quint32 makeOwnCopy(Doc *doc, quint32 containerId, int index);

private:
    bool isCollection() const;
    /** What the edits go to: the collection loaded, or the collection a
     *  chaser's step plays (null: none). */
    class Collection *editCollection() const;
    /** What the layers are of: the function loaded, or a chaser step's. */
    Function *layerSource() const;

    quint32 m_fid;
    int m_step = 0;
};

/** @} */

#endif
