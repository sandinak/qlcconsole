project(resources)

add_subdirectory(deviceprofiles)
add_subdirectory(fixtures)
add_subdirectory(gobos)
add_subdirectory(inputprofiles)
add_subdirectory(miditemplates)
add_subdirectory(modifierstemplates)
add_subdirectory(rgbscripts)
add_subdirectory(effectscripts)
add_subdirectory(effectpresets)
add_subdirectory(bundles)
add_subdirectory(hidprofiles)
add_subdirectory(samples)
if(qmlui)
    add_subdirectory(colorfilters)
    add_subdirectory(meshes)
endif()

# A macOS dev build runs from build/main and finds its resources next to it --
# build/main/../Resources/<Kind>, which is each of these binary dirs (the file
# system ignores case). Copies left there went stale: a new fixture never
# showed in Add Fixture, effect scripts ran a month-old version. So link every
# file and folder in each to its source instead; edits show up as they are made.
if(APPLE AND NOT IOS)
    foreach(DEV_KIND deviceprofiles fixtures gobos inputprofiles miditemplates modifierstemplates
                     rgbscripts effectscripts effectpresets bundles hidprofiles samples)
        file(GLOB DEV_ENTRIES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${DEV_KIND}
             ${CMAKE_CURRENT_SOURCE_DIR}/${DEV_KIND}/*)
        foreach(DEV_ENTRY ${DEV_ENTRIES})
            if(DEV_ENTRY STREQUAL "CMakeLists.txt" OR DEV_ENTRY STREQUAL "Makefile")
                continue()
            endif()
            set(DEV_LINK "${CMAKE_CURRENT_BINARY_DIR}/${DEV_KIND}/${DEV_ENTRY}")
            if(NOT IS_SYMLINK "${DEV_LINK}")
                file(REMOVE_RECURSE "${DEV_LINK}")
                file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/${DEV_KIND}/${DEV_ENTRY}"
                     "${DEV_LINK}" SYMBOLIC)
            endif()
        endforeach()
    endforeach()
endif()
