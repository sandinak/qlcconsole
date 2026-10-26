add_executable(dllsignals_test WIN32
    dllsignals_test.cpp dllsignals_test.h
)

target_link_libraries(dllsignals_test PRIVATE
    Qt6::Core
    Qt6::Test
)

# The sources it reads: every connect() and QSignalSpy in the tree.
target_compile_definitions(dllsignals_test PRIVATE
    DLLSIGNALS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
