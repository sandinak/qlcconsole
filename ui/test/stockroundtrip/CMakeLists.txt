# 0.3.1 (k) 7: stock QLC+ 4.14.4 reads a .qxw saved by qlcconsole.
#
# The reader is stock QLC+'s own engine, built from a pinned copy of the
# upstream source -- `git archive QLC+_4.14.4 engine/src engine/audio/src
# plugins/interfaces` from https://github.com/mcallegari/qlcplus, commit
# 027e6d4fdb0680ce6fc6f1fd2c6d0eebe1d08c94 -- unpacked into the build dir
# and compiled as a static library for this test only, untouched (no
# warnings: it is not our code). hotplugmonitor is the same source in both
# trees, so ours is linked. Its sources compile once per build dir, then
# ccache has them.
set(STOCK_ARCHIVE ${CMAKE_CURRENT_SOURCE_DIR}/stock/qlcplus-4.14.4-engine.tar.xz)
set(STOCK_SHA256 b0084a99a420b8e6cdad59591e273e5bfcc463a358adfdac18def5f047ee97c1)
set(STOCK_ROOT ${CMAKE_CURRENT_BINARY_DIR}/stock/qlcplus-4.14.4)

if(WIN32)
    return()    # the gate and CI run it on macOS and Linux
endif()

file(SHA256 ${STOCK_ARCHIVE} STOCK_GOT)
if(NOT STOCK_GOT STREQUAL STOCK_SHA256)
    message(FATAL_ERROR "ui/test/stockroundtrip: ${STOCK_ARCHIVE} is not the pinned QLC+ 4.14.4 source "
                        "(sha256 ${STOCK_GOT}, want ${STOCK_SHA256})")
endif()
set(STOCK_STAMP ${CMAKE_CURRENT_BINARY_DIR}/stock/.extracted-${STOCK_SHA256})
if(NOT EXISTS ${STOCK_STAMP})
    file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/stock)
    file(ARCHIVE_EXTRACT INPUT ${STOCK_ARCHIVE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/stock)
    file(TOUCH ${STOCK_STAMP})
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${STOCK_ARCHIVE})

# QLC+'s qlcconfig.h is made by its own CMake, which this does not run: the
# names it defines, pointing at nothing the test reads (it loads fixtures
# and scripts from the resources dir it is given).
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/stock/config/qlcconfig.h
"#ifndef QLCCONFIG_H
#define QLCCONFIG_H
#define APPNAME \"Q Light Controller Plus\"
#define FXEDNAME \"Fixture Definition Editor\"
#define APPVERSION \"4.14.4\"
#define DOCSDIR \"Documents\"
#define INPUTPROFILEDIR \"InputProfiles\"
#define USERQLCPLUSDIR \".qlcplus-stockroundtrip\"
#define USERINPUTPROFILEDIR \".qlcplus-stockroundtrip/InputProfiles\"
#define MIDITEMPLATEDIR \"MidiTemplates\"
#define USERMIDITEMPLATEDIR \".qlcplus-stockroundtrip/MidiTemplates\"
#define MODIFIERSTEMPLATEDIR \"ModifiersTemplates\"
#define USERMODIFIERSTEMPLATEDIR \".qlcplus-stockroundtrip/ModifiersTemplates\"
#define FIXTUREDIR \"Fixtures\"
#define USERFIXTUREDIR \".qlcplus-stockroundtrip/Fixtures\"
#define PLUGINDIR \"Plugins\"
#define AUDIOPLUGINDIR \"AudioPlugins\"
#define TRANSLATIONDIR \"Translations\"
#define RGBSCRIPTDIR \"RGBScripts\"
#define USERRGBSCRIPTDIR \".qlcplus-stockroundtrip/RGBScripts\"
#define GOBODIR \"Gobos\"
#define WEBFILESDIR \"Web\"
#endif
")

set(S ${STOCK_ROOT}/engine/src)
set(A ${STOCK_ROOT}/engine/audio/src)
add_library(stockqlcengine STATIC
    ${STOCK_ROOT}/plugins/interfaces/qlcioplugin.cpp
    ${A}/audio.cpp ${A}/audiocapture.cpp ${A}/audiodecoder.cpp ${A}/audioparameters.cpp
    ${A}/audioplugincache.cpp ${A}/audiorenderer.cpp ${A}/beattracker.cpp
    ${A}/audiocapture_qt6.cpp ${A}/audiorenderer_qt6.cpp
    ${S}/avolitesd4parser.cpp ${S}/bus.cpp ${S}/channelmodifier.cpp ${S}/channelsgroup.cpp
    ${S}/chaser.cpp ${S}/chaserrunner.cpp ${S}/chaserstep.cpp ${S}/collection.cpp ${S}/cue.cpp
    ${S}/cuestack.cpp ${S}/dmxdumpfactoryproperties.cpp ${S}/doc.cpp ${S}/efx.cpp
    ${S}/efxfixture.cpp ${S}/fadechannel.cpp ${S}/fixture.cpp ${S}/fixturegroup.cpp
    ${S}/function.cpp ${S}/genericdmxsource.cpp ${S}/genericfader.cpp ${S}/gradient.cpp
    ${S}/grandmaster.cpp ${S}/grouphead.cpp ${S}/inputoutputmap.cpp ${S}/inputpatch.cpp
    ${S}/ioplugincache.cpp ${S}/keypadparser.cpp ${S}/mastertimer.cpp ${S}/mastertimer-unix.cpp
    ${S}/monitorproperties.cpp ${S}/outputpatch.cpp ${S}/qlccapability.cpp ${S}/qlcchannel.cpp
    ${S}/qlcclipboard.cpp ${S}/qlcfile.cpp ${S}/qlcfixturedef.cpp ${S}/qlcfixturedefcache.cpp
    ${S}/qlcfixturehead.cpp ${S}/qlcfixturemode.cpp ${S}/qlci18n.cpp ${S}/qlcinputchannel.cpp
    ${S}/qlcinputfeedback.cpp ${S}/qlcinputprofile.cpp ${S}/qlcinputsource.cpp
    ${S}/qlcmodifierscache.cpp ${S}/qlcpalette.cpp ${S}/qlcphysical.cpp ${S}/qlcpoint.cpp
    ${S}/rgbalgorithm.cpp ${S}/rgbaudio.cpp ${S}/rgbimage.cpp ${S}/rgbmatrix.cpp ${S}/rgbplain.cpp
    ${S}/rgbscriptscache.cpp ${S}/rgbscriptv4.cpp ${S}/rgbtext.cpp ${S}/scene.cpp
    ${S}/scenevalue.cpp ${S}/script.cpp ${S}/sequence.cpp ${S}/show.cpp ${S}/showfunction.cpp
    ${S}/showrunner.cpp ${S}/track.cpp ${S}/universe.cpp ${S}/video.cpp
)
# Its own headers only -- never ours, which share their names.
set_target_properties(stockqlcengine PROPERTIES AUTOMOC ON AUTOUIC OFF AUTORCC OFF)
target_include_directories(stockqlcengine BEFORE PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/stock/config
    ${S}
    ${A}
    ${STOCK_ROOT}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/hotplugmonitor/src
)
target_compile_options(stockqlcengine PRIVATE -w)
target_link_libraries(stockqlcengine PUBLIC
    Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Qml
    hotplugmonitor
)
if(APPLE)
    target_link_libraries(stockqlcengine PUBLIC
        "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation")
endif()

# The reader: stock QLC+'s engine, our fingerprint.
add_executable(stockroundtrip_load stockroundtrip_load.cpp fingerprint.h)
target_compile_options(stockroundtrip_load PRIVATE -w)
target_link_libraries(stockroundtrip_load PRIVATE stockqlcengine)

# The test: the app saves, stock reads.
include(../../src/include_ui.cmake)
set(module_name "stockroundtrip_test")
add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h fingerprint.h
)
target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
)
include_ui_header(${module_name})
target_compile_definitions(${module_name} PRIVATE
    STOCK_LOADER="$<TARGET_FILE:stockroundtrip_load>")
add_dependencies(${module_name} stockroundtrip_load)
target_link_libraries(${module_name} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Test Qt6::Widgets
    qlcplusengine
    qlcplusui
)
