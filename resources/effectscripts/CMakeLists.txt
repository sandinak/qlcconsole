project(effectscripts)

set(SCRIPT_FILES
    breathe.js
    color-chase.js
    fan.js
    fire.js
    followspot.js
    lightning.js
    orbit.js
    rainbow.js
    ramp-chase.js
    sparkle.js
    spatial-gradient.js
    strobe.js
    wander.js
)

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${EFFECTSCRIPTDIR})

# Copy scripts into the build tree so the app finds them when run
# from the build directory on macOS (QLCFile::systemDirectory uses
# applicationDirPath()/../Resources/EffectScripts).
if (APPLE)
    set(BUILD_EFFECTSCRIPTDIR "${CMAKE_BINARY_DIR}/Resources/EffectScripts")
    file(MAKE_DIRECTORY ${BUILD_EFFECTSCRIPTDIR})
    foreach(SCRIPT_FILE IN LISTS SCRIPT_FILES)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_FILE}
             DESTINATION ${BUILD_EFFECTSCRIPTDIR})
    endforeach()
endif()
