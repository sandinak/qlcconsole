project(effectpresets)

set(PRESET_FILES
    "Amber Chase.json"
    "Amber Fire.json"
    "Breathe.json"
    "Candlelight.json"
    "Cool Wave.json"
    "Gobo Chase.json"
    "Gobo Spin.json"
    "Iris Pop.json"
    "Iris Pulse.json"
    "Ramp Chase.json"
    "Shutter Strobe.json"
    "Stagger Strobe.json"
    "Strobe.json"
    "Temperature Chase.json"
    "UV Chase.json"
    "UV Strobe.json"
    "UV Wash.json"
    "Warm Wave.json"
    "Zoom Burst.json"
    "Zoom Wave.json"
)

install(FILES ${PRESET_FILES} DESTINATION ${INSTALLROOT}/${EFFECTPRESETDIR})

# Copy presets into the build tree so the app finds them when run from the
# build directory on macOS (mirrors effectscripts). Done at BUILD time so
# editing a preset and re-running `cmake --build` refreshes the copy.
if (APPLE)
    set(BUILD_EFFECTPRESETDIR "${CMAKE_BINARY_DIR}/Resources/EffectPresets")
    file(MAKE_DIRECTORY ${BUILD_EFFECTPRESETDIR})
    set(COPIED_PRESETS)
    foreach(PRESET_FILE IN LISTS PRESET_FILES)
        add_custom_command(
            OUTPUT "${BUILD_EFFECTPRESETDIR}/${PRESET_FILE}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/${PRESET_FILE}"
                    "${BUILD_EFFECTPRESETDIR}/${PRESET_FILE}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PRESET_FILE}"
            COMMENT "Copying effect preset ${PRESET_FILE} to build tree")
        list(APPEND COPIED_PRESETS "${BUILD_EFFECTPRESETDIR}/${PRESET_FILE}")
    endforeach()
    add_custom_target(effectpresets_build ALL DEPENDS ${COPIED_PRESETS})
endif()
