project(hotplugmonitor)

if(NOT ANDROID AND NOT IOS)
    add_subdirectory(src)
    if(NOT QLC_NO_TESTS)
        add_subdirectory(test)
    endif()
endif()
