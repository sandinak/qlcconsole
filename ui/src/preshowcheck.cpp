/*
  Q Light Controller Plus - qlcconsole
  preshowcheck.cpp

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
#include <QApplication>
#include <QScrollArea>
#include <QSpacerItem>
#include <QGridLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QLocale>
#include <QLabel>
#include <QTimer>

#include "preshowcheck.h"
#include "showstatusdialog.h"
#include "showreviewpanel.h"
#include "inputoutputmap.h"
#include "helpbrowser.h"
#include "showstatus.h"
#include "showreview.h"
#include "outputpatch.h"
#include "universe.h"
#include "fixture.h"
#include "doc.h"

PreshowCheck::Hooks PreshowCheck::s_hooks;
QPointer<PreshowCheck> PreshowCheck::s_shared;
Doc *PreshowCheck::s_doc = nullptr;
QPointer<QWidget> PreshowCheck::s_parent;

namespace
{
const char *const kHelp = "dress-rehearsal.md";

/* The footer's design entries are Show Review's too: the review line has
   them. The pads line has "input.devices", the computer lines Settings ▸
   Performance's own entry. */
bool coveredElsewhere(const QString &key)
{
    return PreshowCheck::isOutputKey(key) || key == QLatin1String("input.devices")
           || key == SystemTuning::statusKey() || !ShowReview::footerCheckId(key).isEmpty();
}

const SystemTuning::Item *itemOf(const QList<SystemTuning::Item> &items, const QString &key)
{
    for (const SystemTuning::Item &it : items)
        if (it.key == key)
            return &it;
    return nullptr;
}

/* The fix a ShowStatus entry offers, on its first item (or @a fallback). */
std::function<void(QWidget *)> statusFix(const QString &key, const QVariantList &data)
{
    return [key, data](QWidget *parent) { ShowStatus::instance()->runAction(key, parent, data); };
}
}

/*****************************************************************************
 * The lines
 *****************************************************************************/

void PreshowCheck::setHooks(const Hooks &hooks)
{
    s_hooks = hooks;
}

const PreshowCheck::Hooks &PreshowCheck::hooks()
{
    return s_hooks;
}

bool PreshowCheck::isOutputKey(const QString &key)
{
    return key == QLatin1String("output.dangling") || key == QLatin1String("outputs.problems")
           || key == QLatin1String("universes.nooutput") || key == QLatin1String("network.route")
           || key == QLatin1String("artnet.ports") || key.startsWith(QLatin1String("wled."));
}

QString PreshowCheck::stateWord(State state)
{
    switch (state)
    {
        case Ready: return tr("Ready");
        case Check: return tr("Check");
        case NotReady: return tr("Not ready");
        case ByHand: return tr("By hand");
        default: return tr("Not used");
    }
}

QString PreshowCheck::stateSymbol(State state)
{
    switch (state)
    {
        case Ready: return QStringLiteral("✓");
        case Check: return QStringLiteral("⚠");
        case NotReady: return QStringLiteral("✕");
        case ByHand: return QStringLiteral("○");
        default: return QStringLiteral("–");
    }
}

QList<PreshowCheck::Line> PreshowCheck::gather(Doc *doc, const Hooks &hooks, const SystemTuning::Probe &probe)
{
    QList<Line> lines;
    if (doc == nullptr)
        return lines;
    ShowStatus *status = ShowStatus::instance();
    const QList<ShowStatus::Entry> entries = status->allEntries();
    InputOutputMap *io = doc->inputOutputMap();

    /* Every universe reaching its node: the footer's output entries */
    {
        Line l;
        l.key = QStringLiteral("outputs");
        l.title = tr("Every universe reaches its node");
        QList<quint32> used;
        for (Fixture *fx : doc->fixtures())
            if (fx != nullptr && fx->isPatched() && !used.contains(fx->universe()))
                used << fx->universe();
        int sending = 0;
        for (Universe *u : io->universes())
            if (u != nullptr && u->outputPatchesCount() > 0)
                sending++;
        QStringList summaries;
        for (const ShowStatus::Entry &e : entries)
        {
            if (!isOutputKey(e.key))
                continue;
            summaries << e.summary;
            l.items << e.items;
            if (!l.fix && !status->actionLabel(e.key).isEmpty())
            {
                QVariantList data = e.itemData.mid(0, 1);
                if (data.isEmpty() && !used.isEmpty())
                    data << used.first();
                l.fixLabel = status->actionLabel(e.key);
                l.fix = statusFix(e.key, data);
            }
        }
        if (!summaries.isEmpty())
        {
            l.state = NotReady;
            l.text = summaries.join(QStringLiteral("; ")) + QLatin1Char('.');
        }
        else if (used.isEmpty())
        {
            l.state = NotUsed;
            l.text = tr("No fixture is patched yet.");
        }
        else if (sending == 0)
        {
            l.state = NotReady;
            l.text = tr("No universe has an output: nothing reaches the rig.");
            l.fixLabel = status->actionLabel(QStringLiteral("connections.universe"));
            if (!l.fixLabel.isEmpty())
                l.fix = statusFix(QStringLiteral("connections.universe"), QVariantList() << used.first());
        }
        else
        {
            l.state = Ready;
            l.text = (sending == 1 ? tr("1 universe sending") : tr("%1 universes sending").arg(sending))
                     + tr(", and no node has stopped answering. Look at each node's own light too.");
        }
        lines << l;
    }

    const QList<SystemTuning::Item> items = SystemTuning::check(probe);
    auto performanceFix = [&hooks](Line *l) {
        if (!hooks.openPerformance)
            return;
        l->fixLabel = tr("Settings ▸ Performance…");
        l->fix = hooks.openPerformance;
    };

    /* macOS can refuse the app the network, silently */
    {
        Line l;
        l.key = QStringLiteral("localnetwork");
        l.title = tr("Local Network allowed");
        const SystemTuning::Item *it = itemOf(items, QStringLiteral("localnetwork"));
        if (it == nullptr)
        {
            l.state = NotUsed;
            l.text = probe.os == SystemTuning::OsMac ? tr("No Art-Net or sACN output to refuse.")
                                                     : tr("Only macOS asks for this.");
        }
        else if (it->status == SystemTuning::CanImprove)
        {
            l.state = NotReady;
            l.text = tr("Every network output is failing: macOS has most likely refused the app "
                        "the network.") + QLatin1Char(' ') + it->howTo;
            performanceFix(&l);
        }
        else
        {
            l.state = Ready;
            l.text = tr("Allowed: the network outputs are sending.");
        }
        lines << l;
    }

    /* The controllers the show is patched to */
    {
        Line l;
        l.key = QStringLiteral("pads");
        l.title = tr("Controllers connected");
        const QList<InputDeviceNotices::Device> pads = hooks.pads ? hooks.pads()
                                                                  : QList<InputDeviceNotices::Device>();
        QStringList here, away;
        quint32 awayUniverse = 0;
        for (const InputDeviceNotices::Device &d : pads)
        {
            const QString name = d.slotCount > 1 && !d.connected
                               ? tr("one of %1").arg(d.slotDevices.join(QStringLiteral(", ")))
                               : d.name;
            const QString row = tr("%1 (universe %2)").arg(name).arg(d.universe + 1);
            if (d.connected)
            {
                here << row;
            }
            else
            {
                if (away.isEmpty())
                    awayUniverse = d.universe;
                away << row;
            }
        }
        if (pads.isEmpty())
        {
            l.state = NotUsed;
            l.text = tr("No pad or MIDI surface is patched in this show.");
        }
        else if (!away.isEmpty())
        {
            l.state = NotReady;
            l.text = (away.count() == 1 ? tr("1 not connected") : tr("%1 not connected").arg(away.count()))
                     + tr(": turn it on, charge it or plug it in.");
            l.items = away;
            const QString key = QStringLiteral("connections.universe");
            if (!status->actionLabel(key).isEmpty())
            {
                l.fixLabel = status->actionLabel(key);
                l.fix = statusFix(key, QVariantList() << awayUniverse);
            }
        }
        else
        {
            l.state = Ready;
            l.text = here.count() == 1 ? tr("1 connected.") : tr("%1 connected.").arg(here.count());
            l.items = here;
        }
        lines << l;
    }

    /* Show Review: the show's design */
    {
        Line l;
        l.key = QStringLiteral("review");
        l.title = tr("Show Review");
        const ShowReview::Result &r = doc->showReview()->lastResult();
        const int blockers = r.open[ReviewFinding::Blocker];
        const int warnings = r.open[ReviewFinding::Warning];
        QString first;
        for (const ReviewFinding &f : r.findings)
        {
            if (!f.isOpen() || f.severity != ReviewFinding::Blocker)
                continue;
            if (first.isEmpty())
                first = f.key();
            if (l.items.count() < 4)
                l.items << QStringLiteral("%1: %2").arg(f.check, f.message);
        }
        if (!r.ran)
        {
            l.state = Check;
            l.text = tr("Not checked yet.");
        }
        else if (blockers > 0)
        {
            l.state = NotReady;
            l.text = (blockers == 1 ? tr("1 blocker open.") : tr("%1 blockers open.").arg(blockers))
                     + (warnings == 1 ? QLatin1Char(' ') + tr("1 warning.")
                        : warnings > 1 ? QLatin1Char(' ') + tr("%1 warnings.").arg(warnings) : QString());
        }
        else if (warnings > 0)
        {
            l.state = Check;
            l.text = warnings == 1 ? tr("No blockers; 1 warning open: look it over.")
                             : tr("No blockers; %1 warnings open: look them over.").arg(warnings);
        }
        else
        {
            l.state = Ready;
            l.text = tr("No blockers or warnings open.");
        }
        if (l.state != Ready)
        {
            l.fixLabel = tr("Open Show Review");
            l.fix = [doc, first](QWidget *parent) {
                ShowReviewPanel *panel = ShowReviewPanel::canOpenShared() ? ShowReviewPanel::openShared()
                                                                         : nullptr;
                if (panel == nullptr)
                {
                    panel = new ShowReviewPanel(doc, parent);
                    panel->setAttribute(Qt::WA_DeleteOnClose);
                    panel->show();
                }
                if (!first.isEmpty())
                    panel->selectFinding(first);
            };
        }
        lines << l;
    }

    /* The readiness footer: everything it says that no line above covers */
    {
        Line l;
        l.key = QStringLiteral("readiness");
        l.title = tr("The footer's readiness list");
        bool error = false;
        for (const ShowStatus::Entry &e : entries)
        {
            if (coveredElsewhere(e.key) || e.severity == ShowStatus::Ok)
                continue;
            error = error || e.severity == ShowStatus::Error;
            l.items << e.summary;
        }
        if (l.items.isEmpty())
        {
            l.state = Ready;
            l.text = tr("Nothing else is listed as not ready.");
        }
        else
        {
            l.state = error ? NotReady : Check;
            l.text = l.items.count() == 1 ? tr("1 more thing listed as not ready.")
                                         : tr("%1 more things listed as not ready.").arg(l.items.count());
            l.fixLabel = tr("Show Details…");
            l.fix = [](QWidget *parent) {
                ShowStatusDialog *dlg = new ShowStatusDialog(parent);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            };
        }
        lines << l;
    }

    /* Mains */
    {
        Line l;
        l.key = QStringLiteral("power");
        l.title = tr("Mains power");
        const SystemTuning::Item *it = itemOf(items, QStringLiteral("power"));
        if (it == nullptr || it->status == SystemTuning::NotAvailable)
        {
            l.state = NotUsed;
            l.text = tr("This computer doesn't say. Check the charger is in.");
        }
        else if (it->status == SystemTuning::CanImprove)
        {
            const bool low = probe.batteryPercent >= 0 && probe.batteryPercent < 25;
            l.state = low ? NotReady : Check;
            l.text = probe.batteryPercent >= 0
                   ? tr("On battery, %1%: plug it in. The show stops when the battery does.").arg(probe.batteryPercent)
                   : tr("On battery: plug it in. The show stops when the battery does.");
        }
        else
        {
            l.state = Ready;
            l.text = tr("Plugged in.");
        }
        lines << l;
    }

    /* The screen */
    {
        Line l;
        l.key = QStringLiteral("screen");
        l.title = tr("Screen stays awake");
        const SystemTuning::Item *it = itemOf(items, QStringLiteral("screenlock"));
        if (it == nullptr || it->status == SystemTuning::NotAvailable)
        {
            l.state = NotUsed;
            l.text = tr("This computer doesn't say.");
        }
        else if (it->status == SystemTuning::Good)
        {
            l.state = Ready;
            l.text = probe.keepDisplayAwake && probe.fullSpeedInOperate
                   ? tr("Held awake during Operate (Settings ▸ Performance).")
                   : it->state.left(1).toUpper() + it->state.mid(1) + QLatin1Char('.');
        }
        else
        {
            l.state = Check;
            l.text = (it->state.isEmpty() ? QString() : it->state.left(1).toUpper() + it->state.mid(1) + QStringLiteral(". "))
                     + it->howTo;
            performanceFix(&l);
        }
        lines << l;
    }

    /* Wi-Fi */
    {
        Line l;
        l.key = QStringLiteral("wifi");
        l.title = tr("DMX off Wi-Fi");
        const SystemTuning::Item *it = itemOf(items, QStringLiteral("wifi"));
        if (it == nullptr)
        {
            l.state = NotUsed;
            l.text = tr("This computer doesn't say.");
        }
        else if (it->status == SystemTuning::CanImprove && it->important)
        {
            l.state = Check;
            l.text = tr("Art-Net or sACN goes out over Wi-Fi: it arrives in bursts. Send it wired.");
            performanceFix(&l);
        }
        else
        {
            l.state = Ready;
            l.text = it->status == SystemTuning::Good ? tr("No Wi-Fi in use.")
                                                      : tr("Wi-Fi is up; no Art-Net or sACN output uses it.");
        }
        lines << l;
    }

    /* The rest of Settings ▸ Performance that matters during Operate */
    {
        Line l;
        l.key = QStringLiteral("computer");
        l.title = tr("This computer, for the show");
        static const QStringList shown { QStringLiteral("localnetwork"), QStringLiteral("power"),
                                         QStringLiteral("screenlock"), QStringLiteral("wifi") };
        for (const SystemTuning::Item &it : SystemTuning::importantProblems(items))
            if (!shown.contains(it.key))
                l.items << QStringLiteral("%1: %2").arg(it.title, it.state);
        if (l.items.isEmpty())
        {
            l.state = Ready;
            l.text = tr("Nothing important in Settings ▸ Performance is off.");
        }
        else
        {
            l.state = Check;
            l.text = (l.items.count() == 1 ? tr("1 thing") : tr("%1 things").arg(l.items.count()))
                     + tr(" in Settings ▸ Performance can make DMX frames late.");
            performanceFix(&l);
        }
        lines << l;
    }

    /* Autosave */
    {
        Line l;
        l.key = QStringLiteral("autosave");
        l.title = tr("Autosave on");
        const QString failed = hooks.autosaveFailure ? hooks.autosaveFailure() : QString();
        if (!hooks.autosaveOn)
        {
            l.state = NotUsed;
            l.text = tr("Not known here.");
        }
        else if (!failed.isEmpty())
        {
            l.state = NotReady;
            l.text = tr("The last autosave failed: %1. Save the show (⌘S).").arg(failed);
        }
        else if (!hooks.autosaveOn())
        {
            l.state = Check;
            l.text = tr("Autosave is off: a crash loses everything since the last save.");
            if (hooks.turnAutosaveOn)
            {
                l.fixLabel = tr("Turn It On");
                l.fix = [hooks](QWidget *) { hooks.turnAutosaveOn(); };
            }
        }
        else
        {
            l.state = Ready;
            const int minutes = hooks.autosaveMinutes ? hooks.autosaveMinutes() : 0;
            l.text = minutes > 0 ? minutes == 1 ? tr("A recovery copy every minute.")
                                 : tr("A recovery copy every %1 minutes.").arg(minutes) : tr("On.");
        }
        lines << l;
    }

    /* The channel check: a person in the room */
    {
        Line l;
        l.key = QStringLiteral("walk");
        l.title = tr("Channel check");
        l.state = ByHand;
        l.text = tr("Walk every light from the pad: Quick Focus, the other heads dark, lights one at a "
                    "time and RB steps to the next. Watch each one come up.");
        if (hooks.quickFocus)
        {
            l.fixLabel = tr("Quick Focus…");
            l.fix = hooks.quickFocus;
            if (doc->mode() == Doc::Operate)
                l.text += QLatin1Char(' ') + tr("It runs in Design: do it before going to Operate.");
        }
        lines << l;
    }
    return lines;
}

/*****************************************************************************
 * The page
 *****************************************************************************/

PreshowCheck::PreshowCheck(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("preshowCheck"));
    setWindowTitle(tr("Before the House Opens"));
    setProperty("helpTopic", QString::fromLatin1(kHelp));

    QVBoxLayout *lay = new QVBoxLayout(this);
    QHBoxLayout *top = new QHBoxLayout;
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("preshowSummary"));
    QFont big = m_summary->font();
    big.setBold(true);
    big.setPointSizeF(big.pointSizeF() * 1.25);
    m_summary->setFont(big);
    top->addWidget(m_summary, 1);
    m_when = new QLabel(this);
    m_when->setObjectName(QStringLiteral("preshowWhen"));
    m_when->setEnabled(false);
    top->addWidget(m_when);
    m_again = new QPushButton(tr("Check Again"), this);
    m_again->setObjectName(QStringLiteral("preshowAgain"));
    m_again->setToolTip(tr("Run Show Review and look at this computer again"));
    connect(m_again, &QPushButton::clicked, this, &PreshowCheck::refresh);
    top->addWidget(m_again);
    lay->addLayout(top);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("preshowLines"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    lay->addWidget(m_scroll, 1);

    QHBoxLayout *bottom = new QHBoxLayout;
    m_checklist = new QPushButton(tr("Dress-Rehearsal Checklist"), this);
    m_checklist->setObjectName(QStringLiteral("preshowChecklist"));
    m_checklist->setToolTip(tr("What to check in the room that the app can't see: candles, lanterns, "
                               "fountains, lasers, the follow spot, power"));
    connect(m_checklist, &QPushButton::clicked, this, []() { HelpBrowser::open(QString::fromLatin1(kHelp)); });
    bottom->addWidget(m_checklist);
    bottom->addStretch(1);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    bottom->addWidget(box);
    lay->addLayout(bottom);

    /* The sources change under it: redraw a moment later, without probing */
    m_later = new QTimer(this);
    m_later->setSingleShot(true);
    m_later->setInterval(150);
    connect(m_later, &QTimer::timeout, this, &PreshowCheck::rebuild);
    connect(ShowStatus::instance(), &ShowStatus::changed, m_later, qOverload<>(&QTimer::start));
    if (m_doc != nullptr)
    {
        connect(m_doc->showReview(), &ShowReview::reviewed, m_later, qOverload<>(&QTimer::start));
        connect(m_doc, &Doc::modeChanged, m_later, qOverload<>(&QTimer::start));
    }

    resize(720, 620);
    refresh();
}

PreshowCheck::Line PreshowCheck::line(const QString &key) const
{
    for (const Line &l : m_lines)
        if (l.key == key)
            return l;
    return Line();
}

void PreshowCheck::refresh()
{
    if (m_doc == nullptr)
        return;
    if (s_hooks.probe)
    {
        m_probe = s_hooks.probe();
    }
    else
    {
        m_probe = SystemTuning::probeDoc(m_doc);
        SystemTuning::probeHost(&m_probe, m_doc->mode() != Doc::Operate);
        if (SystemTuning::instance() != nullptr)
            SystemTuning::instance()->rememberLook(&m_probe);
    }
    m_doc->showReview()->run();      // reviewed() redraws too; this is now
    m_when->setText(tr("Checked %1").arg(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat)));
    rebuild();
}

void PreshowCheck::rebuild()
{
    m_later->stop();
    m_lines = gather(m_doc, s_hooks, m_probe);
    m_stateLabels.clear();
    m_fixButtons.clear();

    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    auto colour = [dark](State s) {
        switch (s)
        {
            case Ready: return QColor(dark ? "#5cc160" : "#2e7d32");
            case Check: return QColor(dark ? "#f0b429" : "#a05a00");
            case NotReady: return QColor(dark ? "#ff6b5e" : "#c0392b");
            default: return QColor();
        }
    };

    QWidget *content = new QWidget;
    content->setObjectName(QStringLiteral("preshowContent"));
    QGridLayout *grid = new QGridLayout(content);
    // The fix sits by its line, not across a wide screen: the words stop
    // at a reading width and what is left goes after the buttons.
    grid->setColumnStretch(1, 4);
    grid->setColumnStretch(3, 1);
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 3);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(12);
    int notReady = 0, check = 0, row = 0;
    for (const Line &l : m_lines)
    {
        notReady += l.state == NotReady;
        check += l.state == Check;

        QLabel *state = new QLabel(stateSymbol(l.state) + QLatin1Char(' ') + stateWord(l.state), content);
        state->setObjectName(QStringLiteral("preshowState_") + l.key);
        QFont bold = state->font();
        bold.setBold(true);
        state->setFont(bold);
        const QColor c = colour(l.state);
        if (c.isValid())
            state->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(c.name()));
        else
            state->setEnabled(l.state == ByHand);
        state->setMinimumWidth(state->fontMetrics().horizontalAdvance(QStringLiteral("✕ Not ready")) + 8);
        grid->addWidget(state, row, 0, Qt::AlignTop);

        QString html = QStringLiteral("<b>%1</b><br>%2").arg(l.title.toHtmlEscaped(), l.text.toHtmlEscaped());
        for (const QString &item : l.items)
            html += QStringLiteral("<br>&nbsp;&nbsp;• ") + item.toHtmlEscaped();
        QLabel *text = new QLabel(html, content);
        text->setObjectName(QStringLiteral("preshowText_") + l.key);
        text->setTextFormat(Qt::RichText);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setMaximumWidth(text->fontMetrics().averageCharWidth() * 100);
        grid->addWidget(text, row, 1, Qt::AlignTop);

        if (l.fix && !l.fixLabel.isEmpty())
        {
            QPushButton *fix = new QPushButton(l.fixLabel, content);
            fix->setObjectName(QStringLiteral("preshowFix_") + l.key);
            const QString key = l.key;
            connect(fix, &QPushButton::clicked, this, [this, key]() { runFix(key); });
            grid->addWidget(fix, row, 2, Qt::AlignTop);
            m_fixButtons.insert(l.key, fix);
        }
        m_stateLabels.insert(l.key, state);
        row++;
    }
    grid->setRowStretch(row, 1);
    m_scroll->setWidget(content);       // the last one goes

    if (notReady > 0)
        m_summary->setText(stateSymbol(NotReady) + QLatin1Char(' ')
                           + tr("Not ready: %1 to fix").arg(notReady)
                           + (check > 0 ? tr(", %1 to check").arg(check) : QString()));
    else if (check > 0)
        m_summary->setText(stateSymbol(Check) + QLatin1Char(' ') + tr("Nothing stops the show; %1 to check").arg(check));
    else
        m_summary->setText(stateSymbol(Ready) + QLatin1Char(' ') + tr("Ready for the house"));
    const QColor c = colour(notReady > 0 ? NotReady : check > 0 ? Check : Ready);
    m_summary->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(c.name()));
}

void PreshowCheck::runFix(const QString &key)
{
    const Line l = line(key);
    if (!l.fix)
        return;
    l.fix(this);
    /* A fix done here (autosave on) shows a moment later -- not now: the
       button pressed is in the rows a redraw replaces. One done elsewhere
       shows as its source changes. */
    m_later->start();
}

/*****************************************************************************
 * The shared page
 *****************************************************************************/

void PreshowCheck::setShared(Doc *doc, QWidget *parent)
{
    s_doc = doc;
    s_parent = parent;
}

PreshowCheck *PreshowCheck::shared()
{
    return s_shared.data();
}

PreshowCheck *PreshowCheck::openShared()
{
    if (s_doc == nullptr)
        return nullptr;
    if (s_shared.isNull())
    {
        s_shared = new PreshowCheck(s_doc, s_parent.data());
        s_shared->setAttribute(Qt::WA_DeleteOnClose);
    }
    else
    {
        s_shared->refresh();
    }
    s_shared->show();
    s_shared->raise();
    s_shared->activateWindow();
    return s_shared.data();
}

PreshowCheck *PreshowCheck::openFor(Doc *doc, QWidget *parent)
{
    if (s_doc != nullptr)
        return openShared();
    PreshowCheck *page = new PreshowCheck(doc, parent);
    page->setAttribute(Qt::WA_DeleteOnClose);
    page->show();
    return page;
}
