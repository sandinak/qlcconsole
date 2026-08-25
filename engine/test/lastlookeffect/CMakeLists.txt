add_executable(lastlookeffect_test WIN32
    lastlookeffect_test.cpp lastlookeffect_test.h
)
target_include_directories(lastlookeffect_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

# This test drives std::thread directly. glibc 2.34+ folded libpthread into
# libc, so it links implicitly on Ubuntu 22.04 (what CI builds on) -- but on
# older distributions (Debian 11 / glibc 2.31) the link fails with
# "DSO missing from command line". Ask for it explicitly.
find_package(Threads REQUIRED)

target_link_libraries(lastlookeffect_test PRIVATE
    Threads::Threads
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
