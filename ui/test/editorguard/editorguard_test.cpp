/*
  Q Light Controller Plus - Test Unit
  editorguard_test.cpp

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

#include <QtTest>
#include <QPointer>
#include <QPushButton>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "timecodecalibrationdialog.h"
#include "functioneditorguard.h"
#include "collectioneditor.h"
#include "rgbmatrixeditor.h"
#include "functionstreewidget.h"
#include "functionmanager.h"
#include "chasereditor.h"
#include "scripteditor.h"
#include "sceneeditor.h"
#include "videoeditor.h"
#include "audioeditor.h"
#include "showeditor.h"
#include "lookeditor.h"
#include "efxeditor.h"
#include "showtreecontroller.h"
#include "collection.h"
#include "rgbmatrix.h"
#include "sequence.h"
#include "fixture.h"
#include "chaser.h"
#include "script.h"
#include "scene.h"
#include "video.h"
#include "audio.h"
#include "show.h"
#include "efx.h"
#include "doc.h"
#undef private
#undef protected

#include "editorguard_test.h"

void EditorGuard_Test::init()
{
    m_doc = new Doc(this);
    m_doc->inputOutputMap()->addUniverse();
    // A fixture the functions use: Doc::clearContents() removes it after
    // the functions are gone, and an editor answered fixtureRemoved by
    // reading its deleted function (scene, EFX).
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Dimmers");
    fxi->setChannels(4);
    fxi->setAddress(0);
    fxi->setUniverse(0);
    QVERIFY(m_doc->addFixture(fxi));
}

void EditorGuard_Test::cleanup()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    delete m_doc;
    m_doc = nullptr;
}

Function *EditorGuard_Test::make(int type)
{
    const quint32 fxi = m_doc->fixtures().first()->id();
    Function *f = nullptr;
    switch (type)
    {
        case Function::SceneType:
        {
            Scene *s = new Scene(m_doc);
            s->setValue(fxi, 0, 255);
            f = s;
            break;
        }
        case Function::ChaserType:
        {
            Scene *s = new Scene(m_doc);
            s->setValue(fxi, 1, 128);
            m_doc->addFunction(s);
            Chaser *c = new Chaser(m_doc);
            c->addStep(ChaserStep(s->id()));
            f = c;
            break;
        }
        case Function::EFXType:
        {
            EFX *e = new EFX(m_doc);
            EFXFixture *ef = new EFXFixture(e);
            ef->setHead(GroupHead(fxi, 0));
            e->addFixture(ef);
            f = e;
            break;
        }
        case Function::RGBMatrixType: f = new RGBMatrix(m_doc); break;
        case Function::CollectionType: f = new Collection(m_doc); break;
        case Function::ScriptType: f = new Script(m_doc); break;
        case Function::ShowType: f = new Show(m_doc); break;
        case Function::AudioType: f = new Audio(m_doc); break;
        case Function::VideoType: f = new Video(m_doc); break;
        default: break;
    }
    if (f != nullptr)
    {
        f->setName(QStringLiteral("Guarded"));
        m_doc->addFunction(f);
    }
    return f;
}

void EditorGuard_Test::eachEditorLetsGoOfItsFunction_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("cleared");
    const QList<QPair<const char *, int>> types = {
        { "scene", Function::SceneType }, { "chaser", Function::ChaserType },
        { "efx", Function::EFXType }, { "rgbmatrix", Function::RGBMatrixType },
        { "collection", Function::CollectionType }, { "script", Function::ScriptType },
        { "show", Function::ShowType }, { "audio", Function::AudioType },
        { "video", Function::VideoType } };
    for (const auto &t : types)
    {
        QTest::newRow(QByteArray(t.first) + " deleted") << t.second << false;
        QTest::newRow(QByteArray(t.first) + " show cleared") << t.second << true;
    }
}

/* The editor stays (its host lets it go later, or never: a host that
   doesn't listen); its function is deleted, by Build or an undo, or with
   the whole show (File > Open, New). Then everything the Doc still says
   (fixtures removed, Operate and back, a GO taking the stage) must not
   reach into the freed function, and its own destructor neither
   (2026-11-13: ChaserEditor stopped its freed chaser going to Operate). */
void EditorGuard_Test::eachEditorLetsGoOfItsFunction()
{
    QFETCH(int, type);
    QFETCH(bool, cleared);

    Function *f = make(type);
    QVERIFY(f != nullptr);
    const quint32 fid = f->id();

    QWidget *ed = nullptr;
    std::function<bool()> letGo;
    switch (type)
    {
        case Function::SceneType:
        {
            SceneEditor *e = new SceneEditor(nullptr, qobject_cast<Scene *>(f), m_doc, false);
            letGo = [e]() { return e->m_scene.isNull(); };
            ed = e;
            break;
        }
        case Function::ChaserType:
        {
            ChaserEditor *e = new ChaserEditor(nullptr, qobject_cast<Chaser *>(f), m_doc);
            letGo = [e]() { return e->m_chaser.isNull(); };
            ed = e;
            break;
        }
        case Function::EFXType:
        {
            EFXEditor *e = new EFXEditor(nullptr, qobject_cast<EFX *>(f), m_doc);
            letGo = [e]() { return e->m_efx.isNull(); };
            ed = e;
            break;
        }
        case Function::RGBMatrixType:
        {
            RGBMatrixEditor *e = new RGBMatrixEditor(nullptr, qobject_cast<RGBMatrix *>(f), m_doc);
            letGo = [e]() { return e->m_matrix.isNull() && !e->m_previewTimer->isActive(); };
            ed = e;
            break;
        }
        case Function::CollectionType:
        {
            CollectionEditor *e = new CollectionEditor(nullptr, qobject_cast<Collection *>(f), m_doc);
            letGo = [e]() { return e->m_collection.isNull(); };
            ed = e;
            break;
        }
        case Function::ScriptType:
        {
            ScriptEditor *e = new ScriptEditor(nullptr, qobject_cast<Script *>(f), m_doc);
            letGo = [e]() { return e->m_script.isNull(); };
            ed = e;
            break;
        }
        case Function::ShowType:
        {
            ShowEditor *e = new ShowEditor(nullptr, qobject_cast<Show *>(f), m_doc);
            letGo = [e]() { return e->m_show.isNull(); };
            ed = e;
            break;
        }
        case Function::AudioType:
        {
            AudioEditor *e = new AudioEditor(nullptr, qobject_cast<Audio *>(f), m_doc);
            letGo = [e]() { return e->m_audio.isNull(); };
            ed = e;
            break;
        }
        case Function::VideoType:
        {
            VideoEditor *e = new VideoEditor(nullptr, qobject_cast<Video *>(f), m_doc);
            letGo = [e]() { return e->m_video.isNull(); };
            ed = e;
            break;
        }
        default:
            QFAIL("no editor for this type");
    }
    QPointer<QWidget> guard(ed);
    ed->show();
    QVERIFY(ed->isEnabled());
    QVERIFY(!letGo());

    if (cleared)
        m_doc->clearContents();
    else
        QVERIFY(m_doc->deleteFunction(fid));
    QVERIFY(m_doc->function(fid) == nullptr);

    // Cut off: nothing more from the Doc, nothing to click, and the
    // pointer null for whatever its host still calls.
    QVERIFY2(letGo(), "the editor still holds its deleted function");
    QVERIFY(!ed->isEnabled());
    QVERIFY(ed->isHidden());

    // What used to reach the freed function: Operate and back (chaser,
    // EFX, RGB matrix, scene), a GO taking the stage, a fixture removed.
    m_doc->setMode(Doc::Operate);
    QStringList held;
    emit m_doc->showTree()->stageTakenForRun(&held);
    QVERIFY(held.isEmpty());
    m_doc->setMode(Doc::Design);
    if (!cleared)
        m_doc->deleteFixture(m_doc->fixtures().first()->id());
    QTest::qWait(50);           // the RGB matrix's preview timer, had it run

    // Its host lets it go at last: the destructor stops nothing freed.
    QVERIFY(!guard.isNull());
    delete ed;
    QVERIFY(guard.isNull());
}

/* The Functions window (Classic) closes the editor of a function deleted
   elsewhere: Build, the Show Tree, an undo. */
void EditorGuard_Test::theFunctionsWindowClosesTheEditor()
{
    Function *efx = make(Function::EFXType);
    FunctionManager fm(nullptr, m_doc);
    fm.resize(1000, 700);
    fm.show();
    fm.editFunction(efx);
    QPointer<QWidget> ed = fm.m_editor;
    QVERIFY(!ed.isNull());
    QVERIFY(qobject_cast<EFXEditor *>(ed) != nullptr);

    m_doc->deleteFunction(efx->id());
    QVERIFY(fm.m_editor == nullptr);
    QCOMPARE(fm.m_editedFunctionId, Function::invalidId());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY2(ed.isNull(), "the editor outlived its function");

    m_doc->setMode(Doc::Operate);
    m_doc->setMode(Doc::Design);
}

/* A sequence's editor is two: its steps and its bound scene's channels.
   The bound scene going first leaves the steps editor in place and the
   scene editor cut off; the sequence going takes both. */
void EditorGuard_Test::aSequenceEditorLetsGoOfItsBoundScene()
{
    Scene *bound = qobject_cast<Scene *>(make(Function::SceneType));
    Sequence *seq = new Sequence(m_doc);
    seq->setBoundSceneID(bound->id());
    seq->setName("Seq");
    QVERIFY(m_doc->addFunction(seq));
    seq->addStep(ChaserStep(bound->id()));

    FunctionManager fm(nullptr, m_doc);
    fm.resize(1000, 700);
    fm.show();
    fm.editFunction(seq);
    QPointer<QWidget> steps = fm.m_editor;
    QPointer<SceneEditor> channels = qobject_cast<SceneEditor *>(fm.m_scene_editor);
    QVERIFY(!steps.isNull());
    QVERIFY(!channels.isNull());

    m_doc->deleteFunction(bound->id());
    QVERIFY(channels->m_scene.isNull());
    QVERIFY(channels->m_source == nullptr);     // its values off the rig
    QVERIFY(!channels->isEnabled());
    m_doc->setMode(Doc::Operate);
    m_doc->setMode(Doc::Design);

    m_doc->deleteFunction(seq->id());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(steps.isNull());
    QVERIFY(channels.isNull());
    m_doc->setMode(Doc::Operate);
    m_doc->setMode(Doc::Design);
}

/* The Look Editor's context scene: a host that doesn't hand over nullptr
   first leaves no dangling one. */
void EditorGuard_Test::theLookEditorForgetsItsScene()
{
    Scene *s = qobject_cast<Scene *>(make(Function::SceneType));
    LookEditor le(m_doc);
    le.setContextScene(s);
    QVERIFY(le.m_contextScene == s);
    QVERIFY(le.hasTargets());

    m_doc->deleteFunction(s->id());
    QVERIFY(le.m_contextScene.isNull());
    QVERIFY(!le.hasTargets());
}

/* Timecode calibration is modeless: the show it calibrates can be deleted
   (or another show opened) while it is up; its live refresh and Apply then
   read and wrote the freed show. */
void EditorGuard_Test::theTimecodeDialogForgetsItsShow()
{
    Show *show = qobject_cast<Show *>(make(Function::ShowType));
    TimecodeCalibrationDialog d(m_doc, show);
    QVERIFY(d.m_show == show);

    m_doc->clearContents();
    QVERIFY(d.m_show.isNull());
    d.refreshStats();
    QVERIFY(!d.m_applyBtn->isEnabled());
    d.slotApply();              // nothing to write to
}

QTEST_MAIN(EditorGuard_Test)
