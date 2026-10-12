set(module_name "fixturedefcheck_test")

add_executable(${module_name} WIN32
    ../common/resource_paths.h
    ${module_name}.cpp ${module_name}.h
)
target_include_directories(${module_name} PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

if ("$ENV{TRAVIS}" STREQUAL "true")
    target_compile_definitions(${module_name} PRIVATE
        SKIP_TEST
    )
endif()

#if(WIN32)

#string(STRIP ${CURRUSR} CURRUSR)

# Consider using qt_generate_deploy_app_script() for app deployment if

target_compile_definitions(${module_name} PRIVATE
    FIXTUREDEFCHECK_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)
