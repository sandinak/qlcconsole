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
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
)
