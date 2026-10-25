/*
  Q Light Controller
  apputil.h

  Copyright (c) Heikki Junnila

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

#ifndef APPUTIL_H
#define APPUTIL_H

#include <QStyledItemDelegate>
#include <QColor>
#include <QIcon>
#include <QSize>

#include <QHash>
class QWidget;
class QTreeWidgetItem;
class QLCFixtureDef;
class QLCFixtureDefCache;
class Doc;
class Scene;

/** @addtogroup ui UI
 * @{
 */

namespace AppUtil
{
    /************************************************************************
     * Widget visibility helper
     ************************************************************************/
    /**
     * This helper makes sure that the given widget is within the widget's
     * parent's visible area (if the parent exists). If there is no parent,
     * it ensures that the widget is exposed on an existing screen. The widget
     * is automatically moved to a safe and visible place ONLY if necessary.
     *
     * @param widget The widget to expose
     */
    void ensureWidgetIsVisible(QWidget* widget);

    /**
     * Make sure a top-level window's geometry actually corresponds to a
     * currently-connected screen, with a usable size. Two distinct failure
     * modes this guards against, both real on a workspace that travels
     * between machines:
     *   - Saved geometry from a completely different screen layout (a
     *     different machine's monitors, or this one with a display since
     *     unplugged) can restore to coordinates off every current screen.
     *   - QWidget::restoreGeometry() can silently fail outright (a Qt
     *     version/format mismatch between the machine that saved it and
     *     this one, or plain corruption) and leave the widget at whatever
     *     tiny default size it had right after construction — present,
     *     on-screen even, but too small to see or use.
     * Finds the screen under the window's centre (falling back to the
     * primary screen if none), then clamps size to fit that screen's
     * available area AND enforces a sane minimum, then repositions so the
     * whole rect fits inside it. Safe to call unconditionally, including on
     * a window that's already fine (a no-op in that case) — e.g. every time
     * a "go to this window" menu action raises an already-detached window,
     * so that action doubles as a recovery path with no separate UI needed.
     *
     * @param window The top-level window to fix up.
     * @param minSize Floor below which the window is treated as unusably
     *                small regardless of what it claims its size is.
     */
    void ensureWindowOnScreen(QWidget *window, QSize minSize = QSize(400, 300));

    /*********************************************************************
     * Application identity
     *********************************************************************/
    /**
     * Tell the desktop which application this is and give every window its
     * icon. Call once from main(), right after the QApplication exists.
     *
     * @a name is the desktop entry's basename and the installed icon's name
     * ("qlcconsole", "qlcconsole-fixtureeditor"). On Wayland the compositor
     * finds the .desktop file -- and with it the icon in the dock, task list
     * and window switcher -- only through the app_id, which Qt otherwise
     * derives from the organisation domain ("org.qlcplus.qlcconsole") and so
     * matched no installed desktop file at all. The window icon is built from
     * the hicolor PNGs at every size, not one 96px picture scaled about.
     *
     * macOS is left alone: setting an application-wide icon there replaces
     * the bundle's .icns in the Dock.
     */
    void setApplicationIdentity(const QString &name);

    /** The application icon @a name at every size shipped in the binary. */
    QIcon applicationIcon(const QString &name);

    /*********************************************************************
     * Sane style
     *********************************************************************/
    /**
     * Attempt to get a sane style that replaces Windows' & OSX's crappy
     * sliders as well as buttons that don't obey background color setting.
     */
    QStyle* saneStyle();

    /************************************************************************
     * Scroll bars
     ************************************************************************/
    /** Settings ▸ Appearance ▸ "Always show scroll bars": macOS draws
     *  scroll bars only while scrolling (when the system says so); with this
     *  on, every scroll bar in the app is drawn all the time, wherever there
     *  is something to scroll. Off macOS (Fusion) they always are, and this
     *  changes nothing. Applies at once; App re-applies the theme after. */
    void setAlwaysShowScrollBars(bool on);
    bool alwaysShowScrollBars();
    /** The app style's name ("fusion", "macos"), under the scroll-bar
     *  style setAlwaysShowScrollBars() puts over it. */
    QString styleName();

    /*********************************************************************
     * Stylesheets
     *********************************************************************/
    /**
     * Search the requested $component in a pre-determined
     * stylesheet file on disk
     */
    QString getStyleSheet(QString component);

    /*********************************************************************
     * Digits
     *********************************************************************/
    /**
     * Helper: get the number of digits in an unsigned int (base 10)
     */
    unsigned int digits(unsigned int num);

    /*********************************************************************
     * Scene content swatch
     *********************************************************************/
    /**
     * A quick visual summary of what a scene paints — the RGB value of the
     * first Color-type palette it references, so a chaser step / show-
     * timeline block can show a swatch instead of just a name. Returns an
     * invalid QColor (check isValid()) if the scene references no Color
     * palette (e.g. a pure pan/tilt or dimmer-only scene) — callers should
     * fall back to their normal icon/no-swatch rendering rather than
     * inventing a color.
     */
    QColor sceneSwatchColor(Doc *doc, Scene *scene);

    /*********************************************************************
     * Fixture Editor launcher
     *********************************************************************/
    /**
     * Launch the standalone Fixture Editor (qlcconsole-fixtureeditor) as a
     * detached process — it has its own window/event loop, entirely separate
     * from the main app's Doc. If @p fixtureFilePath is non-empty, it's
     * opened directly (-o <path>), landing straight in that fixture's
     * definition rather than the editor's blank/new-fixture state.
     *
     * @param fixtureFilePath Absolute path to a .qxf file to open, or empty
     *                        to just launch the editor.
     * @param parentForError  Parent for the "editor not found" QMessageBox,
     *                        shown if the binary can't be located.
     * @return true if the process was started.
     */
    bool launchFixtureEditor(const QString &fixtureFilePath = QString(),
                             QWidget *parentForError = nullptr);

    /*********************************************************************
     * Fixture names
     *********************************************************************/
    /**
     * Fixture names are unique (Doc::fixtureNameTaken()). True -- and a note
     * saying which name is taken -- when fixture @p fid may NOT be renamed to
     * @p name because another fixture has it; the caller keeps the old name.
     */
    bool refuseTakenFixtureName(QWidget *parent, Doc *doc, quint32 fid, const QString &name);

    /*********************************************************************
     * Fixture readiness
     *********************************************************************/
    /**
     * Declare (or clear) the footer's rig problems in ShowStatus, each
     * item carrying what it is for the entry's fix (App sets the actions):
     * "fixtures.unpatched" -- fixtures that send no DMX (fixture ids);
     * "fixtures.output" -- fixture types whose definition declares no light
     * output (QLCPhysical::declaresOutput()), drawn at a guess (files);
     * "fixtures.clash" -- fixtures that loaded unpatched because their
     * address was taken (fixture ids); "fixtures.duplicates" -- names more
     * than one fixture has (names); "definitions.failed" -- definition files
     * that could not be read (files); "universes.nooutput" -- universes
     * fixtures are patched on with no output (universe indexes);
     * "outputs.problems" -- network outputs their plugins report in trouble
     * (QLCIOPlugin::outputProblems()), one per problem (the first universe
     * each hits).
     */
    void updateFixtureReadiness(Doc *doc);

    /** The one name for giving fixtures a universe and address, or moving
     *  them to another: "Patch…" for one, "Patch 4 Fixtures…" for several --
     *  in the Fixture Manager, on the plot and in the studio alike. */
    QString patchActionText(int count);

    /** Whether Patch is offered for @p fixtureIds: always when none of them
     *  has an address yet (giving one is harmless); moving patched fixtures
     *  is Design mode's, as moving them in a running show is not. */
    bool patchAllowed(Doc *doc, const QList<quint32> &fixtureIds);

    /** The footer's "show.load" entry from a load's @p errorLog (Doc's
     *  "<br>"-separated HTML): one item per problem, missing definitions left
     *  out (the footer lists those fixtures already). Cleared when there are
     *  none. Effect looks that cannot run are "effects.failed", kept by
     *  updateFixtureReadiness(). */
    void updateLoadProblems(const QString &errorLog);

    /** The footer's word on what the show carries (SHOW_EMBEDDING_DESIGN.md):
     *  "embedded.showonly" -- definitions and scripts only the show has, in
     *  use from it (information, Install on this computer); "embedded.differ"
     *  -- ones that differ from this computer's, the show's in use (a
     *  warning, Review…); "embedded.problems" -- copies that could not be
     *  read. Item data: each one's ShowEmbedding key. */
    void updateShowEmbeddingStatus(Doc *doc);

    /** The footer's "definitions.outofdate": definitions in the user folder
     *  older than the current version of the same fixture
     *  (FixtureDefUpdates::scan()), the ones the open show uses first --
     *  a warning when the show uses one, information otherwise. Item data:
     *  each file's path. Review… opens Definition Updates. */
    void updateDefinitionUpdatesStatus(Doc *doc);

    /** The address @p n after IPv4 @p ip (n = 0: @p ip itself), the last
     *  octet carrying into the one before it -- one each for a batch of
     *  network fixtures. Empty when @p ip is not an IPv4 address or the run
     *  would leave its /16. */
    QString nthAddress(const QString &ip, int n);

    /*********************************************************************
     * Definition sources
     *********************************************************************/
    /** The cache's definitions by "manufacturer\nmodel", unloaded ones too. */
    QHash<QString, QLCFixtureDef *> definitionsByName(QLCFixtureDefCache *cache);
    /** Show where @p def comes from in @p item's @p column: Included in grey,
     *  Local (and "replaces the included one") in blue, Downloadable in
     *  italics -- the same everywhere a definition is picked. */
    void showDefinitionSource(QTreeWidgetItem *item, int column, const QLCFixtureDef *def);

    /*********************************************************************
     * Cursors
     *********************************************************************/
    /**
     * The cursor to use for @p shape on this platform: always call setCursor()
     * through this instead of naming Qt::SizeAllCursor (or Wait/Busy) directly.
     *
     * On macOS, Qt 6.11.x builds the cursors that have no native NSCursor
     * from PNGs with an embedded colour profile (sizeallcursor.png,
     * spincursor.png, waitcursor.png). QImage::toCGImage() frees that colour
     * space before CGImageCreate() reads it (QTBUG-147602, fixed upstream in
     * qtbase a481ede673), so the first setCursor(Qt::SizeAllCursor) crashed
     * the app inside CoreGraphics. Those shapes are swapped for native ones
     * with the same meaning:
     *   - SizeAll  -> OpenHand (it can be grabbed and moved), ClosedHand when
     *                 @p dragging (it is being moved/orbited right now);
     *   - Wait     -> Arrow (macOS shows its own spinning cursor when the app
     *                 really stops; there is no native "wait" NSCursor);
     *   - Busy     -> Arrow (native only through a private AppKit selector;
     *                 if that is missing Qt falls back to waitcursor.png).
     * Everything else passes through: Forbidden, WhatsThis, Drag*, the
     * Size{Ver,Hor,BDiag,FDiag} resize arrows, Split* and the hands are
     * native NSCursors in Qt 6.11; Blank is a profile-less transparent
     * pixmap, which the bug does not touch.
     *
     * Elsewhere (Linux, Windows) @p shape is returned unchanged.
     */
    Qt::CursorShape nativeCursorShape(Qt::CursorShape shape, bool dragging = false);

    /** The macOS mapping nativeCursorShape() applies, on every platform
     *  (so it can be tested anywhere). */
    Qt::CursorShape macSafeCursorShape(Qt::CursorShape shape, bool dragging = false);
};

/*****************************************************************************
 * NoEditDelegate
 *****************************************************************************/
/**
 * Set as Item Delegate to make sure that the item will never be editable.
 */
class NoEditDelegate final : public QStyledItemDelegate
{
public:
    NoEditDelegate(QObject* parent=0): QStyledItemDelegate(parent) {}
    virtual QWidget* createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return 0;
    }
};

/*****************************************************************************
 * ComboBoxDelegate
 *****************************************************************************/
/**
 * Set as ItemDelegate to make the item editable by a combobox.
 * Value is the index of the selected string.
 */
class ComboBoxDelegate final : public QStyledItemDelegate
{
private:
    const QStringList m_strings;

public:
    /**
     * Create a combobox delegate.
     * @param strings The selectable strings in the combobox, arranged in order.
     */
    ComboBoxDelegate(const QStringList &strings, QWidget *parent = 0);

    QWidget *createEditor(QWidget *parent,
            const QStyleOptionViewItem &option,
            const QModelIndex &index) const override;

    void setEditorData(QWidget *editor,
            const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
            const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor,
            const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

/** @} */

#endif
