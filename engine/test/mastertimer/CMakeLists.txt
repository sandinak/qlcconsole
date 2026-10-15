set(module_name "mastertimer_test")

add_executable(${module_name} WIN32
    # ../../../plugins/interfaces/qlcioplugin.cpp
    ../common/resource_paths.h
    ../function/function_stub.cpp ../function/function_stub.h
    dmxsource_stub.cpp dmxsource_stub.h
    ${module_name}.cpp ${module_name}.h
)
target_include_directories(${module_name} PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../function
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
#    set(CURRUSR $ENV{USERNAME})
#else()
#    set(CURRUSR $ENV{USER})
#endif()

#string(STRIP ${CURRUSR} CURRUSR)
#string(FIND ${CURRUSR} "build" IS_BUILDBOT)
#if(IS_BUILDBOT GREATER -1)
#    target_compile_definitions(${module_name} PRIVATE
#        SKIP_TEST
#    )
#endif()
