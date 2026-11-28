/*
  Q Light Controller Plus
  audiomessage_test.cpp

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
#include <QTemporaryDir>

#include "audiomessage_test.h"
#include "audioplugincache.h"

void AudioMessage_Test::noDecoderLoaded()
{
    // A stub cache: nothing loaded, so no formats either.
    QCOMPARE(AudioPluginCache::cantPlayMessage(QStringLiteral("/shows/Opening Song.mp3"), 0, QStringList()),
             QStringLiteral("Can't play Opening Song.mp3: no audio decoder is loaded "
                            "(the app's audio library failed to load)"));
    // Even a WAV: the library, not the format.
    QVERIFY(AudioPluginCache::cantPlayMessage(QStringLiteral("a.wav"), 0, QStringList())
                .contains(QStringLiteral("no audio decoder is loaded")));
}

void AudioMessage_Test::unsupportedFormat()
{
    const QStringList formats = { QStringLiteral("*.wav"), QStringLiteral("*.flac"), QStringLiteral("*.wav") };
    QCOMPARE(AudioPluginCache::cantPlayMessage(QStringLiteral("/x/Song.mp3"), 1, formats),
             QStringLiteral("Can't play Song.mp3: MP3 isn't supported (supported: FLAC, WAV)"));
}

void AudioMessage_Test::noExtension()
{
    QCOMPARE(AudioPluginCache::cantPlayMessage(QStringLiteral("Song"), 1, { QStringLiteral("*.wav") }),
             QStringLiteral("Can't play Song: this format isn't supported (supported: WAV)"));
}

void AudioMessage_Test::anEmptyCacheSaysNoDecoder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AudioPluginCache cache(nullptr);
    QVERIFY(!cache.loadAttempted());
    cache.load(QDir(dir.path()));
    QVERIFY(cache.loadAttempted());
    QCOMPARE(cache.decoderCount(), 0);
    QVERIFY(cache.cantPlayMessage(QStringLiteral("a.mp3")).contains(QStringLiteral("no audio decoder is loaded")));
}

QTEST_MAIN(AudioMessage_Test)
