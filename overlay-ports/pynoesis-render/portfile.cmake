# Copyright © 2026 CCP ehf.
#
# pynoesis-render is pynr, the C ABI for rendering between pynoesis and the renderer
# that hosts it: two headers and no library. Both sides build against this contract, and
# neither depends on the other.
#
# Overlay-first: the headers are carried in this port directory, copied from
# pynoesis/include. When the port moves to evefrontier/vcpkg-registry, only the
# SOURCE_PATH line changes -- to a vcpkg_from_git of wherever pynr.h is published, pinned
# to a tag.

set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# The port version is the ABI version. A header bump without a port bump fails here,
# not later as a struct_size refusal at runtime.
file(STRINGS "${SOURCE_PATH}/include/pynr.h" PYNR_MAJOR REGEX "^#define PYNR_ABI_VERSION_MAJOR [0-9]+$")
file(STRINGS "${SOURCE_PATH}/include/pynr.h" PYNR_MINOR REGEX "^#define PYNR_ABI_VERSION_MINOR [0-9]+$")
string(REGEX REPLACE "^.* " "" PYNR_MAJOR "${PYNR_MAJOR}")
string(REGEX REPLACE "^.* " "" PYNR_MINOR "${PYNR_MINOR}")
if(NOT VERSION STREQUAL "${PYNR_MAJOR}.${PYNR_MINOR}")
    message(FATAL_ERROR "${PORT}: port version ${VERSION} does not match the pynr.h ABI ${PYNR_MAJOR}.${PYNR_MINOR}")
endif()

set(VCPKG_BUILD_TYPE release)

file(INSTALL
    "${SOURCE_PATH}/include/pynr.h"
    "${SOURCE_PATH}/include/pynr_python.h"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include")

# SameMajorVersion mirrors the runtime rule: majors must agree, and a newer minor only
# appends vtable slots.
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}ConfigVersion.cmake"
    VERSION ${VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)

file(INSTALL
    "${CMAKE_CURRENT_LIST_DIR}/${PORT}Config.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${CMAKE_CURRENT_LIST_DIR}/copyright")
