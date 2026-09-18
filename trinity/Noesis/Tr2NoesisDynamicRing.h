// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisDynamicRing_H
#define Tr2NoesisDynamicRing_H

#include <../trinityal/include/TrinityAL.h>

#include <string>
#include <vector>

// --------------------------------------------------------------------------------------
// Description:
//   A bump allocator over a list of fixed-size GPU buffer chunks, recycled by fence.
//
//   Noesis maps and draws many times per frame -- once per geometry flush, again per
//   render target in the offscreen phase, and once more for every view sharing the device
//   -- while DYNAMIC_VB_SIZE and DYNAMIC_IB_SIZE cap only a single Map. Chunks are
//   appended on demand and recycled once the GPU has finished the frame that last wrote
//   them, so the per-frame budget settles at the high-water mark of the heaviest frame
//   rather than at the per-Map limit.
//
//   Recycling is fence-based rather than frame % depth. The chunks are mapped
//   NON_SYNCRONIZED_WRITE -- NO_OVERWRITE on DX11, a persistently mapped upload resource
//   on DX12 and Metal -- so nothing renames the buffer, and writing over bytes an
//   in-flight draw still references corrupts it.
// --------------------------------------------------------------------------------------
class Tr2NoesisDynamicRing
{
public:
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
	// SetStreamSource offset, and divided by stride as the index base.
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

#endif
