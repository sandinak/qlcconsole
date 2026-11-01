project(ui)

add_subdirectory(src)
if(NOT QLC_NO_TESTS)
    add_subdirectory(test)
endif()
