// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisDynamicRing.h"

#include "Noesis/Tr2NoesisLog.h"

// --------------------------------------------------------------------------------------
// DynamicRing
// --------------------------------------------------------------------------------------

bool Tr2NoesisDynamicRing::Create( uint32_t bufferStride, uint32_t bytesPerChunk, Tr2GpuUsage::Type usage, const char* ringName, Tr2PrimaryRenderContextAL& primary )
{
	CCP_ASSERT_M( bufferStride != 0 && ( bytesPerChunk % bufferStride ) == 0, "Noesis ring chunk size must be a whole number of strides" );

	chunks.clear();
	stride = bufferStride;
	chunkSize = bytesPerChunk;
	gpuUsage = usage;
	name = ringName != nullptr ? ringName : "Ring";
	chunkIndex = INVALID_CHUNK;
	pos = 0;
	drawPos = 0;
	frame = primary.GetRecordingFrameNumber();
	mapped = false;
	growthCapped = false;
	frameBytes = 0;
	reportedPeak = 0;

	// One chunk up front, so a device that cannot get GPU memory reports it at construction
	// rather than on the first heavy frame. AppendChunk leaves it current and stamped with
	// this frame, which is what the frame's first Map wants anyway.
	return AppendChunk( primary );
}

// --------------------------------------------------------------------------------------
// Description:
//   Adds a chunk to the ring and makes it current. Fails once the ring is at its cap.
// Arguments:
//   primary - main-thread primary render context
// Return value:
//   true If a chunk was created
// --------------------------------------------------------------------------------------
bool Tr2NoesisDynamicRing::AppendChunk( Tr2PrimaryRenderContextAL& primary )
{
	if( chunks.size() >= MAX_CHUNKS )
	{
		if( !growthCapped )
		{
			const uint32_t cap = MAX_CHUNKS;
			CCP_NOESIS_LOGERR( "Noesis ring '%s' is at its cap of %u chunks (%u bytes); dropping UI geometry this frame",
							   name.c_str(), cap, cap * chunkSize );
			growthCapped = true;
		}
		return false;
	}

	const uint32_t index = static_cast<uint32_t>( chunks.size() );
	const Tr2CpuUsage::Type cpuUsage = Tr2CpuUsage::WRITE_OFTEN | Tr2CpuUsage::NON_SYNCRONIZED_WRITE;

	Chunk chunk;
	const ALResult result = chunk.buffer.Create( stride, chunkSize / stride, gpuUsage, cpuUsage, nullptr, primary );
	if( FAILED( result ) )
	{
		CCP_NOESIS_LOGERR( "Failed to create Noesis dynamic buffer '%s' chunk %u", name.c_str(), index );
		CCP_ASSERT_M( false, "Failed to create Noesis dynamic buffer" );
		return false;
	}

	char chunkName[64];
	sprintf_s( chunkName, "Noesis_%s_%u", name.c_str(), index );
	chunk.buffer.SetName( chunkName );
	chunk.lastUsedFrame = frame;

	chunks.push_back( chunk );
	chunkIndex = index;
	pos = 0;
	return true;
}

// --------------------------------------------------------------------------------------
// Description:
//   Makes a chunk the GPU has finished with current, growing the ring if they are all
//   still in flight.
// Arguments:
//   primary - main-thread primary render context, for the rendered frame number
// Return value:
//   true If there is a chunk to write into
// --------------------------------------------------------------------------------------
bool Tr2NoesisDynamicRing::AcquireChunk( Tr2PrimaryRenderContextAL& primary )
{
	// Chunks already filled this frame carry the recording frame number, which is always
	// ahead of the rendered one, so they are never handed out twice within a frame.
	const uint64_t rendered = primary.GetRenderedFrameNumber();
	for( uint32_t i = 0; i < static_cast<uint32_t>( chunks.size() ); ++i )
	{
		if( chunks[i].lastUsedFrame != frame && chunks[i].lastUsedFrame <= rendered )
		{
			chunks[i].lastUsedFrame = frame;
			chunkIndex = i;
			pos = 0;
			return true;
		}
	}

	return AppendChunk( primary );
}

// --------------------------------------------------------------------------------------
// Description:
//   Starts a new frame's allocations if the recording frame has moved on.
// Arguments:
//   primary - main-thread primary render context, for the recording frame number
// --------------------------------------------------------------------------------------
void Tr2NoesisDynamicRing::SyncFrame( Tr2PrimaryRenderContextAL& primary )
{
	// Keyed on the frame number rather than an index into a fixed set of pages: every view
	// on this device shares the frame's allocations, and a frame Noesis sat out must not
	// leave the bump offset where it was.
	const uint64_t recording = primary.GetRecordingFrameNumber();
	if( recording == frame )
	{
		return;
	}

	CCP_ASSERT_M( !mapped, "Noesis ring still mapped at a frame boundary" );
	if( mapped )
	{
		return;
	}

	if( frameBytes > reportedPeak )
	{
		reportedPeak = frameBytes;
		if( Tr2Noesis::IsLogVerbose() )
		{
			CCP_NOESIS_LOG( "Noesis ring '%s' peak %u bytes in one frame, %u chunk(s) of %u allocated",
							name.c_str(), frameBytes, static_cast<uint32_t>( chunks.size() ), chunkSize );
		}
	}

	frame = recording;
	chunkIndex = INVALID_CHUNK;
	pos = 0;
	frameBytes = 0;
	growthCapped = false;
}

void* Tr2NoesisDynamicRing::Map( uint32_t bytes, Tr2RenderContextAL& context, Tr2PrimaryRenderContextAL& primary )
{
	if( bytes > chunkSize )
	{
		// Only reachable if a ring is created with chunks below the SDK's per-Map cap.
		CCP_NOESIS_LOGERR( "Noesis Map request %u exceeds ring '%s' chunk size %u", bytes, name.c_str(), chunkSize );
		CCP_ASSERT_M( false, "Noesis Map request exceeds ring chunk size" );
		return nullptr;
	}

	CCP_ASSERT_M( !mapped, "Noesis Map without a matching Unmap" );
	if( mapped )
	{
		Unmap( context );
	}

	if( ( chunkIndex == INVALID_CHUNK || pos + bytes > chunkSize ) && !AcquireChunk( primary ) )
	{
		return nullptr;
	}

	void* data = nullptr;
	const ALResult result = chunks[chunkIndex].buffer.MapForWriting( data, context );
	if( FAILED( result ) || data == nullptr )
	{
		CCP_ASSERT_M( false, "Failed to map Noesis dynamic buffer" );
		return nullptr;
	}

	mapped = true;
	drawPos = pos;
	pos += bytes;
	frameBytes += bytes;
	return static_cast<uint8_t*>( data ) + drawPos;
}

void Tr2NoesisDynamicRing::Unmap( Tr2RenderContextAL& context )
{
	if( !mapped )
	{
		return;
	}
	CCP_ASSERT_M( chunkIndex < chunks.size(), "Noesis ring is mapped without a current chunk" );
	if( chunkIndex < chunks.size() )
	{
		chunks[chunkIndex].buffer.UnmapForWriting( context );
	}
	mapped = false;
}

Tr2BufferAL& Tr2NoesisDynamicRing::CurrentChunk()
{
	if( chunkIndex >= chunks.size() )
	{
		CCP_ASSERT_M( false, "Noesis ring has no current chunk" );
		return fallback;
	}
	return chunks[chunkIndex].buffer;
}
