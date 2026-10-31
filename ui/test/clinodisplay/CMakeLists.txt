set(module_name "clinodisplay_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Test
)

# Runs the real app binary.
add_dependencies(${module_name} qlcconsole)
