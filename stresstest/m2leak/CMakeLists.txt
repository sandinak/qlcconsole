# m2leak: the leak harness (open / Build edits / Run / close cycles of the real
# App, offscreen). Not built by default. To build it, copy this folder to
# ui/test/m2leak and add "add_subdirectory(m2leak)" to ui/test/CMakeLists.txt
# in a (sanitizer) build tree; then ninja m2leak. stresstest/chain_linux.sh
# runs it under ASan/LSan and lsan_parse.py turns the reports into bytes/cycle.
add_executable(m2leak m2leak.cpp)
target_compile_definitions(m2leak PRIVATE QLCCONSOLE)
target_include_directories(m2leak PRIVATE ../../../engine/src ../../../plugins/interfaces ../../src ../../src/virtualconsole ../../src/showmanager ../../src/monitor)
include_ui_header(m2leak)
target_link_libraries(m2leak PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Test qlcplusengine qlcplusui)
