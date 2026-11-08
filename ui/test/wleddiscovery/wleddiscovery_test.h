/*
  Q Light Controller Plus
  wleddiscovery_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDDISCOVERY_TEST_H
#define WLEDDISCOVERY_TEST_H

#include <QObject>

class Doc;

class WledDiscovery_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** The answer a real WLED gave (recorded on the show network) is read:
     *  its name, host, port, IP and MAC; a question or a broken packet is
     *  nothing, and never read past its end. */
    void aRealWledsAnswerIsRead();
    /** The question is one PTR question for _wled._tcp.local. */
    void theQuestionAsksForWleds();
    /** A fixture name becomes the name WLED takes for mDNS. */
    void hostNamesAreWhatWledTakes();
    /** Only the show's rig networks are asked while the gate holds. */
    void onlyRigNetworksAreAskedWhileTheGateHolds();
    /** A WLED found and not in the show is a greyed Inventory row; right-
     *  click ▸ Patch makes it a WLED fixture on a universe of its own, in
     *  one undo; it is in the show then, and back in "found" after undo. */
    void aFoundWledIsPatchedWithOneRightClick();
    /** A fixture learns its WLED's MAC from its IP, and keeps it in the
     *  show file. */
    void aFixtureLearnsItsWledsMacAndSavesIt();
    /** Its WLED answering at a new IP: a footer line, Follow It sends
     *  there, one undo. */
    void aWledAtANewAddressIsFollowed();
    /** Drifted from the show (its name, WLED sync, lit at power-on): Fix on
     *  Device findings; never Wi-Fi, LED type, pins or colour order. */
    void driftIsAFixOnDeviceFinding();
    /** Set Up asks once with what changes, writes only those keys, reads
     *  them back; Not Now writes nothing. */
    void setUpAsksOnceWritesTheKeysAndReadsBack();
    /** A Teaching show holding real output is not asked and writes nothing. */
    void aHeldClassroomIsNotSetUp();
    /** The real network, by hand only (QLC_WLED_LIVE_BROWSE=1): asks, reads
     *  each WLED's info, writes nothing. Skipped in the gate. */
    void liveBrowse();

private:
    Doc *m_doc = nullptr;
};

#endif
