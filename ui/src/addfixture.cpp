/*
  Q Light Controller
  addfixture.cpp

  Copyright (c) Heikki Junnila

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
#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QHeaderView>
#include <QByteArray>
#include <QSettings>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QDebug>
#include <QAction>
#include <QSet>

#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"

#include "addresstool.h"
#include <QGridLayout>
#include "addfixture.h"
#include "apputil.h"
#include "doc.h"

#define SETTINGS_GEOMETRY "addfixture/geometry"

#define KColumnName 0
#define KColumnSource 1

AddFixture::AddFixture(QWidget* parent, const Doc* doc, const Fixture* fxi)
    : QDialog(parent)
    , m_doc(doc)
{
    m_addressValue = 0;
    m_universeValue = 0;
    m_amountValue = 1;
    m_gapValue = 0;
    m_channelsValue = 1;
    m_fixtureDef = NULL;
    m_mode = NULL;
    m_fxiCount = 0;
    m_fixtureID = Fixture::invalidId();
    m_invalidAddressFlag = false;
    m_inventoryMode = false;

    setupUi(this);
    m_addrErrorLabel->hide();
    m_nameErrorLabel->hide();

    /* Connection: DMX (through a converter node / wireless receiver) or
       Network (the fixture speaks ArtNet/sACN itself and takes a unicast IP). */
    m_connectionCombo = new QComboBox(m_propertiesGroup);
    m_connectionCombo->addItem(tr("DMX"), connectionKey(Fixture::Dmx, Fixture::ArtNetTransport));
    m_connectionCombo->addItem(tr("Network (ArtNet/sACN)"), connectionKey(Fixture::Network, Fixture::ArtNetTransport));
    m_connectionCombo->addItem(tr("Network (WLED, JSON over Wi-Fi)"), connectionKey(Fixture::Network, Fixture::WledTransport));
    m_connectionCombo->setToolTip(tr("DMX fixtures hang off a node or wireless receiver. "
                                     "Network fixtures are their own endpoint and are "
                                     "sent to at their own IP -- as Art-Net DMX, or for "
                                     "WLED on Wi-Fi, as its look's state through WLED's "
                                     "own API, only when it changes."));
    m_ipEdit = new QLineEdit(m_propertiesGroup);
    m_ipEdit->setPlaceholderText(tr("unicast IP, e.g. 172.18.2.41"));
    m_ipEdit->setEnabled(false);
    if (QGridLayout *pg = qobject_cast<QGridLayout *>(m_propertiesGroup->layout()))
    {
        const int row = pg->rowCount();
        pg->addWidget(new QLabel(tr("Connection:"), m_propertiesGroup), row, 0);
        pg->addWidget(m_connectionCombo, row, 2);
        pg->addWidget(new QLabel(tr("IP address:"), m_propertiesGroup), row + 1, 0);
        pg->addWidget(m_ipEdit, row + 1, 2);
    }
    connect(m_connectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { m_ipEdit->setEnabled(connection() == Fixture::Network); });

    QAction* action = new QAction(this);
    action->setShortcut(QKeySequence(QKeySequence::Close));
    connect(action, SIGNAL(triggered(bool)), this, SLOT(reject()));
    addAction(action);

    connect(m_tree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotSelectionChanged()));
    connect(m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotTreeDoubleClicked(QTreeWidgetItem*)));
    connect(m_modeCombo, SIGNAL(activated(int)),
            this, SLOT(slotModeActivated(int)));
    connect(m_universeCombo, SIGNAL(activated(int)),
            this, SLOT(slotUniverseActivated(int)));
    connect(m_addressSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotAddressChanged(int)));
    connect(m_channelsSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotChannelsChanged(int)));
    connect(m_nameEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(slotNameEdited(const QString&)));
    connect(m_gapSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotGapSpinChanged(int)));
    connect(m_amountSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotAmountSpinChanged(int)));
    connect(m_searchEdit, SIGNAL(textChanged(QString)),
            this, SLOT(slotSearchFilterChanged(QString)));
    connect(m_diptoolButton, SIGNAL(clicked()),
            this, SLOT(slotDiptoolButtonClicked()));

    /* Fill fixture definition tree (and select a fixture def) */
    if (fxi != NULL)
    {
        fillTree(fxi->fixtureDef()->manufacturer(), fxi->fixtureDef()->model());
        m_fixtureID = fxi->id();
    }
    else
        fillTree(KXMLFixtureGeneric, KXMLFixtureGeneric);

    m_fixturesCount->setText(tr("Fixtures found: %1").arg(m_fxiCount));

    /* Fill universe combo with available universes */
    m_universeCombo->addItems(m_doc->inputOutputMap()->universeNames());

    /* Simulate first selection and find the next free address */
    slotSelectionChanged();

    if (fxi != NULL)
    {
        // Universe
        m_universeCombo->setCurrentIndex(fxi->universe());
        slotUniverseActivated(fxi->universe());

        m_addressSpin->setValue(fxi->address() + 1);
        m_addressValue = fxi->address();

        m_multipleGroup->setEnabled(false);

        m_connectionCombo->setCurrentIndex(m_connectionCombo->findData(
            connectionKey(fxi->connection(), fxi->connection() == Fixture::Network
                                             ? fxi->transport() : Fixture::ArtNetTransport)));
        m_ipEdit->setText(fxi->ipAddress());

        // Name
        m_nameEdit->setText(fxi->name());
        slotNameEdited(fxi->name());
        m_nameEdit->setModified(true); // Prevent auto-naming

        // Mode
        int index = m_modeCombo->findText(fxi->fixtureMode()->name());
        if (index != -1)
        {
            m_channelsSpin->setValue(fxi->channels());
            m_modeCombo->setCurrentIndex(index);
            slotModeActivated(index);
        }
    }
    else
    {
        slotUniverseActivated(0);
        findAddress();

        m_channelsSpin->setValue(1);
    }

    QSettings settings;
    QVariant var = settings.value(SETTINGS_GEOMETRY);
    if (var.isValid() == true)
        restoreGeometry(var.toByteArray());
    AppUtil::ensureWidgetIsVisible(this);

    // Set focus on the search bar
    m_searchEdit->setFocus();
}

AddFixture::~AddFixture()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());

    QList<QVariant> expanded;
    QTreeWidgetItem * root = m_tree->invisibleRootItem();

    for (int i=0; i < root->childCount(); i++)
    {
        QTreeWidgetItem * manuf = root->child(i);
        if (manuf->isExpanded())
        {
            expanded << manuf->text(KColumnName);
        }
    }

    settings.setValue(SETTINGS_EXPANDED, expanded);
}

/*****************************************************************************
 * Value getters
 *****************************************************************************/

QLCFixtureDef* AddFixture::fixtureDef() const
{
    return m_fixtureDef;
}

QLCFixtureMode *AddFixture::mode() const
{
    return m_mode;
}

QString AddFixture::name() const
{
    return m_nameValue;
}

quint32 AddFixture::address() const
{
    return m_addressValue;
}

quint32 AddFixture::universe() const
{
    return m_universeValue;
}

int AddFixture::amount() const
{
    return m_amountValue;
}

quint32 AddFixture::gap() const
{
    return m_gapValue;
}

quint32 AddFixture::channels() const
{
    return m_channelsValue;
}

void AddFixture::setInventoryMode(bool inventory)
{
    m_inventoryMode = inventory;
    QWidget *addressBits[] = { m_universeLabel, m_universeCombo, m_addressLabel, m_addressSpin,
                               m_diptoolButton, m_gapLabel, m_gapSpin, m_addrErrorLabel };
    for (QWidget *w : addressBits)
        w->setVisible(!inventory);
    // The channel count is a footprint decision, so it is made when patching
    // (the Patch dialog has a Mode / channels column). A generic dimmer starts
    // at one channel; a real fixture starts in the mode preselected here, which
    // can be changed then too.
    m_channelsLabel->setVisible(!inventory);
    m_channelsSpin->setVisible(!inventory);
    m_modeCombo->setToolTip(inventory
        ? tr("The mode you expect to use. It can be changed when you patch, unless a "
             "scene sets this fixture's channels directly.")
        : QString());
    if (inventory)
    {
        setWindowTitle(tr("Add to Inventory"));
        m_addrErrorLabel->hide();
        m_invalidAddressFlag = false;
    }
    updateMaximumAmount();
}

int AddFixture::connectionKey(Fixture::Connection c, Fixture::Transport t)
{
    return int(c) * 10 + int(t);
}

Fixture::Connection AddFixture::connection() const
{
    return Fixture::Connection(m_connectionCombo->currentData().toInt() / 10);
}

Fixture::Transport AddFixture::transport() const
{
    return Fixture::Transport(m_connectionCombo->currentData().toInt() % 10);
}

QString AddFixture::ipAddress() const
{
    return connection() == Fixture::Network ? m_ipEdit->text().trimmed() : QString();
}

bool AddFixture::invalidAddress()
{
    return m_invalidAddressFlag;
}

/*****************************************************************************
 * Fillers
 *****************************************************************************/

void AddFixture::fillTree(const QString& selectManufacturer,
                          const QString& selectModel)
{
    QTreeWidgetItem* parent = NULL;
    QTreeWidgetItem* child;
    QString manuf;
    QString model;
    QList<QVariant> expanded;

    QSettings settings;
    QVariant var = settings.value(SETTINGS_EXPANDED);
    if (var.isValid() == true)
    {
        expanded = var.toList();
    }

    /* Clear the tree of any previous data */
    m_tree->clear();

    QString filter = m_searchEdit->text().toLower();
    const QHash<QString, QLCFixtureDef *> defs = AppUtil::definitionsByName(m_doc->fixtureDefCache());
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels(QStringList() << tr("Fixture Model") << tr("Source"));

    /* Add all known fixture definitions to the tree */
    QStringListIterator it(m_doc->fixtureDefCache()->manufacturers());
    while (it.hasNext() == true)
    {
        bool manufAdded = false;

        manuf = it.next();
        if (manuf == KXMLFixtureGeneric)
            continue;

        QStringListIterator modit(m_doc->fixtureDefCache()->models(manuf));
        while (modit.hasNext() == true)
        {
            model = modit.next();

            if (filter.isEmpty() == false &&
                manuf.toLower().contains(filter) == false &&
                model.toLower().contains(filter) == false)
                    continue;

            if (manufAdded == false)
            {
                parent = new QTreeWidgetItem(m_tree);
                parent->setText(KColumnName, manuf);
                manufAdded = true;
            }
            child = new QTreeWidgetItem(parent);
            child->setText(KColumnName, model);
            AppUtil::showDefinitionSource(child, KColumnSource, defs.value(manuf + QLatin1Char('\n') + model));

            if (manuf == selectManufacturer &&
                    model == selectModel)
            {
                parent->setExpanded(true);
                m_tree->setCurrentItem(child);
            }
            else if (expanded.indexOf(manuf) != -1)
            {
                parent->setExpanded(true);
            }
            m_fxiCount++;
        }
    }

    /* Sort the tree A-Z BEFORE appending a generic entries */
    m_tree->sortItems(0, Qt::AscendingOrder);

    /* Create a parent for the generic devices */
    parent = new QTreeWidgetItem(m_tree);
    parent->setText(KColumnName, KXMLFixtureGeneric);
    QStringListIterator modit(m_doc->fixtureDefCache()->models(KXMLFixtureGeneric));
    while (modit.hasNext() == true)
    {
        model = modit.next();
        child = new QTreeWidgetItem(parent);
        child->setText(KColumnName, model);

        if (selectManufacturer == KXMLFixtureGeneric &&
                model == selectModel)
        {
            parent->setExpanded(true);
            m_tree->setCurrentItem(child);
        }
        else if (expanded.indexOf(manuf) != -1)
        {
            parent->setExpanded(true);
        }
        m_fxiCount++;
    }

    /* Create a child for generic dimmer device */
    child = new QTreeWidgetItem(parent);
    child->setText(KColumnName, KXMLFixtureGeneric);

    parent->sortChildren(0, Qt::AscendingOrder);

    /* Select generic dimmer by default */
    if (selectManufacturer == KXMLFixtureGeneric &&
            selectModel == KXMLFixtureGeneric)
    {
        parent->setExpanded(true);
        m_tree->setCurrentItem(child);
    }
}

void AddFixture::fillModeCombo(const QString& text)
{
    m_modeCombo->clear();

    if (m_fixtureDef == NULL)
    {
        m_modeCombo->setEnabled(false);
        m_modeCombo->addItem(text);
        m_modeCombo->setCurrentIndex(0);
        m_mode = NULL;
    }
    else
    {
        m_modeCombo->setEnabled(true);

        QListIterator <QLCFixtureMode*> it(m_fixtureDef->modes());
        while (it.hasNext() == true)
            m_modeCombo->addItem(it.next()->name());

        /* Select the first mode by default */
        m_modeCombo->setCurrentIndex(0);
        slotModeActivated(0);
    }
}

void AddFixture::findAddress()
{
    /* Find the next free address space for x fixtures, each taking y
       channels, leaving z channels gap in-between. */
    quint32 address = findAddress((m_channelsValue + m_gapValue) * m_amountValue,
                                  m_doc->fixtures(),
                                  m_doc->inputOutputMap()->universesCount());

    /* Set the address only if the channel space was really found */
    if (address != QLCChannel::invalid())
    {
        m_universeCombo->setCurrentIndex(address >> 9);
        m_addressSpin->setValue((address & 0x01FF) + 1);
    }
}

quint32 AddFixture::findAddress(quint32 numChannels,
                                QList<Fixture*> const& fixtures,
                                quint32 maxUniverses)
{
    /* Try to find contiguous space from one universe at a time */
    for (quint32 universe = 0; universe < maxUniverses; universe++)
    {
        quint32 ch = findAddress(universe, numChannels, fixtures);
        if (ch != QLCChannel::invalid())
            return ch;
    }

    return QLCChannel::invalid();
}

quint32 AddFixture::findAddress(quint32 universe, quint32 numChannels,
                                QList<Fixture*> const& fixtures, quint32 currentFixture)
{
    quint32 freeSpace = 0;
    const quint32 maxChannels = 512;

    /* Construct a map of unallocated channels */
    int map[maxChannels];
    std::fill(map, map + maxChannels, 0);

    QListIterator <Fixture*> fxit(fixtures);
    while (fxit.hasNext() == true)
    {
        Fixture* fxi(fxit.next());
        Q_ASSERT(fxi != NULL);

        // An inventory fixture's 0/0 (or an address clash's wanted spot)
        // holds no channels: it must not push new fixtures off address 1.
        if (fxi->universe() != universe || !fxi->isPatched())
            continue;

        if (fxi->id() == currentFixture && currentFixture != Fixture::invalidId())
            continue;

        for (quint32 ch = 0; ch < fxi->channels(); ch++)
        {
            quint32 addr = (fxi->universeAddress() & 0x01FF) + ch;
            if (addr > 511)
                continue;

            map[addr] = 1;
        }
    }

    /* Try to find the next contiguous free address space */
    for (quint32 addr = 0; addr < maxChannels; addr++)
    {
        if (map[addr] == 0)
            freeSpace++;
        else
            freeSpace = 0;

        if (freeSpace == numChannels)
            return (addr - freeSpace + 1) | (universe << 9);
    }

    return QLCChannel::invalid();
}

void AddFixture::updateMaximumAmount()
{
    /* Staged fixtures hold no address, so how many fit in the rest of the
       universe has nothing to do with how many can be added. That limit,
       read off the hidden address, held a 165-channel fixture to ONE ("why
       can't I add more than 1 fixture here?"). */
    if (m_inventoryMode)
    {
        m_amountSpin->setRange(1, 999);
        return;
    }
    m_amountSpin->setRange(1, (513 - m_addressSpin->value()) /
                           (m_channelsSpin->value() + m_gapSpin->value()));
}

bool AddFixture::checkAddressAvailability(int value, int channels)
{
    qDebug() << "Check availability for address: " << value;
    for (int i = 0; i < channels; i++)
    {
        quint32 fid = m_doc->fixtureForAddress(value + i);
        if (fid != Fixture::invalidId() && fid != m_fixtureID)
            return false;
    }
    return true;
}

/*****************************************************************************
 * Slots
 *****************************************************************************/

void AddFixture::slotModeActivated(int modeIndex)
{
    if (m_fixtureDef == NULL || m_fixtureDef->modes().isEmpty())
        return;

    m_mode = m_fixtureDef->modes().at(modeIndex);
    if (m_mode == NULL)
    {
        /* Generic dimmers don't have modes, so bail out */
        // slotSelectionChanged();
        return;
    }

    m_channelsSpin->setValue(m_mode->channels().size());

    /* Show all selected mode channels in the list */
    m_channelList->clear();
    for (int i = 0; i < m_mode->channels().size(); i++)
    {
        QLCChannel* channel = m_mode->channel(i);
        Q_ASSERT(channel != NULL);

        new QListWidgetItem(
            QString("%1: %2").arg(i + 1).arg(channel->name()),
            m_channelList);
    }
}

void AddFixture::preselectLike(const Fixture *like)
{
    if (like == NULL || like->fixtureDef() == NULL)
        return;                                   // generic: the default already
    fillTree(like->fixtureDef()->manufacturer(), like->fixtureDef()->model());
    if (like->fixtureMode() != NULL)
    {
        const int idx = m_modeCombo->findText(like->fixtureMode()->name());
        if (idx >= 0)
        {
            m_modeCombo->setCurrentIndex(idx);
            slotModeActivated(idx);
        }
    }
}

void AddFixture::preselectManufacturer(const QString &manufacturer)
{
    if (manufacturer.isEmpty())
        return;
    // Open its models and bring them into view; no model is picked for you.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        if (it->text(KColumnName) != manufacturer)
            continue;
        it->setExpanded(true);
        m_tree->scrollToItem(it, QAbstractItemView::PositionAtTop);
        break;
    }
}

void AddFixture::preselectUniverse(quint32 universe)
{
    if ((int)universe >= m_universeCombo->count())
        return;
    m_universeCombo->setCurrentIndex((int)universe);
    // Same path the combo's activated() signal drives, so the address is
    // re-found within the chosen universe.
    slotUniverseActivated((int)universe);
}

void AddFixture::slotUniverseActivated(int universe)
{
    m_universeValue = universe;

    /* Adjust the available address range */
    slotChannelsChanged(m_channelsValue);

    quint32 addr = findAddress(universe, m_channelsSpin->value(), m_doc->fixtures(), m_fixtureID);
    if (addr != QLCChannel::invalid())
        m_addressSpin->setValue((addr & 0x01FF) + 1);
    else
        m_addressSpin->setValue(1);
}

void AddFixture::slotAddressChanged(int value)
{
    m_addressValue = value - 1;

    /* Set the maximum number of fixtures */
    updateMaximumAmount();

    checkOverlapping();
}

void AddFixture::slotChannelsChanged(int value)
{
    m_channelsValue = value;

    /* Set the maximum possible address so that channels cannot overflow
       beyond DMX's range of 512 channels */
    m_addressSpin->setRange(1, 513 - value);

    /* Set the maximum number of fixtures */
    updateMaximumAmount();

    checkOverlapping();
}

void AddFixture::slotNameEdited(const QString &text)
{
    /* If the user clears the text in the name field,
       start substituting the name with the model again. */
    if (text.length() == 0)
        m_nameEdit->setModified(false);
    else
        m_nameEdit->setModified(true);
    m_nameValue = text;

    checkNameConflict();
}

void AddFixture::slotAmountSpinChanged(int value)
{
    m_amountValue = value;

    checkOverlapping();
    checkNameConflict();
}

QSet<QString> AddFixture::existingFixtureNames() const
{
    QSet<QString> names;
    for (Fixture *fxi : m_doc->fixtures())
    {
        if (fxi != NULL && fxi->id() != m_fixtureID)
            names.insert(fxi->name());
    }
    return names;
}

void AddFixture::checkNameConflict()
{
    /* Fixture names are unique. Adding: a taken name is numbered on (the
       label says to what). Changing an existing fixture: a taken name is
       refused -- OK stays off until the name is free. */
    const QString typed = m_nameEdit->text();
    const bool conflict = m_nameEdit->isEnabled() && existingFixtureNames().contains(typed);
    const bool editing = m_fixtureID != Fixture::invalidId();
    if (conflict)
    {
        QString text;
        if (editing)
            text = tr("Another fixture is already called this.");
        else if (m_amountValue > 1)
            text = tr("That name exists — the new fixtures will be numbered past it.");
        else
            text = tr("That name exists — it will be added as “%1”.")
                       .arg(m_doc->uniqueFixtureName(typed));
        m_nameErrorLabel->setText(QString("<html><body><p><span style=\" color:#e08000;\">%1"
                                          "</span></p></body></html>").arg(text.toHtmlEscaped()));
    }
    m_nameErrorLabel->setVisible(conflict);
    if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok))
        ok->setEnabled(!(editing && conflict));
}

void AddFixture::slotGapSpinChanged(int value)
{
    m_gapValue = value;

    /* Set the maximum number of fixtures */
    updateMaximumAmount();

    checkOverlapping();
}

void AddFixture::slotSearchFilterChanged(QString)
{
    m_tree->blockSignals(true);
    fillTree("", "");
    m_tree->blockSignals(false);
}

void AddFixture::slotSelectionChanged()
{
    /* If there is no valid selection (user has selected only a
       manufacturer or nothing at all) don't let him press OK. */
    QTreeWidgetItem* item = m_tree->currentItem();
    if (item == NULL || item->parent() == NULL)
    {
        /* Reset the selected fixture pointer */
        m_fixtureDef = NULL;

        /* Since there is no m_fixtureDef, mode combo is cleared */
        fillModeCombo();

        /* Clear the name box unless it has been modified by user */
        if (m_nameEdit->isModified() == false)
        {
            m_nameEdit->setText(QString());
            slotNameEdited(QString());
            m_nameEdit->setModified(false);
        }
        m_nameEdit->setEnabled(false);

        m_channelsSpin->setValue(0);
        m_channelList->clear();
        m_addressSpin->setEnabled(false);
        m_universeCombo->setEnabled(false);

        m_multipleGroup->setEnabled(false);
        m_amountSpin->setEnabled(false);
        m_gapSpin->setEnabled(false);
        m_channelsSpin->setEnabled(false);

        m_nameErrorLabel->hide();

        m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel);

        return;
    }

    /* Item & its parent should be valid here */
    QString manuf(item->parent()->text(KColumnName));
    QString model(item->text(KColumnName));
    if (manuf == KXMLFixtureGeneric && model == KXMLFixtureGeneric)
    {
        /* Generic dimmer selected. User enters number of channels. */
        if (m_fixtureID != Fixture::invalidId())
        {
            Fixture *fxi = m_doc->fixture(m_fixtureID);
            if (fxi != NULL)
            {
                m_fixtureDef = fxi->fixtureDef();
                m_mode = fxi->fixtureMode();

                if (m_fixtureDef->manufacturer() != manuf || m_fixtureDef->model() != model)
                {
                    m_fixtureDef = NULL;
                }
            }
            else
            {
                m_fixtureDef = NULL;
            }
        }
        else
        {
            m_fixtureDef = NULL;
        }
        fillModeCombo();
        m_modeCombo->setEnabled(false);
        m_channelsSpin->setValue(1);
        m_channelsSpin->setEnabled(true);
        m_channelList->clear();

        /* Set the model name as the fixture's friendly name ONLY
           if the user hasn't modified the friendly name field. */
        if (m_nameEdit->isModified() == false)
        {
            const QSet<QString> used = existingFixtureNames();
            m_nameEdit->setText(Doc::nextUniqueModelName(tr("Dimmers"),
                [&used](const QString &c) { return used.contains(c); }));
            slotNameEdited(m_nameEdit->text());
            m_nameEdit->setModified(false);
        }
        m_nameEdit->setEnabled(true);
    }
    else
    {
        /* Specific fixture definition selected. */
        m_fixtureDef = m_doc->fixtureDefCache()->fixtureDef(manuf, model);
        Q_ASSERT(m_fixtureDef != NULL);

        /* Put fixture def's modes to the mode combo */
        fillModeCombo();

        /* Fixture def contains number of channels, so disable the
           spin box to prevent user from modifying it. */
        m_channelsSpin->setEnabled(false);

        /* Set the model name as the fixture's friendly name ONLY
           if the user hasn't modified the friendly name field. */
        if (m_nameEdit->isModified() == false)
        {
            const QSet<QString> used = existingFixtureNames();
            m_nameEdit->setText(Doc::nextUniqueModelName(m_fixtureDef->model(),
                [&used](const QString &c) { return used.contains(c); }));
            slotNameEdited(m_nameEdit->text());
            m_nameEdit->setModified(false);
        }
        m_nameEdit->setEnabled(true);
    }

    /* Set the maximum number of fixtures */
    updateMaximumAmount();

    /* Guide the user to edit the friendly name field */
    m_nameEdit->setSelection(0, m_nameEdit->text().length());
    m_nameEdit->setFocus();

    m_addressSpin->setEnabled(true);
    m_universeCombo->setEnabled(true);

    m_multipleGroup->setEnabled(true);
    m_amountSpin->setEnabled(true);
    m_gapSpin->setEnabled(true);

    /* Recalculate the first available address for the newly selected fixture */
    quint32 addr = findAddress(m_universeValue, m_channelsSpin->value(), m_doc->fixtures(), m_fixtureID);
    if (addr != QLCChannel::invalid())
        m_addressSpin->setValue((addr & 0x01FF) + 1);
    else
        m_addressSpin->setValue(1);

    /* OK is again possible */
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok |
                                    QDialogButtonBox::Cancel);

    checkNameConflict();
}

void AddFixture::slotTreeDoubleClicked(QTreeWidgetItem* item)
{
    /* Select and accept (click OK for the user) */
    slotSelectionChanged();
    if (item != NULL && item->parent() != NULL)
        accept();
}

void AddFixture::slotDiptoolButtonClicked()
{
    AddressTool at(this, m_addressSpin->value());
    at.exec();
    m_addressSpin->setValue(at.getAddress());
}

void AddFixture::checkOverlapping()
{
    if (m_inventoryMode)
    {
        // Staged fixtures hold no address, so there is nothing to overlap.
        m_addrErrorLabel->hide();
        m_invalidAddressFlag = false;
        return;
    }

    for (int i = 0; i < m_amountValue; ++i)
    {
        int address = m_addressValue + i * (m_gapValue + m_channelsValue);
        int absAddress = (address & 0x01FF) | (m_universeValue << 9);
        if (checkAddressAvailability(absAddress, m_channelsValue) == false)
        {
            // Show overlapping error
            m_addrErrorLabel->show();
            m_invalidAddressFlag = true;
            return;
        }
    }

    m_addrErrorLabel->hide();
    m_invalidAddressFlag = false;
}
