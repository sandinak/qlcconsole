/*
  Q Light Controller Plus
  outputrulesdialog.cpp

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

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>

#include "outputrulesdialog.h"
#include "outputrulekeeper.h"
#include "fixturegroup.h"
#include "monitor/monitor.h"
#include "doc.h"

OutputRulesDialog::OutputRulesDialog(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("outputRulesDialog"));
    foreach (quint32 id, fixtureIds)
        if (m_doc->fixture(id) != nullptr && m_fixtures.contains(id) == false)
            m_fixtures << id;

    setWindowTitle(m_fixtures.size() == 1 && m_doc->fixture(m_fixtures.first()) != nullptr
                       ? tr("Output Rules: %1").arg(m_doc->fixture(m_fixtures.first())->name())
                       : tr("Output Rules: %n fixture(s)", "", int(m_fixtures.size())));

    QVBoxLayout *lay = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("For circuits that are not lights in the show: power relays, house, "
                                  "work, hall and backstage-blue lights."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    OutputRuleKeeper *k = m_doc->outputRuleKeeper();
    QString onWhen;
    switch (k != nullptr ? k->onAtOpenWhen() : OutputRuleKeeper::kDefaultOnAtOpenWhen)
    {
        case OutputRuleKeeper::OnAtOperate:
            onWhen = tr("from when you switch to Operate (the show's setting, in Settings ▸ Advanced ▸ Output)");
        break;
        case OutputRuleKeeper::OnAtShowOpen:
            onWhen = tr("from when the show opens (the show's setting, in Settings ▸ Advanced ▸ Output)");
        break;
    }
    const bool blindToo = k == nullptr || k->blindHoldsExempt();

    struct Row { Fixture::OutputRule rule; QString name; QString note; const char *object; };
    const QList<Row> rows = {
        { Fixture::ExemptBlackout, tr("Ignore Blackout"),
          blindToo ? tr("Blackout and Blind leave it as it is: it keeps whatever the show gives it.")
                   : tr("Blackout leaves it as it is: it keeps whatever the show gives it."),
          "outputRuleBlackout" },
        { Fixture::ExemptGrandMaster, tr("Ignore the Grand Master"),
          tr("The Grand Master does not dim it."), "outputRuleGrandMaster" },
        { Fixture::Switch, tr("Switch (relay)"),
          tr("On at 50% and over, off below -- never in between, never faded. The Grand Master "
             "cannot switch it off."), "outputRuleSwitch" },
        { Fixture::OnAtOpen, tr("On when not in use"),
          tr("At full whenever nothing in the show sets it -- %1. Anything that sets it, even "
             "to zero, wins.").arg(onWhen), "outputRuleOnAtOpen" },
    };

    foreach (const Row &r, rows)
    {
        int have = 0;
        foreach (quint32 id, m_fixtures)
            if (m_doc->fixture(id)->outputRules() & r.rule)
                ++have;
        QCheckBox *cb = new QCheckBox(r.name, this);
        cb->setObjectName(QString::fromLatin1(r.object));
        cb->setToolTip(r.note);
        if (have > 0 && have < m_fixtures.size())
        {
            // Mixed: left as each has it unless clicked.
            cb->setTristate(true);
            cb->setCheckState(Qt::PartiallyChecked);
        }
        else
        {
            cb->setChecked(have > 0);
        }
        // Once clicked out of mixed, plain on/off.
        connect(cb, &QCheckBox::clicked, cb, [cb]() {
            if (cb->checkState() != Qt::PartiallyChecked)
                cb->setTristate(false);
        });
        m_boxes.insert(r.rule, cb);
        lay->addWidget(cb);
        QLabel *note = new QLabel(r.note, this);
        note->setWordWrap(true);
        note->setIndent(22);
        QFont f = note->font();
        f.setPointSizeF(f.pointSizeF() * 0.9);
        note->setFont(f);
        note->setEnabled(false);    // dimmed, as a hint
        lay->addWidget(note);
    }

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

Fixture::OutputRules OutputRulesDialog::rulesFor(quint32 fixtureId) const
{
    Fixture *f = m_doc->fixture(fixtureId);
    if (f == nullptr)
        return Fixture::OutputRules();
    Fixture::OutputRules rules = f->outputRules();
    for (auto it = m_boxes.constBegin(); it != m_boxes.constEnd(); ++it)
    {
        if (it.value()->checkState() == Qt::Checked)
            rules |= it.key();
        else if (it.value()->checkState() == Qt::Unchecked)
            rules &= ~Fixture::OutputRules(it.key());
    }
    return rules;
}

bool OutputRulesDialog::apply()
{
    bool changed = false;
    foreach (quint32 id, m_fixtures)
    {
        Fixture *f = m_doc->fixture(id);
        const Fixture::OutputRules rules = rulesFor(id);
        if (f == nullptr || f->outputRules() == rules)
            continue;
        f->setOutputRules(rules);   // changed() -> the output follows
        changed = true;
    }
    if (changed)
        m_doc->setModified();
    return changed;
}

QList<quint32> OutputRulesDialog::collect(Doc *doc, const QList<quint32> &groups, const QList<quint32> &fixtures)
{
    QList<quint32> out;
    foreach (quint32 gid, groups)
    {
        FixtureGroup *g = doc->fixtureGroup(gid);
        if (g == nullptr)
            continue;
        foreach (quint32 fid, g->fixtureList())
        {
            if (out.contains(fid) == false && doc->fixture(fid) != nullptr)
                out << fid;
        }
    }
    foreach (quint32 fid, fixtures)
        if (out.contains(fid) == false && doc->fixture(fid) != nullptr)
            out << fid;
    return out;
}

bool OutputRulesDialog::edit(QWidget *parent, Doc *doc, const QList<quint32> &groups,
                             const QList<quint32> &fixtures)
{
    const QList<quint32> ids = collect(doc, groups, fixtures);
    if (ids.isEmpty())
        return false;
    OutputRulesDialog dlg(doc, ids, parent);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    // One rig undo step (⌘Z in the Fixture Manager or the 2D view).
    Monitor *mon = Monitor::instance();
    const QByteArray before = mon != nullptr ? mon->rigSnapshot() : QByteArray();
    const bool changed = dlg.apply();
    if (changed && mon != nullptr)
        mon->pushRigUndo(before);
    return changed;
}

QString OutputRulesDialog::actionText()
{
    return tr("Output Rules…");
}
