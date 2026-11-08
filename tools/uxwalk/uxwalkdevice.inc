/*
  qlcconsole
  uxwalkdevice.inc -- the WLED a flow talks to (included by uxwalkflows.inc)

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

/* The device of a flow that adds a WLED (F-11): by default a WLED on this
 * computer (ui/test/common/fakewled.h) that answers as his office WLED did
 * on 2026-11-07 (tools/uxwalk/data/wled-office: its /json/info, /json/state
 * and /json/cfg, Wi-Fi names and keys scrubbed), at 127.0.0.1:<port> -- the
 * address the flow types into Add Fixture. It keeps every POST the app
 * sends it, with when, so a step can say what the DEVICE got, not only what
 * the screen says.
 *
 * QLC_WALK_WLED=<ip> aims the flow at a real WLED instead (JSON API only).
 * Its /json/state and /json/cfg are read first and put back when the walk
 * is done (the state, and the realtime-receive settings Fix on Device
 * changes). The gate never sets it. */

class WalkDevice
{
public:
    WalkDevice()
    {
        m_real = qEnvironmentVariable("QLC_WALK_WLED").trimmed();
        if (!m_real.isEmpty())
        {
            m_savedState = get(QStringLiteral("/json/state"));
            m_savedCfg = get(QStringLiteral("/json/cfg"));
            qInfo().noquote() << "uxwalk: the flows' WLED is the real one at" << m_real
                              << (m_savedState.isEmpty() ? "(it did not answer)" : "(its state kept, put back after)");
            return;
        }
        const QString dir = QString(QLC_TEST_RESOURCE_ROOT) + "../tools/uxwalk/data/wled-office/";
        m_fake.seed(readJson(dir + "info.json"), readJson(dir + "state.json"), readJson(dir + "cfg.json"));
        m_fake.pal = readArray(dir + "pal.json");
        m_fake.palx = readJson(dir + "palx.json");
        m_fake.listen(QHostAddress::LocalHost);
    }

    ~WalkDevice() { restore(); }

    bool isReal() const { return !m_real.isEmpty(); }

    /** What the flow types as its IP. */
    QString address() const { return isReal() ? m_real : m_fake.address(); }

    /** What it has now: its /json/state, its /json/cfg. */
    QJsonObject state() { return isReal() ? get(QStringLiteral("/json/state")) : m_fake.state; }
    QJsonObject cfg() { return isReal() ? get(QStringLiteral("/json/cfg")) : m_fake.cfg; }

    /** The state POSTs it has had since @a sinceMs (the fake's; a real one
     *  can't say: empty). */
    QList<QPair<qint64, QJsonObject>> postsSince(qint64 sinceMs) const
    {
        QList<QPair<qint64, QJsonObject>> out;
        if (isReal())
            return out;
        for (int i = 0; i < m_fake.posted.size(); ++i)
            if (m_fake.postedAt.value(i) >= sinceMs && m_fake.posted.at(i).first.contains("state"))
                out << qMakePair(m_fake.postedAt.value(i), m_fake.posted.at(i).second);
        return out;
    }
    int postCount() const { return isReal() ? 0 : m_fake.posted.size(); }

    /** A real device back as it was found. */
    void restore()
    {
        if (!isReal() || m_restored)
            return;
        m_restored = true;
        QJsonObject live = m_savedCfg.value(QStringLiteral("if")).toObject().value(QStringLiteral("live")).toObject();
        if (!live.isEmpty())
            post(QStringLiteral("/json/cfg"), QJsonObject{ { QStringLiteral("if"), QJsonObject{ { QStringLiteral("live"), live } } } });
        if (!m_savedState.isEmpty())
        {
            // What a state POST takes back: the top level and each segment,
            // without what WLED only reports.
            QJsonObject st = m_savedState;
            for (const char *k : { "nl", "udpn", "AudioReactive", "lor", "mainseg", "ps", "pl", "ledmap", "bs" })
                st.remove(QLatin1String(k));
            QJsonArray segs;
            for (const QJsonValue &v : m_savedState.value(QStringLiteral("seg")).toArray())
            {
                QJsonObject s = v.toObject();
                s.remove(QStringLiteral("len"));
                segs << s;
            }
            st[QStringLiteral("seg")] = segs;
            st[QStringLiteral("transition")] = 0;
            post(QStringLiteral("/json/state"), st);
        }
        qInfo().noquote() << "uxwalk: the WLED at" << m_real << "put back as it was found";
    }

    static QJsonArray readArray(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return QJsonArray();
        return QJsonDocument::fromJson(f.readAll()).array();
    }

    static QJsonObject readJson(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return QJsonObject();
        return QJsonDocument::fromJson(f.readAll()).object();
    }

private:
    QJsonObject get(const QString &path)
    {
        QNetworkRequest req(QUrl(QStringLiteral("http://%1%2").arg(m_real, path)));
        req.setTransferTimeout(3000);
        QNetworkReply *r = m_nam.get(req);
        QEventLoop loop;
        QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        const QJsonObject o = r->error() == QNetworkReply::NoError ? QJsonDocument::fromJson(r->readAll()).object()
                                                                   : QJsonObject();
        r->deleteLater();
        return o;
    }
    void post(const QString &path, const QJsonObject &body)
    {
        QNetworkRequest req(QUrl(QStringLiteral("http://%1%2").arg(m_real, path)));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setTransferTimeout(3000);
        QNetworkReply *r = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        r->deleteLater();
    }

    FakeWled m_fake;
    QString m_real;
    QNetworkAccessManager m_nam;
    QJsonObject m_savedState, m_savedCfg;
    bool m_restored = false;
};

WalkDevice *g_device = nullptr;

/** "${WLED}" in a step: the device's address. */
QString expand(const QString &text)
{
    if (g_device == nullptr || !text.contains(QLatin1String("${WLED}")))
        return text;
    QString out = text;
    return out.replace(QLatin1String("${WLED}"), g_device->address());
}

/** A dotted path into JSON ("if.live.en"). */
QJsonValue at(const QJsonObject &o, const QString &path)
{
    QJsonValue v = o;
    for (const QString &k : path.split(QLatin1Char('.')))
        v = v.toObject().value(k);
    return v;
}

QString jsonText(const QJsonValue &v)
{
    if (v.isUndefined())
        return QStringLiteral("nothing");
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return v.toVariant().toString();
}

/** Whether segment value @a got is what a check wants: a number, a bool,
 *  or for "col" its first colour [r,g,b]. */
bool segWants(const QString &key, const QJsonValue &got, const QJsonValue &want)
{
    if (key == QLatin1String("col"))
        return got.toArray().at(0).toArray() == want.toArray();
    if (key == QLatin1String("frz") || key == QLatin1String("on"))
        return got.toBool(false) == want.toBool();
    return got == want || (got.isDouble() && want.isDouble() && got.toDouble() == want.toDouble());
}

QString segText(const QJsonObject &seg)
{
    QStringList out;
    for (const char *k : { "fx", "col", "frz", "on" })
    {
        const QJsonValue v = seg.value(QLatin1String(k));
        if (!v.isUndefined())
            out << QStringLiteral("%1 %2").arg(QLatin1String(k),
                                               jsonText(QLatin1String(k) == QLatin1String("col") ? v.toArray().at(0) : v));
    }
    return out.join(QStringLiteral(", "));
}

/** A "device" check: what the WLED has (or got) against what the step
 *  says. Empty when it holds, else what it got, in words. Keys:
 *  - "seg": {key: value}: every segment has them now (fx, frz, on, col, bri);
 *  - "cfg": {path: value}: its settings ("if.live.en": false);
 *  - "got": {key: value}: some state POST since the step began set every
 *    segment so (the Locate flash), and "after": the segments back as they
 *    were before the step (fx, col and frz) once it is over;
 *  - "what": the words before what it got ("after the colour cue"). */
QString deviceProblem(const QJsonObject &c, qint64 stepStartMs, const QJsonObject &stateBefore)
{
    if (g_device == nullptr)
        return QStringLiteral("no device");
    const QString what = c.value(QStringLiteral("what")).toString();
    const QString lead = what.isEmpty() ? QStringLiteral("the WLED") : QStringLiteral("%1 the WLED").arg(what);
    if (c.contains(QStringLiteral("cfg")))
    {
        const QJsonObject cfg = g_device->cfg();
        const QJsonObject want = c.value(QStringLiteral("cfg")).toObject();
        for (auto it = want.begin(); it != want.end(); ++it)
            if (at(cfg, it.key()) != it.value())
                return QStringLiteral("%1 has %2 = %3 in its settings, not %4")
                    .arg(lead, it.key(), jsonText(at(cfg, it.key())), jsonText(it.value()));
    }
    if (c.contains(QStringLiteral("seg")))
    {
        const QJsonArray segs = g_device->state().value(QStringLiteral("seg")).toArray();
        if (segs.isEmpty())
            return QStringLiteral("%1 has no segments").arg(lead);
        const QJsonObject want = c.value(QStringLiteral("seg")).toObject();
        for (const QJsonValue &v : segs)
        {
            const QJsonObject seg = v.toObject();
            for (auto it = want.begin(); it != want.end(); ++it)
                if (!segWants(it.key(), seg.value(it.key()), it.value()))
                    return QStringLiteral("%1 still has %2 %3 on segment %4, not %5 (%6)")
                        .arg(lead, it.key(),
                             jsonText(it.key() == QLatin1String("col") ? seg.value(it.key()).toArray().at(0)
                                                                        : seg.value(it.key())),
                             QString::number(seg.value(QStringLiteral("id")).toInt()), jsonText(it.value()),
                             segText(seg));
        }
    }
    if (c.contains(QStringLiteral("got")))
    {
        const QJsonObject want = c.value(QStringLiteral("got")).toObject();
        const QList<QPair<qint64, QJsonObject>> posts = g_device->postsSince(stepStartMs);
        bool seen = false;
        QStringList sent;
        for (const auto &p : posts)
        {
            const QJsonArray segs = p.second.value(QStringLiteral("seg")).toArray();
            bool all = !segs.isEmpty();
            for (const QJsonValue &v : segs)
                for (auto it = want.begin(); it != want.end(); ++it)
                    all = all && segWants(it.key(), v.toObject().value(it.key()), it.value());
            seen = seen || all;
            if (!segs.isEmpty() && sent.size() < 4)
                sent << segText(segs.at(0).toObject());
        }
        if (!seen)
            return QStringLiteral("%1 got no %2 (%3)").arg(lead, jsonText(want),
                posts.isEmpty() ? QStringLiteral("nothing was sent to it")
                                : QStringLiteral("it got: %1").arg(sent.join(QStringLiteral(" | "))));
    }
    if (c.value(QStringLiteral("after")).toBool())
    {
        const QJsonArray was = stateBefore.value(QStringLiteral("seg")).toArray();
        const QJsonArray now = g_device->state().value(QStringLiteral("seg")).toArray();
        for (int i = 0; i < was.size() && i < now.size(); ++i)
            for (const char *k : { "fx", "col", "frz" })
                if (!segWants(QLatin1String(k), now.at(i).toObject().value(QLatin1String(k)),
                              QLatin1String(k) == QLatin1String("col")
                                  ? was.at(i).toObject().value(QLatin1String(k)).toArray().at(0)
                                  : was.at(i).toObject().value(QLatin1String(k))))
                    return QStringLiteral("%1 was not put back: segment %2 has %3, it had %4")
                        .arg(lead, QString::number(i), segText(now.at(i).toObject()), segText(was.at(i).toObject()));
    }
    return QString();
}
