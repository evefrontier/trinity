// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisRenderDevice_H
#define Tr2NoesisRenderDevice_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include <NsRender/RenderDevice.h>
#include <NsRender/RenderTarget.h>
#include <NsRender/Texture.h>

#include <../trinityal/include/TrinityAL.h>

#include <unordered_map>

// --------------------------------------------------------------------------------------
// Description:
//   NoesisGUI Texture and RenderTarget over Tr2TextureAL, plus the RenderDevice that
//   owns shaders, layouts, samplers and the dynamic vertex/index rings.
//
//   DrawBatch draws every permutation that was compiled and asserts on Custom_Effect,
//   which is supplied by the effect itself rather than compiled here.
//   BeginTile sets the AL scissor to the tile (Y-flipped from Noesis's lower-left origin).
//   EndTile is a no-op: the next SetRenderTarget resets scissor to the full target.
//   EndUpdatingTextures is not overridden: UpdateSubresource restores shader-read
//   state before it returns (see UpdateTexture).
// --------------------------------------------------------------------------------------

class Tr2NoesisTexture : public Noesis::Texture
{
public:
	Tr2NoesisTexture( Tr2TextureAL texture, uint32_t width, uint32_t height, uint32_t levels, bool hasAlpha );

	uint32_t GetWidth() const override;
	uint32_t GetHeight() const override;
	bool HasMipMaps() const override;
	bool IsInverted() const override;
	bool HasAlpha() const override;

	Tr2TextureAL& GetAL();
	const Tr2TextureAL& GetAL() const;

private:
	Tr2TextureAL m_texture;
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_levels;
	bool m_hasAlpha;
};

class Tr2NoesisRenderTarget : public Noesis::RenderTarget
{
public:
	Tr2NoesisRenderTarget( Noesis::Ptr<Tr2NoesisTexture> color, Tr2TextureAL stencil, uint32_t width, uint32_t height );

	Noesis::Texture* GetTexture() override;

	Tr2NoesisTexture* GetColor();
	Tr2TextureAL& GetStencil();
	bool HasStencil() const;
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

private:
	Noesis::Ptr<Tr2NoesisTexture> m_color;
	Tr2TextureAL m_stencil;
	uint32_t m_width;
	uint32_t m_height;
};

class Tr2NoesisRenderDevice : public Noesis::RenderDevice
{
public:
	explicit Tr2NoesisRenderDevice( Tr2PrimaryRenderContextAL& primaryContext );
	~Tr2NoesisRenderDevice();

	// False if any shader, layout, sampler or ring failed during construction. The constructor
	// itself cannot fail; callers must check this and refuse to proceed.
	bool IsValid() const;

	// The frame's deferred context. Must be set before Map*, UpdateTexture, SetRenderTarget
	// or the Begin/End render markers. Defaults to the primary context passed at construction.
	void SetRenderContext( Tr2RenderContextAL& renderContext );

	const Noesis::DeviceCaps& GetCaps() const override;
	Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget( const char* label, uint32_t width, uint32_t height,
														  uint32_t sampleCount, bool needsStencil ) override;
	Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget( const char* label, Noesis::RenderTarget* surface ) override;
	Noesis::Ptr<Noesis::Texture> CreateTexture( const char* label, uint32_t width, uint32_t height,
												uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data ) override;
	void UpdateTexture( Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y,
						uint32_t width, uint32_t height, const void* data ) override;
	void BeginOffscreenRender() override;
	void EndOffscreenRender() override;
	void BeginOnscreenRender() override;
	void EndOnscreenRender() override;
	void SetRenderTarget( Noesis::RenderTarget* surface ) override;
	void BeginTile( Noesis::RenderTarget* surface, const Noesis::Tile& tile ) override;
	void EndTile( Noesis::RenderTarget* surface ) override;
	void ResolveRenderTarget( Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t numTiles ) override;
	void* MapVertices( uint32_t bytes ) override;
	void UnmapVertices() override;
	void* MapIndices( uint32_t bytes ) override;
	void UnmapIndices() override;
	void DrawBatch( const Noesis::Batch& batch ) override;

private:
	struct DynamicRing
	{
		// One page per in-flight swap-chain buffer (Tr2SwapChainUtils::BACK_BUFFER_COUNT).
		// CPU writes only the current back-buffer's page; the GPU is still reading the others.
		static const uint32_t PAGE_COUNT = 3;

		Tr2BufferAL pages[PAGE_COUNT];
		uint32_t pageSize;
		uint32_t pageIndex;
		uint32_t pos;
		// Byte offset of the current Map within the page. DrawBatch uses it as the
		// SetStreamSource offset, and as the index base (drawPos / 2 + startIndex).
		uint32_t drawPos;
		bool mapped;

		// stride matters only for indices: a WRITE_OFTEN index buffer is bound lazily in
		// SetAllState, which picks R16_UINT or R32_UINT from the buffer's own stride. Vertex
		// strides are per-batch and travel through SetStreamSource instead.
		bool Create( uint32_t stride, uint32_t size, Tr2GpuUsage::Type gpuUsage, const char* name, Tr2PrimaryRenderContextAL& primary );
		void SyncFrame( uint32_t frameIndex );
		void* Map( uint32_t bytes, Tr2RenderContextAL& context );
		void Unmap( Tr2RenderContextAL& context );
		Tr2BufferAL& CurrentPage();
	};

	// A resource set is immutable once created, so they are cached per shader and per
	// binding. The description rides along because the hash is 32 bits and a collision
	// would otherwise bind the wrong textures.
	struct ResourceSetEntry
	{
		Tr2ResourceSetDescriptionAL description;
		Tr2ResourceSetAL set;
	};

	void CreateShaders();
	void CreateVertexLayouts();
	void CreateSamplers();
	void CreateRings();
	void SyncRingsToCurrentFrame();
	void ApplyRenderState( const Noesis::Batch& batch );
	void BindUniform( Tr2ConstantBufferAL& buffer, const Noesis::UniformData& uniforms,
					  Tr2RenderContextEnum::ShaderType stage, uint32_t registerIndex, const char* name );
	void BindUniforms( const Noesis::Batch& batch, uint32_t flags );
	void BindResources( const Noesis::Batch& batch, uint32_t flags );
	void ReportUnwiredShader( uint8_t shader );
	void ReportFrameBatches();

	Tr2PrimaryRenderContextAL* m_primary;
	Tr2RenderContextAL* m_context;
	Noesis::DeviceCaps m_caps;
	bool m_valid;

	Tr2ShaderAL m_vertexShaders[Noesis::Shader::Vertex::Count];
	Tr2ShaderAL m_pixelShaders[Noesis::Shader::Count];
	Tr2ShaderProgramAL m_programs[Noesis::Shader::Count];
	Tr2VertexLayoutAL m_vertexLayouts[Noesis::Shader::Vertex::Format::Count];
	// Indexed by Noesis::SamplerState::v. Six meaningful bits (wrapMode:3, minmagFilter:1,
	// mipFilter:2) address 64 slots; unused:2 must stay zero or the index is out of range.
	Tr2SamplerStateAL m_samplers[64];

	// Rewritten per batch. The AL uploads into a per-frame ring at SetConstants time and
	// Unlock invalidates the residency token, so one buffer per slot serves every batch.
	// Grown on demand rather than sized from the SDK's cbuffer layouts.
	Tr2ConstantBufferAL m_vertexUniforms[2];
	Tr2ConstantBufferAL m_pixelUniforms[2];

	// Keyed by shader and binding hash. Unbounded: the key space is the texture and sampler
	// combinations a UI actually uses, which stops growing shortly after steady state.
	std::unordered_map<uint64_t, ResourceSetEntry> m_resourceSets;

	// Per-frame batch histogram, reported from EndOnscreenRender when it changes.
	uint32_t m_batchCounts[Noesis::Shader::Count];
	uint32_t m_reportedCounts[Noesis::Shader::Count];
	// One assert per unwired shader; the histogram carries the recurrence.
	uint64_t m_unwiredReported;
	bool m_logBatchDetail;

	DynamicRing m_vertices;
	DynamicRing m_indices;
};

namespace Tr2Noesis
{

// The one render device for the process, built on first use from the main-thread primary
// render context. Null if construction failed.
//
// Every view's renderer shares it, which is Noesis's own model: the glyph atlas, the 64
// shader programs and the dynamic rings all live here. Deliberately never destroyed --
// TrinityAL objects in a static's destructor would be released after Trinity has torn the
// D3D12 device down. Surviving a device reset is gap G2, not this function's business.
//
// Call only from the render path. Construction creates AL resources, so it must not happen
// from arbitrary Python.
Tr2NoesisRenderDevice* GetRenderDevice();

}

#endif

#endif
