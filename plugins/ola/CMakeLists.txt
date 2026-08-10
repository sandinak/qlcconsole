set(module_name "olaio")

set(TS_FILES
    OLA_fi_FI.ts
    OLA_de_DE.ts
    OLA_es_ES.ts
    OLA_fr_FR.ts
    OLA_it_IT.ts
    OLA_nl_NL.ts
    OLA_cz_CZ.ts
    OLA_pt_BR.ts
    OLA_ca_ES.ts
    OLA_ja_JP.ts
)

if(QT_VERSION_MAJOR GREATER 5)
    qt_add_translation(QM_FILES ${TS_FILES})
else()
    qt5_add_translation(QM_FILES ${TS_FILES})
endif()

add_library(${module_name}
    SHARED
    ${QM_FILES}
)

# OLA library uses deprecated C++ features removed in C++17 (auto_ptr, binary_function, etc.)
# Add compatibility defines to re-enable these features in libc++.
target_compile_definitions(${module_name} PRIVATE
    _LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR
    _LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION
)

target_sources(${module_name} PRIVATE
    ../interfaces/qlcioplugin.cpp ../interfaces/qlcioplugin.h
    configureolaio.cpp configureolaio.h configureolaio.ui
    ${module_name}.cpp ${module_name}.h
    olaoutthread.cpp olaoutthread.h
    qlclogdestination.cpp qlclogdestination.h
)
target_include_directories(${module_name} PRIVATE
    ../interfaces
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
)

if(APPLE)
    set_target_properties(${module_name} PROPERTIES
        MACOSX_BUNDLE FALSE
    )
    if(${LIBOLA_FOUND} AND ${LIBOLASERVER_FOUND})
        target_include_directories(${module_name} PRIVATE
            ${LIBOLA_INCLUDE_DIRS}
        )

        target_link_directories(${module_name} PRIVATE
            ${LIBOLA_LIBRARY_DIRS}
        )

        target_link_libraries(${module_name} PRIVATE
            ${LIBOLA_LIBRARIES}
            ${LIBOLASERVER_LIBRARIES}
        )
    else()
        target_include_directories(${module_name} PRIVATE
            /opt/local/include
        )

        target_link_libraries(${module_name} PRIVATE
            # Remove: L/opt/local/lib
            ola
            olacommon
            olaserver
        )
    endif()
endif()

if(NOT APPLE)
    target_link_libraries(${module_name} PRIVATE
        # Remove: L/usr/local/lib
        ola
        olacommon
        olaserver
    )
endif()

if (UNIX AND NOT APPLE)
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/org.qlcconsole.QLCConsole.ola.metainfo.xml"
           DESTINATION ${METAINFODIR})
endif()

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)
