add_executable(midi_test WIN32 MACOSX_BUNDLE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../src/common/midiprotocol.cpp ../src/common/midiprotocol.h
    ../src/common/midimtcdecoder.cpp ../src/common/midimtcdecoder.h
    ../src/common/midisysexassembler.cpp ../src/common/midisysexassembler.h
    midi_test.cpp midi_test.h
)
target_include_directories(midi_test PRIVATE
    ../../interfaces
    ../src/common
)

target_link_libraries(midi_test PRIVATE
    # Remove: L../src
    Qt6::Core
    Qt6::Test
)
