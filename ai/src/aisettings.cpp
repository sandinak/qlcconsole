/*
  Q Light Controller Plus - qlcconsole
  aisettings.cpp

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

#include <QHostAddress>
#include <QSettings>

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

#include "aisettings.h"

#define KEY(k) QStringLiteral("ai/" k)

AiSettings AiSettings::load()
{
    QSettings s;
    AiSettings a;
    a.enabled = s.value(KEY("enabled"), a.enabled).toBool();
    a.provider = s.value(KEY("provider"), a.provider).toString();
    if (!providers().contains(a.provider))
        a.provider = QStringLiteral("ollama");
    a.endpoint = s.value(KEY("endpoint"), a.endpoint).toString().trimmed();
    a.model = s.value(KEY("model"), a.model).toString().trimmed();
    a.sendShowContext = s.value(KEY("sendShowContext"), a.sendShowContext).toString();
    if (a.sendShowContext != QLatin1String("none"))
        a.sendShowContext = QStringLiteral("summary");
    a.timeoutSec = qBound(5, s.value(KEY("timeoutSec"), a.timeoutSec).toInt(), 600);
    a.mcpEnabled = s.value(KEY("mcpEnabled"), a.mcpEnabled).toBool();
    a.mcpChanges = s.value(KEY("mcpChanges"), a.mcpChanges).toBool();
    return a;
}

void AiSettings::save() const
{
    QSettings s;
    s.setValue(KEY("enabled"), enabled);
    s.setValue(KEY("provider"), provider);
    s.setValue(KEY("endpoint"), endpoint);
    s.setValue(KEY("model"), model);
    s.setValue(KEY("sendShowContext"), sendShowContext);
    s.setValue(KEY("timeoutSec"), timeoutSec);
    s.setValue(KEY("mcpEnabled"), mcpEnabled);
    s.setValue(KEY("mcpChanges"), mcpChanges);
}

QStringList AiSettings::providers()
{
    return { QStringLiteral("ollama"), QStringLiteral("openai-compatible"), QStringLiteral("anthropic") };
}

QString AiSettings::providerName(const QString &provider)
{
    if (provider == QLatin1String("openai-compatible"))
        return tr("OpenAI-compatible server");
    if (provider == QLatin1String("anthropic"))
        return tr("Anthropic (cloud)");
    return tr("Ollama");
}

QString AiSettings::defaultEndpoint(const QString &provider)
{
    if (provider == QLatin1String("anthropic"))
        return QStringLiteral("https://api.anthropic.com");
    if (provider == QLatin1String("openai-compatible"))
        return QStringLiteral("http://localhost:8080");
    return QStringLiteral("http://localhost:11434");
}

bool AiSettings::isLoopback(const QUrl &url)
{
    const QString host = url.host().toLower();
    if (host == QLatin1String("localhost"))
        return true;
    const QHostAddress addr(host);
    return !addr.isNull() && addr.isLoopback();
}

bool AiSettings::sendsOffMachine() const
{
    if (provider == QLatin1String("anthropic"))
        return true;
    return !isLoopback(QUrl(endpoint));
}

bool AiSettings::isLocalOllama() const
{
    return provider == QLatin1String("ollama") && isLoopback(QUrl(endpoint));
}

bool AiSettings::operator==(const AiSettings &o) const
{
    return enabled == o.enabled && provider == o.provider && endpoint == o.endpoint && model == o.model
           && sendShowContext == o.sendShowContext && timeoutSec == o.timeoutSec
           && mcpEnabled == o.mcpEnabled && mcpChanges == o.mcpChanges;
}

/****************************************************************************
 * AiKeychain
 ****************************************************************************/

namespace
{
AiKeychain::Reader s_read;
AiKeychain::Writer s_write;
const char *const kService = "qlcconsole AI Assist";

#ifdef Q_OS_MACOS
CFStringRef cfString(const QString &s)
{
    return s.toCFString();
}

CFMutableDictionaryRef keyQuery(const QString &provider)
{
    CFMutableDictionaryRef q = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFStringRef service = cfString(QString::fromLatin1(kService));
    CFStringRef account = cfString(provider);
    CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(q, kSecAttrService, service);
    CFDictionarySetValue(q, kSecAttrAccount, account);
    CFRelease(service);
    CFRelease(account);
    return q;
}
#endif
}

void AiKeychain::setBackend(const Reader &read, const Writer &write)
{
    s_read = read;
    s_write = write;
}

bool AiKeychain::canStore()
{
    if (s_write)
        return true;
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

QString AiKeychain::read(const QString &provider)
{
    if (s_read)
        return s_read(provider);
#ifdef Q_OS_MACOS
    CFMutableDictionaryRef q = keyQuery(provider);
    CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(q, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef data = nullptr;
    const OSStatus st = SecItemCopyMatching(q, &data);
    CFRelease(q);
    if (st == errSecSuccess && data != nullptr)
    {
        const QByteArray bytes(reinterpret_cast<const char *>(CFDataGetBytePtr(static_cast<CFDataRef>(data))),
                               CFDataGetLength(static_cast<CFDataRef>(data)));
        CFRelease(data);
        return QString::fromUtf8(bytes);
    }
#endif
    QString env = qEnvironmentVariable("QLC_AI_API_KEY");
    if (env.isEmpty() && provider == QLatin1String("anthropic"))
        env = qEnvironmentVariable("ANTHROPIC_API_KEY");
    if (env.isEmpty() && provider == QLatin1String("openai-compatible"))
        env = qEnvironmentVariable("OPENAI_API_KEY");
    return env;
}

bool AiKeychain::write(const QString &provider, const QString &key)
{
    if (s_write)
        return s_write(provider, key);
#ifdef Q_OS_MACOS
    CFMutableDictionaryRef q = keyQuery(provider);
    SecItemDelete(q);
    if (key.isEmpty())
    {
        CFRelease(q);
        return true;
    }
    const QByteArray bytes = key.toUtf8();
    CFDataRef data = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(bytes.constData()), bytes.size());
    CFDictionarySetValue(q, kSecValueData, data);
    const OSStatus st = SecItemAdd(q, nullptr);
    CFRelease(data);
    CFRelease(q);
    return st == errSecSuccess;
#else
    Q_UNUSED(provider)
    Q_UNUSED(key)
    return false;
#endif
}
