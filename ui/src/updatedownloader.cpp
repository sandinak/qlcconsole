/*
  Q Light Controller Plus
  updatedownloader.cpp

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

#include <QNetworkAccessManager>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

#include "updatedownloader.h"

UpdateDownloader::UpdateDownloader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_timeout(60000)
{
}

UpdateDownloader::~UpdateDownloader()
{
    cancel();
}

QNetworkReply *UpdateDownloader::get(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("qlcconsole"));
    // GitHub hands assets out through a redirect to its storage; follow it,
    // never from https down to http.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(m_timeout);
    return m_nam->get(req);
}

void UpdateDownloader::start(const UpdateRelease &release, int assetIndex, const QString &dir)
{
    cancel();
    m_filePath.clear();
    m_expectedSha.clear();
    if (assetIndex < 0 || assetIndex >= release.assets.count())
    {
        fail(tr("This release has no download for this computer."));
        return;
    }
    m_asset = release.assets.at(assetIndex);

    // The asset's name becomes a file name: nothing that walks out of the folder.
    if (m_asset.name.contains(QLatin1Char('/')) || m_asset.name.contains(QLatin1Char('\\'))
        || m_asset.name.startsWith(QLatin1Char('.')))
    {
        fail(tr("The download's name is not a plain file name."));
        return;
    }

    QString folder = dir;
    if (folder.isEmpty())
        folder = QDir::temp().filePath(QStringLiteral("qlcconsole-update-%1").arg(release.version));
    QDir d(folder);
    if (d.exists())
        d.removeRecursively();
    if (QDir().mkpath(folder) == false)
    {
        fail(tr("Could not make a folder for the download (%1).").arg(folder));
        return;
    }
    m_filePath = QDir(folder).filePath(m_asset.name);

    UpdateAsset sums;
    for (const UpdateAsset &a : release.assets)
        if (a.name == UpdateChecker::checksumAssetName())
            sums = a;
    if (sums.name.isEmpty())
    {
        fail(tr("This release has no %1, so its download cannot be checked. "
                "It will not be installed.").arg(UpdateChecker::checksumAssetName()));
        return;
    }

    m_reply = get(sums.url);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateDownloader::slotSumsFinished);
}

void UpdateDownloader::slotSumsFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        fail(tr("Could not get %1: %2").arg(UpdateChecker::checksumAssetName(), reply->errorString()));
        return;
    }
    const QHash<QString, QString> sums = UpdateChecker::parseSha256Sums(reply->read(1 << 20));
    m_expectedSha = sums.value(m_asset.name);
    if (m_expectedSha.isEmpty())
    {
        fail(tr("%1 has no checksum for %2. It will not be installed.")
                 .arg(UpdateChecker::checksumAssetName(), m_asset.name));
        return;
    }

    m_file.setFileName(m_filePath);
    if (m_file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
    {
        fail(tr("Could not write %1.").arg(m_filePath));
        return;
    }
    m_reply = get(m_asset.url);
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownloader::slotReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        emit progress(got, total > 0 ? total : m_asset.size);
    });
    connect(m_reply, &QNetworkReply::finished, this, &UpdateDownloader::slotFileFinished);
}

void UpdateDownloader::slotReadyRead()
{
    if (m_reply == nullptr)
        return;
    // Only the final answer's bytes: a redirect's body is not the file.
    const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code >= 300)
        return;
    const QByteArray chunk = m_reply->readAll();
    if (m_file.write(chunk) != chunk.size())
    {
        m_reply->abort();
    }
}

void UpdateDownloader::slotFileFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError
        && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 300)
    {
        const QByteArray tail = reply->readAll();     // anything not yet read
        if (m_file.write(tail) != tail.size())
        {
            m_file.close();
            fail(tr("Could not write %1.").arg(m_filePath));
            return;
        }
    }
    m_file.close();

    if (reply->error() != QNetworkReply::NoError)
    {
        fail(tr("The download failed: %1").arg(reply->errorString()));
        return;
    }
    const qint64 size = QFileInfo(m_filePath).size();
    if (m_asset.size > 0 && size != m_asset.size)
    {
        fail(tr("The download is %1 bytes, not the %2 GitHub lists. It will not be installed.")
                 .arg(size).arg(m_asset.size));
        return;
    }
    const QString got = sha256Of(m_filePath);
    if (got.isEmpty() || got != m_expectedSha)
    {
        qWarning() << "[updates] SHA-256 mismatch for" << m_asset.name
                   << "expected" << m_expectedSha << "got" << got;
        fail(tr("The download's checksum does not match the one published with the release. "
                "It has been deleted and will not be installed."));
        return;
    }
    emit finished(true, QString());
}

QString UpdateDownloader::sha256Of(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly) == false)
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&f) == false)
        return QString();
    return QString::fromLatin1(hash.result().toHex());
}

void UpdateDownloader::cancel()
{
    if (m_reply != nullptr)
    {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (m_file.isOpen())
    {
        m_file.close();
        m_file.remove();
    }
}

void UpdateDownloader::fail(const QString &error)
{
    if (m_file.isOpen())
        m_file.close();
    if (m_filePath.isEmpty() == false)
        QFile::remove(m_filePath);
    m_filePath.clear();
    emit finished(false, error);
}
