/*
  Q Light Controller Plus - Unit test
  showmanager_test.h

  The Shows timeline's undo: a coarse whole-timeline snapshot taken before
  each edit (ShowManager::pushUndoSnapshot) and put back by Undo.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SHOWMANAGER_TEST_H
#define SHOWMANAGER_TEST_H

#include <QObject>

class ShowManager;
class Show;
class Doc;

class ShowManager_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A function dropped on a track is one Undo away from gone; the item
     *  that was there before stays where it was. */
    void undoTakesBackADrop();
    /** Undo of a marker edit puts the label back, and a marker linked to a
     *  cue list keeps its link. */
    void undoPutsAMarkerBack();
    /** Undo of one edit leaves every track as it was set: hold last look,
     *  solo-safe, priority, intensity and a locked item survive the rebuild. */
    void undoKeepsEveryTracksSettings();

private:
    Doc *m_doc = nullptr;
    ShowManager *m_manager = nullptr;
    Show *m_show = nullptr;
    quint32 m_sceneA = 0;
    quint32 m_sceneB = 0;
};

#endif
