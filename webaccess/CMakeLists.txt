project(webaccess)

add_subdirectory(src)
add_subdirectory(res)
if(NOT qmlui AND NOT ANDROID AND NOT IOS)
    if(NOT QLC_NO_TESTS)
        add_subdirectory(test)
    endif()
endif()
