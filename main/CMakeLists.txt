set(module_name "qlcplus")

add_executable(${module_name} WIN32
    main.cpp
)

target_include_directories(${module_name} PRIVATE
    ../engine/src
    ../ui/src
    ../ui/src/virtualconsole
    ../webaccess/src
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
    qlcplusengine
    qlcplusui
    qlcpluswebaccess
)


if(QT_VERSION_MAJOR EQUAL 5)
    qt5_add_resources(qlcplusui_resource_files ${CMAKE_SOURCE_DIR}/ui/src/qlcui.qrc)
    target_sources(${module_name} PRIVATE
        ${qlcplusui_resource_files}
    )
endif()

if(WIN32 OR APPLE)
    target_sources(${module_name} PRIVATE
        ../ui/src/debugbox.cpp ../ui/src/debugbox.h ../ui/src/debugbox.ui
    )
endif()

if(APPLE)
    # Embed Info.plist (carrying NSMicrophoneUsageDescription) into the bare dev
    # binary via a __TEXT,__info_plist section, so macOS TCC can prompt for
    # microphone access when running build/main/qlcplus directly — not only from
    # the deployed .app bundle. Without this, audio input silently gets no signal
    # on a dev binary. (The bundle uses its own Contents/Info.plist; the embedded
    # copy is harmless there.)
    target_link_options(${module_name} PRIVATE
        "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_SOURCE_DIR}/platforms/macos/Info.plist")
endif()

if(WIN32)
    target_sources(${module_name} PRIVATE
        main.rc
    )
endif()

if(qmlui OR (QT_VERSION_MAJOR GREATER 5))
    target_link_libraries(${module_name} PRIVATE
        Qt${QT_MAJOR_VERSION}::Qml
    )
else()
    target_link_libraries(${module_name} PRIVATE
        Qt${QT_MAJOR_VERSION}::Script
    )
endif()

# DEV builds on macOS: main.cpp restricts the Qt plugin search path to
# <build>/plugins (bundle-style) so a deployed .app only loads its own plugins.
# But that dir has only QLC+'s output plugins — NOT Qt's imageformats — so the
# un-deployed dev binary can decode only PNG (built-in); GIF/JPG/etc silently
# fail to load (blank image objects in the 2D monitor). Mirror Qt's
# imageformats/iconengines into <build>/plugins so the dev run finds them.
# (A real macdeployqt bundle copies these itself, so this is dev-only.)
if(APPLE AND QT_VERSION_MAJOR EQUAL 5 AND TARGET Qt5::qmake)
    get_target_property(_qlc_qmake Qt5::qmake IMPORTED_LOCATION)
    execute_process(COMMAND "${_qlc_qmake}" -query QT_INSTALL_PLUGINS
                    OUTPUT_VARIABLE _qlc_qt_plugins
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS "${_qlc_qt_plugins}/imageformats")
        add_custom_command(TARGET ${module_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qlc_qt_plugins}/imageformats"
                    "${CMAKE_BINARY_DIR}/plugins/imageformats"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qlc_qt_plugins}/iconengines"
                    "${CMAKE_BINARY_DIR}/plugins/iconengines"
            COMMENT "Mirroring Qt imageformats/iconengines into build/plugins (dev)")
    endif()
endif()

install(TARGETS ${module_name}
    DESTINATION ${INSTALLROOT}/${BINDIR}
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
