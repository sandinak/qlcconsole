/*
  Q Light Controller Plus
  importsheet.cpp

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
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

#include "importsheet.h"
#include "virtualconsole.h"
#include "showcontrol.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcframe.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

/****************************************************************************
 * Re-point: what fires the chaser fires the list
 ****************************************************************************/

int ImportSheet::Repoint::count() const
{
    return int(buttons.count() + cueLists.count() + sliders.count()) + (mainCueList ? 1 : 0)
           + (cueListNumber.isEmpty() ? 0 : 1);
}

QStringList ImportSheet::Repoint::lines() const
{
    QStringList out;
    for (const QPointer<VCButton> &b : buttons)
        if (b)
            out << tr("VC button “%1”").arg(b->caption());
    for (const QPointer<VCCueList> &c : cueLists)
        if (c)
            out << tr("VC Cue List “%1”").arg(c->caption());
    for (const QPointer<VCSlider> &s : sliders)
        if (s)
            out << tr("VC slider “%1”").arg(s->caption());
    if (mainCueList)
        out << tr("the main cue list (GO with no list, OSC and MSC)");
    if (!cueListNumber.isEmpty())
        out << tr("cue-list number %1 (OSC and MSC commands to list %1)").arg(cueListNumber);
    return out;
}

void ImportSheet::Repoint::apply(Doc *doc, bool forward) const
{
    const quint32 now = forward ? to : from;
    for (const QPointer<VCButton> &b : buttons)
        if (b)
            b->setFunction(now);
    for (const QPointer<VCCueList> &c : cueLists)
        if (c)
            c->setChaser(now);
    for (const QPointer<VCSlider> &s : sliders)
        if (s)
            s->setPlaybackFunction(now);
    if (mainCueList && doc->showControl() != nullptr)
        doc->showControl()->setMainCueList(now);
    if (!cueListNumber.isEmpty())
    {
        // The number goes with the list: one chaser answers to it.
        Chaser *was = qobject_cast<Chaser *>(doc->function(forward ? from : to));
        Chaser *is = qobject_cast<Chaser *>(doc->function(now));
        if (was != nullptr)
            was->setCueListNumber(QString());
        if (is != nullptr)
            is->setCueListNumber(cueListNumber);
    }
    doc->setModified();
}

ImportSheet::Repoint ImportSheet::planRepoint(Doc *doc, quint32 from, quint32 to)
{
    Repoint r;
    r.from = from;
    r.to = to;
    if (doc == nullptr || doc->function(from) == nullptr || doc->function(to) == nullptr || from == to)
        return r;
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc != nullptr && vc->contents() != nullptr)
    {
        for (VCButton *b : vc->contents()->findChildren<VCButton *>())
            if (b->function() == from)
                r.buttons << b;
        for (VCCueList *c : vc->contents()->findChildren<VCCueList *>())
            if (c->chaserID() == from)
                r.cueLists << c;
        for (VCSlider *s : vc->contents()->findChildren<VCSlider *>())
            if (s->playbackFunction() == from)
                r.sliders << s;
    }
    if (doc->showControl() != nullptr && doc->showControl()->mainCueList() == from)
        r.mainCueList = true;
    if (const Chaser *c = qobject_cast<const Chaser *>(doc->function(from)))
        r.cueListNumber = c->cueListNumber();
    return r;
}

/****************************************************************************
 * The sheet
 ****************************************************************************/

ImportSheet::ImportSheet(Doc *doc, const ShowTreeController::ImportReport &report, quint32 chaserId,
                         QWidget *parent)
    : QDialog(parent)
{
    const Function *chaser = doc->function(chaserId);
    const QString name = chaser == nullptr ? tr("(chaser)") : chaser->name();
    setObjectName(QStringLiteral("stImportSheet"));
    setWindowTitle(tr("Imported “%1”").arg(name));
    setProperty("helpTopic", QStringLiteral("show-tree.md#from-a-chaser-you-already-have"));
    resize(640, 600);

    QVBoxLayout *root = new QVBoxLayout(this);
    m_body = new QTextBrowser(this);
    m_body->setObjectName(QStringLiteral("stImportBody"));
    m_body->setOpenLinks(false);
    m_body->setHtml(html(doc, report, name));
    root->addWidget(m_body, 1);

    // The offer: what fires the chaser fires the list (W0; the rest of
    // Import in place is W5).
    const CueList *list = doc->showTree()->cueList(report.list);
    const Repoint r = planRepoint(doc, chaserId, list == nullptr ? Function::invalidId() : list->chaser);
    m_repoint = new QCheckBox(this);
    m_repoint->setObjectName(QStringLiteral("stImportRepoint"));
    if (r.count() > 0)
    {
        m_repoint->setText(tr("Fire the cue list from what fires “%1” now (%n place(s))", "", r.count()).arg(name));
        m_repoint->setToolTip(r.lines().join(QStringLiteral("\n"))
                              + QStringLiteral("\n\n")
                              + tr("Part of the import: Undo puts them back on the chaser."));
        m_repoint->setChecked(true);
        root->addWidget(m_repoint);
        QLabel *where = new QLabel(QStringLiteral("    ") + r.lines().join(QStringLiteral(" · ")), this);
        where->setObjectName(QStringLiteral("stImportRepointWhere"));
        where->setWordWrap(true);
        QFont f = where->font();
        f.setPointSizeF(f.pointSizeF() * 0.9);
        where->setFont(f);
        root->addWidget(where);
    }
    else
    {
        m_repoint->setChecked(false);
        m_repoint->hide();
        QLabel *none = new QLabel(tr("Nothing on the Virtual Console or show control fires “%1”.").arg(name), this);
        none->setObjectName(QStringLiteral("stImportRepointNone"));
        root->addWidget(none);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    QPushButton *undo = buttons->addButton(tr("Undo import"), QDialogButtonBox::DestructiveRole);
    undo->setObjectName(QStringLiteral("stImportUndo"));
    connect(undo, &QPushButton::clicked, this, [this]() {
        m_undo = true;
        reject();
    });
    QPushButton *done = buttons->addButton(tr("Done"), QDialogButtonBox::AcceptRole);
    done->setObjectName(QStringLiteral("stImportDone"));
    done->setDefault(true);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(buttons);
}

bool ImportSheet::repoint() const
{
    // Not offered (nothing fires the chaser): never ticked.
    return !m_undo && m_repoint->isChecked();
}

QString ImportSheet::text() const
{
    QString out = m_body->toPlainText();
    if (!m_repoint->isHidden())
        out += QStringLiteral("\n[%1] %2").arg(m_repoint->isChecked() ? QStringLiteral("x") : QStringLiteral(" "),
                                               m_repoint->text());
    return out;
}

QString ImportSheet::html(Doc *doc, const ShowTreeController::ImportReport &rep, const QString &chaser) const
{
    ShowTreeController *st = doc->showTree();
    const CueList *list = st->cueList(rep.list);
    auto esc = [](const QString &s) { return s.toHtmlEscaped(); };
    QString h;
    h += QStringLiteral("<h2>%1</h2>").arg(esc(tr("Imported “%1”").arg(chaser)));
    if (list == nullptr)
    {
        h += QStringLiteral("<p>%1</p>").arg(esc(tr("Nothing was imported.")));
        for (const QString &n : rep.notes)
            h += QStringLiteral("<p>%1</p>").arg(esc(n));
        return h;
    }
    const Production *prod = st->production(rep.production);
    h += QStringLiteral("<p>%1</p>").arg(esc(
        tr("A new production, %1, with the cue list %2: %n cue(s), Non-tracking, so each cue plays what its "
           "step played. The chaser itself is unchanged.", "", rep.cues)
        .arg(prod == nullptr ? chaser : prod->name, list->name)));

    // Plays the same?
    h += QStringLiteral("<h3>%1</h3>").arg(esc(tr("Plays the same")));
    if (rep.differChannels == 0)
        h += QStringLiteral("<p>✓ %1</p>").arg(esc(
            tr("0 channels differ: every cue puts out what its step did (%n cue(s) checked).", "",
               rep.checkedCues)));
    else
    {
        h += QStringLiteral("<p>⚠ %1</p><ul>").arg(esc(
            tr("%n channel(s) differ from the chaser:", "", rep.differChannels)));
        for (const QString &l : rep.differCues)
            h += QStringLiteral("<li>%1</li>").arg(esc(l));
        h += QStringLiteral("</ul>");
    }
    h += QStringLiteral("<p><small>%1</small></p>").arg(esc(
        tr("Checked as the cues stand, not as they fade: each cue's channels against its step's. "
           "Effects (EFX, RGB Matrix, effect palettes) play as they are on both sides.")));

    // What each step became.
    h += QStringLiteral("<h3>%1</h3>").arg(esc(tr("What each step became")));
    h += QStringLiteral("<p>%1</p>").arg(esc(
        tr("%1 layer(s) from scenes of groups and palettes, %2 played as they are, %3 include(s); "
           "%4 scene(s) made for the layers.")
        .arg(rep.layers).arg(rep.opaqueLayers).arg(rep.includes).arg(rep.generatedScenes)));
    h += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"3\" border=\"0\" width=\"100%\">"
                        "<tr><th align=\"left\">%1</th><th align=\"left\">%2</th><th align=\"left\">%3</th></tr>")
         .arg(esc(tr("Cue")), esc(tr("Layers")), esc(tr("Plays as it is / includes")));
    for (int i = 0; i < list->cues.count(); ++i)
    {
        const Cue &c = list->cues.at(i);
        QStringList layers, opaque;
        for (const Layer &l : c.layers)
        {
            if (l.target.isOpaque())
                opaque << QStringLiteral("▣ ") + st->targetLabel(l.target);
            else
                layers << (l.name.isEmpty() ? st->targetLabel(l.target) : l.name);
        }
        for (quint32 inc : c.includes)
            opaque << tr("includes %1").arg(st->cueLabel(*list, list->indexOf(inc)));
        QString label = st->cueLabel(*list, i);
        if (c.mixOverlap)
            label += QStringLiteral(" ≈");
        if (c.skip)
            label += QStringLiteral(" ") + tr("(skipped)");
        h += QStringLiteral("<tr><td valign=\"top\">%1</td><td valign=\"top\">%2</td><td valign=\"top\">%3</td></tr>")
             .arg(esc(label), esc(layers.join(QStringLiteral(", "))), esc(opaque.join(QStringLiteral(", "))));
    }
    h += QStringLiteral("</table>");
    if (rep.opaqueLayers + rep.includes > 0 || !rep.mixedIncludes.isEmpty())
        h += QStringLiteral("<p><small>%1</small></p>").arg(esc(
            tr("▣ plays a function as it is: select it in the tree and Open (or double-click it) to edit it in "
               "its own editor. ≈ the cue mixes overlapping layers, as the chaser did.")));

    // Played as they are, and why: once each, with how often.
    if (!rep.opaqueReasons.isEmpty())
    {
        h += QStringLiteral("<h3>%1</h3><ul>").arg(esc(tr("Played as they are, and why")));
        QStringList order;
        QHash<QString, int> times;
        for (const QString &r : rep.opaqueReasons)
        {
            if (!times.contains(r))
                order << r;
            times[r]++;
        }
        for (const QString &r : order)
            h += QStringLiteral("<li>%1</li>").arg(esc(times.value(r) > 1 ? tr("%1 (×%2)").arg(r).arg(times.value(r))
                                                                         : r));
        h += QStringLiteral("</ul>");
    }

    if (!rep.htpConflicts.isEmpty())
    {
        h += QStringLiteral("<h3>%1</h3><ul>").arg(esc(tr("Two scenes on one group")));
        for (const QString &s : rep.htpConflicts)
            h += QStringLiteral("<li>%1</li>").arg(esc(s));
        h += QStringLiteral("</ul>");
    }
    if (!rep.notes.isEmpty())
    {
        h += QStringLiteral("<h3>%1</h3><ul>").arg(esc(tr("Mixes and notes")));
        for (const QString &s : rep.notes)
            h += QStringLiteral("<li>%1</li>").arg(esc(s));
        h += QStringLiteral("</ul>");
    }
    return h;
}
