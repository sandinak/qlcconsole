# The surface simulator (docs/design/SURFACE_SIMULATOR.md, how-to in
# docs/dev/SURFACE_TESTS.md): simulated MIDI boards and game pads that enter
# the app where the real devices do. A test uses it with ONE line:
#
#     qlc_surfacesim(mytest_test)
#
# It compiles in the plugins' own decoders the simulator feeds through (the
# MIDI plugin's midiprotocol.cpp, the HID plugin's gamepadlayout.cpp), adds
# the simulator's headers and links the I/O plugin stub the MIDI boards
# enter through. A pad test also loads the real HID plugin:
# add_dependencies(<t> hidplugin) and QLC_HID_PLUGIN_FILE (see
# engine/test/padsim/CMakeLists.txt).
function(qlc_surfacesim target)
    set(root "${CMAKE_SOURCE_DIR}")
    target_sources(${target} PRIVATE
        ${root}/plugins/midi/src/common/midiprotocol.cpp
        ${root}/plugins/hid/gamepadlayout.cpp
    )
    target_include_directories(${target} PRIVATE
        ${root}/engine/test/common/surfacesim
        ${root}/engine/test/iopluginstub
        ${root}/plugins/interfaces
        ${root}/plugins/midi/src/common
        ${root}/plugins/hid
        ${root}/plugins/hid/test
        ${root}/engine/src
    )
    target_link_libraries(${target} PRIVATE iopluginstub)
endfunction()
