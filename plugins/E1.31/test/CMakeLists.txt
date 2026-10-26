add_executable(e131_test WIN32 MACOSX_BUNDLE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../../interfaces/sendhealth.cpp ../../interfaces/sendhealth.h
    ../e131packetizer.cpp ../e131packetizer.h
    e131_test.cpp e131_test.h
)
target_include_directories(e131_test PRIVATE
    ../../interfaces
    ..
)

target_link_libraries(e131_test PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    e131
)
