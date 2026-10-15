
add_executable(enttecwing_test WIN32 MACOSX_BUNDLE
    main.cpp
    playbackwing_test.cpp playbackwing_test.h
    programwing_test.cpp programwing_test.h
    shortcutwing_test.cpp shortcutwing_test.h
    wing_test.cpp wing_test.h
)
target_include_directories(enttecwing_test PRIVATE
    ../../interfaces
    ../src
)

target_link_libraries(enttecwing_test PRIVATE
    # Remove: L../src
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    enttecwing
)

if(WIN32)
    target_compile_definitions(enttecwing_test PRIVATE
        QLC_EXPORT
    )
endif()
