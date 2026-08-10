project(plugins)

# Same reasoning as plugins/CMakeLists.txt: AudioPluginCache::load() does a
# single-level scan of build/plugins/Audio, so a per-plugin subdirectory
# here means the dev binary never finds the sndfile (or mad) backend.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/Audio)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/Audio)

if((((NOT ANDROID AND NOT IOS))))
#    pkg_check_modules(MAD IMPORTED_TARGET mad)
#    if(${MAD_FOUND})
#        add_subdirectory(mad)
#    endif()

    pkg_check_modules(SNDFILE IMPORTED_TARGET sndfile)
    if(${SNDFILE_FOUND})
        add_subdirectory(sndfile)
    endif()
endif()
