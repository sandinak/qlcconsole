project(windows)

# Where the toolchain's runtime DLLs live: next to the compiler. Upstream
# hard-coded $ENV{SystemDrive}/msys64/mingw64/bin and CI sed-patched it to the
# runner's MSYS2 location; the compiler's own directory is right for any
# MSYS2 environment (MINGW64, UCRT64, ...) wherever MSYS2 is installed.
get_filename_component(SYS_LIBS_PATH "${CMAKE_CXX_COMPILER}" DIRECTORY)

function(copy_system_library target libname)
	if(EXISTS ${SYS_LIBS_PATH}/${libname})
		list(APPEND ${target} ${SYS_LIBS_PATH}/${libname})
		set(${target} "${${target}}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "ERROR: missing DLL detected: ${libname}")
	endif()
endfunction()

# MSYS2 libraries
set(msys_path "${INSTALLROOT}/${LIBSDIR}")
copy_system_library(msys_files "libusb-1.0.dll")
install(FILES ${msys_files} DESTINATION ${msys_path})

# audio libraries
set(mmedia_path "${INSTALLROOT}/${LIBSDIR}")
copy_system_library(mmedia_files "libogg-0.dll")
copy_system_library(mmedia_files "libopus-0.dll")
copy_system_library(mmedia_files "libmp3lame-0.dll")
copy_system_library(mmedia_files "libmpg123-0.dll")
copy_system_library(mmedia_files "libvorbis-0.dll")
copy_system_library(mmedia_files "libvorbisenc-2.dll")
copy_system_library(mmedia_files "libFLAC.dll")
copy_system_library(mmedia_files "libsndfile-1.dll")
copy_system_library(mmedia_files "libfftw3-3.dll")

install(FILES ${mmedia_files} DESTINATION ${mmedia_path})

# NullSoft installer script: the version (VERSION at the repo root) and the
# source tree it takes the icon and licence from are filled in here, so
# nothing has to sed-patch the script before makensis.
set(nsis_path "${INSTALLROOT}/${DATADIR}")
if(qmlui)
    install(FILES qlcconsole5Qt6.nsi DESTINATION ${nsis_path})
else()
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" QLC_NSIS_SRCDIR)
    configure_file(qlcconsole.nsi.in ${CMAKE_CURRENT_BINARY_DIR}/qlcconsole.nsi @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qlcconsole.nsi DESTINATION ${nsis_path})
endif()

# install(FILES ../Sample.qxw DESTINATION ${INSTALLROOT}/${DATADIR})
