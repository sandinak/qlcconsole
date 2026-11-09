/*
  Q Light Controller Plus
  gobowheelsdialog.h

  "Gobo Wheels…" on a fixture (or several of one kind, or a group): see what
  glass is in each gobo wheel slot of THESE units and fit your own (a logo, a
  show's custom gobo) in place of the definition's -- the way a tech swaps
  glass in the fixture -- without touching the shared fixture definition.
  The loaded glass can be named ("Moon"), with or without a picture: the
  unit's slot is called that everywhere (FixtureGlass), and a Gobo look of
  that name finds it in whichever slot each unit carries it.

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
#include <QSet>
#include <QList>
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
    /** Several units at once (a selection, a group's members): every one
     *  must have the same gobo wheels (whyNot()). A slot changed here is
     *  changed in each; the slots left alone keep each unit's own glass. */
    GoboWheelsDialog(Doc *doc, const QList<quint32> &fixtureIds, QWidget *parent = nullptr);

    /** Whether @p fx has a gobo wheel with pictures -- the menu only offers
     *  the dialog when it does. */
    static bool hasGoboWheels(const Fixture *fx);

    /** Of @p fixtureIds, the ones with gobo wheels (hasGoboWheels()). */
    static QList<quint32> withGoboWheels(const Doc *doc, const QList<quint32> &fixtureIds);
    /** Why Gobo Wheels can't open on @p fixtureIds together -- their wheels
     *  differ -- or empty when it can. */
    static QString whyNot(const Doc *doc, const QList<quint32> &fixtureIds);

    /** The slots of one wheel that are the same glass as @p cap: itself and
     *  its "shake" variants (same picture in the definition). */
    static QList<const QLCCapability *> sameGlass(const QLCChannel *ch, const QLCCapability *cap);

    /** Library pictures that look like the slot called @p slotName, best first
     *  (at most @p max): the pictures stock fixture definitions give slots of
     *  the same name, ranked by how closely the names match and how many
     *  definitions agree. Resource names GoboLibrary::resolve() reads. */
    static QStringList suggestPictures(const Doc *doc, const QString &slotName, int max = 3);

    /** How Replace… asks for a picture; a test swaps it for a canned path. */
    std::function<QString(QWidget *)> pickImage;

    /** Name the glass loaded in the current slot (and its shake) -- what
     *  Name… and editing the "Loaded glass" cell do. Empty: unnamed. */
    void setSlotName(const QString &name);

public slots:
    void accept() override;

private slots:
    void slotReplace();
    void slotRestore();
    void slotName();
    void slotFitSuggested();
    void slotOpenFolder();
    void refreshButtons();

private:
    typedef QPair<quint32, int> SlotKey;   ///< (channel, slot min)
    void init();
    QStringList suggestionsFor(const QLCCapability *cap, const SlotKey &key) const;
    void rebuild();
    QString effectiveImage(quint32 channel, const QLCCapability *cap) const;
    QTreeWidgetItem *currentSlot() const;
    void setSlot(QTreeWidgetItem *item, const QString &image);
    void setSlotGlass(QTreeWidgetItem *item, const QString *image, const QString *name);
    /** @p key of the reference unit as it is on unit @p to (its k-th gobo
     *  channel; the wheels are the same, whyNot()). */
    SlotKey keyOn(const Fixture *to, const SlotKey &key) const;
    /** Whether the units disagree about @p key's glass (left alone, each
     *  keeps its own). */
    bool varies(const SlotKey &key) const;

    Doc *m_doc;
    quint32 m_fid;                         ///< the unit shown (the first)
    QList<quint32> m_fids;                 ///< every unit edited
    QMap<SlotKey, QString> m_swaps;        ///< the edit in progress: pictures
    QMap<SlotKey, QString> m_names;        ///< ... and names
    QSet<SlotKey> m_touched;               ///< the slots changed here
    QTreeWidget *m_tree;
    QPushButton *m_replace;
    QPushButton *m_restore;
    QPushButton *m_name = nullptr;
    QPushButton *m_fitSuggested = nullptr;
    QCheckBox *m_allOfType;
    bool m_building = false;
};

#endif
