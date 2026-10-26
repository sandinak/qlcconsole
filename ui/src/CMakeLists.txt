set(module_name "qlcplusui")

include(../../variables.cmake)

set(TS_FILES
    qlcconsole_fi_FI.ts
    qlcconsole_fr_FR.ts
    qlcconsole_es_ES.ts
    qlcconsole_de_DE.ts
    qlcconsole_it_IT.ts
    qlcconsole_nl_NL.ts
    qlcconsole_cz_CZ.ts
    qlcconsole_pt_BR.ts
    qlcconsole_ca_ES.ts
    qlcconsole_ja_JP.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

add_library(${module_name}
    SHARED
    ../../plugins/interfaces/rdmprotocol.cpp ../../plugins/interfaces/rdmprotocol.h
    aboutbox.cpp aboutbox.h aboutbox.ui
    addchannelsgroup.cpp addchannelsgroup.h addchannelsgroup.ui
    addfixture.cpp addfixture.h addfixture.ui
    addresstool.cpp addresstool.h addresstool.ui
    addrgbpanel.cpp addrgbpanel.h addrgbpanel.ui
    app.cpp app.h
    appsettings.cpp appsettings.h
    controllertestdialog.cpp controllertestdialog.h
    apputil.cpp apputil.h
    itemicons.cpp itemicons.h
    assignhotkey.cpp assignhotkey.h assignhotkey.ui
    audiobar.cpp audiobar.h
    audioeditor.cpp audioeditor.h audioeditor.ui
    audiotriggerwidget.cpp audiotriggerwidget.h
    channelmodifiereditor.cpp channelmodifiereditor.h channelmodifiereditor.ui
    channelmodifiergraphicsview.cpp channelmodifiergraphicsview.h
    channelsselection.cpp channelsselection.h channelsselection.ui
    chasereditor.cpp chasereditor.h chasereditor.ui
    chaserwalker.cpp chaserwalker.h
    colourswatchbutton.cpp colourswatchbutton.h
    wledconfigcheck.cpp wledconfigcheck.h
    wleddevicesdialog.cpp wleddevicesdialog.h
    clickandgoslider.cpp clickandgoslider.h
    clickandgowidget.cpp clickandgowidget.h
    collectioneditor.cpp collectioneditor.h collectioneditor.ui
    consolechannel.cpp consolechannel.h
    createfixturegroup.cpp createfixturegroup.h createfixturegroup.ui
    headtypesdialog.cpp headtypesdialog.h
    ledlayouteditor.cpp ledlayouteditor.h
    headtypegroups.cpp headtypegroups.h
    creategroupsdialog.cpp creategroupsdialog.h
    stagegroupdialog.cpp stagegroupdialog.h
    ctkrangeslider.cpp ctkrangeslider.h
    cuestackmodel.cpp cuestackmodel.h
    customfeedbackdialog.cpp customfeedbackdialog.h customfeedbackdialog.ui
    dmxdumpfactory.cpp dmxdumpfactory.h dmxdumpfactory.ui
    efxeditor.cpp efxeditor.h efxeditor.ui
    efxpreviewarea.cpp efxpreviewarea.h
    fixtureconsole.cpp fixtureconsole.h
    fixturecreator.cpp fixturecreator.h
    fixturegroupeditor.cpp fixturegroupeditor.h fixturegroupeditor.ui
    fixturemanager.cpp fixturemanager.h
    fixtureremap.cpp fixtureremap.h fixtureremap.ui
    fixtureselection.cpp fixtureselection.h fixtureselection.ui
    fixturetreewidget.cpp fixturetreewidget.h
    flowlayout.cpp flowlayout.h
    functionliveeditdialog.cpp functionliveeditdialog.h
    functionmanager.cpp functionmanager.h
    functionselection.cpp functionselection.h functionselection.ui
    functionstreewidget.cpp functionstreewidget.h
    functionwizard.cpp functionwizard.h functionwizard.ui
    grandmasterslider.cpp grandmasterslider.h
    groupsconsole.cpp groupsconsole.h
    horizontalpan.cpp horizontalpan.h
    importselectiondialog.cpp importselectiondialog.h
    eosimportwizard.cpp eosimportwizard.h
    inputchanneleditor.cpp inputchanneleditor.h inputchanneleditor.ui
    inputdevicenotices.cpp inputdevicenotices.h
    inputoutputmanager.cpp inputoutputmanager.h
    connectionstree.cpp connectionstree.h
    connectioneditor.cpp connectioneditor.h
    bulkresultdialog.cpp bulkresultdialog.h
    universepatcher.cpp universepatcher.h
    powerprofiledialog.cpp powerprofiledialog.h
    outputrulesdialog.cpp outputrulesdialog.h
    universeoutputview.cpp universeoutputview.h
    deviceprofile.cpp deviceprofile.h
    inputoutputpatcheditor.cpp inputoutputpatcheditor.h inputoutputpatcheditor.ui
    inputprofileeditor.cpp inputprofileeditor.h inputprofileeditor.ui
    inputselectionwidget.cpp inputselectionwidget.h inputselectionwidget.ui
    knobwidget.cpp knobwidget.h
    livecapturedialog.cpp livecapturedialog.h
    timecodecalibrationdialog.cpp timecodecalibrationdialog.h
    virtualconsole/programmermap.cpp virtualconsole/programmermap.h
    virtualconsole/programmerframewizard.cpp virtualconsole/programmerframewizard.h
    monitor/monitor.cpp monitor/monitor.h
    monitor/focuscontrolmap.cpp monitor/focuscontrolmap.h
    monitor/focusmodepanel.cpp monitor/focusmodepanel.h
    monitor/focuscallsdialog.cpp monitor/focuscallsdialog.h
    monitor/focusmenu.cpp monitor/focusmenu.h
    monitor/venuerigdialog.cpp monitor/venuerigdialog.h
    monitor/monitorbackgroundselection.cpp monitor/monitorbackgroundselection.h monitor/monitorbackgroundselection.ui
    monitor/monitorfixture.cpp monitor/monitorfixture.h
    monitor/monitorfixtureitem.cpp monitor/monitorfixtureitem.h
    monitor/monitorfixturepropertieseditor.cpp monitor/monitorfixturepropertieseditor.h monitor/monitorfixturepropertieseditor.ui
    monitor/monitorgraphicsview.cpp monitor/monitorgraphicsview.h
    monitor/monitorlayerspanel.cpp monitor/monitorlayerspanel.h
    monitor/monitorruler.cpp monitor/monitorruler.h
    monitor/monitorimageitem.cpp monitor/monitorimageitem.h
    monitor/monitorinventorypanel.cpp monitor/monitorinventorypanel.h
    monitor/monitorlayout.cpp monitor/monitorlayout.h
    monitor/trussitem.cpp monitor/trussitem.h
    monitor/platformitem.cpp monitor/platformitem.h
    monitor/pipeitem.cpp monitor/pipeitem.h
    monitor/sceneryitem.cpp monitor/sceneryitem.h
    monitor/fountaindiagram.cpp monitor/fountaindiagram.h
    monitor/beamoptics.cpp monitor/beamoptics.h
    monitor/gobowheelsdialog.cpp monitor/gobowheelsdialog.h
    monitor/stagepieceitem.cpp monitor/stagepieceitem.h
    monitor/stagepiecedialog.cpp monitor/stagepiecedialog.h
    monitor/standitem.cpp monitor/standitem.h
    monitor/hangeritem.cpp monitor/hangeritem.h
    monitor/toweritem.cpp monitor/toweritem.h
    monitor/powersourceitem.cpp monitor/powersourceitem.h
    monitor/networknodeitem.cpp monitor/networknodeitem.h
    monitor/riserfaceeditor.cpp monitor/riserfaceeditor.h
    monitor/studiogroupeditor.cpp monitor/studiogroupeditor.h
    monitor/studiotemplate.cpp monitor/studiotemplate.h
    monitor/studiocomponentbrowser.cpp monitor/studiocomponentbrowser.h
    monitor/studioplaneview.cpp monitor/studioplaneview.h
    monitor/structurestudioview.cpp monitor/structurestudioview.h
    monitor/zraster.cpp monitor/zraster.h
    monitor/rhiraster.cpp monitor/rhiraster.h
    monitor/fixturevisualtraits.cpp monitor/fixturevisualtraits.h
    monitor/lanternpixels.cpp monitor/lanternpixels.h
    monitor/lightcontrols.cpp monitor/lightcontrols.h
    monitor/rigeditsession.h
    monitor/fixturefields.cpp monitor/fixturefields.h
    monitor/humanfigure.cpp monitor/humanfigure.h
    monitor/targetitem.cpp monitor/targetitem.h
    palettegenerator.cpp palettegenerator.h
    playbackslider.cpp playbackslider.h
    pmjoverlay.cpp pmjoverlay.h
    positiontool.cpp positiontool.h positiontool.ui
    preferencesdialog.cpp preferencesdialog.h
    rdmmanager.cpp rdmmanager.h rdmmanager.ui
    remapwidget.cpp remapwidget.h
    rgbitem.cpp rgbitem.h
    rgbmatrixeditor.cpp rgbmatrixeditor.h rgbmatrixeditor.ui
    sceneeditor.cpp sceneeditor.h sceneeditor.ui
    scenegrouplooks.cpp scenegrouplooks.h
    fixturegroupsource.cpp fixturegroupsource.h
    capabilitybar.cpp capabilitybar.h
    gradientdirectionwidget.cpp gradientdirectionwidget.h
    bundleeditor.cpp bundleeditor.h
    bundlebrowser.cpp bundlebrowser.h
    effectbrowser.cpp effectbrowser.h
    effectdesigner.cpp effectdesigner.h
    effectspage.cpp effectspage.h
    fadetimespin.cpp fadetimespin.h
    lookeditor.cpp lookeditor.h
    mainbar.cpp mainbar.h
    moveinblackbutton.cpp moveinblackbutton.h
    effectscripteditor.cpp effectscripteditor.h
    pathdrawwidget.cpp pathdrawwidget.h
    networkreportdialog.cpp networkreportdialog.h
    patchlabelsdialog.cpp patchlabelsdialog.h
    patchlistdialog.cpp patchlistdialog.h
    speedmastersfooter.cpp speedmastersfooter.h
    speedmasterspanel.cpp speedmasterspanel.h
    spreadpreview.cpp spreadpreview.h
    linespreadpreview.cpp linespreadpreview.h
    lineplanview.cpp lineplanview.h
    networktopologydialog.cpp networktopologydialog.h
    noteedit.cpp noteedit.h
    patchinventorydialog.cpp patchinventorydialog.h
    helpbrowser.cpp helpbrowser.h
    helpshots.cpp helpshots.h
    showcontrollistener.cpp showcontrollistener.h
    showcontrolsyncpage.cpp showcontrolsyncpage.h
    shrinkablebar.h
    universecombo.cpp universecombo.h
    powerdistributiondialog.cpp powerdistributiondialog.h
    powerdistributionwidget.cpp powerdistributionwidget.h
    universeusagewidget.cpp universeusagewidget.h
    programmingmanager.cpp programmingmanager.h
    programmingoutline.cpp programmingoutline.h
    splitterhandles.cpp splitterhandles.h
    workbench.cpp workbench.h
    workbenchstack.cpp workbenchstack.h
    showtreeview.cpp showtreeview.h
    refinepanel.cpp refinepanel.h
    operatorvoice.cpp operatorvoice.h
    showtreeworkbench.cpp showtreenewhere.cpp treecuestack.cpp treecuestack.h
    importsheet.cpp importsheet.h
    showconverter.cpp showconverter.h
    cuerunner.cpp cuerunner.h
    updatechecker.cpp updatechecker.h
    updatedialog.cpp updatedialog.h
    updatedownloader.cpp updatedownloader.h
    updateinstaller.cpp updateinstaller.h
    updatemanager.cpp updatemanager.h
    usedby.cpp usedby.h
    paletteeditdialog.cpp paletteeditdialog.h
    palettemanager.cpp palettemanager.h
    scripteditor.cpp scripteditor.h scripteditor.ui
    selectinputchannel.cpp selectinputchannel.h selectinputchannel.ui
    showmanager/audioitem.cpp showmanager/audioitem.h
    showmanager/collectionitem.cpp showmanager/collectionitem.h
    showmanager/efxitem.cpp showmanager/efxitem.h
    showmanager/headeritems.cpp showmanager/headeritems.h
    showmanager/multitrackview.cpp showmanager/multitrackview.h
    showmanager/rgbmatrixitem.cpp showmanager/rgbmatrixitem.h
    showmanager/sceneitem.cpp showmanager/sceneitem.h
    showmanager/sequenceitem.cpp showmanager/sequenceitem.h
    showmanager/showeditor.cpp showmanager/showeditor.h showmanager/showeditor.ui
    showmanager/showitem.cpp showmanager/showitem.h
    showmanager/showmanager.cpp showmanager/showmanager.h
    showmanager/showtimelineeditor.cpp showmanager/showtimelineeditor.h
    showmanager/timingstool.cpp showmanager/timingstool.h
    showmanager/trackitem.cpp showmanager/trackitem.h
    showmanager/videoitem.cpp showmanager/videoitem.h
    showstatus.cpp showstatus.h
    showstatusdialog.cpp showstatusdialog.h
    showdifferencesdialog.cpp showdifferencesdialog.h
    definitionupdatesdialog.cpp definitionupdatesdialog.h
    simpledesk.cpp simpledesk.h
    simpledeskengine.cpp simpledeskengine.h
    speeddial.cpp speeddial.h
    speeddialwidget.cpp speeddialwidget.h
    startupwindow.cpp startupwindow.h
    universeitemwidget.cpp universeitemwidget.h
    videoeditor.cpp videoeditor.h videoeditor.ui
    videoprovider.cpp videoprovider.h
    virtualconsole/addvcbuttonmatrix.cpp virtualconsole/addvcbuttonmatrix.h virtualconsole/addvcbuttonmatrix.ui
    virtualconsole/addvcslidermatrix.cpp virtualconsole/addvcslidermatrix.h virtualconsole/addvcslidermatrix.ui
    virtualconsole/vcaudiotriggers.cpp virtualconsole/vcaudiotriggers.h
    virtualconsole/vcaudiotriggersproperties.cpp virtualconsole/vcaudiotriggersproperties.h virtualconsole/vcaudiotriggersproperties.ui
    virtualconsole/vcbutton.cpp virtualconsole/vcbutton.h
    virtualconsole/vcbuttonproperties.cpp virtualconsole/vcbuttonproperties.h virtualconsole/vcbuttonproperties.ui
    virtualconsole/vcclock.cpp virtualconsole/vcclock.h
    virtualconsole/vcclockproperties.cpp virtualconsole/vcclockproperties.h virtualconsole/vcclockproperties.ui
    virtualconsole/vcshowcontrol.cpp virtualconsole/vcshowcontrol.h
    virtualconsole/vccuerunner.cpp virtualconsole/vccuerunner.h
    virtualconsole/vccuerunnerproperties.cpp virtualconsole/vccuerunnerproperties.h
    virtualconsole/vcshowcontrolproperties.cpp virtualconsole/vcshowcontrolproperties.h
    virtualconsole/vccuelist.cpp virtualconsole/vccuelist.h
    virtualconsole/vccuelistproperties.cpp virtualconsole/vccuelistproperties.h virtualconsole/vccuelistproperties.ui
    virtualconsole/vcdockarea.cpp virtualconsole/vcdockarea.h
    virtualconsole/vcframe.cpp virtualconsole/vcframe.h
    virtualconsole/vcframepageshortcut.cpp virtualconsole/vcframepageshortcut.h
    virtualconsole/vcframeproperties.cpp virtualconsole/vcframeproperties.h virtualconsole/vcframeproperties.ui
    virtualconsole/vclabel.cpp virtualconsole/vclabel.h
    virtualconsole/vcmatrix.cpp virtualconsole/vcmatrix.h
    virtualconsole/vcmatrixcontrol.cpp virtualconsole/vcmatrixcontrol.h
    virtualconsole/vcmatrixpresetselection.cpp virtualconsole/vcmatrixpresetselection.h virtualconsole/vcmatrixpresetselection.ui
    virtualconsole/vcmatrixproperties.cpp virtualconsole/vcmatrixproperties.h virtualconsole/vcmatrixproperties.ui
    virtualconsole/vcproperties.cpp virtualconsole/vcproperties.h virtualconsole/vcproperties.ui
    virtualconsole/vcpropertieseditor.cpp virtualconsole/vcpropertieseditor.h
    virtualconsole/vcslider.cpp virtualconsole/vcslider.h
    virtualconsole/vcsliderproperties.cpp virtualconsole/vcsliderproperties.h virtualconsole/vcsliderproperties.ui
    virtualconsole/vcsoloframe.cpp virtualconsole/vcsoloframe.h
    virtualconsole/vcsoloframeproperties.cpp virtualconsole/vcsoloframeproperties.h
    virtualconsole/vcspeeddial.cpp virtualconsole/vcspeeddial.h
    virtualconsole/vcspeeddialfunction.cpp virtualconsole/vcspeeddialfunction.h
    virtualconsole/vcspeeddialpreset.cpp virtualconsole/vcspeeddialpreset.h
    virtualconsole/vcspeeddialproperties.cpp virtualconsole/vcspeeddialproperties.h virtualconsole/vcspeeddialproperties.ui
    virtualconsole/vcwidget.cpp virtualconsole/vcwidget.h
    virtualconsole/vcwidgetproperties.cpp virtualconsole/vcwidgetproperties.h
    virtualconsole/vcwidgetselection.cpp virtualconsole/vcwidgetselection.h virtualconsole/vcwidgetselection.ui
    virtualconsole/vcxypad.cpp virtualconsole/vcxypad.h
    virtualconsole/vcxypadarea.cpp virtualconsole/vcxypadarea.h
    virtualconsole/vcxypadfixture.cpp virtualconsole/vcxypadfixture.h
    virtualconsole/vcxypadfixtureeditor.cpp virtualconsole/vcxypadfixtureeditor.h virtualconsole/vcxypadfixtureeditor.ui
    virtualconsole/vcxypadpreset.cpp virtualconsole/vcxypadpreset.h
    virtualconsole/vcxypadproperties.cpp virtualconsole/vcxypadproperties.h virtualconsole/vcxypadproperties.ui
    virtualconsole/virtualconsole.cpp virtualconsole/virtualconsole.h
    ${QM_FILES}
)

target_include_directories(${module_name} PUBLIC
    ../../engine/audio/src
    ../../engine/src
    ../../plugins/interfaces
    ../../webaccess
    monitor
    showmanager
    virtualconsole
)

target_include_directories(${module_name} PUBLIC ${FFTW3_INCLUDE_DIRS})

target_link_libraries(${module_name} PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate     # QRhi (rhi/qrhi.h): the rig view on the GPU
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Widgets
    qlcplusengine
)

# The voice that tells the operator on stage where the controller is
# (operatorvoice.cpp): Qt's TextToSpeech where this Qt has it. A Mac uses its
# own `say` and needs nothing more deployed, so it is not linked there.
find_package(Qt6 QUIET COMPONENTS TextToSpeech)
if(Qt6TextToSpeech_FOUND AND NOT APPLE)
    target_link_libraries(${module_name} PUBLIC Qt6::TextToSpeech)
    target_compile_definitions(${module_name} PRIVATE QLC_HAVE_TEXTTOSPEECH)
endif()

# Add resources and RGB Script deps
target_link_libraries(${module_name} PUBLIC Qt6::Qml)
qt_add_resources(qlcplusui_resource_files "qlcui.qrc" "help.qrc" "update.qrc"
    "../../resources/icons/hicolor/appicons.qrc")

target_sources(${module_name} PRIVATE ${qlcplusui_resource_files})

# The rig view's GPU shaders (RhiRaster), compiled for every backend QRhi has.
qt_add_shaders(${module_name} "rigshaders"
    PREFIX "/rig"
    BASE "monitor/shaders"
    FILES
        monitor/shaders/rig.vert
        monitor/shaders/rig.frag
        monitor/shaders/rolloff.vert
        monitor/shaders/rolloff.frag
)

if(WIN32)
    target_include_directories(${module_name} PUBLIC ../../hotplugmonitor/src)
endif()

# MinGW exports every symbol of a DLL by itself only while nothing in it is
# marked dllexport; qlcui_export.h marks a few classes, so ask for the
# rest explicitly.
if(MINGW)
    target_link_options(${module_name} PRIVATE -Wl,--export-all-symbols)
endif()

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${LIBSDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${LIBSDIR}
)
