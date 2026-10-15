set(module_name "gpio")

set(TS_FILES
    GPIO_de_DE.ts
    GPIO_es_ES.ts
    GPIO_fi_FI.ts
    GPIO_fr_FR.ts
    GPIO_it_IT.ts
    GPIO_nl_NL.ts
    GPIO_cz_CZ.ts
    GPIO_pt_BR.ts
    GPIO_ca_ES.ts
    GPIO_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

project(${module_name})

find_package(Qt6 REQUIRED COMPONENTS Gui Widgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD IMPORTED_TARGET libgpiodcxx)

if (LIBGPIOD_FOUND)
    include_directories(${module_name} PRIVATE ${LIBGPIOD_INCLUDE_DIRS})
    link_directories(${LIBGPIOD_LIBRARY_DIRS})
    link_libraries(${LIBGPIOD_LIBRARIES})
else()
    message("ERROR: libgpiod not found!")
endif()

add_library(${module_name} SHARED
    ${QM_FILES}
)
target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    gpioconfiguration.cpp gpioconfiguration.h gpioconfiguration.ui
    gpioplugin.cpp gpioplugin.h
    gpioreaderthread.cpp gpioreaderthread.h
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
