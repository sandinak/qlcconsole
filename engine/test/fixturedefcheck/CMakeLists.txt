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
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

if ("$ENV{TRAVIS}" STREQUAL "true")
    target_compile_definitions(${module_name} PRIVATE
        SKIP_TEST
    )
endif()

#if(WIN32)

#string(STRIP ${CURRUSR} CURRUSR)

target_compile_definitions(${module_name} PRIVATE
    FIXTUREDEFCHECK_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)
