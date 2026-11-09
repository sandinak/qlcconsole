/*
  Q Light Controller Plus
  gobowheelsdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QIcon>
#include <QSet>
#include <QPixmap>
#include <QRegularExpression>
#include <QFile>
#include <QHash>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QScopedValueRollback>
#include <QTreeWidgetItemIterator>

#include "gobowheelsdialog.h"
#include "monitor.h"
#include "beamoptics.h"
#include "gobolibrary.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "qlcfixturedefcache.h"
#include "fixture.h"
#include "doc.h"

#define COL_SLOT  0
#define COL_GLASS 1
#define COL_SUGGEST 2
#define COL_NAME  3
#define ROLE_CHANNEL (Qt::UserRole)
#define ROLE_MIN     (Qt::UserRole + 1)

namespace
{
    bool isPicture(const QLCCapability *cap)
    {
        return GoboLibrary::hasPicture(cap);
    }

    QString pictureOf(const QLCCapability *cap)
    {
        return isPicture(cap) ? GoboLibrary::resolve(const_cast<QLCCapability *>(cap)->resource(0).toString())
                              : QString();
    }

    /** Words of a slot name that matter for matching it: lower-case, "gobo"
     *  and "shake" dropped, plurals folded, long words cut to five letters
     *  so a misspelling ("Concentrentic") still meets the right one. */
    QStringList tokensOf(const QString &name)
    {
        static const QRegularExpression split(QStringLiteral("[^\\p{L}\\p{N}]+"));
        QStringList out;
        foreach (QString t, GoboLibrary::slotBaseName(name).split(split, Qt::SkipEmptyParts))
        {
            if (t == QLatin1String("gobo") || t == QLatin1String("gobos") || t == QLatin1String("the"))
                continue;
            if (t.length() > 3 && t.endsWith(QLatin1Char('s')))
                t.chop(1);
            if (t.length() > 5)
                t = t.left(5);
            if (!out.contains(t))
                out << t;
        }
        return out;
    }

    /** A gobo as the beam shows it: light through a dark disc, so a
     *  thumbnail reads on any theme (the pictures themselves are black metal
     *  on clear, which vanishes on a dark background). */
    QIcon lightIcon(const QImage &mask)
    {
        if (mask.isNull())
            return QIcon();
        const int n = 40;
        QImage out(n, n, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(18, 18, 22));
        p.drawEllipse(QRectF(1, 1, n - 2, n - 2));
        // The mask's light, clipped to the disc, in warm white.
        QImage light(n, n, QImage::Format_ARGB32_Premultiplied);
        light.fill(QColor(255, 244, 214));
        const QImage gray = mask.scaled(n, n, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                .convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < n; ++y)
        {
            QRgb *row = reinterpret_cast<QRgb *>(light.scanLine(y));
            for (int x = 0; x < n; ++x)
            {
                const int a = gray.constScanLine(y)[x];       // light through: 255
                row[x] = qPremultiply(qRgba(255, 244, 214, a));
            }
        }
        QPainterPath disc;
        disc.addEllipse(QRectF(2, 2, n - 4, n - 4));
        p.setClipPath(disc);
        p.drawImage(0, 0, light);
        p.setClipping(false);
        p.setPen(QPen(QColor(120, 120, 130), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(1, 1, n - 2, n - 2));
        return QIcon(QPixmap::fromImage(out));
    }

    /** Share of the beam's disc a mask lets through, 0..1. */
    double maskCoverage(const QImage &mask)
    {
        if (mask.isNull())
            return 0.0;
        double sum = 0.0;
        int inside = 0;
        const int n = mask.width();
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const double dx = (x + 0.5) / n * 2.0 - 1.0, dy = (y + 0.5) / n * 2.0 - 1.0;
                if (dx * dx + dy * dy > 1.0)
                    continue;
                sum += mask.constScanLine(y)[x] / 255.0;
                ++inside;
            }
        return inside > 0 ? sum / inside : 0.0;
    }

    struct Named
    {
        QStringList tokens;
        QString picture;
    };

    /** Every named picture slot in the stock definitions, built once. */
    const QList<Named> &libraryIndex(const Doc *doc)
    {
        static QList<Named> index;
        static const QLCFixtureDefCache *builtFor = nullptr;
        static int builtCount = -1;
        if (doc == nullptr || doc->fixtureDefCache() == nullptr)
            return index;
        // Once per cache (and again if definitions are added to it).
        if (builtFor == doc->fixtureDefCache() && builtCount == doc->fixtureDefCache()->definitions().size())
            return index;
        builtFor = doc->fixtureDefCache();
        builtCount = int(builtFor->definitions().size());
        index.clear();
        static const QRegularExpression capRx(
            QStringLiteral("<Capability\\b[^>]*?\\bRes1=\"([^\"]+\\.(?:svg|png|jpe?g|gif|bmp))\"[^>]*>([^<]*)<"),
            QRegularExpression::CaseInsensitiveOption);
        const QString mapPath = doc->fixtureDefCache()->mapPath();
        foreach (QLCFixtureDef *def, doc->fixtureDefCache()->definitions())
        {
            if (!def->channels().isEmpty())
            {
                // Loaded: its capabilities as they are.
                foreach (QLCChannel *ch, def->channels())
                {
                    if (ch->group() != QLCChannel::Gobo)
                        continue;
                    foreach (QLCCapability *cap, ch->capabilities())
                    {
                        const QString pic = GoboLibrary::portableName(pictureOf(cap));
                        if (pic.isEmpty() || GoboLibrary::isOpenSlotName(cap->name())
                            || GoboLibrary::isRotationSlotName(cap->name()))
                            continue;
                        const QStringList tokens = tokensOf(cap->name());
                        if (!tokens.isEmpty())
                            index << Named{ tokens, pic };
                    }
                }
                continue;
            }
            /* The library's own definitions are read as they are needed, and
               there are a couple of thousand: read the file for its gobo
               lines only, not the whole definition. */
            QString file = def->definitionSourceFile();
            if (file.isEmpty())
                continue;
            if (QFileInfo(file).isRelative())
                file = mapPath + QLatin1Char('/') + file;
            QFile f(file);
            if (!f.open(QIODevice::ReadOnly))
                continue;
            const QByteArray bytes = f.readAll();
            if (!bytes.contains("Res1=\""))
                continue;
            const QString text = QString::fromUtf8(bytes);
            QRegularExpressionMatchIterator it = capRx.globalMatch(text);
            while (it.hasNext())
            {
                const QRegularExpressionMatch m = it.next();
                const QString name = m.captured(2).trimmed();
                if (GoboLibrary::isOpenSlotName(name) || GoboLibrary::isRotationSlotName(name)
                    || m.captured(1).contains(QLatin1String("/open."), Qt::CaseInsensitive))
                    continue;
                const QStringList tokens = tokensOf(name);
                if (!tokens.isEmpty())
                    index << Named{ tokens, m.captured(1) };
            }
        }
        return index;
    }
}

QStringList GoboWheelsDialog::suggestPictures(const Doc *doc, const QString &slotName, int max)
{
    const QStringList want = tokensOf(slotName);
    QStringList out;
    if (want.isEmpty())
        return out;
    QHash<QString, double> score;
    foreach (const Named &n, libraryIndex(doc))
    {
        int shared = 0;
        foreach (const QString &t, want)
            if (n.tokens.contains(t))
                ++shared;
        if (shared == 0)
            continue;
        const double jac = double(shared) / (want.size() + n.tokens.size() - shared);
        if (jac >= 0.5)
            score[n.picture] += jac * jac;           // exact names count most
    }
    QList<QPair<double, QString>> ranked;
    for (auto it = score.constBegin(); it != score.constEnd(); ++it)
        ranked << qMakePair(it.value(), it.key());
    std::sort(ranked.begin(), ranked.end(), [](const QPair<double, QString> &a, const QPair<double, QString> &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    for (int i = 0; i < ranked.size() && out.size() < max; ++i)
    {
        const QString path = GoboLibrary::resolve(ranked.at(i).second);
        // Mostly metal (a thin ring for "Spots") is a poor stand-in: next.
        if (!path.isEmpty() && maskCoverage(BeamOptics::goboMask(path)) >= 0.1)
            out << ranked.at(i).second;
    }
    return out;
}

namespace
{
    /** The gobo channels of @p fx, in order. */
    QList<quint32> goboChannels(const Fixture *fx)
    {
        QList<quint32> out;
        for (quint32 i = 0; fx != nullptr && i < fx->channels(); ++i)
        {
            const QLCChannel *ch = fx->channel(i);
            if (ch != nullptr && ch->group() == QLCChannel::Gobo)
                out << i;
        }
        return out;
    }

    /** What makes two units' gobo wheels the same: each wheel's slots and
     *  their pictures, in order (two modes of one definition, or two
     *  definitions of one unit, share them). */
    QString wheelKey(const Fixture *fx)
    {
        QStringList parts;
        for (quint32 i : goboChannels(fx))
        {
            QStringList wheelSlots;
            foreach (const QLCCapability *cap, fx->channel(i)->capabilities())
                wheelSlots << QStringLiteral("%1-%2:%3:%4").arg(cap->min()).arg(cap->max()).arg(cap->name(),
                             const_cast<QLCCapability *>(cap)->resource(0).toString());
            parts << wheelSlots.join(QLatin1Char(','));
        }
        return parts.join(QLatin1Char('|'));
    }
}

QList<quint32> GoboWheelsDialog::withGoboWheels(const Doc *doc, const QList<quint32> &fixtureIds)
{
    QList<quint32> out;
    if (doc == nullptr)
        return out;
    for (quint32 id : fixtureIds)
    {
        if (!out.contains(id) && hasGoboWheels(const_cast<Doc *>(doc)->fixture(id)))
            out << id;
    }
    return out;
}

QString GoboWheelsDialog::whyNot(const Doc *doc, const QList<quint32> &fixtureIds)
{
    const QList<quint32> ids = withGoboWheels(doc, fixtureIds);
    if (ids.isEmpty())
        return tr("None of these has a gobo wheel");
    const Fixture *first = const_cast<Doc *>(doc)->fixture(ids.first());
    const QString key = wheelKey(first);
    for (quint32 id : ids)
    {
        const Fixture *fx = const_cast<Doc *>(doc)->fixture(id);
        if (wheelKey(fx) != key)
            return tr("%1 and %2 have different gobo wheels. Pick lights of one kind.")
                   .arg(first->name(), fx->name());
    }
    return QString();
}

GoboWheelsDialog::GoboWheelsDialog(Doc *doc, quint32 fixtureId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fid(fixtureId)
{
    m_fids << fixtureId;
    init();
}

GoboWheelsDialog::GoboWheelsDialog(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fid(Fixture::invalidId())
{
    m_fids = withGoboWheels(doc, fixtureIds);
    if (!m_fids.isEmpty())
        m_fid = m_fids.first();
    init();
}

void GoboWheelsDialog::init()
{
    Fixture *fx = m_doc->fixture(m_fid);
    const bool many = m_fids.size() > 1;
    if (many)
        setWindowTitle(tr("Gobo Wheels — %1 fixtures").arg(m_fids.size()));
    else
        setWindowTitle(tr("Gobo Wheels — %1").arg(fx != nullptr ? fx->name() : QString()));
    resize(640, 480);

    pickImage = [](QWidget *parent) {
        return QFileDialog::getOpenFileName(parent, tr("Fit a gobo"),
            GoboLibrary::userDirectory().absolutePath(),
            tr("Gobo pictures (%1)").arg(GoboLibrary::imageFilters().join(' ')));
    };

    if (fx != nullptr)
    {
        m_swaps = m_doc->monitorProperties()->goboSwaps(m_fid);
        m_names = m_doc->monitorProperties()->goboNames(m_fid);
    }

    QVBoxLayout *root = new QVBoxLayout(this);
    QString text;
    if (many)
    {
        QStringList names;
        for (int i = 0; i < m_fids.size() && i < 4; ++i)
        {
            if (const Fixture *f = m_doc->fixture(m_fids.at(i)))
                names << f->name();
        }
        if (m_fids.size() > 4)
            names << tr("%n more", nullptr, int(m_fids.size()) - 4);
        text = tr("The glass in each slot of these %n fixtures (%1). A slot you change is changed in "
                  "every one; the slots you leave alone keep each unit's own glass.", nullptr,
                  int(m_fids.size())).arg(names.join(QStringLiteral(", ")));
    }
    else
    {
        text = tr("The glass in each slot of this unit.");
    }
    text += QLatin1Char(' ') + tr("Name the glass you loaded (\"Moon\") so every list calls the slot "
                                  "that, and a Gobo look of that name finds it in each unit. Replace "
                                  "a slot's picture with your own: black blocks the light, clear or "
                                  "white lets it through. The fixture definition is not changed.");
    QLabel *intro = new QLabel(text, this);
    intro->setObjectName(QStringLiteral("goboWheelsIntro"));
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels(QStringList() << tr("Slot") << tr("Glass") << tr("Suggested") << tr("Loaded glass"));
    m_tree->headerItem()->setToolTip(COL_NAME, tr("What the glass in this slot is called. Double-click to "
                                                  "name it."));
    m_tree->setIconSize(QSize(32, 32));
    m_tree->setRootIsDecorated(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->header()->setSectionResizeMode(COL_SLOT, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(COL_GLASS, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_SUGGEST, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_NAME, QHeaderView::ResizeToContents);
    root->addWidget(m_tree, 1);

    QHBoxLayout *row = new QHBoxLayout();
    m_name = new QPushButton(tr("Name…"), this);
    m_name->setToolTip(tr("Name the glass loaded in this slot (\"Moon\"). A picture is optional."));
    m_replace = new QPushButton(tr("Replace…"), this);
    m_restore = new QPushButton(tr("Restore"), this);
    m_restore->setToolTip(tr("Put the definition's own glass back in this slot: its picture and its name"));
    QPushButton *folder = new QPushButton(tr("Open Gobo Folder"), this);
    row->addWidget(m_name);
    row->addWidget(m_replace);
    row->addWidget(m_restore);
    m_fitSuggested = new QPushButton(tr("Fit Suggested Pictures"), this);
    m_fitSuggested->setToolTip(tr("Fit the first suggestion in every slot that has no picture of its "
                                  "own. Nothing is kept until you press OK."));
    row->addWidget(m_fitSuggested);
    row->addStretch(1);
    row->addWidget(folder);
    root->addLayout(row);

    int sameType = 0;
    if (fx != nullptr && !many)
    {
        foreach (Fixture *other, m_doc->fixtures())
            if (other->fixtureDef() == fx->fixtureDef() && other->fixtureMode() == fx->fixtureMode())
                ++sameType;
    }
    m_allOfType = new QCheckBox(tr("Fit the same glass in all %1 fixtures of this type").arg(sameType), this);
    m_allOfType->setVisible(sameType > 1);
    root->addWidget(m_allOfType);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &GoboWheelsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &GoboWheelsDialog::reject);
    connect(m_name, &QPushButton::clicked, this, &GoboWheelsDialog::slotName);
    connect(m_replace, &QPushButton::clicked, this, &GoboWheelsDialog::slotReplace);
    connect(m_restore, &QPushButton::clicked, this, &GoboWheelsDialog::slotRestore);
    connect(m_fitSuggested, &QPushButton::clicked, this, &GoboWheelsDialog::slotFitSuggested);
    connect(folder, &QPushButton::clicked, this, &GoboWheelsDialog::slotOpenFolder);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &GoboWheelsDialog::refreshButtons);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (item == nullptr || item->parent() == nullptr)
            return;
        if (column == COL_NAME)
            slotName();
        else
            slotReplace();
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        // The "Loaded glass" cell typed in.
        if (m_building || column != COL_NAME || item == nullptr || item->parent() == nullptr)
            return;
        m_tree->setCurrentItem(item);
        const QString typed = item->text(COL_NAME).trimmed();
        QMetaObject::invokeMethod(this, [this, typed]() { setSlotName(typed); }, Qt::QueuedConnection);
    });

    rebuild();
    refreshButtons();
}

bool GoboWheelsDialog::hasGoboWheels(const Fixture *fx)
{
    if (fx == nullptr)
        return false;
    for (quint32 i = 0; i < fx->channels(); ++i)
    {
        const QLCChannel *ch = fx->channel(i);
        if (ch == nullptr || ch->group() != QLCChannel::Gobo)
            continue;
        foreach (QLCCapability *cap, ch->capabilities())
            if (isPicture(cap))
                return true;
        if (!GoboLibrary::standInCaps(ch).isEmpty())
            return true;
    }
    return false;
}

QList<const QLCCapability *> GoboWheelsDialog::sameGlass(const QLCChannel *ch, const QLCCapability *cap)
{
    QList<const QLCCapability *> out;
    if (ch == nullptr || cap == nullptr)
        return out;
    out << cap;
    const QString pic = pictureOf(cap);
    if (pic.isEmpty())
    {
        // No picture: a slot and its shake share a name.
        const QString base = GoboLibrary::slotBaseName(cap->name());
        foreach (QLCCapability *other, GoboLibrary::standInCaps(ch))
            if (other != cap && GoboLibrary::slotBaseName(other->name()) == base)
                out << other;
        return out;
    }
    foreach (QLCCapability *other, ch->capabilities())
        if (other != cap && pictureOf(other) == pic)
            out << other;
    return out;
}

GoboWheelsDialog::SlotKey GoboWheelsDialog::keyOn(const Fixture *to, const SlotKey &key) const
{
    if (to == nullptr || to->id() == m_fid)
        return key;
    const int k = goboChannels(m_doc->fixture(m_fid)).indexOf(key.first);
    const QList<quint32> theirs = goboChannels(to);
    return (k >= 0 && k < theirs.size()) ? SlotKey(theirs.at(k), key.second) : key;
}

bool GoboWheelsDialog::varies(const SlotKey &key) const
{
    if (m_fids.size() < 2 || m_touched.contains(key))
        return false;
    const MonitorProperties *props = m_doc->monitorProperties();
    const QString swap = m_swaps.value(key), name = m_names.value(key);
    for (quint32 id : m_fids)
    {
        const SlotKey k = keyOn(m_doc->fixture(id), key);
        if (props->goboSwap(id, k.first, uchar(k.second)) != swap
            || props->goboName(id, k.first, uchar(k.second)) != name)
            return true;
    }
    return false;
}

QString GoboWheelsDialog::effectiveImage(quint32 channel, const QLCCapability *cap) const
{
    const QString swap = m_swaps.value(qMakePair(channel, int(cap->min())));
    if (!swap.isEmpty())
    {
        const QString path = GoboLibrary::resolve(swap);
        if (!path.isEmpty())
            return path;
    }
    return pictureOf(cap);
}

void GoboWheelsDialog::rebuild()
{
    QScopedValueRollback<bool> building(m_building, true);
    m_tree->clear();
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx == nullptr)
        return;
    for (quint32 i = 0; i < fx->channels(); ++i)
    {
        const QLCChannel *ch = fx->channel(i);
        if (ch == nullptr || ch->group() != QLCChannel::Gobo)
            continue;
        QTreeWidgetItem *wheel = nullptr;
        QSet<const QLCCapability *> shown;
        const QList<QLCCapability *> standIn = GoboLibrary::standInCaps(ch);
        foreach (QLCCapability *cap, ch->capabilities())
        {
            if ((!isPicture(cap) && !standIn.contains(cap)) || shown.contains(cap))
                continue;
            const QList<const QLCCapability *> glass = sameGlass(ch, cap);
            for (const QLCCapability *g : glass)
                shown.insert(g);
            if (wheel == nullptr)
            {
                wheel = new QTreeWidgetItem(m_tree);
                wheel->setText(COL_SLOT, ch->name());
                wheel->setFirstColumnSpanned(true);
                wheel->setExpanded(true);
            }
            const SlotKey key(i, int(cap->min()));
            QTreeWidgetItem *slot = new QTreeWidgetItem(wheel);
            slot->setFlags(slot->flags() | Qt::ItemIsEditable);
            slot->setData(COL_SLOT, ROLE_CHANNEL, i);
            slot->setData(COL_SLOT, ROLE_MIN, int(cap->min()));
            slot->setText(COL_SLOT, glass.size() > 1 ? tr("%1 (and its shake)").arg(cap->name())
                                                     : cap->name());
            const QString swap = m_swaps.value(key);
            const QString name = m_names.value(key);
            const QString img = effectiveImage(i, cap);
            if (img.isEmpty())
            {
                // The rig view's stand-in for it.
                const QImage m = BeamOptics::standInMask(cap->name(),
                    qMax(0, GoboLibrary::standInIndex(ch, cap)));
                slot->setIcon(COL_SLOT, lightIcon(m));
            }
            else
            {
                const QIcon lit = lightIcon(BeamOptics::goboMask(img));
                slot->setIcon(COL_SLOT, lit.isNull() ? QIcon(img) : lit);
            }
            // Pictures offered for it: a bare slot by its own name, any slot
            // you named and gave no picture by that name.
            if (swap.isEmpty() && (img.isEmpty() || !name.isEmpty()))
            {
                const QStringList sug = suggestionsFor(cap, key);
                if (!sug.isEmpty())
                {
                    // Up to three, best first; Fit takes the one chosen.
                    QComboBox *box = new QComboBox(m_tree);
                    box->setObjectName(QStringLiteral("suggestions"));
                    box->setIconSize(QSize(28, 28));
                    foreach (const QString &pic, sug)
                        box->addItem(lightIcon(BeamOptics::goboMask(GoboLibrary::resolve(pic))),
                                     QFileInfo(pic).completeBaseName(), pic);
                    m_tree->setItemWidget(slot, COL_SUGGEST, box);
                }
            }
            slot->setText(COL_GLASS, swap.isEmpty() ? (img.isEmpty() ? tr("no picture (stand-in)") : tr("as built"))
                                                    : QFileInfo(swap).fileName());
            if (!swap.isEmpty())
            {
                QFont f = slot->font(COL_GLASS);
                f.setBold(true);
                slot->setFont(COL_GLASS, f);
                if (GoboLibrary::resolve(swap).isEmpty())
                    slot->setText(COL_GLASS, tr("%1 (missing)").arg(swap));
            }
            slot->setText(COL_NAME, name);
            if (!name.isEmpty())
            {
                QFont f = slot->font(COL_NAME);
                f.setBold(true);
                slot->setFont(COL_NAME, f);
            }
            slot->setToolTip(COL_NAME, tr("What the glass in this slot is called. Double-click to name it."));
            if (varies(key))
            {
                // Left alone, each unit keeps its own.
                QFont f = slot->font(COL_GLASS);
                f.setItalic(true);
                slot->setFont(COL_GLASS, f);
                slot->setText(COL_GLASS, tr("varies"));
                slot->setText(COL_NAME, name.isEmpty() ? tr("varies") : tr("%1 (varies)").arg(name));
                slot->setToolTip(COL_GLASS, tr("The units differ in this slot. Leave it, and each keeps its own."));
            }
        }
    }
}

QStringList GoboWheelsDialog::suggestionsFor(const QLCCapability *cap, const SlotKey &key) const
{
    if (cap == nullptr)
        return QStringList();
    const QString name = m_names.value(key);
    return suggestPictures(m_doc, name.isEmpty() ? GoboLibrary::slotBaseName(cap->name()) : name);
}

void GoboWheelsDialog::slotFitSuggested()
{
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx == nullptr)
        return;
    // Each empty slot's chosen suggestion (the one showing in its dropdown).
    for (int w = 0; w < m_tree->topLevelItemCount(); ++w)
        for (int k = 0; k < m_tree->topLevelItem(w)->childCount(); ++k)
        {
            QTreeWidgetItem *it = m_tree->topLevelItem(w)->child(k);
            QComboBox *box = qobject_cast<QComboBox *>(m_tree->itemWidget(it, COL_SUGGEST));
            if (box == nullptr || box->currentIndex() < 0)
                continue;
            const quint32 chn = it->data(COL_SLOT, ROLE_CHANNEL).toUInt();
            const QLCChannel *ch = fx->channel(chn);
            const QLCCapability *cap = ch != nullptr
                ? ch->searchCapability(uchar(it->data(COL_SLOT, ROLE_MIN).toInt())) : nullptr;
            if (cap == nullptr || m_swaps.contains(qMakePair(chn, int(cap->min()))))
                continue;
            const QString name = box->currentData().toString();
            for (const QLCCapability *g : sameGlass(ch, cap))
            {
                m_swaps.insert(qMakePair(chn, int(g->min())), name);
                m_touched.insert(qMakePair(chn, int(g->min())));
            }
        }
    rebuild();
    refreshButtons();
}

QTreeWidgetItem *GoboWheelsDialog::currentSlot() const
{
    QTreeWidgetItem *it = m_tree->currentItem();
    return (it != nullptr && it->parent() != nullptr) ? it : nullptr;
}

void GoboWheelsDialog::refreshButtons()
{
    QTreeWidgetItem *slot = currentSlot();
    m_replace->setEnabled(slot != nullptr);
    m_name->setEnabled(slot != nullptr);
    if (m_fitSuggested != nullptr)
    {
        // Offered where there are suggestions: bare slots, named ones.
        bool any = false, offered = false;
        for (QTreeWidgetItemIterator it(m_tree); *it != nullptr; ++it)
        {
            if (m_tree->itemWidget(*it, COL_SUGGEST) != nullptr)
                any = true;
        }
        Fixture *fx = m_doc->fixture(m_fid);
        for (quint32 i = 0; fx != nullptr && i < fx->channels() && !offered; ++i)
        {
            if (!GoboLibrary::standInCaps(fx->channel(i)).isEmpty())
                offered = true;
        }
        offered = offered || any;
        m_fitSuggested->setVisible(offered);
        m_fitSuggested->setEnabled(any);
        m_tree->setColumnHidden(COL_SUGGEST, !offered);
    }
    bool restorable = false;
    if (slot != nullptr)
    {
        const SlotKey key(slot->data(COL_SLOT, ROLE_CHANNEL).toUInt(), slot->data(COL_SLOT, ROLE_MIN).toInt());
        restorable = m_swaps.contains(key) || m_names.contains(key) || varies(key);
    }
    m_restore->setEnabled(restorable);
}

void GoboWheelsDialog::setSlotGlass(QTreeWidgetItem *item, const QString *image, const QString *name)
{
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx == nullptr || item == nullptr)
        return;
    const quint32 chn = item->data(COL_SLOT, ROLE_CHANNEL).toUInt();
    const QLCChannel *ch = fx->channel(chn);
    const QLCCapability *cap = ch != nullptr ? ch->searchCapability(uchar(item->data(COL_SLOT, ROLE_MIN).toInt()))
                                             : nullptr;
    // The slot and its shake: the same piece of glass.
    for (const QLCCapability *g : sameGlass(ch, cap))
    {
        const SlotKey key(chn, int(g->min()));
        m_touched.insert(key);
        if (image != nullptr)
        {
            if (image->isEmpty())
                m_swaps.remove(key);
            else
                m_swaps.insert(key, *image);
        }
        if (name != nullptr)
        {
            if (name->trimmed().isEmpty())
                m_names.remove(key);
            else
                m_names.insert(key, name->trimmed());
        }
    }
    const int min = item->data(COL_SLOT, ROLE_MIN).toInt();
    rebuild();
    // Keep the same slot current.
    for (int w = 0; w < m_tree->topLevelItemCount(); ++w)
        for (int s = 0; s < m_tree->topLevelItem(w)->childCount(); ++s)
        {
            QTreeWidgetItem *it = m_tree->topLevelItem(w)->child(s);
            if (it->data(COL_SLOT, ROLE_CHANNEL).toUInt() == chn && it->data(COL_SLOT, ROLE_MIN).toInt() == min)
                m_tree->setCurrentItem(it);
        }
    refreshButtons();
}

void GoboWheelsDialog::setSlot(QTreeWidgetItem *item, const QString &image)
{
    setSlotGlass(item, &image, nullptr);
}

void GoboWheelsDialog::setSlotName(const QString &name)
{
    const QString n = name.trimmed();
    setSlotGlass(currentSlot(), nullptr, &n);
}

void GoboWheelsDialog::slotName()
{
    QTreeWidgetItem *slot = currentSlot();
    if (slot == nullptr)
        return;
    // Typed in place: the cell shows the glass's name, not "varies".
    {
        QScopedValueRollback<bool> building(m_building, true);
        const SlotKey key(slot->data(COL_SLOT, ROLE_CHANNEL).toUInt(), slot->data(COL_SLOT, ROLE_MIN).toInt());
        slot->setText(COL_NAME, m_names.value(key));
    }
    m_tree->editItem(slot, COL_NAME);
}

void GoboWheelsDialog::slotReplace()
{
    QTreeWidgetItem *slot = currentSlot();
    if (slot == nullptr || !pickImage)
        return;
    const QString path = pickImage(this);
    if (path.isEmpty())
        return;
    const QString name = GoboLibrary::import(path);
    if (name.isEmpty() || BeamOptics::goboMask(GoboLibrary::resolve(name)).isNull())
    {
        QMessageBox::warning(this, tr("Fit a gobo"),
                             tr("“%1” could not be read as a picture.").arg(QFileInfo(path).fileName()));
        return;
    }
    setSlot(slot, name);
}

void GoboWheelsDialog::slotRestore()
{
    // The definition's own glass: no picture of yours, no name.
    const QString none;
    setSlotGlass(currentSlot(), &none, &none);
}

void GoboWheelsDialog::slotOpenFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(GoboLibrary::userDirectory().absolutePath()));
}

void GoboWheelsDialog::accept()
{
    Fixture *fx = m_doc->fixture(m_fid);
    if (fx != nullptr)
    {
        // One rig undo step (⌘Z in the Fixture Manager or the 2D view),
        // however many units it changes.
        Monitor *mon = Monitor::instance();
        const QByteArray before = mon != nullptr ? mon->rigSnapshot() : QByteArray();
        MonitorProperties *props = m_doc->monitorProperties();
        if (!m_allOfType->isHidden() && m_allOfType->isChecked())
        {
            // Every unit of the type made like this one, every slot.
            foreach (Fixture *other, m_doc->fixtures())
            {
                if (other->fixtureDef() != fx->fixtureDef() || other->fixtureMode() != fx->fixtureMode())
                    continue;
                props->clearGoboSwaps(other->id());
                for (auto it = m_swaps.constBegin(); it != m_swaps.constEnd(); ++it)
                    props->setGoboSwap(other->id(), it.key().first, uchar(it.key().second), it.value());
                for (auto it = m_names.constBegin(); it != m_names.constEnd(); ++it)
                    props->setGoboName(other->id(), it.key().first, uchar(it.key().second), it.value());
            }
        }
        else
        {
            // The slots changed here, in every unit; the rest as each has it.
            for (quint32 id : m_fids)
            {
                const Fixture *unit = m_doc->fixture(id);
                if (unit == nullptr)
                    continue;
                for (const SlotKey &key : m_touched)
                {
                    const SlotKey k = keyOn(unit, key);
                    props->setGoboSwap(id, k.first, uchar(k.second), m_swaps.value(key));
                    props->setGoboName(id, k.first, uchar(k.second), m_names.value(key));
                }
            }
        }
        m_doc->setModified();
        if (mon != nullptr)
            mon->pushRigUndo(before);
    }
    QDialog::accept();
}
