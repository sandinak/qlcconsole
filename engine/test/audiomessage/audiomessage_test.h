/*
  Q Light Controller Plus
  audiomessage_test.h

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

#ifndef AUDIOMESSAGE_TEST_H
#define AUDIOMESSAGE_TEST_H

#include <QObject>

/* What a person is told when a sound file can't be added: the app's audio
   library not loading is said as that, a format miss as the format. */
class AudioMessage_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** No decoder loaded: the app's fault, whatever the file is. */
    void noDecoderLoaded();
    /** A decoder loaded that lacks the format: the format, and what works. */
    void unsupportedFormat();
    /** A file with no extension. */
    void noExtension();
    /** The cache itself: an empty folder loads nothing, and says so. */
    void anEmptyCacheSaysNoDecoder();
};

#endif
