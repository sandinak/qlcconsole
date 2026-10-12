/*
  Q Light Controller Plus
  wled_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLED_TEST_H
#define WLED_TEST_H

#include <QObject>

class Wled_Test final : public QObject
{
    Q_OBJECT

private slots:
    void effectChannelsAreReadAsWledReadsThem();
    void colourModesAreSolidColours();
    void onlyChangesAreSentAndBurstsCollapse();
    void aWeakOrSilentLanternIsAProblem();
    void aWatchedLanternSaysWhatItsLedsShow();
    /** Against a real WLED, when WLED_REAL_IP names one (skipped otherwise):
     *  the look reaches it, and it answers. Its state is put back after. */
    void aRealLanternShowsTheLook();
};

#endif
