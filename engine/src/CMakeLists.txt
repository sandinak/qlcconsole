set(module_name "qlcplusengine")

include(../../variables.cmake)

add_library(${module_name} SHARED
    ../../plugins/interfaces/qlcioplugin.cpp ../../plugins/interfaces/qlcioplugin.h
    ../../plugins/interfaces/utils.h
    autosavemanager.cpp autosavemanager.h
    avolitesd4parser.cpp avolitesd4parser.h
    bus.cpp bus.h
    capturemanager.cpp capturemanager.h
    channelmodifier.cpp channelmodifier.h
    channelsgroup.cpp channelsgroup.h
    chaser.cpp chaser.h
    chaseraction.h
    chaserrunner.cpp chaserrunner.h
    chaserstep.cpp chaserstep.h
    collection.cpp collection.h
    cue.cpp cue.h
    cuestack.cpp cuestack.h
    dmxdumpfactoryproperties.cpp dmxdumpfactoryproperties.h
    dmxsource.h
    doc.cpp doc.h
    efx.cpp efx.h
    efxfixture.cpp efxfixture.h
    fadechannel.cpp fadechannel.h
    fixture.cpp fixture.h
    fixturepower.cpp fixturepower.h
    powerlimiter.cpp powerlimiter.h
    shutterautoopen.cpp shutterautoopen.h
    showwebaccess.cpp showwebaccess.h
    controlmap.cpp controlmap.h
    showoperate.cpp showoperate.h
    outputrulekeeper.cpp outputrulekeeper.h
    fixturegroup.cpp fixturegroup.h
    function.cpp function.h
    genericdmxsource.cpp genericdmxsource.h
    genericfader.cpp genericfader.h
    gradient.cpp gradient.h
    grandmaster.cpp grandmaster.h
    grouphead.cpp grouphead.h
    inputoutputmap.cpp inputoutputmap.h
    patchundo.cpp patchundo.h
    inputpatch.cpp inputpatch.h
    ioplugincache.cpp ioplugincache.h
    keypadparser.cpp keypadparser.h
    mastertimer.cpp mastertimer.h
    monitorproperties.cpp monitorproperties.h
    networkreport.cpp networkreport.h
    patchlabels.cpp patchlabels.h
    paletteops.cpp paletteops.h
    palettenaming.cpp palettenaming.h
    patchlist.cpp patchlist.h
    xlsxwriter.cpp xlsxwriter.h
    csvfield.h
    speedmasters.cpp speedmasters.h
    showcontrol.cpp showcontrol.h
    showtreecommand.cpp showtreecommand.h
    showtreecontroller.cpp showtreecontroller.h
    showtreebridge.cpp
    showtreeconvert.cpp
    showtreelooks.cpp
    showtreeproductions.cpp
    showtreesubcues.cpp
    showtreesections.cpp
    showtreenumbering.cpp
    showtreerunner.cpp
    showtreereusable.cpp
    showtreemodel.cpp showtreemodel.h
    spreadlayer.cpp spreadlayer.h
    linecurve.cpp linecurve.h
    linespread.cpp linespread.h
    networktopology.cpp networktopology.h
    powerdistribution.cpp powerdistribution.h
    venue.cpp venue.h
    aimsolver.cpp aimsolver.h
    beamocclusion.cpp beamocclusion.h
    stageplatform.cpp stageplatform.h
    pipe.cpp pipe.h
    scenery.cpp scenery.h
    stockqlccheck.cpp stockqlccheck.h
    gobolibrary.cpp gobolibrary.h
    headlayout.cpp headlayout.h
    righeads.cpp righeads.h
    hanger.cpp hanger.h
    wledfixture.cpp wledfixture.h
    scenetransition.cpp scenetransition.h
    fixturedefcheck.cpp fixturedefcheck.h
    networkgearcatalog.cpp networkgearcatalog.h
    stand.cpp stand.h
    tower.cpp tower.h
    performerfill.cpp performerfill.h
    stagetarget.cpp stagetarget.h
    truss.cpp truss.h
    networkroute.cpp networkroute.h
    rignetwork.cpp rignetwork.h
    outputpatch.cpp outputpatch.h
    patchplanner.cpp patchplanner.h
    qlccapability.cpp qlccapability.h
    qlcchannel.cpp qlcchannel.h
    qlcclipboard.cpp qlcclipboard.h
    legacysettings.cpp legacysettings.h
    qlcfile.cpp qlcfile.h
    qlcfixturedef.cpp qlcfixturedef.h
    qlcfixturedefcache.cpp qlcfixturedefcache.h
    qlcfixturehead.cpp qlcfixturehead.h
    qlcfixturemode.cpp qlcfixturemode.h
    qlci18n.cpp qlci18n.h
    qlcinputchannel.cpp qlcinputchannel.h
    qlcinputfeedback.cpp qlcinputfeedback.h
    qlcinputprofile.cpp qlcinputprofile.h
    qlcinputsource.cpp qlcinputsource.h
    qlcmodifierscache.cpp qlcmodifierscache.h
    qlcpalette.cpp qlcpalette.h
    measurementunits.cpp measurementunits.h
    qlcphysical.cpp qlcphysical.h
    qlcpoint.cpp qlcpoint.h
    qxwimporter.cpp qxwimporter.h
    eosasciiparser.cpp eosasciiparser.h
    eosfixturemapper.cpp eosfixturemapper.h
    eosimporter.cpp eosimporter.h
    eosmagicsheetparser.cpp eosmagicsheetparser.h
    rgbalgorithm.cpp rgbalgorithm.h
    rgbaudio.cpp rgbaudio.h
    rgbimage.cpp rgbimage.h
    rgbmatrix.cpp rgbmatrix.h
    rgbplain.cpp rgbplain.h
    rgbscriptproperty.h
    rgbscriptscache.cpp rgbscriptscache.h
    rgbtext.cpp rgbtext.h
    programmerflasher.cpp programmerflasher.h
    highlighteffect.cpp highlighteffect.h
    focussession.cpp focussession.h
    focuscalls.cpp focuscalls.h
    focuscallcue.cpp focuscallcue.h
    venuecorrections.cpp venuecorrections.h
    venuerig.cpp venuerig.h
    rigowner.cpp rigowner.h
    focusreport.cpp focusreport.h
    controllerscheme.cpp controllerscheme.h
    refinesession.cpp refinesession.h
    refinescene.cpp
    controllerkeepalive.cpp controllerkeepalive.h
    gamepadrole.cpp gamepadrole.h
    padnames.cpp padnames.h
    touchpadgesture.cpp touchpadgesture.h
    parkeffect.cpp parkeffect.h
    outputlayer.cpp outputlayer.h
    markeffect.cpp markeffect.h
    cueoutput.cpp cueoutput.h
    cuelookahead.cpp cuelookahead.h
    markplanner.cpp markplanner.h
    controlsurface.h
    controlsurfaceengine.cpp controlsurfaceengine.h
    lastlookeffect.cpp lastlookeffect.h
    effectscript.cpp effectscript.h
    effectscriptcache.cpp effectscriptcache.h
    lookstackcheck.cpp lookstackcheck.h
    effectpresetcache.cpp effectpresetcache.h
    qlcbundle.cpp qlcbundle.h
    bundlecache.cpp bundlecache.h
    effectinstance.cpp effectinstance.h
    matriximage.cpp matriximage.h
    effectscriptrunner.cpp effectscriptrunner.h
    showembedding.cpp showembedding.h
    fixturedefupdates.cpp fixturedefupdates.h
    programmercontroller.cpp programmercontroller.h
    scene.cpp scene.h
    scenevalue.cpp scenevalue.h
    scriptwrapper.h
    sequence.cpp sequence.h
    show.cpp show.h
    showfunction.cpp showfunction.h
    timecodesource.cpp timecodesource.h
    showrunner.cpp showrunner.h
    track.cpp track.h
    universe.cpp universe.h
    video.cpp video.h
)
target_include_directories(${module_name} PUBLIC
    ../../plugins/interfaces
    ../audio/src
)

target_link_libraries(${module_name} PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Network
)

target_link_libraries(${module_name} PRIVATE qlcplusaudio)

if(NOT ANDROID AND NOT IOS)
    target_include_directories(${module_name} PUBLIC ../../hotplugmonitor/src)
    target_link_libraries(${module_name} PUBLIC hotplugmonitor)
    target_link_directories(${module_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../../hotplugmonitor/src)
endif()

if(UNIX AND NOT ANDROID AND NOT IOS AND NOT APPLE)
    target_link_libraries(${module_name} PUBLIC asound)
endif()

if(UNIX)
    target_sources(${module_name} PRIVATE mastertimer-unix.cpp mastertimer-unix.h)
endif()

if(WIN32)
    target_sources(${module_name} PRIVATE mastertimer-win32.cpp mastertimer-win32.h)
    target_include_directories(${module_name} PUBLIC /)

    target_link_libraries(${module_name} PUBLIC
        Qt6::Widgets
        winmm
        avrt
    )
endif()

if((NOT ANDROID AND NOT IOS) AND (iokit))
    target_link_libraries(${module_name} PUBLIC
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

if(APPLE)
    set_target_properties(${module_name} PROPERTIES
        MACOSX_BUNDLE FALSE
    )
    # ControllerKeepAlive's macOS side: App Nap, sleep/wake, Input Monitoring.
    target_sources(${module_name} PRIVATE controllerkeepalive_mac.mm)
    set_source_files_properties(controllerkeepalive_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(${module_name} PUBLIC
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
    target_link_libraries(${module_name} PRIVATE
        "-framework AppKit"
        "-framework Foundation"
        "-framework IOKit"
    )
endif()

target_sources(${module_name} PRIVATE rgbscriptv4.cpp rgbscriptv4.h)

target_link_libraries(${module_name} PUBLIC Qt6::Qml)

if(qmlui)
    target_sources(${module_name} PRIVATE
        scriptrunner.cpp scriptrunner.h
        scriptv4.cpp scriptv4.h
    )
else()
    target_sources(${module_name} PRIVATE script.cpp script.h)
endif()

if((NOT ANDROID AND NOT IOS) AND (${FFTW3_FOUND}))
    target_compile_definitions(${module_name} PUBLIC HAS_FFTW3)
    target_include_directories(${module_name} PUBLIC ${FFTW3_INCLUDE_DIRS})
    target_link_directories(${module_name} PUBLIC ${FFTW3_LIBRARY_DIRS})
    target_link_libraries(${module_name} PUBLIC ${FFTW3_LIBRARIES})
endif()

set(CONFIGFILE ${CMAKE_SOURCE_DIR}/engine/src/qlcconfig.h)
set(LITERAL_HASH "#")

if(WIN32 OR APPLE OR appimage)
    configure_file(qlcconfig.h.noroot.in ${CONFIGFILE})
elseif(UNIX OR ANDROID OR IOS)
    configure_file(qlcconfig.h.in ${CONFIGFILE})
endif()

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${CONFIGFILE})

# MinGW exports every symbol of a DLL by itself only while nothing in it is
# marked dllexport; qlcengine_export.h marks a few classes, so ask for the
# rest explicitly.
if(MINGW)
    target_link_options(${module_name} PRIVATE -Wl,--export-all-symbols)
endif()

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${LIBSDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${LIBSDIR}
)
