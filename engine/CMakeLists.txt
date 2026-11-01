project(engine VERSION 1.0.0)

pkg_check_modules(FFTW3 IMPORTED_TARGET fftw3)

add_subdirectory(audio)
add_subdirectory(src)
if(NOT ANDROID AND NOT IOS)
    if(NOT QLC_NO_TESTS)
        add_subdirectory(test)
    endif()
endif()
