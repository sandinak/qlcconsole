project(effectscripts)

set(SCRIPT_FILES
    breathe.js
    color-chase.js
    drawn-path.js
    ColorAlternate.js
    ColorGradient.js
    ColorSpread.js
    fan.js
    figure8.js
    fire.js
    followspot.js
    lightning.js
    orbit.js
    rainbow.js
    ramp-chase.js
    sparkle.js
    spatial-gradient.js
    spotlight-chase.js
    strobe.js
    sweep.js
    wander.js
    wave.js
)

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${EFFECTSCRIPTDIR})

# Copy scripts into the build tree so the app finds them when run
# from the build directory on macOS (QLCFile::systemDirectory uses
# applicationDirPath()/../Resources/EffectScripts).
# Done at BUILD time (custom command keyed on each source) rather than configure
# time, so editing a script and re-running `cmake --build` refreshes the copy the
# app loads — without a reconfigure.  copy_if_different keeps it a no-op when
# nothing changed.
if (APPLE)
    set(BUILD_EFFECTSCRIPTDIR "${CMAKE_BINARY_DIR}/Resources/EffectScripts")
    file(MAKE_DIRECTORY ${BUILD_EFFECTSCRIPTDIR})
    set(COPIED_SCRIPTS)
    foreach(SCRIPT_FILE IN LISTS SCRIPT_FILES)
        add_custom_command(
            OUTPUT ${BUILD_EFFECTSCRIPTDIR}/${SCRIPT_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_FILE}
                    ${BUILD_EFFECTSCRIPTDIR}/${SCRIPT_FILE}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_FILE}
            COMMENT "Copying effect script ${SCRIPT_FILE} to build tree")
        list(APPEND COPIED_SCRIPTS ${BUILD_EFFECTSCRIPTDIR}/${SCRIPT_FILE})
    endforeach()
    add_custom_target(effectscripts_build ALL DEPENDS ${COPIED_SCRIPTS})
endif()
