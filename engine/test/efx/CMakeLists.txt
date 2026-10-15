add_executable(efx_test WIN32
    ../common/resource_paths.h
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    efx_test.cpp efx_test.h
)
target_include_directories(efx_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(efx_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

if(APPLE)
    set_target_properties(efx_test PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()
