project(bundles)

set(BUNDLE_FILES
    "Candlelight.json"
    "Ice Blue Chase.json"
    "Sunset Gradient.json"
    "UV Black Light.json"
)

install(FILES ${BUNDLE_FILES} DESTINATION ${INSTALLROOT}/${BUNDLEDIR})

if (APPLE)
    set(BUILD_BUNDLEDIR "${CMAKE_BINARY_DIR}/Resources/Bundles")
    file(MAKE_DIRECTORY ${BUILD_BUNDLEDIR})
    set(COPIED_BUNDLES)
    foreach(BUNDLE_FILE IN LISTS BUNDLE_FILES)
        add_custom_command(
            OUTPUT "${BUILD_BUNDLEDIR}/${BUNDLE_FILE}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/${BUNDLE_FILE}"
                    "${BUILD_BUNDLEDIR}/${BUNDLE_FILE}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${BUNDLE_FILE}"
            COMMENT "Copying bundle ${BUNDLE_FILE} to build tree")
        list(APPEND COPIED_BUNDLES "${BUILD_BUNDLEDIR}/${BUNDLE_FILE}")
    endforeach()
    add_custom_target(bundles_build ALL DEPENDS ${COPIED_BUNDLES})
endif()
