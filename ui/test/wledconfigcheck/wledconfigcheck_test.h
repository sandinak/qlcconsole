/*
  Q Light Controller Plus
  wledconfigcheck_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDCONFIGCHECK_TEST_H
#define WLEDCONFIGCHECK_TEST_H

#include <QObject>

class Doc;

class WledConfigCheck_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Driven through WLED's API: nothing may take it over with a stream. */
    void aLanternDrivenByItsApiMustNotTakeStreams();
    /** Driven by Art-Net: the device's protocol, universe, address and DMX
     *  mode are the patch's and the definition's. */
    void anArtNetLanternMustListenWhereItIsPatched();
    /** Read from the device, in the footer, fixed on the device, read back. */
    void aMismatchIsFoundShownAndFixed();

private:
    Doc *m_doc = nullptr;
};

#endif
