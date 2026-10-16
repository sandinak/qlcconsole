set(module_name "artnet")

set(TS_FILES
    ArtNet_de_DE.ts
    ArtNet_es_ES.ts
    ArtNet_fi_FI.ts
    ArtNet_fr_FR.ts
    ArtNet_it_IT.ts
    ArtNet_nl_NL.ts
    ArtNet_cz_CZ.ts
    ArtNet_pt_BR.ts
    ArtNet_ca_ES.ts
    ArtNet_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

add_library(${module_name}
    SHARED
    ${QM_FILES}
)

target_sources(${module_name} PRIVATE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../../interfaces/rdmprotocol.cpp ../../interfaces/rdmprotocol.h
    artnetcontroller.cpp artnetcontroller.h
    artnetpacketizer.cpp artnetpacketizer.h
    artnetplugin.cpp artnetplugin.h
    ../../interfaces/sendhealth.cpp ../../interfaces/sendhealth.h
    artnetnodewatch.cpp artnetnodewatch.h
    artnetnodesetup.cpp artnetnodesetup.h
    configureartnet.cpp configureartnet.h configureartnet.ui
)
target_include_directories(${module_name} PRIVATE
    ../../interfaces
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
