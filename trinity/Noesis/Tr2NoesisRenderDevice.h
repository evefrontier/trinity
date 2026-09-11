// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisRenderDevice_H
#define Tr2NoesisRenderDevice_H

#if WITH_NOESIS

#include <NsRender/RenderDevice.h>
#include <NsRender/RenderTarget.h>
#include <NsRender/Texture.h>

#include <../trinityal/include/TrinityAL.h>

#include <string>
#include <unordered_map>
#include <vector>

// --------------------------------------------------------------------------------------
// Description:
//   NoesisGUI Texture and RenderTarget over Tr2TextureAL, plus the RenderDevice that
//   owns shaders, layouts, samplers and the dynamic vertex/index rings.
//
//   DrawBatch draws every compiled permutation. Custom_Effect and BrushShader
//   permutations come from CreatePixelShader; the batch carries that handle in
//   pixelShader. WrapTexture lets a host Tr2TextureAL be sampled as a Noesis texture.
//   BeginTile sets the AL scissor to the tile (Y-flipped from Noesis's lower-left origin).
//   EndTile is a no-op: the next SetRenderTarget resets scissor to the full target.
//   BeginOnscreenRender binds a depth-stencil (D24S8 on D3D, D32S8 on Metal;
//   ClipToBounds stencil, Transform3D depth)
//   and scissors to the current viewport intersected with an optional host clip
//   (CarbonUI clipChildren); EndOnscreenRender restores both.
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

	// Extra onscreen scissor in render-target pixels, intersected with the viewport
	// (already clamped to the target). Sprite 2d uses this for parent clipChildren
	// so Noesis can keep its layout viewport while the GPU clips overflow. The
	// overlay path leaves it cleared. Set before IRenderer::Render; clear after.
	void SetHostScissor( const Tr2ScissorRect& rect );
	void ClearHostScissor();

	const Noesis::DeviceCaps& GetCaps() const override;
	Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget( const char* label, uint32_t width, uint32_t height,
														  uint32_t sampleCount, bool needsStencil ) override;
	Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget( const char* label, Noesis::RenderTarget* surface ) override;
	Noesis::Ptr<Noesis::Texture> CreateTexture( const char* label, uint32_t width, uint32_t height,
												uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data ) override;
	// Wraps an existing Trinity texture so Noesis can sample it. The AL handle is copied
	// (shared ownership of the GPU resource). hasAlpha is what Noesis reports to brushes.
	Noesis::Ptr<Noesis::Texture> WrapTexture( const Tr2TextureAL& texture, bool hasAlpha );
	// Compiles a custom pixel shader from a ShaderCompiler blob (4-byte root-signature
	// flags, then DXBC) and pairs it with the stock vertex shader for `shader`.
	// The returned handle is what ShaderEffect::SetPixelShader / BrushShader::SetPixelShader
	// store; DrawBatch looks it up from Batch::pixelShader. Null on failure.
	void* CreatePixelShader( const char* label, uint8_t shader, const void* hlsl, uint32_t size );
	void ClearPixelShaders();
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
		// A bump allocator over a list of fixed-size chunks. Noesis maps and draws many
		// times per frame -- once per geometry flush, again per render target in the
		// offscreen phase, and once more for every view sharing this device -- while
		// DYNAMIC_VB_SIZE / DYNAMIC_IB_SIZE only cap a single Map. Chunks are appended on
		// demand and recycled once the GPU has finished the frame that last wrote them, so
		// the per-frame budget settles at the high-water mark of the heaviest frame instead
		// of being capped by the per-Map limit.
		//
		// Recycling is fence-based rather than frame % depth: the chunks are mapped
		// NON_SYNCRONIZED_WRITE, which is NO_OVERWRITE on DX11 and a persistently mapped
		// upload resource on DX12 and Metal, so nothing renames the buffer and writing over
		// bytes an in-flight draw still references corrupts it.
		struct Chunk
		{
			Tr2BufferAL buffer;
			// Recording frame that last wrote this chunk; reusable once the AL reports that
			// frame rendered.
			uint64_t lastUsedFrame = 0;
		};

		// Ceiling on growth, per ring. A frame that wants more drops geometry -- Map returns
		// null, which the SDK tolerates -- rather than growing without bound.
		static const uint32_t MAX_CHUNKS = 64;
		static const uint32_t INVALID_CHUNK = 0xffffffff;

		std::vector<Chunk> chunks;
		// Chunk being filled, or INVALID_CHUNK before the frame's first Map.
		uint32_t chunkIndex = INVALID_CHUNK;
		// Bump offset within that chunk.
		uint32_t pos = 0;
		// Byte offset of the current Map within its chunk. DrawBatch uses it as the
		// SetStreamSource offset, and as the index base (drawPos / 2 + startIndex).
		uint32_t drawPos = 0;
		// Recording frame the allocator is filling. Compared as a full frame number, not an
		// index: a UI that skips frames in a multiple of the in-flight depth would otherwise
		// keep accumulating into chunks it had already filled this cycle.
		uint64_t frame = 0;
		// stride matters only for indices: a WRITE_OFTEN index buffer is bound lazily in
		// SetAllState, which picks R16_UINT or R32_UINT from the buffer's own stride. Vertex
		// strides are per-batch and travel through SetStreamSource instead.
		uint32_t stride = 1;
		uint32_t chunkSize = 0;
		Tr2GpuUsage::Type gpuUsage = Tr2GpuUsage::VERTEX_BUFFER;
		bool mapped = false;
		// One log per frame that hits the cap.
		bool growthCapped = false;
		std::string name;
		// Bytes handed out this frame, and the largest already reported, so the log fires
		// only when a frame is heavier than every frame before it.
		uint32_t frameBytes = 0;
		uint32_t reportedPeak = 0;
		// Returned by CurrentChunk when there is no current chunk, so a device that failed
		// to create GPU memory binds an invalid buffer rather than reading off the end.
		Tr2BufferAL fallback;

		bool Create( uint32_t bufferStride, uint32_t bytesPerChunk, Tr2GpuUsage::Type usage, const char* ringName, Tr2PrimaryRenderContextAL& primary );
		void SyncFrame( Tr2PrimaryRenderContextAL& primary );
		void* Map( uint32_t bytes, Tr2RenderContextAL& context, Tr2PrimaryRenderContextAL& primary );
		void Unmap( Tr2RenderContextAL& context );
		Tr2BufferAL& CurrentChunk();
		// Claims a chunk the GPU is done with, appending a new one if every chunk is busy.
		bool AcquireChunk( Tr2PrimaryRenderContextAL& primary );
		bool AppendChunk( Tr2PrimaryRenderContextAL& primary );
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
	void BindResources( const Noesis::Batch& batch, uint32_t flags, Tr2ShaderProgramAL& program, uint64_t programId );
	void ReportUnwiredShader( uint8_t shader );
	void ReportFrameBatches();
	bool EnsureOnscreenStencil( uint32_t width, uint32_t height );

	Tr2PrimaryRenderContextAL* m_primary;
	Tr2RenderContextAL* m_context;
	Noesis::DeviceCaps m_caps;
	bool m_valid;
	// Onscreen ClipToBounds is stencil; Transform3D is a reverse-Z depth test
	// with writes off. The sprite/UI path has no S8 plane (null DS, or the 3D
	// D32F buffer). One depth-stencil matching the colour target, cleared each frame.
	Tr2TextureAL m_onscreenStencil;
	bool m_pushedOnscreenStencil;
	bool m_hasHostScissor;
	Tr2ScissorRect m_hostScissor;

	Tr2ShaderAL m_vertexShaders[Noesis::Shader::Vertex::Count];
	Tr2ShaderAL m_pixelShaders[Noesis::Shader::Count];
	Tr2ShaderProgramAL m_programs[Noesis::Shader::Count];
	Tr2VertexLayoutAL m_vertexLayouts[Noesis::Shader::Vertex::Format::Count];

	// Handles returned by CreatePixelShader are 1-based indices into this vector.
	// ShaderEffect / BrushShader store them in Batch::pixelShader.
	struct CustomProgram
	{
		Tr2ShaderAL pixelShader;
		Tr2ShaderProgramAL program;
		uint32_t flags = 0;
		uint8_t vertexFormat = 0;
	};
	std::vector<CustomProgram> m_customShaders;
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
// GPU device down. Surviving a device reset is gap G2, not this function's business.
//
// Call only from the render path. Construction creates AL resources, so it must not happen
// from arbitrary Python.
Tr2NoesisRenderDevice* GetRenderDevice();

}

#endif

#endif
