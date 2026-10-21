/*
  Q Light Controller Plus
  headtypesdialog.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QDialogButtonBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QSet>
#include <QRegularExpression>

#include "createfixturegroup.h"
#include "monitorproperties.h"
#include "headtypesdialog.h"
#include "ledlayouteditor.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"

/*****************************************************************************
 * HeadTypesDialog
 *****************************************************************************/

bool HeadTypesDialog::anyMixed(Doc *doc, const QList<quint32> &fixtureIds)
{
    /* Mixed heads in one fixture (an XL-450's RGB and white rows) -- or
       across the selection: a bar patched as two fixtures, its RGB part and
       its white part ("the same problem we have for the other bars .. two
       head types"), wants a group per kind just the same. */
    QSet<QString> kinds;
    foreach (quint32 fid, fixtureIds)
    {
        const Fixture *fx = doc->fixture(fid);
        if (HeadKinds::isMixed(fx))
            return true;
        foreach (const QString &k, HeadKinds::kindsOf(fx))
            kinds.insert(k);
        if (kinds.size() > 1)
            return true;
    }
    return false;
}

QString HeadTypesDialog::suggestName(Doc *doc, const QList<quint32> &fixtureIds)
{
    QString common;
    bool first = true;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = doc->fixture(fid);
        if (fx == nullptr)
            continue;
        if (first) { common = fx->name(); first = false; continue; }
        int n = 0;
        while (n < common.size() && n < fx->name().size() && common.at(n) == fx->name().at(n))
            ++n;
        common.truncate(n);
    }
    // Drop what is left of a trailing number and its separator.
    common.remove(QRegularExpression(QStringLiteral("[\\s#._-]*\\d*$")));
    return common.trimmed();
}

int HeadTypesDialog::offerSplit(Doc *doc, const QList<quint32> &fixtureIds, const QString &baseName,
                                QWidget *parent, QList<FixtureGroup *> *made)
{
    if (!anyMixed(doc, fixtureIds))
        return 0;
    QMessageBox box(QMessageBox::Question, tr("Fixture Group"),
                    tr("Some of these fixtures have more than one kind of head (RGB and white, "
                       "say). Make a group per kind of head, laid out from where each head "
                       "really is?"), QMessageBox::Cancel, parent);
    QPushButton *split = box.addButton(tr("Split by Head Type…"), QMessageBox::AcceptRole);
    QPushButton *one = box.addButton(tr("One Group"), QMessageBox::NoRole);
    box.setDefaultButton(split);
    box.exec();
    if (box.clickedButton() == one)
        return 0;
    if (box.clickedButton() != split)
        return -1;
    HeadTypesDialog dlg(doc, fixtureIds, baseName, parent);
    if (dlg.exec() != QDialog::Accepted)
        return -1;
    if (made != nullptr)
        *made = dlg.createdGroups();
    return 1;
}

HeadTypesDialog::HeadTypesDialog(Doc *doc, const QList<quint32> &fixtureIds,
                                 const QString &baseName, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_ids(fixtureIds)
{
    setWindowTitle(tr("Split by Head Type"));
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#split-by-head-type"));
    MonitorProperties *mp = m_doc->monitorProperties();

    // The mixed models among the fixtures, and every kind of head there is.
    QStringList kinds;
    foreach (quint32 fid, m_ids)
    {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            continue;
        foreach (const QString &k, HeadKinds::kindsOf(fx))
            if (!kinds.contains(k))
                kinds << k;
        const QString key = HeadLayout::keyFor(fx);
        if (key.isEmpty() || m_maps.contains(key) || !HeadKinds::isMixed(fx))
            continue;
        m_maps.insert(key, mp->headLayoutFor(fx));
        m_like.insert(key, fid);
    }

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("These fixtures have more than one kind of head. Check where each "
                                  "head really is -- the LED layout -- then make a fixture group per "
                                  "kind. The layout is kept in this show (and, below, as your "
                                  "default for the model)."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    QHBoxLayout *modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel(tr("Model:"), this));
    m_modelCombo = new QComboBox(this);
    QStringList keys = m_maps.keys();
    keys.sort();
    foreach (const QString &k, keys)
        m_modelCombo->addItem(QString(k).replace(QLatin1Char('|'), QStringLiteral(" · ")), k);
    modelRow->addWidget(m_modelCombo, 1);
    root->addLayout(modelRow);
    m_modelInfo = new QLabel(this);
    root->addWidget(m_modelInfo);

    m_editor = new LedLayoutEditor(m_doc, this);
    root->addWidget(m_editor, 1);
    m_saveDefault = new QCheckBox(tr("Also save as the default for this model"), this);
    m_saveDefault->setToolTip(tr("Keep the map on this computer too, for new shows and shows "
                                 "with no map of their own for this model"));
    m_saveDefault->setChecked(true);
    root->addWidget(m_saveDefault);

    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    root->addWidget(sep);
    /* One row per kind of head: make it or not, and its EXACT name. A single
       name field with the kind added on behind it made "T1-CW-RGB RGB" out
       of a name that already said RGB ("it appended RGB after I'd already
       put it there"). */
    QLabel *groupsLabel = new QLabel(tr("Fixture groups to make:"), this);
    root->addWidget(groupsLabel);
    QFormLayout *groupForm = new QFormLayout;
    m_viewCombo = new QComboBox(this);
    m_viewCombo->addItem(tr("From the front — as the audience sees it"), int(CreateFixtureGroup::ViewFront));
    m_viewCombo->addItem(tr("From above — the plan"), int(CreateFixtureGroup::ViewTop));
    m_viewCombo->addItem(tr("Automatic"), int(CreateFixtureGroup::ViewAuto));
    groupForm->addRow(tr("Laid out:"), m_viewCombo);
    m_closeGaps = new QCheckBox(tr("Close up the gaps"), this);
    m_closeGaps->setChecked(true);
    groupForm->addRow(QString(), m_closeGaps);
    foreach (const QString &k, kinds)
    {
        QCheckBox *cb = new QCheckBox(k, this);
        cb->setChecked(true);
        cb->setProperty("kind", k);
        QLineEdit *name = new QLineEdit(CreateFixtureGroup::headKindGroupName(baseName, k), this);
        connect(cb, &QCheckBox::toggled, name, &QLineEdit::setEnabled);
        groupForm->addRow(cb, name);
        m_kindChecks << cb;
        m_kindNames << name;
    }
    root->addLayout(groupForm);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    bb->addButton(tr("Make Groups"), QDialogButtonBox::AcceptRole);
    root->addWidget(bb);

    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HeadTypesDialog::slotModelChanged);
    connect(m_editor, &LedLayoutEditor::layoutChanged, this, &HeadTypesDialog::slotMapEdited);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    slotModelChanged();
}

const Fixture *HeadTypesDialog::likeFor(const QString &key) const
{
    return m_doc->fixture(m_like.value(key, Fixture::invalidId()));
}

void HeadTypesDialog::slotModelChanged()
{
    showModel(m_modelCombo->currentData().toString());
}

void HeadTypesDialog::showModel(const QString &key)
{
    const Fixture *fx = likeFor(key);
    if (fx == nullptr)
        return;
    const QLCFixtureMode *mode = fx->fixtureMode();
    QMap<QString, int> count;
    for (int h = 0; h < mode->heads().size(); ++h)
        count[HeadKinds::kindOf(mode, h)]++;
    QStringList parts;
    for (auto it = count.constBegin(); it != count.constEnd(); ++it)
        parts << tr("%1 %2").arg(it.value()).arg(it.key());
    int n = 0;
    foreach (quint32 fid, m_ids)
        if (HeadLayout::keyFor(m_doc->fixture(fid)) == key)
            n++;
    m_modelInfo->setText(tr("%1 of these, %2 heads each: %3")
                             .arg(n).arg(mode->heads().size()).arg(parts.join(QStringLiteral(" + "))));
    m_editor->setFixture(fx, m_maps.value(key));
}

void HeadTypesDialog::slotMapEdited()
{
    m_maps.insert(m_modelCombo->currentData().toString(), m_editor->layout());
}

void HeadTypesDialog::accept()
{
    // The maps first: the groups are laid out from them.
    MonitorProperties *mp = m_doc->monitorProperties();
    for (auto it = m_maps.constBegin(); it != m_maps.constEnd(); ++it)
    {
        mp->setHeadLayout(it.value());
        if (m_saveDefault->isChecked())
            HeadLayoutLibrary::save(it.value());
    }
    const int view = m_viewCombo->currentData().toInt();
    const bool gaps = m_closeGaps->isChecked();
    QList<QPair<QString, QString> > named, again;
    QList<FixtureGroup *> same;
    QStringList existing;
    for (int i = 0; i < m_kindChecks.size(); ++i)
    {
        if (!m_kindChecks[i]->isChecked())
            continue;
        const QString kind = m_kindChecks[i]->property("kind").toString();
        const QPair<QString, QString> entry(kind, m_kindNames[i]->text().trimmed());
        // Not the same group twice by accident: is there one with these heads?
        FixtureGroup probe(m_doc);
        probe.setSize(QSize(1, 1));
        CreateFixtureGroup::layoutFixturesByStage(m_doc, m_ids, &probe, view, gaps, kind);
        if (FixtureGroup *g = CreateFixtureGroup::findSameGroup(m_doc, probe.headsMap()))
        {
            existing << tr("%1 (as “%2”)").arg(kind, g->name());
            same << g;
            again << entry;
            continue;
        }
        named << entry;
    }
    if (!existing.isEmpty())
    {
        /* Kept by default -- but a second layout of the same heads (from
           above as well as from the front) is a real want, so it can be made
           too, as the one-group dialog allows. */
        QMessageBox box(QMessageBox::Question, windowTitle(),
                        tr("These already exist with exactly the same heads: %1.")
                            .arg(existing.join(QStringLiteral(", "))),
                        QMessageBox::Cancel, this);
        QPushButton *keep = box.addButton(tr("Keep Them"), QMessageBox::AcceptRole);
        QPushButton *another = box.addButton(tr("Make Another"), QMessageBox::DestructiveRole);
        box.setDefaultButton(keep);
        box.exec();
        if (box.clickedButton() == keep)
            m_created << same;
        else if (box.clickedButton() == another)
            named << again;
        else
        {
            m_created.clear();
            return;
        }
    }
    m_created << CreateFixtureGroup::createByHeadKind(m_doc, m_ids, named, view, gaps);
    m_doc->setModified();
    QDialog::accept();
}
