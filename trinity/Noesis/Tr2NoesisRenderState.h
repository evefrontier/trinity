// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisRenderState_H
#define Tr2NoesisRenderState_H

#include <nxt.h>

#include <stdint.h>

// --------------------------------------------------------------------------------------
// Description:
//   One batch's render state, as the flat state/value pairs Tr2RenderContext takes.
//
//   A pure translation from nxt_batch, with no device and no context in it, so the
//   blend and stencil policy can be read on its own. The device issues what comes back.
//
//   Emitted in full for every batch, never as a delta: any state left out can survive
//   from whichever render step ran before this one.
// --------------------------------------------------------------------------------------

// Two entries per state. Sized for the worst-case batch, which is well under this.
const uint32_t TR2_NOESIS_RENDER_STATE_ENTRIES = 2 * 32;

// Fills `pairs` and returns how many ENTRIES were written -- SetRenderStates wants half
// that, being a count of pairs.
uint32_t Tr2NoesisBuildRenderStates( const nxt_batch& batch,
									 uint32_t ( &pairs )[TR2_NOESIS_RENDER_STATE_ENTRIES] );

#endif
