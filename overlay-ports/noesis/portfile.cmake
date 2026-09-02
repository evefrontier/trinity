# Copyright © 2026 CCP ehf.
#
# Overlay-first: pack.py writes dist/noesis-v${VERSION}-<platform>.zip next to this
# file. Until that zip is published to vcpkg-prebuilt-sdks, the local copy is required.

if(VCPKG_TARGET_IS_WINDOWS)
    set(NOESIS_DIST_FILENAME "noesis-v${VERSION}-windows.zip")
elseif(VCPKG_TARGET_IS_OSX)
    set(NOESIS_DIST_FILENAME "noesis-v${VERSION}-macos.zip")
else()
    message(FATAL_ERROR "noesis port supports Windows and macOS only")
endif()

set(NOESIS_LOCAL_ZIP "${CMAKE_CURRENT_LIST_DIR}/dist/${NOESIS_DIST_FILENAME}")
if(NOT EXISTS "${NOESIS_LOCAL_ZIP}")
    message(FATAL_ERROR
        "noesis: '${NOESIS_LOCAL_ZIP}' not found.\n"
        "Pack a trimmed SDK zip first:\n"
        "  python overlay-ports/noesis/pack.py --sdk <NoesisSDK> --platform <windows|macos>")
endif()

message(STATUS "noesis: using local overlay zip ${NOESIS_LOCAL_ZIP}")

vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${NOESIS_LOCAL_ZIP}"
    NO_REMOVE_ONE_LEVEL
)

file(COPY "${SOURCE_PATH}/include/" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
file(COPY "${SOURCE_PATH}/shaders/" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}/shaders")

if(VCPKG_TARGET_IS_WINDOWS)
    file(COPY "${SOURCE_PATH}/lib/" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    file(COPY "${SOURCE_PATH}/bin/" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
elseif(VCPKG_TARGET_IS_OSX)
    file(COPY "${SOURCE_PATH}/bin/" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
endif()

file(COPY "${CMAKE_CURRENT_LIST_DIR}/${PORT}Config.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

if(EXISTS "${SOURCE_PATH}/THIRD_PARTY.txt")
    file(INSTALL "${SOURCE_PATH}/THIRD_PARTY.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
elseif(EXISTS "${SOURCE_PATH}/version.txt")
    file(INSTALL "${SOURCE_PATH}/version.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
endif()
