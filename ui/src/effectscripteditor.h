/*
  Q Light Controller Plus
  effectscripteditor.h

  Fork-owned minimal in-app editor for effect-script (.js) files. A monospace
  text area with Save (writes the file + rescans the effect-script cache so the
  change is picked up live) and Reload. Used by the Look Editor's "New/Edit
  effect script…" flow when the app preference selects the in-app editor (vs the
  OS default editor). Deliberately small — not a full IDE.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTSCRIPTEDITOR_H
#define EFFECTSCRIPTEDITOR_H

#include <QDialog>

class QPlainTextEdit;
class QLabel;
class Doc;

class EffectScriptEditor final : public QDialog
{
    Q_OBJECT

public:
    EffectScriptEditor(const QString &filePath, Doc *doc, QWidget *parent = nullptr);

private slots:
    void save();
    void reloadFromDisk();

private:
    void loadFile();

    Doc     *m_doc;
    QString  m_path;
    QPlainTextEdit *m_edit;
    QLabel  *m_status;
};

#endif // EFFECTSCRIPTEDITOR_H
