# studiosoak: the on-screen memory soak, offscreen (studiosoak.cpp). Not built
# by default. To build it, add to ui/test/CMakeLists.txt in a build tree:
#   add_subdirectory(${CMAKE_SOURCE_DIR}/stresstest/studiosoak studiosoak)
# then build the studiosoak target and run studiosoak.sh.
include(${CMAKE_SOURCE_DIR}/ui/src/include_ui.cmake)
add_executable(studiosoak studiosoak.cpp)
target_include_directories(studiosoak PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/ui/src ${CMAKE_SOURCE_DIR}/ui/src/showmanager
    ${CMAKE_SOURCE_DIR}/ui/src/monitor)
include_ui_header(studiosoak)
target_link_libraries(studiosoak PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test qlcplusengine qlcplusui)
