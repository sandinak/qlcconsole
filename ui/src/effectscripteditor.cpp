/*
  Q Light Controller Plus
  effectscripteditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMessageBox>

#include "effectscripteditor.h"
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "doc.h"

EffectScriptEditor::EffectScriptEditor(const QString &filePath, Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_path(filePath)
{
    setWindowTitle(tr("Effect Script — %1").arg(QFileInfo(filePath).fileName()));
    resize(760, 620);

    QVBoxLayout *lay = new QVBoxLayout(this);

    m_edit = new QPlainTextEdit(this);
    m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_edit->setTabStopDistance(4 * m_edit->fontMetrics().horizontalAdvance(' '));
    lay->addWidget(m_edit, 1);

    QHBoxLayout *row = new QHBoxLayout();
    m_status = new QLabel(this);
    m_status->setStyleSheet("color: gray;");
    row->addWidget(m_status, 1);

    QPushButton *reload = new QPushButton(tr("Reload"), this);
    reload->setToolTip(tr("Discard changes and reload the file from disk"));
    connect(reload, &QPushButton::clicked, this, &EffectScriptEditor::reloadFromDisk);
    row->addWidget(reload);

    QPushButton *saveBtn = new QPushButton(tr("Save"), this);
    saveBtn->setDefault(true);
    saveBtn->setToolTip(tr("Write the file and rescan effect scripts so the change takes effect"));
    connect(saveBtn, &QPushButton::clicked, this, &EffectScriptEditor::save);
    row->addWidget(saveBtn);

    QPushButton *closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(closeBtn);

    lay->addLayout(row);

    loadFile();
}

void EffectScriptEditor::loadFile()
{
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_edit->setPlainText(QString::fromUtf8(f.readAll()));
        m_status->setText(tr("Loaded %1").arg(m_path));
    }
    else
    {
        m_status->setText(tr("Could not read %1").arg(m_path));
    }
}

void EffectScriptEditor::reloadFromDisk()
{
    loadFile();
}

void EffectScriptEditor::save()
{
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Save"),
            tr("Could not write \"%1\". Check folder permissions.").arg(m_path));
        return;
    }
    f.write(m_edit->toPlainText().toUtf8());
    f.close();

    // Rescan so the picker + running effects pick up the edited script.
    if (m_doc != nullptr && m_doc->effectScriptRunner() != nullptr
            && m_doc->effectScriptRunner()->cache() != nullptr)
        m_doc->effectScriptRunner()->cache()->rescan();

    m_status->setText(tr("Saved + rescanned."));
}
