/*
  Q Light Controller Plus
  wleddevices_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDDEVICES_TEST_H
#define WLEDDEVICES_TEST_H

#include <QObject>

class Doc;

/* Backup lanterns: a WLED universe sent to several devices. */
class WledDevices_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** "outputIP2"... read and written; one device is what it always was. */
    void parametersReadAndWritten();
    /** Saved, loaded and saved again through the real WLED output: 1, 2 and
     *  3 devices, byte for byte. */
    void showsRoundTripByteIdentical_data();
    void showsRoundTripByteIdentical();
    /** Empty, malformed, twice, or another universe's: refused, and why. */
    void devicesAreValidated();
    /** Add, reorder (the primary), remove -- each backup keeps reading
     *  what it read. */
    void addReorderAndRemove();
    /** From Connections: applied to the patch and the output, one undo step. */
    void editedOnThePatchAsOneUndoStep();
    /** The footer names a silent backup after the fixture it mirrors. */
    void theFooterNamesASilentBackup();

private:
    Doc *m_doc = nullptr;
};

#endif
