project(hidprofiles)

set(PROFILE_FILES
    xbox.qxhid
)
# A PlayStation pad is read into the Xbox layout on a Mac only (board Q139).
if(APPLE)
    list(APPEND PROFILE_FILES playstation.qxhid)
endif()

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
