/*
  Q Light Controller Plus
  surfacesetupui_test.h

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

#ifndef SURFACESETUPUI_TEST_H
#define SURFACESETUPUI_TEST_H

#include <QObject>
#include <QTemporaryDir>

class Doc;
class IOPluginStub;

/* Connections ▸ Control surfaces ▸ Controller setup: the Set Up dialog on a
   simulated PMJ Black 1 (its OpenDeck config, engine/test/common/surfacesim)
   and APC40 mkII, and the hub's row and Rescan. */
class SurfaceSetupUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** Opening checks; Align Lamps aligns (backup path shown); Undo Align. */
    void theDialogChecksAlignsAndUndoes();
    /** In Operate, a write asks; No writes nothing. */
    void inOperateTheDialogAsksFirst();
    /** Identify Lamps…: flash, press, next; Finish puts the board back. */
    void identifyPairsByPress();
    /** The APC40 mkII: its mode, Send Again; no lamp buttons. */
    void anInitOnlySurfaceOffersSendAgain();
    /** The hub's Controller setup row, its Set Up… and Rescan. */
    void theHubRowAndRescan();
    /** No feedback line: the hub row and the dialog offer Patch Feedback, one click pairs the board's own output. */
    void patchFeedbackIsOneClick();
    /** With QLC_SHOT_DIR: the dialog and the hub at three sizes. */
    void shots();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QTemporaryDir m_backups;
};

#endif
