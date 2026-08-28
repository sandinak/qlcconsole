# Bench instrument, not a shipped component: built so it cannot rot, never
# installed. See main.cpp for what it answers and why a unit test cannot.
add_executable(artnetprobe
    # The packetizer pulls in RDMProtocol, same as plugins/artnet/test does.
    ../../plugins/interfaces/rdmprotocol.cpp
    ../../plugins/artnet/src/artnetpacketizer.cpp
    main.cpp
)

target_include_directories(artnetprobe PRIVATE
    ../../plugins/artnet/src
    ../../plugins/interfaces
)

target_link_libraries(artnetprobe PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
)
