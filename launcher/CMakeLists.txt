set(module_name "qlcconsole-launcher")

set(TS_FILES
    launcher_fi_FI.ts
    launcher_de_DE.ts
    launcher_fr_FR.ts
    launcher_es_ES.ts
    launcher_it_IT.ts
    launcher_nl_NL.ts
    launcher_cz_CZ.ts
    launcher_pt_BR.ts
    launcher_ca_ES.ts
    launcher_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

add_executable(${module_name}
    launcher.cpp launcher.h
    main.cpp
    ${QM_FILES}
)

target_include_directories(${module_name} PRIVATE
    ../engine/src
    ../plugins/interfaces
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
)

# Resources:
set_source_files_properties("../resources/icons/png/qlcconsole-fixtureeditor.png"
    PROPERTIES QT_RESOURCE_ALIAS "qlcconsole-fixtureeditor.png"
)
set_source_files_properties("../resources/icons/png/qlcconsole.png"
    PROPERTIES QT_RESOURCE_ALIAS "qlcconsole.png"
)
set(launcher_resource_files
    "../resources/icons/png/qlcconsole-fixtureeditor.png"
    "../resources/icons/png/qlcconsole.png"
)

qt_add_resources(${module_name} "launcher"
PREFIX
    "/"
BASE
    "."
FILES
    ${launcher_resource_files}
)

install(TARGETS ${module_name}
    DESTINATION ${INSTALLROOT}/${BINDIR}
)
