/*
  Q Light Controller Plus
  effectspage.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QTreeWidgetItemIterator>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QHeaderView>
#include <QLineEdit>
#include <QSplitter>
#include <QSettings>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <algorithm>
#include <functional>

#include "effectspage.h"
#include "effectdesigner.h"
#include "rgbmatrixeditor.h"
#include "showtreecontroller.h"
#include "efxeditor.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "rgbmatrix.h"
#include "efx.h"
#include "doc.h"

#define SETTINGS_GEOMETRY QStringLiteral("effectspage/geometry")

namespace
{
    enum { KindRole = Qt::UserRole, IdRole = Qt::UserRole + 1 };
    enum { KindHeader = 0, KindNewEffect, KindPalette, KindFunction };

    QPointer<EffectsPage> s_page;
}

EffectsPage::EffectsPage(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_function(Function::invalidId())
    , m_selectAfterRebuild(QLCPalette::invalidId())
{
    setObjectName(QStringLiteral("effectsPage"));
    setWindowTitle(tr("Effects"));
    QHBoxLayout *h = new QHBoxLayout(this);
    h->setContentsMargins(4, 4, 4, 4);
    QSplitter *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    h->addWidget(split);

    // The navigator: effect palettes, then the classic effects.
    QWidget *left = new QWidget(split);
    QVBoxLayout *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *top = new QHBoxLayout();
    m_filter = new QLineEdit(left);
    m_filter->setObjectName(QStringLiteral("effectsFilter"));
    m_filter->setPlaceholderText(tr("Filter…"));
    m_filter->setClearButtonEnabled(true);
    top->addWidget(m_filter, 1);
    m_newBtn = new QToolButton(left);
    m_newBtn->setObjectName(QStringLiteral("effectsNew"));
    m_newBtn->setText(tr("+ New"));
    m_newBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *newMenu = new QMenu(m_newBtn);
    newMenu->setToolTipsVisible(true);
    QAction *a = newMenu->addAction(tr("Effect palette"), this, &EffectsPage::newEffect);
    a->setToolTip(tr("A script effect, designed here on the bench: plays in scenes and the Show Tree"));
    newMenu->addSection(tr("Classic effects (QLC+ functions)"));
    a = newMenu->addAction(QIcon(QStringLiteral(":/efx.png")), tr("EFX"), this, [this]() { newEfx(); });
    a->setToolTip(tr("A QLC+ EFX function on the Try on groups' heads, opened in its editor"));
    a = newMenu->addAction(QIcon(QStringLiteral(":/rgbmatrix.png")), tr("RGB Matrix"), this, [this]() { newRgbMatrix(); });
    a->setToolTip(tr("A QLC+ RGB Matrix function on the first Try on group, opened in its editor"));
    m_newBtn->setMenu(newMenu);
    top->addWidget(m_newBtn);
    lv->addLayout(top);

    m_nav = new QTreeWidget(left);
    m_nav->setObjectName(QStringLiteral("effectsNavigator"));
    m_nav->setHeaderHidden(true);
    m_nav->setColumnCount(2);
    m_nav->header()->setStretchLastSection(false);
    m_nav->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_nav->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_nav->setRootIsDecorated(true);
    lv->addWidget(m_nav, 1);
    connect(m_nav, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        itemActivated(item);
    });
    connect(m_filter, &QLineEdit::textChanged, this, &EffectsPage::applyFilter);

    // The designer on the bench, or a classic effect's own editor.
    m_stack = new QStackedWidget(split);
    m_designer = new EffectDesigner(doc, m_stack);
    m_designer->setCancelVisible(false);
    m_stack->addWidget(m_designer);
    m_editorPage = new QWidget(m_stack);
    QVBoxLayout *ev = new QVBoxLayout(m_editorPage);
    ev->setContentsMargins(0, 0, 0, 0);
    m_editorNote = new QLabel(m_editorPage);
    m_editorNote->setObjectName(QStringLiteral("effectsEditorNote"));
    m_editorNote->setWordWrap(true);
    m_editorNote->setStyleSheet(QStringLiteral("color: palette(mid);"));
    ev->addWidget(m_editorNote);
    m_stack->addWidget(m_editorPage);

    split->addWidget(left);
    split->addWidget(m_stack);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 4);
    split->setSizes({ 260, 1000 });

    // A palette saved: listed, and shown again (a copy of it, on the bench).
    connect(m_designer, &EffectDesigner::effectChosen, this,
            [this](quint32 pid, const QList<quint32> &, EffectDesigner::Outcome) {
        m_selectAfterRebuild = pid;
        scheduleRebuild();
    });
    // Make a layer: shown where it went (the Show Tree, Build).
    connect(m_designer, &EffectDesigner::layerMade, this,
            [this](quint32 cueId, quint32 layerId, quint32 sceneId, quint32 paletteId) {
        EffectDesigner::revealLayer(m_doc, nullptr, cueId, layerId, sceneId, paletteId);
    });

    if (doc != nullptr)
    {
        connect(doc, &Doc::paletteAdded, this, &EffectsPage::scheduleRebuild);
        connect(doc, &Doc::paletteRemoved, this, &EffectsPage::scheduleRebuild);
        connect(doc, &Doc::functionAdded, this, &EffectsPage::scheduleRebuild);
        connect(doc, &Doc::functionRemoved, this, [this](quint32 id) {
            if (id == m_function)
                closeEditor();
            scheduleRebuild();
        });
        // Before the Doc deletes the function the editor edits.
        connect(doc, &Doc::clearing, this, &EffectsPage::closeEditor);
        connect(doc, &Doc::cleared, this, &EffectsPage::scheduleRebuild);
        connect(doc, &Doc::loaded, this, [this]() {
            scheduleRebuild();
            newEffect();
        });
    }
    rebuild();
    newEffect();
}

EffectsPage::~EffectsPage()
{
    // The designer's draft and preview go with it, telling the Doc: not us,
    // half gone by then.
    if (m_doc != nullptr)
        disconnect(m_doc, nullptr, this, nullptr);
    closeEditor();
    delete m_designer;
    m_designer = nullptr;
}

EffectsPage *EffectsPage::open(Doc *doc, QWidget *parent)
{
    if (s_page.isNull())
    {
        s_page = new EffectsPage(doc, parent);
        s_page->setWindowFlags(Qt::Window);
        s_page->setAttribute(Qt::WA_DeleteOnClose, false);
        s_page->resize(1320, 820);
        const QByteArray geo = QSettings().value(SETTINGS_GEOMETRY).toByteArray();
        if (!geo.isEmpty())
            s_page->restoreGeometry(geo);
        // Remember where it was put.
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, s_page, []() {
            if (!s_page.isNull())
                QSettings().setValue(SETTINGS_GEOMETRY, s_page->saveGeometry());
        });
    }
    s_page->show();
    s_page->raise();
    s_page->activateWindow();
    return s_page;
}

QStringList EffectsPage::navigatorText() const
{
    QStringList out;
    for (QTreeWidgetItemIterator it(m_nav); *it; ++it)
    {
        if ((*it)->isHidden())
            continue;
        int depth = 0;
        for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
            ++depth;
        QString line = QString(depth * 2, QLatin1Char(' ')) + (*it)->text(0);
        if (!(*it)->text(1).isEmpty())
            line += QStringLiteral(" [%1]").arg((*it)->text(1));
        out << line;
    }
    return out;
}

void EffectsPage::scheduleRebuild()
{
    if (m_rebuildQueued)
        return;
    m_rebuildQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_rebuildQueued = false;
        rebuild();
    });
}

void EffectsPage::rebuild()
{
    if (m_doc == nullptr)
        return;
    QTreeWidgetItem *cur = m_nav->currentItem();
    int curKind = cur == nullptr ? -1 : cur->data(0, KindRole).toInt();
    quint32 curId = cur == nullptr ? 0 : cur->data(0, IdRole).toUInt();
    if (m_selectAfterRebuild != QLCPalette::invalidId())
    {
        curKind = KindPalette;
        curId = m_selectAfterRebuild;
        m_selectAfterRebuild = QLCPalette::invalidId();
    }

    QSignalBlocker block(m_nav);
    m_nav->clear();
    QFont bold = m_nav->font();
    bold.setBold(true);
    auto header = [&](const QString &text) {
        QTreeWidgetItem *h = new QTreeWidgetItem(m_nav, QStringList(text));
        h->setData(0, KindRole, KindHeader);
        h->setFlags(Qt::ItemIsEnabled);
        h->setFont(0, bold);
        h->setExpanded(true);
        return h;
    };

    QTreeWidgetItem *newRow = new QTreeWidgetItem(m_nav, QStringList(tr("✦ New effect")));
    newRow->setData(0, KindRole, KindNewEffect);
    newRow->setToolTip(0, tr("Start from nothing: pick an effect, preset or WLED effect in the browser"));

    // Effect palettes, in their folders.
    QTreeWidgetItem *pals = header(tr("Effect palettes"));
    pals->setToolTip(0, tr("Script effects: play in scenes, looks and the Show Tree, and in the designer"));
    ShowTreeController *st = m_doc->showTree();
    QList<QLCPalette *> effects;
    for (QLCPalette *p : m_doc->palettes())
        if (p->type() == QLCPalette::Effect && !st->isInlinePalette(p->id()))
            effects << p;
    std::sort(effects.begin(), effects.end(), [](const QLCPalette *a, const QLCPalette *b) {
        if (a->path() != b->path())
            return a->path().compare(b->path(), Qt::CaseInsensitive) < 0;
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    QHash<QString, QTreeWidgetItem *> folders;
    for (QLCPalette *p : effects)
    {
        QString folder = p->path();
        if (folder.startsWith(QLatin1String("Palettes/")))
            folder = folder.mid(9);
        while (folder.endsWith(QLatin1Char('/')))
            folder.chop(1);
        QTreeWidgetItem *parent = pals;
        QString sofar;
        for (const QString &part : folder.split(QLatin1Char('/'), Qt::SkipEmptyParts))
        {
            sofar += QLatin1Char('/') + part;
            QTreeWidgetItem *f = folders.value(sofar);
            if (f == nullptr)
            {
                f = new QTreeWidgetItem(parent, QStringList(part));
                f->setIcon(0, QIcon(QStringLiteral(":/folder.png")));
                f->setData(0, KindRole, KindHeader);
                f->setFlags(Qt::ItemIsEnabled);
                f->setExpanded(true);
                folders.insert(sofar, f);
            }
            parent = f;
        }
        QTreeWidgetItem *it = new QTreeWidgetItem(parent, QStringList(QStringLiteral("✦ ") + p->name()));
        it->setData(0, KindRole, KindPalette);
        it->setData(0, IdRole, p->id());
        it->setText(1, p->effectPreset().isEmpty() ? QString() : tr("preset"));
    }
    if (effects.isEmpty())
    {
        QTreeWidgetItem *none = new QTreeWidgetItem(pals, QStringList(tr("none yet: + New ▸ Effect palette")));
        none->setFlags(Qt::NoItemFlags);
    }

    // The classic effects: QLC+ functions of their own.
    QTreeWidgetItem *classic = header(tr("Classic effects"));
    classic->setToolTip(0, tr("QLC+ EFX and RGB Matrix functions: each is a function of its own, "
                              "with its own editor, and plays as a layer of its own"));
    QList<Function *> funcs;
    for (Function *f : m_doc->functions())
        if ((f->type() == Function::EFXType || f->type() == Function::RGBMatrixType) && f->isVisible()
            && !st->isGenerated(f->id()))
            funcs << f;
    std::sort(funcs.begin(), funcs.end(), [](const Function *a, const Function *b) {
        if (a->type() != b->type())
            return a->type() == Function::EFXType;
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (Function *f : funcs)
    {
        QTreeWidgetItem *it = new QTreeWidgetItem(classic, QStringList(f->name()));
        it->setIcon(0, QIcon(f->type() == Function::EFXType ? QStringLiteral(":/efx.png")
                                                             : QStringLiteral(":/rgbmatrix.png")));
        it->setData(0, KindRole, KindFunction);
        it->setData(0, IdRole, f->id());
        QString what = f->type() == Function::EFXType ? tr("EFX") : tr("RGB Matrix");
        if (const RGBMatrix *m = qobject_cast<const RGBMatrix *>(f))
            if (m->algorithm() != nullptr)
                what += QStringLiteral(" · ") + m->algorithm()->name();
        it->setText(1, what);
    }
    if (funcs.isEmpty())
    {
        QTreeWidgetItem *none = new QTreeWidgetItem(classic, QStringList(tr("none in this show: + New ▸ EFX / RGB Matrix")));
        none->setFlags(Qt::NoItemFlags);
    }

    // Back where it was.
    for (QTreeWidgetItemIterator it(m_nav); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == curKind
            && (curKind == KindNewEffect || (*it)->data(0, IdRole).toUInt() == curId))
        {
            m_nav->setCurrentItem(*it);
            break;
        }
    applyFilter();
    block.unblock();
    // A palette just saved: show it (a fresh copy on the bench).
    if (curKind == KindPalette && m_nav->currentItem() != nullptr
        && m_nav->currentItem()->data(0, IdRole).toUInt() == curId && m_designer->draftId() == QLCPalette::invalidId())
        itemActivated(m_nav->currentItem());
}

void EffectsPage::applyFilter()
{
    const QString q = m_filter->text().trimmed();
    // Leaves match by name; a folder or header shows when any leaf under it does.
    std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) -> bool {
        bool any = false;
        for (int i = 0; i < item->childCount(); ++i)
            any |= visit(item->child(i));
        const int kind = item->data(0, KindRole).toInt();
        bool show;
        if (kind == KindHeader && item->childCount() > 0)
            show = any || q.isEmpty();
        else
            show = q.isEmpty() || item->text(0).contains(q, Qt::CaseInsensitive)
                   || item->text(1).contains(q, Qt::CaseInsensitive);
        item->setHidden(!show);
        return show;
    };
    for (int i = 0; i < m_nav->topLevelItemCount(); ++i)
        visit(m_nav->topLevelItem(i));
}

void EffectsPage::itemActivated(QTreeWidgetItem *item)
{
    if (item == nullptr)
        return;
    const int kind = item->data(0, KindRole).toInt();
    const quint32 id = item->data(0, IdRole).toUInt();
    if (kind == KindPalette)
        selectPalette(id);
    else if (kind == KindFunction)
        selectFunction(id);
    else if (kind == KindNewEffect)
        newEffect();
}

bool EffectsPage::selectPalette(quint32 paletteId)
{
    const QLCPalette *p = m_doc == nullptr ? nullptr : m_doc->palette(paletteId);
    if (p == nullptr || p->type() != QLCPalette::Effect)
        return false;
    closeEditor();
    m_stack->setCurrentWidget(m_designer);
    m_designer->setTargets(QList<quint32>(), QList<quint32>());
    m_designer->setEffect(paletteId);
    for (QTreeWidgetItemIterator it(m_nav); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == KindPalette && (*it)->data(0, IdRole).toUInt() == paletteId)
        {
            QSignalBlocker block(m_nav);
            m_nav->setCurrentItem(*it);
        }
    return true;
}

void EffectsPage::newEffect()
{
    closeEditor();
    m_stack->setCurrentWidget(m_designer);
    m_designer->setTargets(QList<quint32>(), QList<quint32>());
    m_designer->setEffect(QLCPalette::invalidId());
}

bool EffectsPage::selectFunction(quint32 functionId)
{
    Function *f = m_doc == nullptr ? nullptr : m_doc->function(functionId);
    if (f == nullptr || (f->type() != Function::EFXType && f->type() != Function::RGBMatrixType))
        return false;
    // The designer's draft goes: this is another kind of effect.
    if (m_designer->draftId() != QLCPalette::invalidId())
        m_designer->cancel();
    closeEditor();
    QWidget *ed = nullptr;
    if (f->type() == Function::EFXType)
        ed = new EFXEditor(m_editorPage, qobject_cast<EFX *>(f), m_doc);
    else
        ed = new RGBMatrixEditor(m_editorPage, qobject_cast<RGBMatrix *>(f), m_doc);
    m_function = functionId;
    showEditor(ed, f->type() == Function::EFXType
                       ? tr("%1 is an EFX: a QLC+ function of its own, edited live here. It plays as a "
                            "layer of its own in a cue or collection.").arg(f->name())
                       : tr("%1 is an RGB Matrix: a QLC+ function of its own, edited live here. It plays "
                            "as a layer of its own in a cue or collection.").arg(f->name()));
    for (QTreeWidgetItemIterator it(m_nav); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == KindFunction && (*it)->data(0, IdRole).toUInt() == functionId)
        {
            QSignalBlocker block(m_nav);
            m_nav->setCurrentItem(*it);
        }
    return true;
}

void EffectsPage::showEditor(QWidget *editor, const QString &note)
{
    m_editor = editor;
    m_editorNote->setText(note);
    m_editorPage->layout()->addWidget(editor);
    editor->show();
    m_stack->setCurrentWidget(m_editorPage);
}

void EffectsPage::closeEditor()
{
    if (m_editor != nullptr)
    {
        if (EFXEditor *e = qobject_cast<EFXEditor *>(m_editor))
            e->stopTest();
        else if (RGBMatrixEditor *r = qobject_cast<RGBMatrixEditor *>(m_editor))
            r->stopTest();
        delete m_editor;
        m_editor = nullptr;
    }
    m_function = Function::invalidId();
}

quint32 EffectsPage::newEfx()
{
    if (m_doc == nullptr)
        return Function::invalidId();
    EFX *efx = new EFX(m_doc);
    if (!m_doc->addFunction(efx))
    {
        delete efx;
        return Function::invalidId();
    }
    efx->setName(tr("New EFX %1").arg(efx->id()));
    // Its heads: the Try on groups', as the bench plays.
    for (quint32 gid : m_designer->tryOnGroups())
        if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
            for (const GroupHead &head : g->headList())
                efx->addFixture(head.fxi, head.head);
    scheduleRebuild();
    selectFunction(efx->id());
    return efx->id();
}

quint32 EffectsPage::newRgbMatrix()
{
    if (m_doc == nullptr)
        return Function::invalidId();
    RGBMatrix *m = new RGBMatrix(m_doc);
    if (!m_doc->addFunction(m))
    {
        delete m;
        return Function::invalidId();
    }
    m->setName(tr("New RGB Matrix %1").arg(m->id()));
    if (!m_designer->tryOnGroups().isEmpty())
        m->setFixtureGroup(m_designer->tryOnGroups().first());
    scheduleRebuild();
    selectFunction(m->id());
    return m->id();
}
