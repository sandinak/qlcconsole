set(module_name "qlci18n_test")

set(TS_FILES
    qlci18n_fi_FI.ts
)

qt_add_translation(QM_FILES ${TS_FILES})

add_executable(${module_name} WIN32
    ${module_name}.cpp ${module_name}.h
    ${QM_FILES}
)

target_include_directories(${module_name} PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
