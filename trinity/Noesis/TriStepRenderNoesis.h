// Copyright © 2026 CCP ehf.

#pragma once
#ifndef TriStepRenderNoesis_H
#define TriStepRenderNoesis_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisView.h"
#include "RenderJob/TriRenderStep.h"

// --------------------------------------------------------------------------------------
// Description:
//   Renders one Tr2NoesisView into whatever render target the job has bound.
//
//   The whole per-frame sequence lives here -- size, Update, UpdateRenderTree, the
//   offscreen phase and the onscreen draw -- because Noesis requires that exact order
//   and a Python-driven equivalent would be able to get it wrong.
// --------------------------------------------------------------------------------------

BLUE_DECLARE( TriStepRenderNoesis );

class TriStepRenderNoesis : public TriRenderStep
{
public:
	EXPOSE_TO_BLUE();
	TriStepRenderNoesis( IRoot* lockobj = NULL );

	// IRenderStep
	TriStepResult Execute( Be::Time realTime, Be::Time simTime, Tr2RenderContext& renderContext );

	void py__init__( Tr2NoesisView* view );

private:
	Tr2NoesisViewPtr m_view;
};

TYPEDEF_BLUECLASS( TriStepRenderNoesis );

#endif

#endif
