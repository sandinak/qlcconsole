/*
  Q Light Controller Plus
  gobowheelsdialog.h

  "Gobo Wheels…" on a fixture: see what glass is in each gobo wheel slot of
  THIS unit and fit your own (a logo, a show's custom gobo) in place of the
  definition's -- the way a tech swaps glass in the fixture -- without
  touching the shared fixture definition.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef GOBOWHEELSDIALOG_H
#define GOBOWHEELSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QPair>
#include <functional>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QCheckBox;
class QLCChannel;
class QLCCapability;
class Fixture;
class Doc;

class GoboWheelsDialog final : public QDialog
{
    Q_OBJECT

public:
    GoboWheelsDialog(Doc *doc, quint32 fixtureId, QWidget *parent = nullptr);

    /** Whether @p fx has a gobo wheel with pictures -- the menu only offers
     *  the dialog when it does. */
    static bool hasGoboWheels(const Fixture *fx);

    /** The slots of one wheel that are the same glass as @p cap: itself and
     *  its "shake" variants (same picture in the definition). */
    static QList<const QLCCapability *> sameGlass(const QLCChannel *ch, const QLCCapability *cap);

    /** How Replace… asks for a picture; a test swaps it for a canned path. */
    std::function<QString(QWidget *)> pickImage;

public slots:
    void accept() override;

private slots:
    void slotReplace();
    void slotRestore();
    void slotOpenFolder();
    void refreshButtons();

private:
    typedef QPair<quint32, int> SlotKey;   ///< (channel, slot min)
    void rebuild();
    QString effectiveImage(quint32 channel, const QLCCapability *cap) const;
    QTreeWidgetItem *currentSlot() const;
    void setSlot(QTreeWidgetItem *item, const QString &image);

    Doc *m_doc;
    quint32 m_fid;
    QMap<SlotKey, QString> m_swaps;        ///< the edit in progress
    QTreeWidget *m_tree;
    QPushButton *m_replace;
    QPushButton *m_restore;
    QCheckBox *m_allOfType;
};

#endif
