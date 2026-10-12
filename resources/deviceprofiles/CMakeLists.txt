project(deviceprofiles)

# One subdirectory per manufacturer, same shape as resources/fixtures.
file(GLOB DEVICEPROFILE_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
list(REMOVE_ITEM DEVICEPROFILE_DIRS ".")

if(ANDROID)
    set(DEVICEPROFILE_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${DEVICEPROFILEDIR}")
    foreach(DEVICEPROFILE_DIR IN LISTS DEVICEPROFILE_DIRS)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DEVICEPROFILE_DIR}")
            file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${DEVICEPROFILE_DIR}
                 DESTINATION ${DEVICEPROFILE_ASSETS_DIR})
        endif()
    endforeach()
endif()

foreach(DEVICEPROFILE_DIR IN LISTS DEVICEPROFILE_DIRS)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DEVICEPROFILE_DIR}")
        install(DIRECTORY ${DEVICEPROFILE_DIR} DESTINATION ${INSTALLROOT}/${DEVICEPROFILEDIR})
    endif()
endforeach()

# Copy the library into the build tree so the dev build on macOS finds it.
# QLCFile::systemDirectory uses applicationDirPath()/../Resources/DeviceProfiles.
if(APPLE)
    set(BUILD_DEVICEPROFILEDIR "${CMAKE_BINARY_DIR}/Resources/DeviceProfiles")
    file(MAKE_DIRECTORY ${BUILD_DEVICEPROFILEDIR})
    foreach(DEVICEPROFILE_DIR IN LISTS DEVICEPROFILE_DIRS)
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DEVICEPROFILE_DIR}")
            file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${DEVICEPROFILE_DIR}
                 DESTINATION ${BUILD_DEVICEPROFILEDIR})
        endif()
    endforeach()
endif()
