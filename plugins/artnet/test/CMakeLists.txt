
add_executable(artnet_test WIN32 MACOSX_BUNDLE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../../interfaces/rdmprotocol.cpp ../../interfaces/rdmprotocol.h
    ../src/artnetpacketizer.cpp
    artnet_test.cpp artnet_test.h
)
target_include_directories(artnet_test PRIVATE
    ../../interfaces
    ../src
)

target_link_libraries(artnet_test PRIVATE
    # Remove: L../src
    Qt6::Core
    Qt6::Network
    Qt6::Test
    artnet
)
