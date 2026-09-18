// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisBatchStats_H
#define Tr2NoesisBatchStats_H

#include <stdint.h>

#include <string>
#include <vector>

// --------------------------------------------------------------------------------------
// Description:
//   What the device reports about the batches it drew, and about the ones it could not.
//
//   Per-shader counts reported once a frame and only when they change, plus a latch so an
//   unwired permutation is named once rather than at batch rate. Apart from the device
//   because none of it affects a draw: it is what someone reads when the UI looks wrong.
// --------------------------------------------------------------------------------------
class Tr2NoesisBatchStats
{
public:
	// Sized once the shader count is known. Names are library-owned and outlive this.
	void Reset( const std::vector<const char*>& shaderNames );

	void CountBatch( uint8_t shader );

	// True the first time this shader is reported, so the caller logs once.
	bool NoteUnwired( uint8_t shader );

	// Ends the frame, clearing the counts.
	//
	// With `wantHistogram`, returns the per-shader histogram -- or empty when this frame
	// matches the one already reported -- and sets `outTotal`. Without it nothing is
	// formatted and nothing is remembered as reported, so a build that never logs does no
	// work here beyond the clear.
	std::string EndFrame( bool wantHistogram, uint32_t& outTotal );

	// One batch per frame is logged in detail; this says whether the next one is it.
	bool WantsBatchDetail() const;
	void ClearBatchDetail();

private:
	std::vector<const char*> m_names;
	std::vector<uint32_t> m_counts;
	std::vector<uint32_t> m_reported;
	std::vector<bool> m_unwiredReported;
	bool m_logBatchDetail = true;
};

#endif
