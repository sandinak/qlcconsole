/*
  Q Light Controller Plus
  audioplugincache.h

  Copyright (c) Massimo Callegari

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

#ifndef AUDIOPLUGINCACHE_H
#define AUDIOPLUGINCACHE_H

#include <QtGlobal>
 #include <QAudioDevice>
#include <QObject>
#include <QDir>

#include "audiorenderer.h"
#include "audiocapture.h"

/** @addtogroup engine Engine
 * @{
 */

class AudioDecoder;

class AudioPluginCache final : public QObject
{
    Q_OBJECT

public:
    AudioPluginCache(QObject* parent);
    ~AudioPluginCache();

    /** Load plugins from the given directory. */
    void load(const QDir& dir);

    /** Get a list of the audio formats supported by the
     *  loaded plugins */
    QStringList getSupportedFormats();

    /** Get an audio decoder instance suitable for the given $filename.
     *  If $filename can't be decoded, this method returns NULL */
    AudioDecoder *getDecoderForFile(const QString& filename);

    /** How many decoder plugins loaded; 0 when none did (the app's audio
     *  library failed to load, or no plugin is installed). */
    int decoderCount() const;

    /** Why each plugin file in the folder failed to load (one line each),
     *  logged when load() ran. Empty when they all did. */
    QStringList loadErrors() const;

    /** Whether load() has run (a Doc that never loads plugins -- a headless
     *  tool -- has no verdict on whether audio can play). */
    bool loadAttempted() const;

    /** What to tell a person whose file @a fileName can't be played, given
     *  how many decoders loaded and the formats they list ("*.wav", ...):
     *  no decoder at all is the app's fault, a format miss is the file's. */
    static QString cantPlayMessage(const QString& fileName, int decoderCount,
                                   const QStringList& supportedFormats);
    /** The same, from this cache's own state. */
    QString cantPlayMessage(const QString& fileName);

    /** Get the list of cached audio devices detected on creation */
    QList<AudioDeviceInfo> audioDevicesList() const;

    /** Return a Qt output device info match based on $devName */
    QAudioDevice getOutputDeviceInfo(QString devName) const;

private:
    /** a map of the vailable plugins ordered by priority */
    QMap<int, QString> m_pluginsMap;

    QStringList m_loadErrors;
    bool m_loadAttempted = false;

    /** a list of all input/output audio devices arranged in
     *  AudioDeviceInfo structure */
    QList<AudioDeviceInfo> m_audioDevicesList;

    /** a list of output audio device for faster lookup */
    QList<QAudioDevice> m_outputDevicesList;
};

/** @} */

#endif
