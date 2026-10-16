/*
  Q Light Controller Plus
  artnetnodesetup.h

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

#ifndef ARTNETNODESETUP_H
#define ARTNETNODESETUP_H

#include <QDialog>
#include <QHostAddress>
#include <functional>

#include "artnetpacketizer.h"

class QRadioButton;
class QPushButton;
class QGroupBox;
class QLineEdit;
class QCheckBox;
class QTimer;
class QLabel;
class ArtNetPlugin;

/**
 * Set an Art-Net node's IP address, subnet mask, gateway or DHCP -- the
 * DMX-Workshop "configure the node's network" step, with ArtIpProg.
 *
 * Nothing is sent unless the operator presses a button, and nothing that
 * changes the node is sent without a confirmation naming the node, the old
 * and new settings and the interface it leaves by.
 *
 * ArtIpProg goes to the node's own address, unicast (Art-Net 4). It carries
 * no MAC or serial, so a broadcast one would re-address every node on the
 * cable that implements it -- this dialog never broadcasts it. A node whose
 * address is on no network this computer has an address on (a new node still
 * on its factory 2.x.x.x) therefore cannot be programmed until this computer
 * has an address on the node's network too: the dialog says so, and gives
 * the command that adds a temporary one.
 */
class ArtNetNodeSetup final : public QDialog
{
    Q_OBJECT

public:
    ArtNetNodeSetup(ArtNetPlugin *plugin, const QString &address, QWidget *parent = 0);
    ~ArtNetNodeSetup();

    /** The mask a node at @a ip most likely uses: Art-Net's own 2.x and
     *  10.x networks are /8, link-local is /16, anything else /24. */
    static QHostAddress guessMask(const QHostAddress &ip);

    /** An address for this computer on @a ip's network that is not @a ip:
     *  near the top of the range, where a node is least likely to be. */
    static QHostAddress suggestAlias(const QHostAddress &ip, const QHostAddress &mask);

    /** The command that adds (or removes) @a alias on @a iface, for this OS */
    static QString aliasCommand(const QString &iface, const QHostAddress &alias,
                                const QHostAddress &mask, bool add);

    /** Check the form and build the request; returns why it can't, or empty */
    QString validate(ArtNetIpProgRequest *request) const;

    /** Ask for confirmation, then send. Returns true if it was sent. */
    bool apply();

    /** Send a query (an ArtIpProg that programs nothing). Returns true if sent. */
    bool query();

    /** Can this computer send to the node directly? */
    bool reachable() const { return m_reachable; }

private slots:
    void slotModeChanged();
    void slotReply(const QString &sender, const ArtNetIpProgReply &reply);
    void slotNoAnswer();

private:
    QString describe(const ArtNetIpProgRequest &request) const;
    void waitForAnswer(const QString &what);

private:
    ArtNetPlugin *m_plugin;
    QString m_address;      //!< where the node is now
    QString m_mac;
    QString m_nodeName;
    QString m_viaText;      //!< "vlan0 (172.18.2.219)"
    bool m_reachable;
    QString m_expectFrom;   //!< the address a reply may come from besides m_address

    QLabel *m_header;
    QLabel *m_reach;
    QGroupBox *m_aliasBox;
    QLineEdit *m_aliasAdd;
    QLineEdit *m_aliasRemove;
    QRadioButton *m_static;
    QRadioButton *m_dhcp;
    QRadioButton *m_defaults;
    QLineEdit *m_ip;
    QLineEdit *m_mask;
    QCheckBox *m_setGateway;
    QLineEdit *m_gateway;
    QPushButton *m_queryButton;
    QPushButton *m_applyButton;
    QLabel *m_status;
    QTimer *m_answerTimer;

public:
    /** Asks the operator to confirm @a text; a QMessageBox unless a test
     *  replaces it. */
    std::function<bool(const QString &text)> m_confirm;
};

#endif
