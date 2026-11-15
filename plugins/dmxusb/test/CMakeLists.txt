add_executable(dmxusb_test WIN32 MACOSX_BUNDLE
    ../src/writehealth.cpp ../src/writehealth.h
    ../src/dmxinterface.cpp ../src/dmxinterface.h
    ../src/dmxusbwidget.cpp ../src/dmxusbwidget.h
    ../src/dmxusbopenrx.cpp ../src/dmxusbopenrx.h
    ../src/enttecdmxusbopen.cpp ../src/enttecdmxusbopen.h
    ../src/enttecdmxusbpro.cpp ../src/enttecdmxusbpro.h
    ../src/stageprofi.cpp ../src/stageprofi.h
    ../src/vinceusbdmx512.cpp ../src/vinceusbdmx512.h
    ../src/usbdmxlegacy.cpp ../src/usbdmxlegacy.h
    ../../interfaces/rdmprotocol.cpp ../../interfaces/rdmprotocol.h
    ../../midi/src/common/midiprotocol.cpp ../../midi/src/common/midiprotocol.h
    dmxusb_test.cpp dmxusb_test.h
)
if(UNIX)
    target_sources(dmxusb_test PRIVATE
        ../src/euroliteusbdmxpro.cpp ../src/euroliteusbdmxpro.h
        ../src/nanodmx.cpp ../src/nanodmx.h
    )
endif()
target_include_directories(dmxusb_test PRIVATE
    ../src
    ../../interfaces
    ../../midi/src/common
    ../../../engine/test/common
)

target_link_libraries(dmxusb_test PRIVATE
    Qt6::Core
    Qt6::Test
)
