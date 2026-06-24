project(hidprofiles)

set(PROFILE_FILES
    xbox.qxhid
)

install(FILES ${PROFILE_FILES} DESTINATION ${INSTALLROOT}/${HIDPROFILEDIR})

# Copy profiles into the build tree so the dev build on macOS finds them.
# QLCFile::systemDirectory uses applicationDirPath()/../Resources/HIDProfiles.
if(APPLE)
    set(BUILD_HIDPROFILEDIR "${CMAKE_BINARY_DIR}/Resources/HIDProfiles")
    file(MAKE_DIRECTORY ${BUILD_HIDPROFILEDIR})
    foreach(PROFILE_FILE IN LISTS PROFILE_FILES)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${PROFILE_FILE}
             DESTINATION ${BUILD_HIDPROFILEDIR})
    endforeach()
endif()
