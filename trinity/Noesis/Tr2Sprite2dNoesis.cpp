// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisHost.h"

#include "Noesis/TriStepRenderNoesis.h"
#include "RenderJob/TriRenderJob.h"
#include "Sprite2d/Tr2Sprite2dPickingMask.h"
#include "Sprite2d/Tr2Sprite2dScene.h"
#include "Tr2RenderContext.h"

#include <cmath>

Tr2Sprite2dNoesis::Tr2Sprite2dNoesis( IRoot* /*lockobj*/ )
{
}

Tr2Sprite2dNoesis::~Tr2Sprite2dNoesis()
{
}

void Tr2Sprite2dNoesis::EnsureJob()
{
	if( !m_step )
	{
		if( !m_step.CreateInstance() )
		{
			CCP_NOESIS_LOGERR( "Tr2Sprite2dNoesis failed to create TriStepRenderNoesis" );
			return;
		}
	}

	// Python has already written both by the time the step above first exists, so this is
	// the only place they reach it. Miss either and Execute takes its "nothing wired" path
	// every frame: no drawing, no error, an empty rectangle where the UI should be.
	m_step->SetView( m_view );
	m_step->SetHost( m_host );

	if( !m_job )
	{
		if( !m_job.CreateInstance() )
		{
			CCP_NOESIS_LOGERR( "Tr2Sprite2dNoesis failed to create TriRenderJob" );
			return;
		}
	}

	if( m_job->Steps().empty() )
	{
		m_job->Steps().Append( m_step->GetRawRoot() );
		if( m_job->Steps().empty() )
		{
			CCP_NOESIS_LOGERR( "Tr2Sprite2dNoesis failed to attach TriStepRenderNoesis to its internal job" );
		}
	}
}

bool Tr2Sprite2dNoesis::SyncOverrideViewport( Tr2Sprite2dScene* renderer )
{
	// Sprite layout is in scene pixels with (0,0) at the top-left of the current D3D
	// viewport. A D3D viewport is in render-target pixels, so add the current origin.
	USE_MAIN_THREAD_RENDER_CONTEXT();
	const TriViewport& vp = renderContext.m_esm.GetViewport();

	const Vector2 origin = renderer->TransformPoint( m_translation );
	const int x = vp.x + static_cast<int>( floorf( origin.x + 0.5f ) );
	const int y = vp.y + static_cast<int>( floorf( origin.y + 0.5f ) );
	const int width = static_cast<int>( floorf( m_displayWidth + 0.5f ) );
	const int height = static_cast<int>( floorf( m_displayHeight + 0.5f ) );

	if( width < 1 || height < 1 )
	{
		return false;
	}

	// Layout stays the sprite rect. Parent clipChildren (scroll, clipper, ...) is a
	// tighter scissor so the XAML does not reflow as it scrolls out of view.
	// Only convert a clip edge that actually tightens the sprite, so the
	// unconstrained (-FLT_MAX/FLT_MAX) stack default is never cast to int.
	const Tr2Sprite2dClipRect& clip = renderer->GetClipRectangle();
	int clipLeft = x;
	int clipTop = y;
	int clipRight = x + width;
	int clipBottom = y + height;
	if( clip.left > origin.x )
	{
		clipLeft = vp.x + static_cast<int>( floorf( clip.left ) );
	}
	if( clip.top > origin.y )
	{
		clipTop = vp.y + static_cast<int>( floorf( clip.top ) );
	}
	if( clip.right < origin.x + m_displayWidth )
	{
		clipRight = vp.x + static_cast<int>( ceilf( clip.right ) );
	}
	if( clip.bottom < origin.y + m_displayHeight )
	{
		clipBottom = vp.y + static_cast<int>( ceilf( clip.bottom ) );
	}
	if( clipRight <= clipLeft || clipBottom <= clipTop )
	{
		return false;
	}

	m_step->SetOverrideViewport( x, y, width, height );
	m_step->SetOverrideClip( clipLeft, clipTop, clipRight, clipBottom );

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Tr2Sprite2dNoesis viewport (%d, %d) %dx%d clip (%d,%d)-(%d,%d) (sprite %.0f,%.0f %.0fx%.0f)",
						x, y, width, height,
						clipLeft, clipTop, clipRight, clipBottom,
						origin.x, origin.y, m_displayWidth, m_displayHeight );
	}

	return true;
}

void Tr2Sprite2dNoesis::GatherSprites( Tr2Sprite2dScene* renderer )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_display )
	{
		return;
	}

	const nhi_view_api* viewApi = Nhi::QueryViewApi( m_view );
	if( viewApi == nullptr || !viewApi->is_loaded( viewApi->header.self ) )
	{
		if( Tr2Noesis::IsLogVerbose() )
		{
			CCP_NOESIS_LOG( "Tr2Sprite2dNoesis skip: %s", m_view == nullptr ? "no view" : "view not loaded" );
		}
		return;
	}

	if( m_displayWidth <= 0.0f || m_displayHeight <= 0.0f )
	{
		if( Tr2Noesis::IsLogVerbose() )
		{
			CCP_NOESIS_LOG( "Tr2Sprite2dNoesis skip: display size %.0fx%.0f", m_displayWidth, m_displayHeight );
		}
		return;
	}

	EnsureJob();
	if( !m_job || !m_step || m_job->Steps().empty() )
	{
		return;
	}

	if( !SyncOverrideViewport( renderer ) )
	{
		return;
	}

	renderer->RunJob( m_job );
}

ITr2SpriteObject* Tr2Sprite2dNoesis::PickPoint( float x, float y, Tr2Sprite2dScene* renderer )
{
	if( !m_display )
	{
		return NULL;
	}

	if( m_pickState == TR2_SPS_ON )
	{
		if( renderer->IsInside( Vector2( x, y ), m_translation, m_displayWidth, m_displayHeight, 0.0f ) )
		{
			if( !m_pickingMask || m_pickingMask->SampleMask( renderer->InverseTransformPoint( Vector2( x, y ) ), m_translation, m_displayWidth, m_displayHeight ) )
			{
				return this;
			}
		}
	}

	return NULL;
}

unsigned int Tr2Sprite2dNoesis::GetVertexCount()
{
	return 0;
}
