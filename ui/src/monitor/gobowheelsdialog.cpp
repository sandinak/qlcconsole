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

#include "gobowheelsdialog.h"
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
        QImage alpha = mask.scaled(n, n, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_Alpha8);
        light.setAlphaChannel(alpha);
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

GoboWheelsDialog::GoboWheelsDialog(Doc *doc, quint32 fixtureId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_fid(fixtureId)
{
    Fixture *fx = m_doc->fixture(m_fid);
    setWindowTitle(tr("Gobo Wheels — %1").arg(fx != nullptr ? fx->name() : QString()));
    resize(560, 460);

    pickImage = [](QWidget *parent) {
        return QFileDialog::getOpenFileName(parent, tr("Fit a gobo"),
            GoboLibrary::userDirectory().absolutePath(),
            tr("Gobo pictures (%1)").arg(GoboLibrary::imageFilters().join(' ')));
    };

    if (fx != nullptr)
        m_swaps = m_doc->monitorProperties()->goboSwaps(m_fid);

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("The glass in each slot of this unit. Replace a slot with "
                                  "your own picture — black blocks the light, clear or white "
                                  "lets it through. The fixture definition is not changed."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels(QStringList() << tr("Slot") << tr("Glass") << tr("Suggested"));
    m_tree->setIconSize(QSize(32, 32));
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setSectionResizeMode(COL_SLOT, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(COL_GLASS, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(COL_SUGGEST, QHeaderView::ResizeToContents);
    root->addWidget(m_tree, 1);

    QHBoxLayout *row = new QHBoxLayout();
    m_replace = new QPushButton(tr("Replace…"), this);
    m_restore = new QPushButton(tr("Restore"), this);
    QPushButton *folder = new QPushButton(tr("Open Gobo Folder"), this);
    row->addWidget(m_replace);
    row->addWidget(m_restore);
    m_fitSuggested = new QPushButton(tr("Fit suggested pictures"), this);
    m_fitSuggested->setToolTip(tr("Fit the first suggestion in every slot that has no picture of its "
                                  "own. Nothing is kept until you press OK."));
    row->addWidget(m_fitSuggested);
    row->addStretch(1);
    row->addWidget(folder);
    root->addLayout(row);

    int sameType = 0;
    if (fx != nullptr)
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
    connect(m_replace, &QPushButton::clicked, this, &GoboWheelsDialog::slotReplace);
    connect(m_restore, &QPushButton::clicked, this, &GoboWheelsDialog::slotRestore);
    connect(m_fitSuggested, &QPushButton::clicked, this, &GoboWheelsDialog::slotFitSuggested);
    connect(folder, &QPushButton::clicked, this, &GoboWheelsDialog::slotOpenFolder);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &GoboWheelsDialog::refreshButtons);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->parent() != nullptr)
            slotReplace();
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
            QTreeWidgetItem *slot = new QTreeWidgetItem(wheel);
            slot->setData(COL_SLOT, ROLE_CHANNEL, i);
            slot->setData(COL_SLOT, ROLE_MIN, int(cap->min()));
            slot->setText(COL_SLOT, glass.size() > 1 ? tr("%1 (and its shake)").arg(cap->name())
                                                     : cap->name());
            const QString swap = m_swaps.value(qMakePair(i, int(cap->min())));
            const QString img = effectiveImage(i, cap);
            if (img.isEmpty())
            {
                // The rig view's stand-in for it.
                const QImage m = BeamOptics::standInMask(cap->name(),
                    qMax(0, GoboLibrary::standInIndex(ch, cap)));
                slot->setIcon(COL_SLOT, lightIcon(m));
                if (swap.isEmpty())
                {
                    const QStringList sug = suggestionsFor(cap);
                    if (!sug.isEmpty())
                    {
                        // Up to three, best first; Fit takes the one chosen.
                        QComboBox *box = new QComboBox(m_tree);
                        box->setObjectName(QStringLiteral("suggestions"));
                        box->setIconSize(QSize(28, 28));
                        foreach (const QString &name, sug)
                            box->addItem(lightIcon(BeamOptics::goboMask(GoboLibrary::resolve(name))),
                                         QFileInfo(name).completeBaseName(), name);
                        m_tree->setItemWidget(slot, COL_SUGGEST, box);
                    }
                }
            }
            else
            {
                const QIcon lit = lightIcon(BeamOptics::goboMask(img));
                slot->setIcon(COL_SLOT, lit.isNull() ? QIcon(img) : lit);
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
        }
    }
}

QStringList GoboWheelsDialog::suggestionsFor(const QLCCapability *cap) const
{
    return cap != nullptr ? suggestPictures(m_doc, GoboLibrary::slotBaseName(cap->name())) : QStringList();
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
                m_swaps.insert(qMakePair(chn, int(g->min())), name);
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
    if (m_fitSuggested != nullptr)
    {
        bool any = false, standIns = false;
        Fixture *fx = m_doc->fixture(m_fid);
        for (quint32 i = 0; fx != nullptr && i < fx->channels(); ++i)
            foreach (QLCCapability *cap, GoboLibrary::standInCaps(fx->channel(i)))
            {
                standIns = true;
                if (!any && !m_swaps.contains(qMakePair(i, int(cap->min()))) && !suggestionsFor(cap).isEmpty())
                    any = true;
            }
        m_fitSuggested->setVisible(standIns);
        m_fitSuggested->setEnabled(any);
        m_tree->setColumnHidden(COL_SUGGEST, !standIns);
    }
    m_restore->setEnabled(slot != nullptr
        && m_swaps.contains(qMakePair(slot->data(COL_SLOT, ROLE_CHANNEL).toUInt(),
                                      slot->data(COL_SLOT, ROLE_MIN).toInt())));
}

void GoboWheelsDialog::setSlot(QTreeWidgetItem *item, const QString &image)
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
        if (image.isEmpty())
            m_swaps.remove(qMakePair(chn, int(g->min())));
        else
            m_swaps.insert(qMakePair(chn, int(g->min())), image);
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
    setSlot(currentSlot(), QString());
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
        MonitorProperties *props = m_doc->monitorProperties();
        QList<quint32> targets;
        if (!m_allOfType->isHidden() && m_allOfType->isChecked())
        {
            foreach (Fixture *other, m_doc->fixtures())
                if (other->fixtureDef() == fx->fixtureDef() && other->fixtureMode() == fx->fixtureMode())
                    targets << other->id();
        }
        else
            targets << m_fid;
        foreach (quint32 t, targets)
        {
            props->clearGoboSwaps(t);
            for (auto it = m_swaps.constBegin(); it != m_swaps.constEnd(); ++it)
                props->setGoboSwap(t, it.key().first, uchar(it.key().second), it.value());
        }
        m_doc->setModified();
    }
    QDialog::accept();
}
