project(common)

find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(common WIN32 MACOSX_BUNDLE
    inputdevice.cpp inputdevice.h
    iodevice.cpp iodevice.h
    ioenumerator.cpp ioenumerator.h
    main.cpp
    outputdevice.cpp outputdevice.h
)

target_link_libraries(common PRIVATE
    Qt6::Core
    Qt6::Gui
)

if(WIN32)
    target_sources(common PRIVATE
        ../win32/peperoni/usbdmx-dynamic.cpp ../win32/peperoni/usbdmx-dynamic.h
        win32ioenumerator.cpp win32ioenumerator.h
        win32peperonidevice.cpp win32peperonidevice.h
    )

    set_target_properties(common PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
    target_include_directories(common PRIVATE
        ../win32/peperoni
    )
endif()

if(APPLE)
    set_target_properties(common PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

if(UNIX)
    target_sources(common PRIVATE
        unixioenumerator.cpp unixioenumerator.h
        unixpeperonidevice.cpp unixpeperonidevice.h
    )
endif()
