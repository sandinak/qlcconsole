/*
  Q Light Controller Plus
  wleddevicesdialog.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDDEVICESDIALOG_H
#define WLEDDEVICESDIALOG_H

#include <QDialog>
#include <QVariant>
#include <QHash>
#include <QList>
#include <QMap>

#include "qlcui_export.h"

class QDialogButtonBox;
class QTableWidget;
class QPushButton;
class OutputPatch;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The devices a WLED universe is sent to: the primary, and any backups that
 * mirror it -- "a backup lantern mirrors the primary without double
 * programming". Each is an IP, and (backups) optionally an address and a
 * channel count of its own; otherwise the primary's.
 *
 * Kept in the patch's plugin parameters as the WLED output reads them:
 * "outputIP"/"wledAddress"/"wledChannels" for the primary, as every show
 * before backups had them, then "outputIP2", "wledAddress2"... -- which an
 * older build ignores, sending to the primary alone.
 */
class QLC_UI_EXPORT WledDevicesDialog final : public QDialog
{
    Q_OBJECT

public:
    struct Device
    {
        QString ip;
        int address = 0;        ///< 0: the primary's (never 0 for the primary)
        int channels = 0;       ///< 0: the primary's (never 0 for the primary)
        bool operator==(const Device &o) const
        { return ip == o.ip && address == o.address && channels == o.channels; }
        bool operator!=(const Device &o) const { return !(*this == o); }
    };

    /** The devices @p params describe, the primary first. */
    static QList<Device> fromParameters(const QMap<QString, QVariant> &params);
    /** The parameters for @p devices: only the WLED device ones. */
    static QMap<QString, QVariant> toParameters(const QList<Device> &devices);
    /** Is @p key one of the device parameters ("outputIP", "wledAddress2"...)? */
    static bool isDeviceKey(const QString &key);
    /** Write @p devices onto @p op: what is unchanged is left alone (a show
     *  saved again reads as it did), what is gone is unset. */
    static void apply(OutputPatch *op, const QList<Device> &devices);
    /** Why @p devices cannot be used ("" when they can). @p usedElsewhere:
     *  device IPs other universes send to, and which (0-based). */
    static QString problem(const QList<Device> &devices,
                           const QHash<QString, quint32> &usedElsewhere);
    /** Is @p ip an address WLED can be reached at ("172.18.2.50",
     *  "lantern-2.local", either with ":port")? */
    static bool validAddress(const QString &ip);

    /** Ask for the devices of @p universe's WLED patch on @p line and apply
     *  them, as one undo step. True if anything changed. */
    static bool edit(Doc *doc, quint32 universe, quint32 line, QWidget *parent);

    WledDevicesDialog(const QList<Device> &devices, const QStringList &suggestions,
                      const QHash<QString, quint32> &usedElsewhere, QWidget *parent = nullptr);

    QList<Device> devices() const { return m_devices; }
    /** Why OK is off, or "" when it is not. */
    QString problemText() const;

    void addDevice(const QString &ip = QString());
    void removeDevice(int row);
    /** Up (-1) or down (+1); to the top, it is the primary. */
    void moveDevice(int row, int delta);
    void setDeviceIP(int row, const QString &ip);
    QTableWidget *table() const { return m_table; }

public slots:
    void accept() override;

private:
    void rebuild();
    void updateState();
    int currentRow() const;

    QList<Device> m_devices;
    QStringList m_suggestions;
    QHash<QString, quint32> m_usedElsewhere;
    QTableWidget *m_table = nullptr;
    QLabel *m_problem = nullptr;
    QPushButton *m_add = nullptr;
    QPushButton *m_remove = nullptr;
    QPushButton *m_up = nullptr;
    QPushButton *m_down = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};

/** @} */

#endif
