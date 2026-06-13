cmake_minimum_required(VERSION 3.16)
project(qlcstress LANGUAGES CXX)

# Standalone: configure with
#   -DQLC_ROOT=<repo root> -DQLC_BUILD=<arm64 build dir> -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
set(QLC_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "QLC+ repo root")
set(QLC_BUILD "${QLC_ROOT}/build-arm64"           CACHE PATH "QLC+ build dir with libqlcplusengine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Optional sanitizers. Must match the sanitizer the engine lib was built with
# (see build_sanitizers.sh). -DSANITIZE=address  or  -DSANITIZE=thread
set(SANITIZE "" CACHE STRING "address | thread | '' (off)")
if(SANITIZE STREQUAL "address")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
elseif(SANITIZE STREQUAL "thread")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

# Qml is needed so QT_QML_LIB is defined: rgbmatrix.h then uses the Qt6
# rgbscriptv4 path instead of the Qt5-only QtScript rgbscript.h.
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Qml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Qml)

add_executable(qlcstress qlcstress.cpp showbuilder.cpp showbuilder.h)

target_include_directories(qlcstress PRIVATE
    ${QLC_ROOT}/engine/src
    ${QLC_ROOT}/plugins/interfaces)

# link the already-built engine shared library
find_library(QLCPLUSENGINE_LIB
    NAMES qlcplusengine libqlcplusengine
    PATHS ${QLC_BUILD}/engine/src
    NO_DEFAULT_PATH REQUIRED)

target_link_libraries(qlcstress PRIVATE
    ${QLCPLUSENGINE_LIB}
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Qml)

# so the dylib resolves at runtime without copying
set_target_properties(qlcstress PROPERTIES
    BUILD_RPATH "${QLC_BUILD}/engine/src")
