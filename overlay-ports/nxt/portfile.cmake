# Copyright © 2026 CCP ehf.
#
# nxt is the C ABI between frontier-noesis and its rendering host: two headers and no
# library. Both sides build against this contract, and neither depends on the other.
#
# Overlay-first: the headers are carried in this port directory, copied from
# frontier-noesis/include. When the port moves to evefrontier/vcpkg-registry, only the
# SOURCE_PATH line changes -- to a vcpkg_from_git of wherever nxt.h is published, pinned
# to a tag.

set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# The port version is the ABI version. A header bump without a port bump fails here,
# not later as a struct_size refusal at runtime.
file(STRINGS "${SOURCE_PATH}/include/nxt.h" NXT_MAJOR REGEX "^#define NXT_ABI_VERSION_MAJOR [0-9]+$")
file(STRINGS "${SOURCE_PATH}/include/nxt.h" NXT_MINOR REGEX "^#define NXT_ABI_VERSION_MINOR [0-9]+$")
string(REGEX REPLACE "^.* " "" NXT_MAJOR "${NXT_MAJOR}")
string(REGEX REPLACE "^.* " "" NXT_MINOR "${NXT_MINOR}")
if(NOT VERSION STREQUAL "${NXT_MAJOR}.${NXT_MINOR}")
    message(FATAL_ERROR "${PORT}: port version ${VERSION} does not match the nxt.h ABI ${NXT_MAJOR}.${NXT_MINOR}")
endif()

set(VCPKG_BUILD_TYPE release)

file(INSTALL
    "${SOURCE_PATH}/include/nxt.h"
    "${SOURCE_PATH}/include/nxt_python.h"
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
