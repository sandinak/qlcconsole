# AI Assist (docs/design/AI_ASSIST.md): settings, provider adapters, the
# assistant and its Design/Operate gating, the command-line hook and the
# settings page. Static (position independent) so the app bundle gains no
# library to deploy; linked privately into qlcplusui.
set(module_name "qlcconsoleai")

add_library(${module_name} STATIC
    aisettings.cpp aisettings.h
    aiprovider.cpp aiprovider.h
    aiassist.cpp aiassist.h
    aicommandline.cpp aicommandline.h
    aisettingspage.cpp aisettingspage.h
)
set_target_properties(${module_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${module_name} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../engine/src
    ../../engine/audio/src
    ../../plugins/interfaces
)

target_link_libraries(${module_name} PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
    qlcplusengine
)

if(APPLE)
    target_link_libraries(${module_name} PUBLIC "-framework Security" "-framework CoreFoundation")
endif()
