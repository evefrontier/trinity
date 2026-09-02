# Copyright © 2026 CCP ehf.

get_filename_component(_NOESIS_SHARE "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_NOESIS_PREFIX "${_NOESIS_SHARE}/../.." ABSOLUTE)

set(Noesis_INCLUDE_DIR "${_NOESIS_PREFIX}/include")
set(Noesis_SHADER_SOURCE_DIR "${_NOESIS_SHARE}/shaders")

if(NOT EXISTS "${Noesis_SHADER_SOURCE_DIR}/ShaderVS.hlsl" OR NOT EXISTS "${Noesis_SHADER_SOURCE_DIR}/ShaderPS.hlsl")
    message(FATAL_ERROR
        "noesis: ShaderVS.hlsl / ShaderPS.hlsl not found under ${Noesis_SHADER_SOURCE_DIR}")
endif()

add_library(Noesis::Noesis SHARED IMPORTED GLOBAL)
add_library(Noesis::NoesisEditor SHARED IMPORTED GLOBAL)

if(WIN32)
    set_target_properties(Noesis::Noesis PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Noesis_INCLUDE_DIR}"
        IMPORTED_IMPLIB "${_NOESIS_PREFIX}/lib/Noesis.lib"
        IMPORTED_LOCATION "${_NOESIS_PREFIX}/bin/Noesis.dll"
    )
    set_target_properties(Noesis::NoesisEditor PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Noesis_INCLUDE_DIR}"
        IMPORTED_IMPLIB "${_NOESIS_PREFIX}/lib/NoesisEditor.lib"
        IMPORTED_LOCATION "${_NOESIS_PREFIX}/bin/NoesisEditor.dll"
    )
else()
    set_target_properties(Noesis::Noesis PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Noesis_INCLUDE_DIR}"
        IMPORTED_LOCATION "${_NOESIS_PREFIX}/bin/libNoesis.dylib"
    )
    set_target_properties(Noesis::NoesisEditor PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Noesis_INCLUDE_DIR}"
        IMPORTED_LOCATION "${_NOESIS_PREFIX}/bin/libNoesisEditor.dylib"
    )
endif()

unset(_NOESIS_SHARE)
unset(_NOESIS_PREFIX)
