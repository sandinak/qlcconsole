set(module_name "osc")

set(TS_FILES
    OSC_de_DE.ts
    OSC_es_ES.ts
    OSC_fi_FI.ts
    OSC_fr_FR.ts
    OSC_it_IT.ts
    OSC_nl_NL.ts
    OSC_cz_CZ.ts
    OSC_pt_BR.ts
    OSC_ca_ES.ts
    OSC_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

add_library(${module_name}
    SHARED
    ${QM_FILES}
)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    configureosc.cpp configureosc.h configureosc.ui
    osccontroller.cpp osccontroller.h
    oscpacketizer.cpp oscpacketizer.h
    oscplugin.cpp oscplugin.h
)
target_include_directories(${module_name} PRIVATE
    ../interfaces
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
)

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)

if (UNIX AND NOT APPLE)
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/org.qlcconsole.QLCConsole.osc.metainfo.xml"
           DESTINATION ${METAINFODIR})
endif()

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(test)
endif()
