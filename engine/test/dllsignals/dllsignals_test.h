/*
  Q Light Controller Plus - Unit test
  dllsignals_test.h

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

#ifndef DLLSIGNALS_TEST_H
#define DLLSIGNALS_TEST_H

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QSet>

/* On Windows (MinGW) a signal named by member pointer -- &Class::signal in
   connect() or QSignalSpy -- from outside the DLL that defines Class connects
   to nothing unless Class is dllimport'ed there (QLC_ENGINE_EXPORT,
   QLC_UI_EXPORT): the address is a local import thunk, and moc finds no
   signal at it. The app runs on, a list never refreshes (TEST_MATRIX
   W1-W3). Nothing on macOS or Linux shows it, so this reads the sources. */
class DllSignals_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /** The scan itself finds what it is looking for: Doc's signals, the
     *  export macro on Doc, QLCIOPlugin's signals. */
    void theScanSeesTheTree();
    /** Every &Class::signal used outside Class's own DLL names a class
     *  exported from it. */
    void signalsAcrossADllAreExported();
    /** QLCIOPlugin's signals and a plugin's own are connected by name
     *  outside that plugin: every plugin DLL has its own QLCIOPlugin. */
    void pluginSignalsAreConnectedByName();
    /** No braced list of &Class::signal from another DLL ("for (auto s :
     *  { &Doc::a, &Doc::b })"): even an exported class's signals connect
     *  to nothing from one on Windows (W1). */
    void noBracedListsOfSignals();

private:
    struct ClassInfo
    {
        QString name;
        QString module;
        QString file;
        QString base;
        bool exported = false;
        QSet<QString> signalNames;
    };
    struct Ref
    {
        QString file;
        int line = 0;
        QString cls;            ///< as written
        QString member;
        const ClassInfo *decl = nullptr;   ///< the class declaring the signal
        bool inBracedList = false;
    };

    static QString moduleOf(const QString &relPath);
    /** True when @p module is a DLL whose classes need exporting. */
    static bool isDllModule(const QString &module);
    const ClassInfo *signalOwner(const QString &cls, const QString &member,
                                 const QString &fromModule) const;
    QString describe(const Ref &r) const;

    QString m_root;
    QStringList m_files;                        ///< relative paths
    QHash<QString, QList<ClassInfo>> m_classes; ///< by name (names repeat across apps)
    QList<Ref> m_refs;                          ///< signal refs outside the class's module
};

#endif
