project(licenses)

# The licence texts of what the installers carry (THIRD-PARTY.txt says
# which library is under which). Next to the samples: Resources/licenses in
# the macOS bundle, licenses\ in the Windows install folder,
# share/qlcconsole/licenses on Linux.
file(GLOB LICENSE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.txt")
install(FILES ${LICENSE_FILES} DESTINATION ${SAMPLESDIR}/licenses)
install(FILES ${CMAKE_SOURCE_DIR}/COPYING DESTINATION ${SAMPLESDIR}/licenses RENAME Apache-2.0.txt)
