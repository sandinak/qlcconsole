add_executable(padtraces_test WIN32
    padtraces_test.cpp padtraces_test.h
)
target_include_directories(padtraces_test PRIVATE
    ../../../plugins/interfaces
    ../../../plugins/hid
    ../../../plugins/hid/test
    ../../src
)

target_link_libraries(padtraces_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# The real HID plugin, loaded as the app loads it, with a fake backend
# (plugins/hid/test/fakehidbackend.h) behind it, played from the recorded
# traces in traces/ (plugins/hid/test/padtraceplayer.h).
add_dependencies(padtraces_test hidplugin)
target_compile_definitions(padtraces_test PRIVATE
    QLC_HID_PLUGIN_FILE="$<TARGET_FILE:hidplugin>"
    QLC_PAD_TRACE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/traces")
