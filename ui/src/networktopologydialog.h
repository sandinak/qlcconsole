/*
  Q Light Controller Plus
  networktopologydialog.h

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

#ifndef NETWORKTOPOLOGYDIALOG_H
#define NETWORKTOPOLOGYDIALOG_H

#include <QDialog>
#include <QList>

#include "networktopology.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QTableWidget;
class QPushButton;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The Network pane: add/remove nodes (ArtNet/sACN gateways, wireless DMX
 * transmitters), set their transport, give each
 * a set of ports carrying a universe, and place a wireless port's receiver.
 *
 * Edits a WORKING COPY of Doc::networkTopology() and writes it back only on OK,
 * so Cancel really cancels and the Monitor's index-keyed map items are rebuilt
 * once from a settled list rather than chasing live edits. The warnings list is
 * NetworkTopology::warnings() run over the working copy on every change.
 */
class NetworkTopologyDialog : public QDialog
{
    Q_OBJECT

public:
    /** @param selectNode  node index to show first (-1 = the first node) */
    NetworkTopologyDialog(Doc *doc, int selectNode = -1, QWidget *parent = NULL);

    /** The edited nodes; what accept() commits. Exposed for tests. */
    const QList<NetworkNode> &nodes() const { return m_nodes; }

public slots:
    void accept() override;
    /** Cancel: the network as it was when the dialog opened. */
    void reject() override;

signals:
    /** The network changed -- it is written to the show as it is edited,
     *  so the plot can follow (Cancel puts it back). */
    void changed();

private slots:
    void slotNodeRowChanged(int row);
    void slotAddNode();
    void slotRemoveNode();
    void slotNodeFieldEdited();
    void slotAddPort();
    void slotRemovePort();
    void slotPortCellEdited();

private:
    void reloadList(int selectRow);
    void loadNode(int row);
    void storeNodeFields();
    void storePorts();
    void updatePortsEnabled();
    void refreshWarnings();
    QWidget *makeUniverseCombo(quint32 universe);

private:
    Doc *m_doc;
    QList<NetworkNode> m_nodes;
    QList<NetworkNode> m_original;   ///< as it was when the dialog opened (reject())
    /** Write the edit into the show now: live, like every rig dialog. */
    void pushLive();
    int m_row;              //!< node currently shown in the editor, -1 = none
    bool m_loading;         //!< true while widgets are being filled from the model

    QListWidget *m_list;
    QPushButton *m_addNodeBtn;
    QPushButton *m_removeNodeBtn;
    QLineEdit *m_name;
    QComboBox *m_transport;
    QLineEdit *m_ip;
    QTableWidget *m_ports;
    QPushButton *m_addPortBtn;
    QPushButton *m_removePortBtn;
    QListWidget *m_warnings;
};

/** @} */

#endif
