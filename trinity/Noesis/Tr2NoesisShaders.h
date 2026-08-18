// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisShaders_H
#define Tr2NoesisShaders_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

// --------------------------------------------------------------------------------------
// Description:
//   The NoesisGUI shader permutations, compiled from the SDK's own HLSL at build time and
//   embedded as DXBC blobs. The tables are ordered exactly as NOESIS_VERTEX_SHADERS and
//   NOESIS_PIXEL_SHADERS in trinity/CMakeLists.txt; the render device maps Noesis's
//   Shader::Vertex::Enum and Shader::Enum onto these indices.
// --------------------------------------------------------------------------------------
namespace Tr2Noesis
{

struct ShaderBytecode
{
	const char* name;
	const uint8_t* code;
	size_t size;
};

extern const ShaderBytecode VERTEX_SHADERS[];
constexpr size_t VERTEX_SHADER_COUNT = 21;

extern const ShaderBytecode PIXEL_SHADERS[];
constexpr size_t PIXEL_SHADER_COUNT = 52;

}

#endif

#endif
