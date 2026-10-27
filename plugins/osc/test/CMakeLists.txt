add_executable(osc_test WIN32 MACOSX_BUNDLE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../osccontroller.cpp ../osccontroller.h
    ../oscpacketizer.cpp ../oscpacketizer.h
    osc_test.cpp osc_test.h
)
target_include_directories(osc_test PRIVATE
    ..
    ../../interfaces
    ../../../engine/test/common
)

target_link_libraries(osc_test PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
    Qt6::Test
)
