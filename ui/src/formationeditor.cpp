/*
  Q Light Controller Plus
  formationeditor.cpp

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

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <climits>
#include <algorithm>

#include "formationeditor.h"
#include "linespreadpreview.h"
#include "lineplanview.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const int kGroupData[] = { 1, 2, 3, 4, 0 };     // each, pairs, threes, fours, halves
const char *kGroupWords[] = { "each", "pairs", "threes", "fours", "halves" };
}

FormationEditor::FormationEditor(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("formationEditor"));
    QVBoxLayout *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("formationSummary"));
    m_summary->setWordWrap(true);
    col->addWidget(m_summary);

    QGridLayout *grid = new QGridLayout();
    m_spreadSet = new QCheckBox(tr("Spread"), this);
    m_spreadSet->setObjectName(QStringLiteral("formationSpreadSet"));
    m_spreadSet->setToolTip(tr("Ticked: this Formation sets the Spread. Unticked: it comes from elsewhere "
                               "(the cue's formation, else the Aim look's own)."));
    m_spreadSlider = new QSlider(Qt::Horizontal, this);
    m_spreadSlider->setRange(-50, 50);
    m_spreadSpin = new QDoubleSpinBox(this);
    m_spreadSpin->setObjectName(QStringLiteral("formationSpreadSpin"));
    m_spreadSpin->setRange(-5.0, 5.0);
    m_spreadSpin->setSingleStep(0.5);
    m_spreadSpin->setDecimals(1);
    m_spreadSpin->setToolTip(tr("+5 end to end, 0 all on the centre, below 0 crossed"));
    grid->addWidget(m_spreadSet, 0, 0);
    grid->addWidget(m_spreadSlider, 0, 1);
    grid->addWidget(m_spreadSpin, 0, 2);

    m_groupSet = new QCheckBox(tr("Grouping"), this);
    m_groupSet->setObjectName(QStringLiteral("formationGroupSet"));
    m_groupCombo = new QComboBox(this);
    m_groupCombo->setObjectName(QStringLiteral("formationGroupCombo"));
    const QStringList groupLabels { tr("Each light"), tr("Pairs"), tr("Threes"), tr("Fours"), tr("Halves") };
    for (int i = 0; i < groupLabels.count(); ++i)
        m_groupCombo->addItem(groupLabels.at(i), QString::fromLatin1(kGroupWords[i]));
    grid->addWidget(m_groupSet, 1, 0);
    grid->addWidget(m_groupCombo, 1, 1, 1, 2);

    m_crossSet = new QCheckBox(tr("Cross"), this);
    m_crossSet->setObjectName(QStringLiteral("formationCrossSet"));
    m_crossCombo = new QComboBox(this);
    m_crossCombo->setObjectName(QStringLiteral("formationCrossCombo"));
    m_crossCombo->addItem(tr("Groups swap sides"), QStringLiteral("swap"));
    m_crossCombo->addItem(tr("Each group flips (mirror)"), QStringLiteral("mirror"));
    grid->addWidget(m_crossSet, 2, 0);
    grid->addWidget(m_crossCombo, 2, 1, 1, 2);
    col->addLayout(grid);

    // The preview: a line and lights to see it on -- never saved.
    QLabel *previewHead = new QLabel(tr("<b>Preview</b> -- only to see it: drawn here, nothing plays on stage "
                                        "(like Blind), and the pick isn't saved."), this);
    previewHead->setObjectName(QStringLiteral("formationPreviewNote"));
    previewHead->setWordWrap(true);
    col->addWidget(previewHead);

    QHBoxLayout *lineRow = new QHBoxLayout();
    lineRow->addWidget(new QLabel(tr("Line"), this));
    m_lineCombo = new QComboBox(this);
    m_lineCombo->setObjectName(QStringLiteral("formationPreviewLine"));
    lineRow->addWidget(m_lineCombo, 1);
    col->addLayout(lineRow);

    m_pickList = new QListWidget(this);
    m_pickList->setObjectName(QStringLiteral("formationPreviewPick"));
    m_pickList->setToolTip(tr("Tick groups (several make one formation, numbered by where the lights hang, as a "
                              "cue-wide aim does) or fixtures one by one."));
    m_pickList->setMaximumHeight(110);
    col->addWidget(m_pickList);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("formationPreviewHint"));
    m_hint->setWordWrap(true);
    m_hint->setTextFormat(Qt::RichText);
    connect(m_hint, &QLabel::linkActivated, this, [this](const QString &) { clearPick(); });
    col->addWidget(m_hint);

    m_strip = new LineSpreadPreview(m_doc, this);
    col->addWidget(m_strip);
    m_plan = new LinePlanView(m_doc, this);
    col->addWidget(m_plan, 1);

    connect(m_spreadSet, &QCheckBox::toggled, this, &FormationEditor::slotSettingChanged);
    connect(m_groupSet, &QCheckBox::toggled, this, &FormationEditor::slotSettingChanged);
    connect(m_crossSet, &QCheckBox::toggled, this, &FormationEditor::slotSettingChanged);
    connect(m_groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FormationEditor::slotSettingChanged);
    connect(m_crossCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FormationEditor::slotSettingChanged);
    connect(m_spreadSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_loading)
            return;
        m_loading = true;
        m_spreadSpin->setValue(v / 10.0);
        m_loading = false;
        slotSettingChanged();
    });
    connect(m_spreadSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_loading)
            return;
        m_loading = true;
        m_spreadSlider->setValue(qRound(v * 10.0));
        m_loading = false;
        slotSettingChanged();
    });
    connect(m_lineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &FormationEditor::slotPickChanged);
    connect(m_pickList, &QListWidget::itemChanged, this, &FormationEditor::slotPickChanged);
    if (m_doc != nullptr)
        connect(m_doc, &Doc::cleared, this, &FormationEditor::slotDocCleared);
    // The pop-out (as the Show Tree's): the same preview, bigger.
    connect(m_strip, &LineSpreadPreview::popOutRequested, this, &FormationEditor::openPlanWindow);
    connect(m_plan, &LinePlanView::popOutRequested, this, &FormationEditor::openPlanWindow);
}

LinePlanWindow *FormationEditor::planWindow() const
{
    return m_planWindow.data();
}

void FormationEditor::openPlanWindow()
{
    if (m_planWindow == nullptr)
        m_planWindow = new LinePlanWindow(m_doc, this);
    m_planWindow->show();
    m_planWindow->raise();
    m_planWindow->activateWindow();
    updatePreview();
}

FormationEditor::~FormationEditor()
{
}

void FormationEditor::slotDocCleared()
{
    // Another show: its palette ids mean other palettes.
    m_picks.clear();
    m_pick = Pick();
    m_paletteId = QLCPalette::invalidId();
}

void FormationEditor::setPalette(quint32 paletteId)
{
    const QLCPalette *p = m_doc == nullptr ? nullptr : m_doc->palette(paletteId);
    if (p == nullptr || p->type() != QLCPalette::Formation)
    {
        m_paletteId = QLCPalette::invalidId();
        return;
    }
    const bool other = paletteId != m_paletteId;
    m_paletteId = paletteId;
    if (other)
        m_pick = m_picks.contains(paletteId) ? m_picks.value(paletteId) : defaultPick(m_doc);
    syncControls();
    syncPicker();
    updatePreview();
}

void FormationEditor::syncControls()
{
    const QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr)
        return;
    m_loading = true;
    const bool hasSpread = p->formationHas(QLCPalette::FormationSpread);
    const bool hasGroup = p->formationHas(QLCPalette::FormationGroup);
    const bool hasCross = p->formationHas(QLCPalette::FormationCross);
    m_spreadSet->setChecked(hasSpread);
    const double spread = hasSpread ? p->formationValue(QLCPalette::FormationSpread) : 5.0;
    m_spreadSpin->setValue(spread);
    m_spreadSlider->setValue(qRound(spread * 10.0));
    m_spreadSpin->setEnabled(hasSpread);
    m_spreadSlider->setEnabled(hasSpread);
    m_groupSet->setChecked(hasGroup);
    const int g = hasGroup ? int(p->formationValue(QLCPalette::FormationGroup)) : 1;
    for (int i = 0; i < 5; ++i)
        if (kGroupData[i] == g)
            m_groupCombo->setCurrentIndex(i);
    m_groupCombo->setEnabled(hasGroup);
    m_crossSet->setChecked(hasCross);
    m_crossCombo->setCurrentIndex(hasCross && p->formationValue(QLCPalette::FormationCross) != 0.0 ? 1 : 0);
    m_crossCombo->setEnabled(hasCross);
    m_summary->setText(tr("<b>Formation:</b> %1. Drop it on a layer (or a cue, for the cue's formation) "
                          "beside an Aim at a line. A layer's own Spread / Grouping / Cross rows still win "
                          "over it; what it leaves unset comes from the cue, else the Aim look. Shared: "
                          "changing it changes every cue that uses it.")
                       .arg(ShowTreeController::formationSummary(p).toHtmlEscaped()));
    m_loading = false;
}

void FormationEditor::commit(const QString &key, const QString &value)
{
    ShowTreeController *st = m_doc->showTree();
    if (st == nullptr)
        return;
    if (st->setFormationValue(m_paletteId, key, value))
        emit edited(m_paletteId);
}

void FormationEditor::slotSettingChanged()
{
    if (m_loading)
        return;
    const QLCPalette *p = m_doc->palette(m_paletteId);
    if (p == nullptr)
        return;
    // Each control says what it wants; only what differs is written.
    const QList<Look> now = ShowTreeController::formationLooks(p);
    auto have = [&now](const QString &key) {
        for (const Look &l : now)
            if (l.key == key)
                return l.value;
        return QString();
    };
    const QString spread = m_spreadSet->isChecked()
            ? ShowTreeController::lineParamValue(KeySpread, QString::number(m_spreadSpin->value())) : QString();
    const QString group = m_groupSet->isChecked() ? m_groupCombo->currentData().toString() : QString();
    const QString cross = m_crossSet->isChecked() ? m_crossCombo->currentData().toString() : QString();
    if (spread != have(KeySpread))
        commit(KeySpread, spread);
    if (group != have(KeyGrouping))
        commit(KeyGrouping, group);
    if (cross != have(KeyCross))
        commit(KeyCross, cross);
    syncControls();
    updatePreview();
}

/*****************************************************************************
 * Preview
 *****************************************************************************/

static QList<const StageTarget *> lineTargets(Doc *doc)
{
    QList<const StageTarget *> out;
    const MonitorProperties *mp = doc == nullptr ? nullptr : doc->monitorProperties();
    if (mp == nullptr)
        return out;
    for (const StageTarget *t : mp->stageTargets())
        if (t != nullptr && t->isLine())
            out << t;
    std::sort(out.begin(), out.end(), [](const StageTarget *a, const StageTarget *b) { return a->id() < b->id(); });
    return out;
}

FormationEditor::Pick FormationEditor::defaultPick(Doc *doc)
{
    Pick pick;
    const QList<const StageTarget *> lines = lineTargets(doc);
    if (lines.isEmpty())
        return pick;
    pick.line = lines.first()->id();
    ShowTreeController *st = doc->showTree();
    if (st == nullptr)
        return pick;

    // A target's groups, a set as its groups.
    auto groupsOf = [st](const Target &t) {
        QList<quint32> out = t.groups;
        for (quint32 sid : t.sets)
            for (const GroupSet &gs : st->groupSets())
                if (gs.id == sid)
                    for (quint32 g : gs.groups)
                        if (!out.contains(g))
                            out << g;
        return out;
    };
    auto aimsHere = [doc, &pick](const Look *look) {
        if (look == nullptr || look->none || look->palettes.isEmpty())
            return false;
        const QLCPalette *p = doc->palette(look->palettes.first());
        return p != nullptr && p->type() == QLCPalette::Aim && p->stageTargetId() == pick.line;
    };
    // How often each set of groups is aimed there, in the order first seen,
    // and the cue that first does.
    QList<QList<quint32>> seen;
    QList<int> counts;
    QStringList firstCue;
    QString cueNow;
    auto tally = [&seen, &counts, &firstCue, &cueNow](QList<quint32> groups) {
        if (groups.isEmpty())
            return;
        std::sort(groups.begin(), groups.end());
        const int at = seen.indexOf(groups);
        if (at < 0)
        {
            seen << groups;
            counts << 1;
            firstCue << cueNow;
        }
        else
            ++counts[at];
    };
    for (const Production &prod : st->productions())
        for (const CueList &list : prod.lists)
            for (int ci = 0; ci < list.cues.count(); ++ci)
            {
                const Cue &cue = list.cues.at(ci);
                cueNow = st->cueLabel(list, ci);
                const bool cueAim = aimsHere(cue.look(KeyPosition));
                QList<quint32> together;
                for (const Layer &layer : cue.layers)
                {
                    if (layer.target.isOpaque())
                        continue;
                    const Look *own = layer.look(KeyPosition);
                    if (aimsHere(own))
                        tally(groupsOf(layer.target));
                    else if (cueAim && own == nullptr && layer.parent == invalidId())
                        for (quint32 g : groupsOf(layer.target))
                            if (!together.contains(g))
                                together << g;
                }
                tally(together);
            }
    int best = -1;
    for (int i = 0; i < counts.count(); ++i)
        if (best < 0 || counts.at(i) > counts.at(best))
            best = i;
    if (best >= 0)
    {
        pick.groups = seen.at(best);
        QStringList names;
        for (quint32 gid : pick.groups)
            if (const FixtureGroup *g = doc->fixtureGroup(gid))
                names << g->name();
        const MonitorProperties *mp = doc->monitorProperties();
        const StageTarget *line = mp == nullptr ? nullptr : mp->stageTarget(pick.line);
        pick.note = tr("Showing %1 (aimed at %2 in cue %3).")
                        .arg(names.join(QStringLiteral(" + ")),
                             line != nullptr ? line->name() : tr("this line"),
                             firstCue.at(best).trimmed());
    }
    return pick;
}

void FormationEditor::clearPick()
{
    Pick none;
    none.line = m_pick.line;
    setPick(none);
}

void FormationEditor::syncPicker()
{
    m_loading = true;
    m_lineCombo->clear();
    for (const StageTarget *t : lineTargets(m_doc))
        m_lineCombo->addItem(t->name(), QVariant(t->id()));
    const int li = m_lineCombo->findData(QVariant(m_pick.line));
    m_lineCombo->setCurrentIndex(li);
    m_lineCombo->setEnabled(m_lineCombo->count() > 0);

    m_pickList->clear();
    QList<FixtureGroup *> groups = m_doc->fixtureGroups();
    std::sort(groups.begin(), groups.end(), [](const FixtureGroup *a, const FixtureGroup *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (const FixtureGroup *g : groups)
    {
        QListWidgetItem *it = new QListWidgetItem(tr("Group: %1").arg(g->name()), m_pickList);
        it->setData(Qt::UserRole, QStringLiteral("g"));
        it->setData(Qt::UserRole + 1, QVariant(g->id()));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(m_pick.groups.contains(g->id()) ? Qt::Checked : Qt::Unchecked);
    }
    QList<Fixture *> fixtures = m_doc->fixtures();
    std::sort(fixtures.begin(), fixtures.end(), [](const Fixture *a, const Fixture *b) { return a->id() < b->id(); });
    for (const Fixture *f : fixtures)
    {
        QListWidgetItem *it = new QListWidgetItem(f->name(), m_pickList);
        it->setData(Qt::UserRole, QStringLiteral("f"));
        it->setData(Qt::UserRole + 1, QVariant(f->id()));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(m_pick.fixtures.contains(f->id()) ? Qt::Checked : Qt::Unchecked);
    }
    m_loading = false;
}

void FormationEditor::slotPickChanged()
{
    if (m_loading)
        return;
    Pick pick;
    pick.line = m_lineCombo->currentIndex() < 0 ? QLCPalette::invalidId()
                                                : m_lineCombo->currentData().toUInt();
    for (int i = 0; i < m_pickList->count(); ++i)
    {
        const QListWidgetItem *it = m_pickList->item(i);
        if (it->checkState() != Qt::Checked)
            continue;
        const quint32 id = it->data(Qt::UserRole + 1).toUInt();
        if (it->data(Qt::UserRole).toString() == QLatin1String("g"))
            pick.groups << id;
        else
            pick.fixtures << id;
    }
    m_pick = pick;
    m_picks.insert(m_paletteId, m_pick);
    updatePreview();
}

void FormationEditor::setPick(const Pick &pick)
{
    m_pick = pick;
    if (m_paletteId != QLCPalette::invalidId())
        m_picks.insert(m_paletteId, m_pick);
    syncPicker();
    updatePreview();
}

void FormationEditor::updatePreview()
{
    const QLCPalette *f = m_doc->palette(m_paletteId);
    const MonitorProperties *mp = m_doc->monitorProperties();
    const StageTarget *t = mp == nullptr ? nullptr : mp->stageTarget(m_pick.line);
    if (f == nullptr || t == nullptr || !t->isLine())
    {
        m_hint->setText(lineTargets(m_doc).isEmpty()
                        ? tr("The show has no line target: draw one in the Lighting Studio to see a formation on it.")
                        : tr("Pick a line to see it on."));
        m_hint->show();
        m_strip->setLook(nullptr, nullptr);
        m_plan->setFormation(nullptr, nullptr);
        if (m_planWindow != nullptr)
            m_planWindow->setNoAim();
        return;
    }
    // The lights, as one formation: the picked groups' and fixtures'.
    Scene scene(m_doc);
    QList<quint32> lights;
    for (quint32 gid : m_pick.groups)
        if (const FixtureGroup *g = m_doc->fixtureGroup(gid))
            for (quint32 fid : g->fixtureList())
                if (!lights.contains(fid))
                    lights << fid;
    for (quint32 fid : m_pick.fixtures)
        if (m_doc->fixture(fid) != nullptr && !lights.contains(fid))
            lights << fid;
    for (quint32 fid : lights)
        scene.addFixture(fid);

    // The Aim a line aim there reuses, with this Formation over it: the
    // same settings a layer playing both gets.
    QLCPalette aim(QLCPalette::Aim);
    aim.setStageTargetId(t->id());
    QMap<QString, double> params;
    if (ShowTreeController *st = m_doc->showTree())
        if (const QLCPalette *base = m_doc->palette(st->namedAimPalette(t->id())))
            params = base->effectParamValues();
    aim.setEffectParamValues(ShowTreeController::lineParamsWith(params, ShowTreeController::formationLooks(f)));

    if (lights.isEmpty())
    {
        m_hint->setText(tr("Tick groups or fixtures to see the formation on them (six stand in until you do)."));
        m_hint->show();
        m_strip->setLook(nullptr, &aim);
        m_plan->setFormation(nullptr, nullptr);
        if (m_planWindow != nullptr)
            m_planWindow->setNoAim();
        return;
    }
    if (!m_pick.note.isEmpty())
    {
        // The editor chose these lights: say which, why, and how to see none.
        m_hint->setText(m_pick.note.toHtmlEscaped()
                        + QStringLiteral(" <a href=\"none\">%1</a>").arg(tr("Show none")));
        m_hint->setToolTip(tr("You haven't picked lights for this preview: these are the ones the show "
                              "most often aims at this line. Tick others, or Show none."));
        m_hint->show();
    }
    else
        m_hint->hide();
    m_strip->setLook(&scene, &aim);
    m_plan->setFormation(&scene, &aim);
    if (m_planWindow != nullptr)
        m_planWindow->setFormation(&scene, &aim, nullptr, nullptr);
}
