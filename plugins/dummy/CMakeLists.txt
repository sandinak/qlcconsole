set(module_name "dummy")

set(TS_FILES
    Dummy_de_DE.ts
    Dummy_es_ES.ts
    Dummy_fi_FI.ts
    Dummy_fr_FR.ts
    Dummy_it_IT.ts
    Dummy_nl_NL.ts
    Dummy_cz_CZ.ts
    Dummy_pt_BR.ts
    Dummy_ca_ES.ts
    Dummy_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

project(${module_name})

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)

add_library(${module_name} SHARED
    ${QM_FILES}
)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    dummyconfiguration.cpp dummyconfiguration.h dummyconfiguration.ui
    dummyplugin.cpp dummyplugin.h
)
target_include_directories(${module_name} PRIVATE
    ../interfaces
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)
