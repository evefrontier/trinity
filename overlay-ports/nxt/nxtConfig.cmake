# Copyright © 2026 CCP ehf.

get_filename_component(_NXT_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT TARGET nxt::nxt)
    add_library(nxt::nxt INTERFACE IMPORTED)
    set_target_properties(nxt::nxt PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_NXT_PREFIX}/include"
    )
endif()

unset(_NXT_PREFIX)
