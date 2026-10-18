/*
  Q Light Controller Plus
  vccuerunner.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDebug>

#include "showtreecontroller.h"
#include "vccuerunner.h"
#include "doc.h"

using namespace ShowTree;

VCCueRunner::VCCueRunner(QWidget *parent, Doc *doc)
    : VCWidget(parent, doc)
    , m_runner(new CueRunner(doc, CueRunner::Compact, this))
{
    setObjectName(VCCueRunner::staticMetaObject.className());
    setType(VCWidget::CueRunnerWidget);
    setCaption(QString());
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(2, 2, 2, 2);
    v->addWidget(m_runner);
    resize(QSize(300, 140));
    connect(m_runner, &CueRunner::followChanged, this, [this]() { m_doc->setModified(); });
}

VCCueRunner::~VCCueRunner()
{
}

void VCCueRunner::setID(quint32 id)
{
    VCWidget::setID(id);
    if (caption().isEmpty())
        setCaption(tr("Cue Runner %1").arg(id));
}

quint32 VCCueRunner::follow() const
{
    return m_runner->follow();
}

void VCCueRunner::setFollow(quint32 listId)
{
    m_runner->setFollow(listId);
}

CueRunner::Size VCCueRunner::runnerSize() const
{
    return m_runner->runnerSize();
}

void VCCueRunner::setRunnerSize(CueRunner::Size size)
{
    if (size == CueRunner::Full)
        size = CueRunner::List;             // full is the Run tab's
    m_runner->setRunnerSize(size);
}

/*********************************************************************
 * Clipboard
 *********************************************************************/

VCWidget *VCCueRunner::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != NULL);
    VCCueRunner *r = new VCCueRunner(parent, m_doc);
    if (r->copyFrom(this) == false)
    {
        delete r;
        r = NULL;
    }
    return r;
}

bool VCCueRunner::copyFrom(const VCWidget *widget)
{
    const VCCueRunner *r = qobject_cast<const VCCueRunner *>(widget);
    if (r == NULL)
        return false;
    setRunnerSize(r->runnerSize());
    setFollow(r->follow());
    return VCWidget::copyFrom(widget);
}

/*********************************************************************
 * Properties
 *********************************************************************/

void VCCueRunner::editProperties()
{
    QDialog d(this);
    d.setObjectName(QStringLiteral("cueRunnerProperties"));
    d.setWindowTitle(tr("Cue Runner properties"));
    QFormLayout *f = new QFormLayout(&d);
    QLineEdit *caption = new QLineEdit(this->caption(), &d);
    f->addRow(tr("Caption"), caption);
    QComboBox *follow = new QComboBox(&d);
    follow->setObjectName(QStringLiteral("crPropFollow"));
    ShowTreeController *st = m_doc->showTree();
    follow->addItem(tr("The current production"), invalidId());
    for (const Production &p : st->productions())
        for (quint32 lid : st->playableLists(p.id))
            follow->addItem(QStringLiteral("%1 · %2").arg(p.name, st->cueList(lid)->name), lid);
    follow->setCurrentIndex(qMax(0, follow->findData(this->follow())));
    f->addRow(tr("Plays"), follow);
    QComboBox *size = new QComboBox(&d);
    size->setObjectName(QStringLiteral("crPropSize"));
    size->addItem(tr("Compact: on now, next, GO and Sub GO"), int(CueRunner::Compact));
    size->addItem(tr("List: the whole runner"), int(CueRunner::List));
    size->setCurrentIndex(runnerSize() == CueRunner::List ? 1 : 0);
    f->addRow(tr("Size"), size);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    f->addRow(bb);
    if (d.exec() != QDialog::Accepted)
        return;
    setCaption(caption->text());
    setRunnerSize(CueRunner::Size(size->currentData().toInt()));
    setFollow(follow->currentData().toUInt());
    m_doc->setModified();
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCCueRunner::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCCueRunner)
    {
        qWarning() << Q_FUNC_INFO << "Cue runner node not found";
        return false;
    }
    const QXmlStreamAttributes attrs = root.attributes();
    loadXMLCommon(root);
    setRunnerSize(CueRunner::stringToSize(attrs.value(KXMLQLCVCCueRunnerSize).toString()));
    bool ok = false;
    const quint32 lid = attrs.value(KXMLQLCVCCueRunnerFollow).toString().toUInt(&ok);
    setFollow(ok ? lid : invalidId());

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            int x = 0, y = 0, w = 0, h = 0;
            bool visible = true;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(x, y, w, h);
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else
        {
            root.skipCurrentElement();
        }
    }
    return true;
}

bool VCCueRunner::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);
    doc->writeStartElement(KXMLQLCVCCueRunner);
    if (follow() != invalidId())
        doc->writeAttribute(KXMLQLCVCCueRunnerFollow, QString::number(follow()));
    doc->writeAttribute(KXMLQLCVCCueRunnerSize, CueRunner::sizeToString(runnerSize()));
    saveXMLCommon(doc);
    saveXMLWindowState(doc);
    saveXMLAppearance(doc);
    doc->writeEndElement();
    return true;
}
