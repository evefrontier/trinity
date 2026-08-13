// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisRenderDevice_H
#define Tr2NoesisRenderDevice_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include <NsRender/RenderDevice.h>
#include <NsRender/RenderTarget.h>
#include <NsRender/Texture.h>

#include <../trinityal/include/TrinityAL.h>

// --------------------------------------------------------------------------------------
// Description:
//   NoesisGUI Texture and RenderTarget over Tr2TextureAL, plus the RenderDevice that
//   owns shaders, layouts, samplers and the dynamic vertex/index rings.
//
//   DrawBatch asserts until later milestones wire individual shader permutations.
//   BeginTile/EndTile are no-ops (gap G1: TrinityAL has no scissor rect).
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
		// Byte offset of the current Map within the page. DrawBatch will use this for
		// SetStreamSource, and as the MapIndices base (plus startIndex * 2).
		uint32_t drawPos;
		bool mapped;

		bool Create( uint32_t size, Tr2GpuUsage::Type gpuUsage, const char* name, Tr2PrimaryRenderContextAL& primary );
		void SyncFrame( uint32_t frameIndex );
		void* Map( uint32_t bytes, Tr2RenderContextAL& context );
		void Unmap( Tr2RenderContextAL& context );
	};

	void CreateShaders();
	void CreateVertexLayouts();
	void CreateSamplers();
	void CreateRings();
	void SyncRingsToCurrentFrame();
	void ApplyRenderState( const Noesis::Batch& batch );

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

	DynamicRing m_vertices;
	DynamicRing m_indices;
};

#endif

#endif
