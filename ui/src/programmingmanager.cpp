/*
  Q Light Controller Plus
  programmingmanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMimeData>
#include <QDataStream>

#include <algorithm>

#include "programmingmanager.h"
#include "functionstreewidget.h"   // palette MIME type
#include "fixturegroupsource.h"    // fixture-group MIME type
#include "scenegrouplooks.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

/**
 * Minimal drag-source list: each row carries a quint32 ID in Qt::UserRole,
 * dragged out as a stream under the configured MIME type. No Q_OBJECT
 * needed (only mimeData() is overridden).
 */
class SourceListWidget : public QListWidget
{
public:
    SourceListWidget(const char *mimeType, QWidget *parent = nullptr)
        : QListWidget(parent)
        , m_mime(mimeType)
    {
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setDragEnabled(true);
        setAcceptDrops(false);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    QMimeData *mimeData(const QList<QListWidgetItem*> items) const override
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        int count = 0;
        foreach (QListWidgetItem *it, items)
        {
            QVariant v = it->data(Qt::UserRole);
            if (v.isValid())
            {
                stream << v.toUInt();
                count++;
            }
        }
        if (count == 0)
            return QListWidget::mimeData(items);

        QMimeData *mime = new QMimeData();
        mime->setData(m_mime, data);
        return mime;
    }

private:
    QByteArray m_mime;
};

ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_canvas(nullptr)
    , m_currentScene(Function::invalidId())
{
    QHBoxLayout *root = new QHBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter);

    // --- Left: scenes you're building ---
    QWidget *scenePanel = new QWidget(this);
    QVBoxLayout *sceneCol = new QVBoxLayout(scenePanel);
    sceneCol->setContentsMargins(0, 0, 0, 0);
    sceneCol->addWidget(new QLabel(tr("Scenes"), this));
    m_sceneList = new QListWidget(this);
    sceneCol->addWidget(m_sceneList, 1);
    QPushButton *newScene = new QPushButton(tr("New scene"), this);
    sceneCol->addWidget(newScene);
    splitter->addWidget(scenePanel);

    // --- Center: the selected scene's canvas (SceneGroupLooks) ---
    QWidget *canvasPanel = new QWidget(this);
    m_canvasLayout = new QVBoxLayout(canvasPanel);
    m_canvasPlaceholder = new QLabel(
        tr("Select or create a scene, then drag palettes and fixture groups "
           "from the right onto it."), this);
    m_canvasPlaceholder->setWordWrap(true);
    m_canvasPlaceholder->setAlignment(Qt::AlignCenter);
    m_canvasLayout->addWidget(m_canvasPlaceholder);
    m_canvasLayout->addStretch(1);
    splitter->addWidget(canvasPanel);

    // --- Right: searchable drag sources (palettes + fixture groups) ---
    QWidget *sourcePanel = new QWidget(this);
    QVBoxLayout *srcCol = new QVBoxLayout(sourcePanel);
    srcCol->setContentsMargins(0, 0, 0, 0);

    srcCol->addWidget(new QLabel(tr("Palettes (looks)"), this));
    m_paletteFilter = new QLineEdit(this);
    m_paletteFilter->setPlaceholderText(tr("Filter palettes…"));
    m_paletteFilter->setClearButtonEnabled(true);
    srcCol->addWidget(m_paletteFilter);
    m_paletteList = new SourceListWidget(
        FunctionsTreeWidget::paletteDragMimeType(), this);
    srcCol->addWidget(m_paletteList, 1);

    srcCol->addWidget(new QLabel(tr("Fixture groups"), this));
    m_groupFilter = new QLineEdit(this);
    m_groupFilter->setPlaceholderText(tr("Filter groups…"));
    m_groupFilter->setClearButtonEnabled(true);
    srcCol->addWidget(m_groupFilter);
    m_groupList = new SourceListWidget(
        FixtureGroupSource::fixtureGroupMimeType(), this);
    srcCol->addWidget(m_groupList, 1);

    splitter->addWidget(sourcePanel);

    splitter->setStretchFactor(0, 1); // scenes
    splitter->setStretchFactor(1, 3); // canvas
    splitter->setStretchFactor(2, 2); // sources

    connect(newScene, SIGNAL(clicked()), this, SLOT(slotNewScene()));
    connect(m_sceneList, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotSceneSelected()));
    connect(m_paletteFilter, SIGNAL(textChanged(QString)),
            this, SLOT(slotPaletteFilter(QString)));
    connect(m_groupFilter, SIGNAL(textChanged(QString)),
            this, SLOT(slotGroupFilter(QString)));

    // Keep both columns and the source lists in sync with the Doc.
    connect(m_doc, SIGNAL(functionAdded(quint32)), this, SLOT(slotReloadScenes()));
    connect(m_doc, SIGNAL(functionRemoved(quint32)), this, SLOT(slotReloadScenes()));
    connect(m_doc, SIGNAL(functionChanged(quint32)), this, SLOT(slotReloadScenes()));
    connect(m_doc, SIGNAL(loaded()), this, SLOT(slotReloadScenes()));
    connect(m_doc, SIGNAL(loaded()), this, SLOT(slotReloadSources()));
    connect(m_doc, SIGNAL(cleared()), this, SLOT(slotReloadScenes()));

    connect(m_doc, SIGNAL(paletteAdded(quint32)), this, SLOT(slotReloadSources()));
    connect(m_doc, SIGNAL(paletteRemoved(quint32)), this, SLOT(slotReloadSources()));
    connect(m_doc, SIGNAL(fixtureGroupAdded(quint32)), this, SLOT(slotReloadSources()));
    connect(m_doc, SIGNAL(fixtureGroupRemoved(quint32)), this, SLOT(slotReloadSources()));
    connect(m_doc, SIGNAL(fixtureGroupChanged(quint32)), this, SLOT(slotReloadSources()));

    slotReloadScenes();
    slotReloadSources();
}

ProgrammingManager::~ProgrammingManager()
{
}

void ProgrammingManager::slotReloadScenes()
{
    const quint32 keep = m_currentScene;

    m_sceneList->blockSignals(true);
    m_sceneList->clear();

    QList<Function*> scenes;
    foreach (Function *f, m_doc->functions())
        if (f != NULL && f->type() == Function::SceneType && f->isVisible())
            scenes.append(f);
    std::sort(scenes.begin(), scenes.end(),
              [](Function *a, Function *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });

    QListWidgetItem *toSelect = NULL;
    foreach (Function *f, scenes)
    {
        QListWidgetItem *it = new QListWidgetItem(f->name(), m_sceneList);
        it->setData(Qt::UserRole, f->id());
        if (f->id() == keep)
            toSelect = it;
    }
    m_sceneList->blockSignals(false);

    if (toSelect != NULL)
        toSelect->setSelected(true);
    else if (m_currentScene != Function::invalidId())
        loadCanvas(Function::invalidId()); // the edited scene went away
}

void ProgrammingManager::slotSceneSelected()
{
    const QList<QListWidgetItem*> sel = m_sceneList->selectedItems();
    if (sel.isEmpty())
        return;
    loadCanvas(sel.first()->data(Qt::UserRole).toUInt());
}

void ProgrammingManager::loadCanvas(quint32 sceneId)
{
    m_currentScene = sceneId;

    if (m_canvas != NULL)
    {
        m_canvas->deleteLater();
        m_canvas = NULL;
    }

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (scene == NULL)
    {
        m_canvasPlaceholder->show();
        return;
    }

    m_canvasPlaceholder->hide();
    m_canvas = new SceneGroupLooks(scene, m_doc, this);
    // Insert above the trailing stretch.
    m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, m_canvas, 1);
}

void ProgrammingManager::slotNewScene()
{
    Scene *scene = new Scene(m_doc);
    if (m_doc->addFunction(scene) == false)
    {
        delete scene;
        return;
    }
    scene->setName(tr("New Scene %1").arg(scene->id()));
    m_currentScene = scene->id();
    slotReloadScenes();
}

void ProgrammingManager::slotReloadSources()
{
    // --- Palettes ---
    m_paletteList->clear();
    QList<QLCPalette*> palettes;
    foreach (QLCPalette *p, m_doc->palettes())
        if (p != NULL)
            palettes.append(p);
    std::sort(palettes.begin(), palettes.end(),
              [](QLCPalette *a, QLCPalette *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });
    foreach (QLCPalette *p, palettes)
    {
        QListWidgetItem *it = new QListWidgetItem(
            tr("%1  (%2)").arg(p->name())
                .arg(QLCPalette::typeToString(p->type())), m_paletteList);
        it->setData(Qt::UserRole, p->id());
    }

    // --- Fixture groups ---
    m_groupList->clear();
    QList<FixtureGroup*> groups;
    foreach (FixtureGroup *g, m_doc->fixtureGroups())
        if (g != NULL)
            groups.append(g);
    std::sort(groups.begin(), groups.end(),
              [](FixtureGroup *a, FixtureGroup *b) {
                  return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
              });
    foreach (FixtureGroup *g, groups)
    {
        const int n = g->fixtureList().count();
        QListWidgetItem *it = new QListWidgetItem(
            tr("%1  (%n fixture(s))", "", n).arg(g->name()), m_groupList);
        it->setData(Qt::UserRole, g->id());
    }

    slotPaletteFilter(m_paletteFilter->text());
    slotGroupFilter(m_groupFilter->text());
}

void ProgrammingManager::slotPaletteFilter(const QString &text)
{
    for (int i = 0; i < m_paletteList->count(); i++)
    {
        QListWidgetItem *it = m_paletteList->item(i);
        it->setHidden(text.isEmpty() == false
                      && it->text().contains(text, Qt::CaseInsensitive) == false);
    }
}

void ProgrammingManager::slotGroupFilter(const QString &text)
{
    for (int i = 0; i < m_groupList->count(); i++)
    {
        QListWidgetItem *it = m_groupList->item(i);
        it->setHidden(text.isEmpty() == false
                      && it->text().contains(text, Qt::CaseInsensitive) == false);
    }
}
