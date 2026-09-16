// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisRenderDevice_H
#define Tr2NoesisRenderDevice_H

#include <nxt.h>

#include <../trinityal/include/TrinityAL.h>

#include <string>
#include <unordered_map>
#include <vector>

// --------------------------------------------------------------------------------------
// Description:
//   GPU resources over Tr2TextureAL, plus the device that owns shaders, layouts,
//   samplers and the dynamic vertex/index rings.
//
//   No NoesisGUI types appear here. The library owns the SDK; this side is reached only
//   through the nxt.h vtables, and the handles it hands out are pointers to the two
//   structs below. Releasing a handle destroys the wrapper, which for a wrapped host
//   texture leaves the underlying AL resource alone -- see release_texture in nxt.h.
//
//   DrawBatch draws every compiled permutation. Custom_Effect and BrushShader
//   permutations come from CreatePixelShader; the batch carries that handle in
//   pixelShader, and they live until the device does -- release_pixel_shader has
//   nothing per-handle to free. WrapTexture lets a host Tr2TextureAL be sampled as a
//   Noesis texture.
//   BeginTile sets the AL scissor to the tile (Y-flipped from Noesis's lower-left origin).
//   EndTile is a no-op: the next SetRenderTarget resets scissor to the full target.
//   BeginOnscreenRender binds a depth-stencil (D24S8 on D3D, D32S8 on Metal; stencil
//   for ClipToBounds, depth for Transform3D) and scissors to the current viewport
//   intersected with an optional host clip (CarbonUI clipChildren); EndOnscreenRender
//   restores both.
// --------------------------------------------------------------------------------------

class Tr2NoesisTexture
{
public:
	Tr2NoesisTexture( Tr2TextureAL texture, uint32_t width, uint32_t height, uint32_t levels, bool hasAlpha );

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	uint32_t GetLevels() const;
	bool HasMipMaps() const;
	bool HasAlpha() const;

	Tr2TextureAL& GetAL();
	const Tr2TextureAL& GetAL() const;

private:
	Tr2TextureAL m_texture;
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_levels;
	bool m_hasAlpha;
};

class Tr2NoesisRenderTarget
{
public:
	Tr2NoesisRenderTarget( Tr2NoesisTexture* color, Tr2TextureAL stencil, uint32_t width, uint32_t height );

	// Borrowed, not owned. The library names this texture through
	// get_render_target_texture and releases that handle itself, after
	// release_render_target returns; freeing it here would double-free.
	Tr2NoesisTexture* GetColor();
	Tr2TextureAL& GetStencil();
	bool HasStencil() const;
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;

private:
	Tr2NoesisTexture* m_color;
	Tr2TextureAL m_stencil;
	uint32_t m_width;
	uint32_t m_height;
};

class Tr2NoesisRenderDevice
{
public:
	// Takes the shader permutations and vertex formats from the library; there is no SDK
	// on this side to get them from.
	Tr2NoesisRenderDevice( Tr2PrimaryRenderContextAL& primaryContext,
						   const nxt_shader_source& shaders );
	~Tr2NoesisRenderDevice();

	// False if any shader, layout, sampler or ring failed during construction. The constructor
	// itself cannot fail; callers must check this and refuse to proceed.
	bool IsValid() const;

	// The frame's deferred context. Must be set before Map*, UpdateTexture, SetRenderTarget
	// or the Begin/End render markers. Defaults to the primary context passed at construction.
	void SetRenderContext( Tr2RenderContextAL& renderContext );
	// Dropped at the end of the frame, so an out-of-order frame-half call trips the assert
	// at the top of each of those methods instead of recording into a context the step has
	// finished with. Resource creation is unaffected: it uses the primary context, which
	// lives as long as the device.
	void ClearRenderContext();

	// Extra onscreen scissor in render-target pixels, intersected with the viewport
	// (already clamped to the target). Sprite 2d uses this for parent clipChildren
	// so Noesis can keep its layout viewport while the GPU clips overflow. The
	// overlay path leaves it cleared. Set before IRenderer::Render; clear after.
	void SetHostScissor( const Tr2ScissorRect& rect );
	void ClearHostScissor();

	void GetCaps( nxt_device_caps& out ) const;
	Tr2NoesisRenderTarget* CreateRenderTarget( const char* label, uint32_t width, uint32_t height,
											   uint32_t sampleCount, bool needsStencil );
	Tr2NoesisRenderTarget* CloneRenderTarget( const char* label, Tr2NoesisRenderTarget* surface );
	Tr2NoesisTexture* CreateTexture( const char* label, uint32_t width, uint32_t height,
									 uint32_t numLevels, nxt_texture_format format, const void** data );
	// Wraps an existing Trinity texture so Noesis can sample it. The AL handle is copied
	// (shared ownership of the GPU resource). hasAlpha is what Noesis reports to brushes.
	Tr2NoesisTexture* WrapTexture( const Tr2TextureAL& texture, bool hasAlpha );
	// Compiles a custom pixel shader from a ShaderCompiler blob (4-byte root-signature
	// flags, then DXBC) and pairs it with the stock vertex shader for `shader`.
	// The returned handle is what ShaderEffect::SetPixelShader / BrushShader::SetPixelShader
	// store; DrawBatch looks it up from Batch::pixelShader. Null on failure.
	void* CreatePixelShader( const char* label, uint8_t shader, const void* hlsl, uint32_t size );
	void UpdateTexture( Tr2NoesisTexture* texture, uint32_t level, uint32_t x, uint32_t y,
						uint32_t width, uint32_t height, const void* data );
	void BeginOffscreenRender();
	void EndOffscreenRender();
	void BeginOnscreenRender();
	void EndOnscreenRender();
	void SetRenderTarget( Tr2NoesisRenderTarget* surface );
	void BeginTile( Tr2NoesisRenderTarget* surface, const nxt_tile& tile );
	void EndTile( Tr2NoesisRenderTarget* surface );
	void ResolveRenderTarget( Tr2NoesisRenderTarget* surface, const nxt_tile* tiles, uint32_t numTiles );
	void* MapVertices( uint32_t bytes );
	void UnmapVertices();
	void* MapIndices( uint32_t bytes );
	void UnmapIndices();
	void DrawBatch( const nxt_batch& batch );

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

	bool ReadShaderSource( const nxt_shader_source& shaders );
	void CreateShaders();
	void CreateVertexLayouts();
	void CreateSamplers();
	void CreateRings();
	void SyncRingsToCurrentFrame();
	void ApplyRenderState( const nxt_batch& batch );
	void BindUniform( Tr2ConstantBufferAL& buffer, const nxt_uniform_data& uniforms,
					  Tr2RenderContextEnum::ShaderType stage, uint32_t registerIndex, const char* name );
	void BindUniforms( const nxt_batch& batch, uint32_t flags );
	void BindResources( const nxt_batch& batch, uint32_t flags, Tr2ShaderProgramAL& program, uint64_t programId );
	void ReportUnwiredShader( uint8_t shader );
	void ReportFrameBatches();
	bool EnsureOnscreenStencil( uint32_t width, uint32_t height );

	Tr2PrimaryRenderContextAL* m_primary;
	Tr2RenderContextAL* m_context;
	nxt_device_caps m_caps;
	bool m_valid;
	// Onscreen ClipToBounds is stencil; Transform3D is a reverse-Z depth test
	// with writes off. The sprite/UI path has no S8 plane (null DS, or the 3D
	// D32F buffer). One depth-stencil matching the colour target, cleared each frame.
	Tr2TextureAL m_onscreenStencil;
	bool m_pushedOnscreenStencil;
	bool m_hasHostScissor;
	Tr2ScissorRect m_hostScissor;

	// What the library says about a shader, cached at build time rather than asked for
	// again per batch. A blob carries everything needed to build a pipeline, so there is
	// no SDK-shaped table to keep in step with it.
	struct ShaderInfo
	{
		uint8_t vertexShader = 0;
		uint8_t vertexFormat = 0;
		uint32_t resourceFlags = 0;
		// Library-owned and valid for the process lifetime, so holding the pointer is
		// enough. Null for a slot the library did not supply, such as custom effects.
		const void* bytecode = nullptr;
		uint32_t bytecodeSize = 0;
		const char* name = nullptr;
	};
	std::vector<ShaderInfo> m_shaderInfo;   // indexed by pixel shader id
	std::vector<ShaderInfo> m_vertexInfo;   // indexed by vertex shader id

	// The attributes of each vertex format, in declaration order, as the library
	// described them.
	std::vector<std::vector<nxt_vertex_attribute>> m_vertexFormats;
	// Byte stride of each format, summed from its attributes.
	std::vector<uint32_t> m_vertexStrides;


	// Sized from the shader source rather than an SDK constant: how many permutations and
	// vertex formats exist is the library's to decide, and a build of it with more or
	// fewer must not need a matching change here.
	std::vector<Tr2ShaderAL> m_vertexShaders;
	std::vector<Tr2ShaderAL> m_pixelShaders;
	std::vector<Tr2ShaderProgramAL> m_programs;
	std::vector<Tr2VertexLayoutAL> m_vertexLayouts;

	// Handles returned by CreatePixelShader are 1-based indices into this vector.
	// ShaderEffect / BrushShader store them in nxt_batch::pixel_shader.
	struct CustomProgram
	{
		Tr2ShaderAL pixelShader;
		Tr2ShaderProgramAL program;
		uint32_t flags = 0;
		uint8_t vertexFormat = 0;
	};
	std::vector<CustomProgram> m_customShaders;
	// Indexed by nxt_sampler_state. Six meaningful bits (wrapMode:3, minmagFilter:1,
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
	std::vector<uint32_t> m_batchCounts;
	std::vector<uint32_t> m_reportedCounts;
	// One assert per unwired shader; the histogram carries the recurrence.
	uint64_t m_unwiredReported;
	bool m_logBatchDetail;

	DynamicRing m_vertices;
	DynamicRing m_indices;
};

#endif
