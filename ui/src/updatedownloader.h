/*
  Q Light Controller Plus
  updatedownloader.h

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

#ifndef UPDATEDOWNLOADER_H
#define UPDATEDOWNLOADER_H

#include <QObject>
#include <QFile>

#include "updatechecker.h"
#include "qlcui_export.h"

class QNetworkAccessManager;
class QNetworkReply;

/** @addtogroup ui UI
 * @{
 */

/**
 * Fetch one release download and prove it is the one published.
 *
 * First the release's SHA256SUMS, then the file itself -- into a folder of
 * its own under the temporary directory, with progress -- and then its
 * SHA-256 against the line SHA256SUMS has for it. No SHA256SUMS, no line
 * for the file, a size other than GitHub listed or a different hash: the
 * file is deleted and finished() says why. Nothing is installed from here.
 */
class QLC_UI_EXPORT UpdateDownloader : public QObject
{
    Q_OBJECT

public:
    explicit UpdateDownloader(QObject *parent = nullptr);
    ~UpdateDownloader();

    /** Download @p release's asset @p assetIndex into @p dir (default: a
     *  folder of its own in the temporary directory). */
    void start(const UpdateRelease &release, int assetIndex, const QString &dir = QString());
    void cancel();
    bool isRunning() const { return m_reply != nullptr; }

    /** The verified file, once finished(true). */
    QString filePath() const { return m_filePath; }

    /** SHA-256 of a file, lowercase hex; empty if it cannot be read. */
    static QString sha256Of(const QString &path);

    void setTimeout(int msec) { m_timeout = msec; }

signals:
    void progress(qint64 received, qint64 total);
    void finished(bool ok, const QString &error);

private slots:
    void slotSumsFinished();
    void slotReadyRead();
    void slotFileFinished();

private:
    void fail(const QString &error);
    QNetworkReply *get(const QUrl &url);

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;
    UpdateAsset m_asset;
    QString m_expectedSha;
    QString m_filePath;
    QFile m_file;
    int m_timeout;
};

/** @} */

#endif
