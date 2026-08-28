// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"
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

void Tr2Sprite2dNoesis::SetView( Tr2NoesisView* view )
{
	m_view = view;
	if( m_step )
	{
		m_step->SetView( m_view );
	}
}

Tr2NoesisView* Tr2Sprite2dNoesis::GetView() const
{
	return m_view;
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

	// Python writes m_view through the Blue attribute; keep the step in sync every gather.
	m_step->SetView( m_view );

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

	m_step->SetOverrideViewport( x, y, width, height );

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "Tr2Sprite2dNoesis viewport (%d, %d) %dx%d (sprite %.0f,%.0f %.0fx%.0f)",
						x, y, width, height,
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

	if( !m_view || !m_view->GetIsLoaded() )
	{
		if( Tr2Noesis::IsLogVerbose() )
		{
			CCP_NOESIS_LOG( "Tr2Sprite2dNoesis skip: %s", !m_view ? "no view" : "view not loaded" );
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

#endif
