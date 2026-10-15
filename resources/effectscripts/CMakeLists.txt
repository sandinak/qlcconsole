project(effectscripts)

# Every effect script here ships. Globbed, not listed: a hand-kept list
# once left 89 of the WLED ports out of the app (the tests read this
# directory, so they never noticed). CONFIGURE_DEPENDS re-globs on each
# build, so a new script is picked up without a reconfigure.
file(GLOB SCRIPT_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/*.js)
list(SORT SCRIPT_FILES)

# Helpers several scripts share (effect.requires): not effects themselves.
set(SCRIPT_LIBS
    lib/wled.js
)

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${EFFECTSCRIPTDIR})
install(FILES ${SCRIPT_LIBS} DESTINATION ${INSTALLROOT}/${EFFECTSCRIPTDIR}/lib)

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
    foreach(SCRIPT_FILE IN LISTS SCRIPT_FILES SCRIPT_LIBS)
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
