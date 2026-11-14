/*
  Q Light Controller Plus
  functioneditorguard.cpp

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

#include <QWidget>
#include <QTimer>

#include "functioneditorguard.h"
#include "showtreecontroller.h"
#include "function.h"
#include "doc.h"

void FunctionEditorGuard::watch(QWidget *editor, Doc *doc, quint32 fid)
{
    if (editor == nullptr || doc == nullptr || fid == Function::invalidId())
        return;
    QObject::connect(doc, &Doc::functionRemoved, editor, [editor, doc, fid](quint32 id) {
        if (id == fid)
            cutOff(editor, doc);
    });
}

void FunctionEditorGuard::cutOff(QWidget *editor, Doc *doc)
{
    if (editor == nullptr || doc == nullptr)
        return;
    QList<QObject *> all = editor->findChildren<QObject *>();
    all.prepend(editor);
    ShowTreeController *tree = doc->showTree();
    for (QObject *o : all)
    {
        QObject::disconnect(doc, nullptr, o, nullptr);
        if (tree != nullptr)
            QObject::disconnect(tree, nullptr, o, nullptr);
        if (QTimer *t = qobject_cast<QTimer *>(o))
            t->stop();
    }
    editor->setEnabled(false);
    editor->hide();
}

void FunctionEditorGuard::retire(QWidget *editor, Doc *doc)
{
    if (editor == nullptr)
        return;
    cutOff(editor, doc);
    editor->deleteLater();
}
