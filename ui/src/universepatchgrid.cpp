/*
  Q Light Controller Plus
  universepatchgrid.cpp

  Copyright (c) Massimo Callegari

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QColor>
#include <QPalette>
#include <QFont>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>
#include <QIcon>
#include <QTimer>
#include <QSet>

#include "universepatchgrid.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "inputpatch.h"    // KInputNone
#include "qlcioplugin.h"   // QLCIOPlugin::invalidLine()
#include "ioplugincache.h"
#include "fixture.h"
#include "doc.h"

// Combo item roles: the plugin name and its line index behind each entry.
static const int R_PLUGIN = Qt::UserRole;
static const int R_LINE   = Qt::UserRole + 1;

// Column layout. Output is always shown; Input / Feedback / Passthrough are
// collapsible groups toggled from the toolbar.
enum {
    COL_UNI = 0, COL_NAME, COL_USAGE,
    COL_OUT, COL_OUT_IP, COL_OUT_UNI, COL_OUT_MODE,   // output (always)
    COL_IN, COL_IN_UNI, COL_IN_PROFILE,               // input (collapsible)
    COL_FB, COL_FB_IP, COL_FB_UNI,                    // feedback (collapsible)
    COL_PT,                                           // passthrough (collapsible)
    COL_COUNT
};

static QList<int> inputCols()    { return QList<int>() << COL_IN << COL_IN_UNI << COL_IN_PROFILE; }
static QList<int> feedbackCols() { return QList<int>() << COL_FB << COL_FB_IP << COL_FB_UNI; }
static QList<int> ptCols()       { return QList<int>() << COL_PT; }

// Plugin parameter keys (ArtNet/sACN). Feedback is an output patch, so it uses
// the output keys too; only the input universe differs.
static const char *P_OUT_IP  = "outputIP";
static const char *P_OUT_UNI = "outputUni";
static const char *P_MODE    = "transmitMode";
static const char *P_IN_UNI  = "inputUni";

// A muted per-protocol tint so rows read at a glance. Low alpha so it blends
// over the base row colour in both light and dark themes.
static QColor protocolTint(const QString &plugin, int alpha)
{
    if (plugin.contains("ArtNet", Qt::CaseInsensitive))
        return QColor(66, 133, 244, alpha);
    if (plugin.contains("E1.31", Qt::CaseInsensitive) || plugin.contains("sACN", Qt::CaseInsensitive))
        return QColor(160, 90, 210, alpha);
    if (plugin.contains("MIDI", Qt::CaseInsensitive))
        return QColor(76, 175, 80, alpha);
    if (plugin.contains("OSC", Qt::CaseInsensitive))
        return QColor(0, 172, 193, alpha);
    if (plugin.contains("DMX", Qt::CaseInsensitive) || plugin.contains("USB", Qt::CaseInsensitive)
            || plugin.contains("ENTTEC", Qt::CaseInsensitive) || plugin.contains("uDMX", Qt::CaseInsensitive))
        return QColor(245, 150, 50, alpha);
    return QColor();
}

// Direction accent colours.
static const QColor kOutAccent(0, 150, 136);    // teal  — output
static const QColor kInAccent(92, 107, 192);    // indigo — input
static const QColor kFbAccent(255, 152, 0);     // amber — feedback

static void tintCombo(QComboBox *c, const QColor &base)
{
    c->setStyleSheet(QString("QComboBox { background-color: rgba(%1,%2,%3,40); }")
                     .arg(base.red()).arg(base.green()).arg(base.blue()));
}

static QTableWidgetItem *roItem(const QString &text)
{
    QTableWidgetItem *it = new QTableWidgetItem(text);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}

UniversePatchGrid::UniversePatchGrid(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_ioMap(doc->inputOutputMap())
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);

    QToolBar *bar = new QToolBar(this);
    bar->setIconSize(QSize(20, 20));
    QAction *addAct = bar->addAction(QIcon(":/edit_add.png"), tr("Add Universe"));
    m_removeAction = bar->addAction(QIcon(":/edit_remove.png"), tr("Remove Universe"));
    bar->addSeparator();
    QAction *rescanAct = bar->addAction(QIcon(":/refresh.png"), tr("Rescan Devices"));
    rescanAct->setToolTip(tr("Re-detect connected input/output devices (USB/MIDI/network)"));
    bar->addSeparator();
    // Collapsible column groups — output is always shown.
    m_inputToggle = bar->addAction(tr("Input"));
    m_inputToggle->setCheckable(true);
    m_inputToggle->setToolTip(tr("Show the Input columns (device · universe · profile)"));
    m_feedbackToggle = bar->addAction(tr("Feedback"));
    m_feedbackToggle->setCheckable(true);
    m_feedbackToggle->setToolTip(tr("Show the Feedback columns (network feedback device · IP · universe)"));
    m_ptToggle = bar->addAction(tr("Passthrough"));
    m_ptToggle->setCheckable(true);
    m_ptToggle->setToolTip(tr("Show the Passthrough column"));
    connect(addAct, &QAction::triggered, this, &UniversePatchGrid::onAddUniverse);
    connect(m_removeAction, &QAction::triggered, this, &UniversePatchGrid::onRemoveUniverse);
    connect(rescanAct, &QAction::triggered, this, &UniversePatchGrid::onRescan);
    connect(m_inputToggle, &QAction::toggled, this, &UniversePatchGrid::toggleInputGroup);
    connect(m_feedbackToggle, &QAction::toggled, this, &UniversePatchGrid::toggleFeedbackGroup);
    connect(m_ptToggle, &QAction::toggled, this, &UniversePatchGrid::togglePassthroughGroup);
    lay->addWidget(bar);

    QLabel *hint = new QLabel(
        tr("Output-first: every universe's output and its network target are shown "
           "here — double-click a cell to edit. Use the Input / Feedback / "
           "Passthrough buttons to reveal those columns. Cells are tinted by "
           "protocol; teal = output, indigo = input, amber = feedback."),
        this);
    hint->setWordWrap(true);
    QColor dim = hint->palette().color(QPalette::PlaceholderText);
    if (!dim.isValid())
        dim = hint->palette().color(QPalette::WindowText);
    QPalette hpal = hint->palette();
    hpal.setColor(QPalette::WindowText, dim);
    hint->setPalette(hpal);
    lay->addWidget(hint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels(QStringList()
        << tr("Uni") << tr("Name") << tr("Usage")
        << tr("Output") << tr("Out IP") << tr("Out U") << tr("Mode")
        << tr("Input") << tr("In U") << tr("Profile")
        << tr("Feedback") << tr("FB IP") << tr("FB U")
        << tr("PT"));

    const char *tips[COL_COUNT] = {
        QT_TR_NOOP("Internal universe number (1-based)"),
        QT_TR_NOOP("Universe name"),
        QT_TR_NOOP("Usage — patched fixtures and the highest DMX channel used (of 512)"),
        QT_TR_NOOP("Output device — plugin and line (NIC) this universe sends to. MIDI adds an Out/FB role toggle"),
        QT_TR_NOOP("Output target IP / host (ArtNet / sACN)"),
        QT_TR_NOOP("Output universe sent to the node (ArtNet / sACN)"),
        QT_TR_NOOP("ArtNet transmission mode (Standard / Full / Partial)"),
        QT_TR_NOOP("Input device — plugin and line this universe listens to"),
        QT_TR_NOOP("Input universe listened on (ArtNet) — independent of the output universe"),
        QT_TR_NOOP("Input profile applied to the input device"),
        QT_TR_NOOP("Feedback device (network) — sends state back; MIDI feedback uses the Output role toggle"),
        QT_TR_NOOP("Feedback target IP / host (network)"),
        QT_TR_NOOP("Feedback universe (network)"),
        QT_TR_NOOP("Passthrough — forward incoming input straight to the output, unprocessed"),
    };
    for (int c = 0; c < COL_COUNT; c++)
        if (QTableWidgetItem *h = m_table->horizontalHeaderItem(c))
            h->setToolTip(tr(tips[c]));

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    setupColumnSizing();
    lay->addWidget(m_table, 1);

    // Start with the collapsible groups hidden (output-first); reload() reveals
    // any that the workspace already uses.
    setColumnsHidden(inputCols(), true);
    setColumnsHidden(feedbackCols(), true);
    setColumnsHidden(ptCols(), true);

    connect(m_table, &QTableWidget::itemChanged, this, &UniversePatchGrid::onItemChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &UniversePatchGrid::onSelectionChanged);
    connect(m_ioMap, SIGNAL(universeAdded(quint32)), this, SLOT(reload()));
    connect(m_ioMap, SIGNAL(universeRemoved(quint32)), this, SLOT(reload()));

    reload();
}

bool UniversePatchGrid::isNetworkPlugin(const QString &p) const
{
    return p.contains("ArtNet", Qt::CaseInsensitive)
        || p.contains("E1.31", Qt::CaseInsensitive)
        || p.contains("sACN", Qt::CaseInsensitive);
}

bool UniversePatchGrid::isMidiPlugin(const QString &p) const
{
    return p.contains("MIDI", Qt::CaseInsensitive);
}

void UniversePatchGrid::setColumnsHidden(const QList<int> &cols, bool hidden)
{
    foreach (int c, cols)
        m_table->setColumnHidden(c, hidden);
}

void UniversePatchGrid::toggleInputGroup(bool on)    { setColumnsHidden(inputCols(), !on); }
void UniversePatchGrid::toggleFeedbackGroup(bool on) { setColumnsHidden(feedbackCols(), !on); }
void UniversePatchGrid::togglePassthroughGroup(bool on) { setColumnsHidden(ptCols(), !on); }

void UniversePatchGrid::refreshOptionCaches()
{
    m_outPlugins.clear();
    foreach (const QString &p, m_ioMap->outputPluginNames())
        m_outPlugins << qMakePair(p, m_ioMap->pluginOutputs(p));
    m_inPlugins.clear();
    foreach (const QString &p, m_ioMap->inputPluginNames())
        m_inPlugins << qMakePair(p, m_ioMap->pluginInputs(p));
    m_profiles = m_ioMap->profileNames();
}

void UniversePatchGrid::scheduleReload()
{
    if (m_reloadScheduled)
        return;
    m_reloadScheduled = true;
    QTimer::singleShot(0, this, [this]() { m_reloadScheduled = false; reload(); });
}

void UniversePatchGrid::reload()
{
    m_loading = true;
    m_table->setRowCount(0);
    refreshOptionCaches();
    const int count = int(m_ioMap->universesCount());
    m_table->setRowCount(count);
    for (int i = 0; i < count; i++)
        populateRow(i, i);
    m_loading = false;

    // First load: auto-reveal the groups the workspace actually uses.
    if (!m_autoRevealDone)
    {
        bool inUsed = false, fbUsed = false, ptUsed = false;
        for (int i = 0; i < count; i++)
        {
            if (m_ioMap->inputPatch(i) != NULL) inUsed = true;
            OutputPatch *fb = m_ioMap->feedbackPatch(i);
            if (fb != NULL && isNetworkPlugin(fb->pluginName())) fbUsed = true;
            if (m_ioMap->getUniversePassthrough(i)) ptUsed = true;
        }
        m_inputToggle->setChecked(inUsed);
        m_feedbackToggle->setChecked(fbUsed);
        m_ptToggle->setChecked(ptUsed);
        m_autoRevealDone = true;
    }
    onSelectionChanged();
}

void UniversePatchGrid::setupColumnSizing()
{
    QHeaderView *h = m_table->horizontalHeader();
    h->setStretchLastSection(false);
    h->setSectionResizeMode(COL_UNI,        QHeaderView::ResizeToContents);
    h->setSectionResizeMode(COL_NAME,       QHeaderView::Interactive);
    h->setSectionResizeMode(COL_USAGE,      QHeaderView::ResizeToContents);
    h->setSectionResizeMode(COL_OUT,        QHeaderView::Stretch);
    h->setSectionResizeMode(COL_OUT_IP,     QHeaderView::Interactive);
    h->setSectionResizeMode(COL_OUT_UNI,    QHeaderView::ResizeToContents);
    h->setSectionResizeMode(COL_OUT_MODE,   QHeaderView::ResizeToContents);
    h->setSectionResizeMode(COL_IN,         QHeaderView::Stretch);
    h->setSectionResizeMode(COL_IN_UNI,     QHeaderView::ResizeToContents);
    h->setSectionResizeMode(COL_IN_PROFILE, QHeaderView::Interactive);
    h->setSectionResizeMode(COL_FB,         QHeaderView::Stretch);
    h->setSectionResizeMode(COL_FB_IP,      QHeaderView::Interactive);
    h->setSectionResizeMode(COL_FB_UNI,     QHeaderView::ResizeToContents);
    h->setSectionResizeMode(COL_PT,         QHeaderView::ResizeToContents);
    m_table->setColumnWidth(COL_NAME, 140);
    m_table->setColumnWidth(COL_OUT_IP, 120);
    m_table->setColumnWidth(COL_IN_PROFILE, 100);
    m_table->setColumnWidth(COL_FB_IP, 120);
}

QComboBox *UniversePatchGrid::buildDeviceCombo(bool inputs, bool excludeMidi,
                                               const QString &curPlugin, int curLine)
{
    QComboBox *combo = new QComboBox(m_table);
    combo->addItem(tr("(none)"));
    combo->setItemData(0, QString(), R_PLUGIN);
    combo->setItemData(0, -1, R_LINE);
    const QList<QPair<QString, QStringList> > &plugins = inputs ? m_inPlugins : m_outPlugins;
    int sel = 0, idx = 1;
    for (int p = 0; p < plugins.size(); ++p)
    {
        const QString &plugin = plugins.at(p).first;
        if (excludeMidi && isMidiPlugin(plugin))
            continue;
        const QStringList &lines = plugins.at(p).second;
        for (int li = 0; li < lines.size(); ++li)
        {
            combo->addItem(QString("%1 · %2").arg(plugin).arg(lines.at(li)));
            combo->setItemData(idx, plugin, R_PLUGIN);
            combo->setItemData(idx, li, R_LINE);
            if (plugin == curPlugin && li == curLine)
                sel = idx;
            idx++;
        }
    }
    combo->setCurrentIndex(sel);
    return combo;
}

void UniversePatchGrid::populateRow(int row, int uniIndex)
{
    OutputPatch *op = m_ioMap->outputPatch(uniIndex);
    InputPatch  *ip = m_ioMap->inputPatch(uniIndex);
    OutputPatch *fb = m_ioMap->feedbackPatch(uniIndex);

    const QString outPlugin = op ? op->pluginName() : QString();
    const bool outNet = op && isNetworkPlugin(outPlugin);
    const QMap<QString, QVariant> outParams = op ? op->getPluginParameters() : QMap<QString, QVariant>();

    // --- Uni / Name / Usage --------------------------------------------------
    m_table->setItem(row, COL_UNI, roItem(QString::number(m_ioMap->getUniverseID(uniIndex) + 1)));

    const QStringList names = m_ioMap->universeNames();
    m_table->setItem(row, COL_NAME, new QTableWidgetItem(
        uniIndex < names.size() ? names.at(uniIndex) : tr("Universe %1").arg(uniIndex + 1)));

    {
        const quint32 uniID = m_ioMap->getUniverseID(uniIndex);
        int n = 0, lastCh = 0;
        foreach (Fixture *fx, m_doc->fixtures())
        {
            if (fx == NULL || fx->universe() != uniID)
                continue;
            n++;
            lastCh = qMax(lastCh, int(fx->address() + fx->channels()));
        }
        QTableWidgetItem *useIt = roItem(n > 0 ? tr("%1 fx · %2/512").arg(n).arg(lastCh) : QString());
        if (lastCh > 512)
            useIt->setForeground(QColor(220, 90, 90));
        m_table->setItem(row, COL_USAGE, useIt);
    }

    // --- OUTPUT device (+ MIDI Out/FB role) ---------------------------------
    // The output cell shows the output patch; for a MIDI feedback patch (which
    // shares the port) it shows that device with role = FB instead.
    QString outDevPlugin = outPlugin;
    int outDevLine = op ? int(op->output()) : -1;
    bool roleFB = false;
    if (op == NULL && fb != NULL && isMidiPlugin(fb->pluginName()))
    {
        outDevPlugin = fb->pluginName();
        outDevLine = int(fb->output());
        roleFB = true;
    }
    {
        QWidget *cell = new QWidget(m_table);
        QHBoxLayout *hl = new QHBoxLayout(cell);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(2);
        QComboBox *dev = buildDeviceCombo(false, false, outDevPlugin, outDevLine);
        dev->setObjectName("dev");
        dev->setParent(cell);
        QComboBox *role = new QComboBox(cell);
        role->setObjectName("role");
        role->addItem(tr("Out"));
        role->addItem(tr("FB"));
        role->setCurrentIndex(roleFB ? 1 : 0);
        role->setVisible(isMidiPlugin(outDevPlugin));
        role->setToolTip(tr("MIDI port role: Output or Feedback (they share one port)"));
        if (!outDevPlugin.isEmpty())
            tintCombo(dev, kOutAccent);
        hl->addWidget(dev, 1);
        hl->addWidget(role);
        connect(dev, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int) { onOutputDeviceChanged(row); });
        connect(role, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int) { onOutputRoleChanged(row); });
        m_table->setCellWidget(row, COL_OUT, cell);
    }

    // Output IP / universe (network) + transmission mode (ArtNet).
    QTableWidgetItem *ipIt = new QTableWidgetItem(outNet ? outParams.value(P_OUT_IP).toString() : QString());
    if (!outNet) ipIt->setFlags(ipIt->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, COL_OUT_IP, ipIt);

    QTableWidgetItem *ouIt = new QTableWidgetItem(outNet ? QString::number(outParams.value(P_OUT_UNI, 0).toInt()) : QString());
    if (!outNet) ouIt->setFlags(ouIt->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, COL_OUT_UNI, ouIt);

    if (op && outPlugin.contains("ArtNet", Qt::CaseInsensitive))
    {
        QComboBox *modeCombo = new QComboBox(m_table);
        modeCombo->addItems(QStringList() << "Standard" << "Full" << "Partial");
        modeCombo->setCurrentText(outParams.value(P_MODE, "Standard").toString());
        tintCombo(modeCombo, kOutAccent);
        connect(modeCombo, &QComboBox::currentTextChanged, this,
                [this, row](const QString &m) { onModeChanged(row, m); });
        m_table->setCellWidget(row, COL_OUT_MODE, modeCombo);
    }
    else
    {
        m_table->setItem(row, COL_OUT_MODE, roItem(QString()));
    }

    // --- INPUT group ---------------------------------------------------------
    const QString inPlugin = ip ? ip->pluginName() : QString();
    const bool inNet = ip && isNetworkPlugin(inPlugin);
    const QMap<QString, QVariant> inParams = ip ? ip->getPluginParameters() : QMap<QString, QVariant>();
    {
        QComboBox *inDev = buildDeviceCombo(true, false, inPlugin, ip ? int(ip->input()) : -1);
        if (ip != NULL)
            tintCombo(inDev, kInAccent);
        connect(inDev, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int) { onInputChanged(row); });
        m_table->setCellWidget(row, COL_IN, inDev);
    }
    QTableWidgetItem *inUniIt = new QTableWidgetItem(inNet ? QString::number(inParams.value(P_IN_UNI, 0).toInt()) : QString());
    if (!inNet) inUniIt->setFlags(inUniIt->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, COL_IN_UNI, inUniIt);
    {
        QComboBox *profCombo = new QComboBox(m_table);
        profCombo->addItem(tr("(none)"));
        int sel = 0;
        for (int pi = 0; pi < m_profiles.size(); ++pi)
        {
            profCombo->addItem(m_profiles.at(pi));
            if (ip != NULL && ip->profileName() == m_profiles.at(pi))
                sel = pi + 1;
        }
        profCombo->setCurrentIndex(sel);
        profCombo->setEnabled(ip != NULL);
        if (ip != NULL)
            tintCombo(profCombo, kInAccent);
        connect(profCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int) { onProfileChanged(row); });
        m_table->setCellWidget(row, COL_IN_PROFILE, profCombo);
    }

    // --- FEEDBACK group (network only; MIDI feedback lives on the output cell) -
    const bool fbNet = fb && isNetworkPlugin(fb->pluginName());
    const QString fbPlugin = fbNet ? fb->pluginName() : QString();
    const QMap<QString, QVariant> fbParams = fbNet ? fb->getPluginParameters() : QMap<QString, QVariant>();
    {
        QComboBox *fbDev = buildDeviceCombo(false, true /*excludeMidi*/, fbPlugin, fbNet ? int(fb->output()) : -1);
        if (fbNet)
            tintCombo(fbDev, kFbAccent);
        connect(fbDev, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int) { onFeedbackChanged(row); });
        m_table->setCellWidget(row, COL_FB, fbDev);
    }
    QTableWidgetItem *fbIpIt = new QTableWidgetItem(fbNet ? fbParams.value(P_OUT_IP).toString() : QString());
    if (!fbNet) fbIpIt->setFlags(fbIpIt->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, COL_FB_IP, fbIpIt);
    QTableWidgetItem *fbUniIt = new QTableWidgetItem(fbNet ? QString::number(fbParams.value(P_OUT_UNI, 0).toInt()) : QString());
    if (!fbNet) fbUniIt->setFlags(fbUniIt->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, COL_FB_UNI, fbUniIt);

    // --- Passthrough badge ---------------------------------------------------
    const bool pt = m_ioMap->getUniversePassthrough(uniIndex);
    QTableWidgetItem *ptIt;
    if (pt)
    {
        ptIt = roItem(tr("PASS"));
        QFont f = ptIt->font();
        f.setBold(true);
        ptIt->setFont(f);
        ptIt->setForeground(QColor(40, 28, 0));
        ptIt->setBackground(QColor(255, 171, 64));
        ptIt->setToolTip(tr("Passthrough: input is forwarded straight to output"));
    }
    else
    {
        ptIt = roItem(QString());
    }
    ptIt->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(row, COL_PT, ptIt);

    // --- Row protocol wash (text cells only; combos carry the direction tint) -
    const QString typePlugin = op ? outPlugin : (ip ? inPlugin : (fbNet ? fbPlugin : QString()));
    const QColor wash   = protocolTint(typePlugin, 34);
    const QColor swatch = protocolTint(typePlugin, 150);
    for (int c = 0; c < COL_COUNT; c++)
    {
        if (c == COL_PT && pt)
            continue;
        QTableWidgetItem *cell = m_table->item(row, c);
        if (cell == NULL)
            continue;
        if (c == COL_UNI && swatch.isValid())
            cell->setBackground(swatch);
        else if (wash.isValid())
            cell->setBackground(wash);
    }
}

// ---------------------------------------------------------------------------
// Edits
// ---------------------------------------------------------------------------

void UniversePatchGrid::onItemChanged(QTableWidgetItem *item)
{
    if (m_loading || item == nullptr)
        return;
    const int row = item->row();
    const int col = item->column();

    if (col == COL_NAME)
    {
        m_ioMap->setUniverseName(row, item->text());
        m_doc->setModified();
        return;
    }
    if (col == COL_OUT_IP)
    {
        applyToSelection(P_OUT_IP, item->text(), false, row);
        return;
    }
    if (col == COL_OUT_UNI)
    {
        applyToSelection(P_OUT_UNI, item->text().toInt(),
                         m_table->selectionModel()->selectedRows().size() > 1, row);
        return;
    }
    if (col == COL_IN_UNI)
    {
        if (InputPatch *ip = m_ioMap->inputPatch(row))
        {
            ip->setPluginParameter(P_IN_UNI, item->text().toInt());
            m_doc->setModified();
            scheduleReload();
        }
        return;
    }
    if (col == COL_FB_IP || col == COL_FB_UNI)
    {
        if (OutputPatch *fb = m_ioMap->feedbackPatch(row))
        {
            if (col == COL_FB_IP)
                fb->setPluginParameter(P_OUT_IP, item->text());
            else
                fb->setPluginParameter(P_OUT_UNI, item->text().toInt());
            m_doc->setModified();
            scheduleReload();
        }
        return;
    }
}

void UniversePatchGrid::onModeChanged(int row, const QString &mode)
{
    if (m_loading)
        return;
    applyToSelection(P_MODE, mode, false, row);
}

void UniversePatchGrid::applyToSelection(const QString &prop, const QVariant &value,
                                         bool autoIncrement, int triggerRow)
{
    QList<int> rows;
    foreach (const QModelIndex &idx, m_table->selectionModel()->selectedRows())
        rows << idx.row();
    if (!rows.contains(triggerRow))
        rows = QList<int>() << triggerRow;
    std::sort(rows.begin(), rows.end());

    int step = 0;
    bool changed = false;
    foreach (int r, rows)
    {
        OutputPatch *op = m_ioMap->outputPatch(r);
        if (op == nullptr || !isNetworkPlugin(op->pluginName()))
            continue;
        QVariant v = value;
        if (autoIncrement)
            v = value.toInt() + step;
        op->setPluginParameter(prop, v);
        changed = true;
        step++;
    }
    if (changed)
    {
        m_doc->setModified();
        scheduleReload();
    }
}

void UniversePatchGrid::onOutputDeviceChanged(int row)
{
    if (m_loading)
        return;
    QWidget *cell = m_table->cellWidget(row, COL_OUT);
    if (cell == NULL)
        return;
    QComboBox *dev = cell->findChild<QComboBox *>("dev");
    QComboBox *role = cell->findChild<QComboBox *>("role");
    if (dev == NULL)
        return;
    const QString plugin = dev->currentData(R_PLUGIN).toString();
    const int line = dev->currentData(R_LINE).toInt();
    const bool midi = isMidiPlugin(plugin);
    if (role != NULL)
        role->setVisible(midi);

    // Clear any MIDI feedback this cell previously owned (the port is exclusive).
    OutputPatch *curFb = m_ioMap->feedbackPatch(row);
    const bool hadMidiFb = curFb != NULL && isMidiPlugin(curFb->pluginName());

    if (plugin.isEmpty())
    {
        m_ioMap->setOutputPatch(row, KInputNone, "", QLCIOPlugin::invalidLine(), false);
        if (hadMidiFb)
            m_ioMap->setOutputPatch(row, KInputNone, "", QLCIOPlugin::invalidLine(), true);
    }
    else if (midi && role != NULL && role->currentIndex() == 1)   // MIDI, role = Feedback
    {
        m_ioMap->setOutputPatch(row, KInputNone, "", QLCIOPlugin::invalidLine(), false);   // no output
        m_ioMap->setOutputPatch(row, plugin, "", quint32(line), true);                     // feedback
    }
    else
    {
        m_ioMap->setOutputPatch(row, plugin, "", quint32(line), false);                    // output
        if (hadMidiFb)
            m_ioMap->setOutputPatch(row, KInputNone, "", QLCIOPlugin::invalidLine(), true);
    }
    m_doc->setModified();
    scheduleReload();
}

void UniversePatchGrid::onOutputRoleChanged(int row)
{
    // Role is just another facet of the output-cell patch — re-apply it.
    onOutputDeviceChanged(row);
}

void UniversePatchGrid::onInputChanged(int row)
{
    if (m_loading)
        return;
    QComboBox *c = qobject_cast<QComboBox *>(m_table->cellWidget(row, COL_IN));
    if (c == nullptr)
        return;
    const QString plugin = c->currentData(R_PLUGIN).toString();
    const int line = c->currentData(R_LINE).toInt();
    if (plugin.isEmpty())
        m_ioMap->setInputPatch(row, KInputNone, "", QLCIOPlugin::invalidLine());
    else
        m_ioMap->setInputPatch(row, plugin, "", quint32(line));
    m_doc->setModified();
    scheduleReload();
}

void UniversePatchGrid::onProfileChanged(int row)
{
    if (m_loading)
        return;
    QComboBox *c = qobject_cast<QComboBox *>(m_table->cellWidget(row, COL_IN_PROFILE));
    if (c == nullptr)
        return;
    const QString prof = (c->currentIndex() == 0) ? QString() : c->currentText();
    m_ioMap->setInputProfile(row, prof);
    m_doc->setModified();
}

void UniversePatchGrid::onFeedbackChanged(int row)
{
    if (m_loading)
        return;
    QComboBox *c = qobject_cast<QComboBox *>(m_table->cellWidget(row, COL_FB));
    if (c == nullptr)
        return;
    const QString plugin = c->currentData(R_PLUGIN).toString();
    const int line = c->currentData(R_LINE).toInt();
    if (plugin.isEmpty())
        m_ioMap->setOutputPatch(row, KInputNone, "", QLCIOPlugin::invalidLine(), true);
    else
        m_ioMap->setOutputPatch(row, plugin, "", quint32(line), true);
    m_doc->setModified();
    scheduleReload();
}

void UniversePatchGrid::onRescan()
{
    foreach (QLCIOPlugin *plugin, m_doc->ioPluginCache()->plugins())
        plugin->rescan();
    reload();
}

// ---------------------------------------------------------------------------
// Add / remove universes
// ---------------------------------------------------------------------------

int UniversePatchGrid::fixturesOnUniverse(int uniIndex) const
{
    const quint32 uniID = m_ioMap->getUniverseID(uniIndex);
    if (uniID == m_ioMap->invalidUniverse())
        return 0;
    int n = 0;
    foreach (Fixture *fx, m_doc->fixtures())
        if (fx != NULL && fx->universe() == uniID)
            n++;
    return n;
}

void UniversePatchGrid::onSelectionChanged()
{
    if (m_removeAction != nullptr)
        m_removeAction->setEnabled(m_ioMap->universesCount() > 0);
}

void UniversePatchGrid::onAddUniverse()
{
    m_ioMap->addUniverse();
    m_ioMap->startUniverses();
    m_doc->setModified();
    reload();
    const int last = m_table->rowCount() - 1;
    if (last >= 0)
    {
        m_table->clearSelection();
        m_table->selectRow(last);
        m_table->scrollToItem(m_table->item(last, 0));
    }
}

void UniversePatchGrid::onRemoveUniverse()
{
    QSet<int> selected;
    foreach (const QModelIndex &idx, m_table->selectionModel()->selectedRows())
        selected.insert(idx.row());

    int last = int(m_ioMap->universesCount()) - 1;
    if (last < 0)
        return;
    if (selected.isEmpty())
        selected.insert(last);
    if (!selected.contains(last))
    {
        QMessageBox::information(this, tr("Remove Universe"),
            tr("QLC+ can only remove universes from the end of the list "
               "(removing one in the middle would renumber everything after it). "
               "Select the last universe — or a block of universes ending at the "
               "last one — and try again."));
        return;
    }

    int removed = 0;
    while (last >= 0 && selected.contains(last))
    {
        const int nFix = fixturesOnUniverse(last);
        const bool patched = m_ioMap->isUniversePatched(last);
        if (nFix > 0 || patched)
        {
            const QString what = nFix > 0
                ? tr("%n fixture(s) are patched to \"%1\".", "", nFix)
                      .arg(m_ioMap->universeNames().value(last))
                : tr("\"%1\" has an input/output patch.")
                      .arg(m_ioMap->universeNames().value(last));
            const QMessageBox::StandardButton r = QMessageBox::question(this,
                tr("Remove Universe"),
                tr("%1\nRemoving it will unbind them. Remove anyway?").arg(what),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r != QMessageBox::Yes)
                break;
        }
        if (!m_ioMap->removeUniverse(last))
            break;
        removed++;
        last--;
    }

    if (removed > 0)
    {
        m_ioMap->startUniverses();
        m_doc->setModified();
        reload();
    }
}
