add_executable(wled_test WIN32 MACOSX_BUNDLE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../../interfaces/rdmprotocol.cpp ../../interfaces/rdmprotocol.h
    ../src/wledstate.cpp ../src/wledstate.h
    ../src/wledplugin.cpp ../src/wledplugin.h
    wled_test.cpp wled_test.h
)
target_include_directories(wled_test PRIVATE
    ../../interfaces
    ../src
)

target_link_libraries(wled_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    Qt6::Test
)
