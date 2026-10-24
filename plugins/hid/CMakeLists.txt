set(module_name "hidplugin")

set(TS_FILES
    HID_fi_FI.ts
    HID_de_DE.ts
    HID_es_ES.ts
    HID_fr_FR.ts
    HID_it_IT.ts
    HID_nl_NL.ts
    HID_cz_CZ.ts
    HID_pt_BR.ts
    HID_ca_ES.ts
    HID_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

add_library(${module_name} SHARED
    ${QM_FILES}
)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    configurehid.cpp configurehid.h configurehid.ui
    gamepadlayout.cpp gamepadlayout.h
    hiddevice.cpp hiddevice.h
    hiddmxdevice.cpp hiddmxdevice.h
    hidjsdevice.cpp hidjsdevice.h
    hidprofile.cpp hidprofile.h
    hidprofileeditor.cpp hidprofileeditor.h
    ${module_name}.cpp ${module_name}.h
    ../../engine/src/qlcfile.cpp ../../engine/src/qlcfile.h
)
target_include_directories(${module_name} PRIVATE
    ../interfaces
    ../../engine/src
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(UNIX AND NOT APPLE)
    target_sources(${module_name} PRIVATE
        linux/hidapi.cpp
        linux/hidlinuxjoystick.cpp linux/hidlinuxjoystick.h
    )

    target_include_directories(${module_name} PRIVATE
        linux
    )
endif()

if(APPLE)
    target_sources(${module_name} PRIVATE
        macx/hidapi.cpp
        macx/hidosxjoystick.cpp macx/hidosxjoystick.h
        macx/hidgamecontroller.mm macx/hidgamecontroller.h
        macx/hidosxhotplug.mm macx/hidosxhotplug.h
    )
    set_source_files_properties(macx/hidgamecontroller.mm macx/hidosxhotplug.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

    target_include_directories(${module_name} PRIVATE
        macx
    )

    # GameController: Xbox pads by name, and their input while another app
    # is in front. AppKit: the wake notifications.
    target_link_libraries(${module_name} PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework Foundation"
        "-framework AppKit"
        "-framework GameController"
        "-framework CoreHaptics"
    )
endif()

if(WIN32)
    target_sources(${module_name} PRIVATE
        win32/hidapi.cpp
        win32/hidwindowsjoystick.cpp win32/hidwindowsjoystick.h
    )

    target_link_libraries(${module_name} PRIVATE
        hid
        setupapi
        winmm
    )
endif()

if (UNIX AND NOT APPLE)
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/linux/z65-fx5-hid.rules"
           DESTINATION ${UDEVRULESDIR})
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/linux/org.qlcconsole.QLCConsole.hid.metainfo.xml"
           DESTINATION ${METAINFODIR})
endif()

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)
