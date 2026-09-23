// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisRenderState_H
#define Tr2NoesisRenderState_H

#include <pynr.h>

#include <stdint.h>

// --------------------------------------------------------------------------------------
// Description:
//   One batch's render state, as the flat state/value pairs Tr2RenderContext takes.
//
//   A pure translation, so the blend and stencil policy can be read on its own. The
//   device issues what comes back.
//
//   Every batch gets the full set, never a delta: any state left out can survive from
//   whichever render step ran before this one.
// --------------------------------------------------------------------------------------

// Two entries per state. Sized for the worst-case batch, which is well under this.
const uint32_t TR2_NOESIS_RENDER_STATE_ENTRIES = 2 * 32;

// Fills `pairs` and returns the number of entries written. SetRenderStates counts pairs,
// so it wants half that.
uint32_t Tr2NoesisBuildRenderStates( const pynr_batch& batch,
									 uint32_t ( &pairs )[TR2_NOESIS_RENDER_STATE_ENTRIES] );

#endif
