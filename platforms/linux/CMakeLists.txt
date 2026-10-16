project(icons)

set(desktop_FILES qlcconsole.desktop)
if(NOT qmlui)
    list(APPEND desktop_FILES qlcconsole-fixtureeditor.desktop)
endif()
install(FILES ${desktop_FILES} DESTINATION ${INSTALLROOT}/share/applications/)

set(icons_SRCS ../../resources/icons/png/qlcconsole.png)
if(NOT qmlui)
    list(APPEND icons_SRCS ../../resources/icons/png/qlcconsole-fixtureeditor.png)
endif()
install(FILES ${icons_SRCS} DESTINATION ${INSTALLROOT}/share/pixmaps/)

# The freedesktop icon theme, which is where desktops look up Icon= first:
# every rendered size (resources/icons/hicolor/render.sh) plus the SVG, so any
# other size is drawn sharp rather than scaled up from the 96px pixmap above.
set(icon_NAMES qlcconsole)
if(NOT qmlui)
    list(APPEND icon_NAMES qlcconsole-fixtureeditor)
endif()
foreach(icon_SIZE 16 22 24 32 48 64 128 256)
    foreach(icon_NAME ${icon_NAMES})
        install(FILES ../../resources/icons/hicolor/${icon_SIZE}x${icon_SIZE}/apps/${icon_NAME}.png
                DESTINATION ${INSTALLROOT}/share/icons/hicolor/${icon_SIZE}x${icon_SIZE}/apps)
    endforeach()
endforeach()
install(FILES ../../resources/icons/svg/qlcconsole.svg
        DESTINATION ${INSTALLROOT}/share/icons/hicolor/scalable/apps)
if(NOT qmlui)
    install(FILES ../../resources/icons/svg/fixture-editor.svg
            DESTINATION ${INSTALLROOT}/share/icons/hicolor/scalable/apps
            RENAME qlcconsole-fixtureeditor.svg)
endif()

# Refresh the theme cache on a direct install, so the new icons show without a
# relogin. Packaging (DESTDIR set, or an AppImage staging tree) is left alone:
# the package manager's own triggers do it on the target system.
if(NOT appimage)
    install(CODE "
        if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
            find_program(GTK_UPDATE_ICON_CACHE NAMES gtk-update-icon-cache gtk4-update-icon-cache)
            if(GTK_UPDATE_ICON_CACHE AND EXISTS \"${INSTALLROOT}/share/icons/hicolor/index.theme\")
                execute_process(COMMAND \${GTK_UPDATE_ICON_CACHE} -q -t -f \"${INSTALLROOT}/share/icons/hicolor\")
            endif()
        endif()
    ")
endif()

install(FILES qlcconsole.xml DESTINATION ${INSTALLROOT}/share/mime/packages)

set(appdata_FILES org.qlcconsole.QLCConsole.appdata.xml)
if(NOT qmlui)
    list(APPEND appdata_FILES org.qlcconsole.QLCConsoleFixtureEditor.appdata.xml)
endif()
install(FILES ${appdata_FILES} DESTINATION ${METAINFODIR})

if(NOT qmlui)
    file(GLOB manpages_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.1")
    install(FILES ${manpages_FILES} DESTINATION ${INSTALLROOT}/${MANDIR})
endif()

# install(FILES ../Sample.qxw DESTINATION ${INSTALLROOT}/${DATADIR})
# install(FILES qlcconsole-start.sh DESTINATION ${INSTALLROOT}/sbin)

if(appimage)
    if (QT_DIR STREQUAL "/usr/lib/x86_64-linux-gnu/cmake/Qt5")
        set(QT_LIBS_PATH "/usr/lib/x86_64-linux-gnu")
        set(QT_PLUGINS_PATH "/usr/lib/x86_64-linux-gnu/qt5/plugins")
        set(QT_QML_PATH "/usr/lib/x86_64-linux-gnu/qt5/qml")
    elseif(QT_DIR STREQUAL "/usr/lib/x86_64-linux-gnu/cmake/Qt6")
        set(QT_LIBS_PATH "/usr/lib/x86_64-linux-gnu")
        set(QT_PLUGINS_PATH "/usr/lib/x86_64-linux-gnu/qt6/plugins")
        set(QT_QML_PATH "/usr/lib/x86_64-linux-gnu/qt6/qml")
    else()
        get_filename_component(QT_LIBS_PATH ${QT_DIR}/../../../lib ABSOLUTE)
        get_filename_component(QT_PLUGINS_PATH ${QT_LIBS_PATH}/../plugins ABSOLUTE)
        get_filename_component(QT_QML_PATH ${QT_LIBS_PATH}/../qml ABSOLUTE)
    endif()
    message("QT_LIBS_PATH ${QT_LIBS_PATH}")
    message("QT_PLUGINS_PATH ${QT_PLUGINS_PATH}")
    message("QT_QML_PATH ${QT_QML_PATH}")
    # Qt dependencies
    file(GLOB qtdeps_FILES "${QT_LIBS_PATH}/libicu*")
    install(
        FILES ${qtdeps_FILES}
        DESTINATION ${INSTALLROOT}/${LIBSDIR}
    )

    # Qt Libraries
    set(qtlibs_FILES
        ${QT_LIBS_PATH}/libQt6Core.so
        ${QT_LIBS_PATH}/libQt6Network.so
        ${QT_LIBS_PATH}/libQt6Gui.so
        ${QT_LIBS_PATH}/libQt6Qml.so
        ${QT_LIBS_PATH}/libQt6Svg.so
        ${QT_LIBS_PATH}/libQt6Widgets.so
        ${QT_LIBS_PATH}/libQt6OpenGL.so
        ${QT_LIBS_PATH}/libQt6Multimedia.so
        ${QT_LIBS_PATH}/libQt6MultimediaWidgets.so
        ${QT_LIBS_PATH}/libQt6SerialPort.so
        ${QT_LIBS_PATH}/libQt6XcbQpa.so
        ${QT_LIBS_PATH}/libQt6DBus.so
        ${QT_LIBS_PATH}/libQt6WebSockets.so)

    if(qmlui)
        list(APPEND qtlibs_FILES
            ${QT_LIBS_PATH}/libQt6LabsFolderListModel.so
            ${QT_LIBS_PATH}/libQt6MultimediaQuick.so
            ${QT_LIBS_PATH}/libQt6MultimediaGstTools.so
            ${QT_LIBS_PATH}/libQt6QmlModels.so
            ${QT_LIBS_PATH}/libQt6QmlMeta.so
            ${QT_LIBS_PATH}/libQt6QmlWorkerScript.so
            ${QT_LIBS_PATH}/libQt6Quick.so
            ${QT_LIBS_PATH}/libQt6QuickLayouts.so
            ${QT_LIBS_PATH}/libQt6QuickControls2.so
            ${QT_LIBS_PATH}/libQt6QuickControls2Impl.so
            ${QT_LIBS_PATH}/libQt6QuickControls2Basic.so
            ${QT_LIBS_PATH}/libQt6QuickControls2BasicStyleImpl.so
            ${QT_LIBS_PATH}/libQt6QuickControls2Fusion.so
            ${QT_LIBS_PATH}/libQt6QuickControls2FusionStyleImpl.so
            ${QT_LIBS_PATH}/libQt6QuickTemplates2.so
            ${QT_LIBS_PATH}/libQt6QuickDialogs2.so
            ${QT_LIBS_PATH}/libQt6QuickDialogs2QuickImpl.so
            ${QT_LIBS_PATH}/libQt6QuickDialogs2Utils.so
            ${QT_LIBS_PATH}/libQt63DCore.so
            ${QT_LIBS_PATH}/libQt63DExtras.so
            ${QT_LIBS_PATH}/libQt63DInput.so
            ${QT_LIBS_PATH}/libQt63DLogic.so
            ${QT_LIBS_PATH}/libQt63DAnimation.so
            ${QT_LIBS_PATH}/libQt63DQuick.so
            ${QT_LIBS_PATH}/libQt63DQuickExtras.so
            ${QT_LIBS_PATH}/libQt63DQuickInput.so
            ${QT_LIBS_PATH}/libQt63DQuickRender.so
            ${QT_LIBS_PATH}/libQt63DQuickScene3D.so
            ${QT_LIBS_PATH}/libQt63DRender.so
            ${QT_LIBS_PATH}/libQt6Concurrent.so
            ${QT_LIBS_PATH}/libQt6Gamepad.so
            ${QT_LIBS_PATH}/libQt6PrintSupport.so)

        list(APPEND qtlibs_FILES
            ${QT_LIBS_PATH}/libavcodec.so
            ${QT_LIBS_PATH}/libavformat.so
            ${QT_LIBS_PATH}/libavutil.so
            ${QT_LIBS_PATH}/libswresample.so
            ${QT_LIBS_PATH}/libswscale.so
            ${QT_LIBS_PATH}/libQt6FFmpegStub-crypto.so.3
            ${QT_LIBS_PATH}/libQt6FFmpegStub-ssl.so.3
            ${QT_LIBS_PATH}/libQt6FFmpegStub-va-drm.so.2
            ${QT_LIBS_PATH}/libQt6FFmpegStub-va-x11.so.2
            ${QT_LIBS_PATH}/libQt6FFmpegStub-va.so.2)

    endif()
    
    set(_resolved_qtlibs_FILES "")
    foreach (_file ${qtlibs_FILES})
        file(GLOB _files "${_file}*")
        list(FILTER _files EXCLUDE REGEX ".debug")
        list (APPEND _resolved_qtlibs_FILES "${_files}")
    endforeach()
    install(FILES ${_resolved_qtlibs_FILES} DESTINATION ${INSTALLROOT}/${LIBSDIR})

    # Qt plugins
    set(PLUGINS_DESTINATION ${INSTALLROOT}/plugins)

    install(
        FILES 
            ${QT_PLUGINS_PATH}/platforms/libqlinuxfb.so
            ${QT_PLUGINS_PATH}/platforms/libqxcb.so
            ${QT_PLUGINS_PATH}/platforms/libqminimal.so
        DESTINATION ${PLUGINS_DESTINATION}/platforms
    )

    install(
        FILES ${QT_PLUGINS_PATH}/xcbglintegrations/libqxcb-glx-integration.so
        DESTINATION ${PLUGINS_DESTINATION}/xcbglintegrations
    )

    install(
        FILES ${QT_PLUGINS_PATH}/multimedia/libffmpegmediaplugin.so
        DESTINATION ${PLUGINS_DESTINATION}/multimedia
    )
    get_filename_component(LIBAVFORMAT_REAL "${QT_LIBS_PATH}/libavformat.so" REALPATH)
    install(
        FILES
            ${QT_LIBS_PATH}/libavformat.so
            ${QT_LIBS_PATH}/libavformat.so.61
            ${LIBAVFORMAT_REAL}
        DESTINATION ${INSTALLROOT}/${LIBSDIR}
    )

    install(
        FILES
            ${QT_PLUGINS_PATH}/imageformats/libqsvg.so
        DESTINATION ${PLUGINS_DESTINATION}/imageformats
    )

    if(qmlui)
        install(
            FILES
                ${QT_PLUGINS_PATH}/printsupport/libcupsprintersupport.so
            DESTINATION ${PLUGINS_DESTINATION}/printsupport
        )

        install(
            FILES
                ${QT_PLUGINS_PATH}/sceneparsers/libassimpsceneimport.so
            OPTIONAL DESTINATION ${PLUGINS_DESTINATION}/sceneparsers
        )

        install(
            FILES
                ${QT_PLUGINS_PATH}/geometryloaders/libdefaultgeometryloader.so
            DESTINATION ${PLUGINS_DESTINATION}/geometryloaders
        )

        if(QT_VERSION VERSION_GREATER_EQUAL 5.15.0)
            install(FILES ${QT_PLUGINS_PATH}/renderers/libopenglrenderer.so DESTINATION ${PLUGINS_DESTINATION}/renderers)
        endif()

        set(qmldeps_files
            ${QT_QML_PATH}/Qt
            ${QT_QML_PATH}/QtQml
            ${QT_QML_PATH}/QtQuick
            ${QT_QML_PATH}/Qt3D
            ${QT_QML_PATH}/QtMultimedia
        )
        install(DIRECTORY ${qmldeps_files} DESTINATION ${INSTALLROOT}/bin)

    endif()

endif()
