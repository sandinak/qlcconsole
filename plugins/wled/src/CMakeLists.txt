set(module_name "wled")

add_library(${module_name}
    SHARED
)

target_sources(${module_name} PRIVATE
    ../../interfaces/qlcioplugin.cpp ../../interfaces/qlcioplugin.h
    ../../interfaces/rdmprotocol.cpp ../../interfaces/rdmprotocol.h
    wledplugin.cpp wledplugin.h
    wledstate.cpp wledstate.h
)
target_include_directories(${module_name} PRIVATE
    ../../interfaces
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
)

install(TARGETS ${module_name}
    LIBRARY DESTINATION ${INSTALLROOT}/${PLUGINDIR}
    RUNTIME DESTINATION ${INSTALLROOT}/${PLUGINDIR}
)
