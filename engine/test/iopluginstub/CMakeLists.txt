
add_library(iopluginstub SHARED)
target_sources(iopluginstub PRIVATE
    ../../../plugins/interfaces/qlcioplugin.cpp ../../../plugins/interfaces/qlcioplugin.h
    iopluginstub.cpp iopluginstub.h
)
target_include_directories(iopluginstub PRIVATE
    ../../../plugins/interfaces
)

target_link_libraries(iopluginstub PRIVATE
    Qt6::Core
    Qt6::Gui
)
