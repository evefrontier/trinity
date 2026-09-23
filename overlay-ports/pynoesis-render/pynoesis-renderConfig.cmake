# Copyright © 2026 CCP ehf.

get_filename_component(_PYNR_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT TARGET pynoesis-render::pynoesis-render)
    add_library(pynoesis-render::pynoesis-render INTERFACE IMPORTED)
    set_target_properties(pynoesis-render::pynoesis-render PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_PYNR_PREFIX}/include"
    )
endif()

unset(_PYNR_PREFIX)
