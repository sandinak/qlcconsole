/*
  Q Light Controller Plus
  networkgearcatalog.h

  Models of network gear -- ArtNet/sACN nodes, USB interfaces, wireless DMX
  transmitters -- to place a NetworkNode from, with its transport and ports
  already filled in. A built-in list of common gear, plus the operator's own
  models as JSON files in NetworkGear in the user folder (~/Library/Application
  Support/qlcconsole on macOS)
  (one model, or an array of them, per file):

      { "manufacturer": "ENTTEC", "model": "ODE Mk3",
        "transport": "ArtNet", "ports": 2 }

  transport is one of "ArtNet", "sACN", "Interface", "Wireless DMX".

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef NETWORKGEARCATALOG_H
#define NETWORKGEARCATALOG_H

#include <QString>
#include <QList>
#include <QDir>

#include "networktopology.h"

struct NetworkGearModel
{
    QString manufacturer;
    QString model;
    int transport = NetworkNode::ArtNet;   //!< NetworkNode::Transport
    int ports = 1;                         //!< DMX outputs (wireless: universes sent)
    bool builtIn = true;                   //!< false: from the user folder

    /** "ENTTEC ODE Mk3" -- what a node made from it records as its model. */
    QString displayName() const;
};

namespace NetworkGearCatalog
{
    /** The built-in list: common gear, as starting points (check a model's
     *  port count against its spec sheet; a node's ports can be edited). */
    QList<NetworkGearModel> builtIn();

    /** The operator's folder (created if missing). */
    QDir userDirectory();
    /** Point userDirectory() somewhere else (tests); empty restores it. */
    void setUserDirectoryOverride(const QString &path);

    /** The operator's models: every readable model in every .json file of
     *  the user folder. A file that isn't valid is skipped. */
    QList<NetworkGearModel> userModels();

    /** Everything: the operator's models first (theirs win over a built-in
     *  of the same name), then the built-in list; sorted by manufacturer,
     *  then model, within each. */
    QList<NetworkGearModel> all();

    /** A node as that model would be delivered: its name (@p name, or the
     *  model's), transport, the model recorded, and one unassigned port per
     *  output -- a wireless one with a receiver named after the port. */
    NetworkNode makeNode(const NetworkGearModel &model, const QString &name = QString());

    /** Save @p model in the user folder, as "<manufacturer> <model>.json"
     *  (replacing a model of that name). Returns the file written, or empty on
     *  failure (no model name, no ports, or the file can't be written). */
    QString saveUserModel(const NetworkGearModel &model);

    /** "ArtNet" etc. to a NetworkNode::Transport, -1 if unknown. */
    int transportFromName(const QString &name);
    /** The name transportFromName() reads. */
    QString transportToName(int transport);
}

#endif
