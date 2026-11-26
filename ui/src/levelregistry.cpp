/*
  Q Light Controller Plus - qlcconsole
  levelregistry.cpp

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

#include "levelregistry.h"

namespace
{
LevelRegistry::Row makeRow(const char *key, ShowLevel::Level level, ShowLevel::Treatment treatment,
                           const QString &words, const QString &tab = QString(), bool followUp = false)
{
    LevelRegistry::Row r;
    r.key = QString::fromLatin1(key);
    r.level = level;
    r.treatment = treatment;
    r.words = words;
    r.tab = tab;
    r.followUp = followUp;
    return r;
}
}

const QList<LevelRegistry::Row> &LevelRegistry::rows()
{
    using L = ShowLevel;
    /* Phase L1 (WORKSPACE_LEVELS.md §6): the menus and the tabs, and the
       effect script editor that moves up to Advanced (Q343). Student covers
       course Levels 0-2 completely (Q340): nothing a lesson there uses is
       on this list. Stop All, GO, Back, Blackout, the Grand Master, Blind
       and the readiness footer are never on it (ShowLevel::setNeverHidden). */
    static const QList<Row> table = {
        // File
        makeRow("file.import",          L::Standard, L::Fold, tr("Importing another show or an Eos file")),
        makeRow("file.collect",         L::Standard, L::Fold, tr("Collecting a show for another computer")),
        makeRow("file.showdefinitions", L::Standard, L::Fold, tr("The definitions a show carries")),
        makeRow("file.upgrade",         L::Standard, L::Fold, tr("Upgrading a QLC+ show")),
        // View
        makeRow("view.addresstool",     L::Standard, L::Fold, tr("The DIP-switch address tool")),
        makeRow("view.dmxmonitor",      L::Standard, L::Fold, tr("Raw DMX values")),
        // Fixtures
        makeRow("fixtures.tools",       L::Standard, L::Fold, tr("Fixture definitions, import, export and remapping")),
        // Control
        makeRow("control.dumpdmx",      L::Standard, L::Fold, tr("Dumping DMX values to a function")),
        makeRow("control.liveedit",     L::Standard, L::Fold, tr("Live editing a running function")),
        makeRow("control.followmtc",    L::Standard, L::Fold, tr("Following MIDI time code")),
        makeRow("control.timeline",     L::Standard, L::Fold, tr("Timeline control")),
        makeRow("control.holdatend",    L::Standard, L::Fold, tr("Holding a show's look at its end")),
        // Run (Operate): trimming the running cue from a board, behind the PIN.
        makeRow("operate.editlive",     L::Standard, L::Grey, tr("Edit Live, trimming the running cue from a board")),
        /* Phase L2 (the course path, WORKSPACE_LEVELS.md §2): the Show Tree,
           Run, Fixtures and Lighting Studio. Nothing here is used by a
           lesson of course Levels 0-2 (levels_test, courseshow_test). */
        // Show Tree
        makeRow("showtree.workbench",   L::Standard, L::Grey, tr("The Workbench, the stock editors for scenes, effects and matrices")),
        makeRow("showtree.mib",         L::Standard, L::Grey, tr("Automatic move in black")),
        makeRow("showtree.cue.mix",     L::Standard, L::Grey, tr("Overlapping layers mixing, as a chaser does")),
        makeRow("showtree.cue.textcolour", L::Standard, L::Grey, tr("A cue's text colour")),
        makeRow("showtree.timecode",    L::Standard, L::Grey, tr("Cues at a timecode")),
        makeRow("showtree.focuscue",    L::Standard, L::Grey, tr("Focus This Cue and the venue focus cue")),
        makeRow("showtree.reuse",       L::Standard, L::Grey, tr("Converting a list to tracking, detaching to a plain chaser")),
        // Fixtures
        makeRow("fixtures.inventory",   L::Standard, L::Grey, tr("The inventory, patching and removing fixtures")),
        makeRow("fixtures.groups",      L::Standard, L::Grey, tr("Fixture group folders and the head-layout grid")),
        makeRow("fixtures.rules",       L::Standard, L::Grey, tr("Changing a fixture's definition, its plot symbol and its power circuit")),
        // Lighting Studio
        makeRow("studio.overlays",      L::Standard, L::Grey, tr("The Power, DMX, Network and Owner views")),
        makeRow("studio.structures",    L::Standard, L::Grey, tr("Stage structures, mounting, scenery and the plot's locks")),
        makeRow("studio.venues",        L::Standard, L::Grey, tr("Venues and editing the focus calls")),
        // Tabs (the Window menu names them)
        makeRow("tab.build",            L::Standard, L::Tab,  tr("Building scenes, chasers and collections"),
                QStringLiteral("Build")),
        makeRow("tab.functions",        L::Standard, L::Tab,  tr("Every function by type"),
                QStringLiteral("Functions")),
        makeRow("tab.simpledesk",       L::Standard, L::Tab,  tr("Raw channel faders"),
                QStringLiteral("Simple Desk")),
        makeRow("tab.timeline",         L::Standard, L::Tab,  tr("The timeline"),
                QStringLiteral("Shows")),
        // Advanced (Q343): the effect script editor moves up from Standard.
        makeRow("effects.script",       L::Advanced, L::Fold, tr("Writing effect scripts")),
        /* Advanced's new expert settings (Q343): the switch points only, the
           settings themselves are follow-ups (docs/project/TODO.md). */
        makeRow("settings.engine.framerate", L::Advanced, L::Grey, tr("The engine's frame rate"),
                QString(), true),
        makeRow("settings.engine.threads",   L::Advanced, L::Grey, tr("Real-time universe threads"),
                QString(), true),
        makeRow("settings.engine.priority",  L::Advanced, L::Grey, tr("Output thread priority"),
                QString(), true),
        makeRow("values.rawbesidenames",     L::Advanced, L::Grey, tr("DMX values beside palette names"),
                QString(), true),
        makeRow("help.diagnostics",          L::Advanced, L::Fold, tr("The engine's diagnostics"),
                QString(), true),
    };
    return table;
}

const LevelRegistry::Row *LevelRegistry::row(const QString &key)
{
    for (const Row &r : rows())
        if (r.key == key)
            return &r;
    return nullptr;
}
