project(wled)

add_subdirectory(src)
if(NOT ANDROID AND NOT IOS)
    if(NOT QLC_NO_TESTS)
        add_subdirectory(test)
    endif()
endif()
