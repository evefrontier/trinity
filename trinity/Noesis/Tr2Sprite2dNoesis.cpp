// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2Sprite2dNoesis.h"

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisPynrInterface.h"
#include "Noesis/Tr2NoesisRenderDevice.h"

#include "Noesis/TriStepRenderNoesis.h"
#include "RenderJob/TriRenderJob.h"
#include "Sprite2d/Tr2Sprite2dPickingMask.h"
#include "Sprite2d/Tr2Sprite2dScene.h"
#include "Tr2RenderContext.h"

#include <cmath>

Tr2Sprite2dNoesis::Tr2Sprite2dNoesis( IRoot* /*lockobj*/ ) :
	m_view( nullptr )
{
}

Tr2Sprite2dNoesis::~Tr2Sprite2dNoesis()
{
	SetView( nullptr );
}

void Tr2Sprite2dNoesis::SetView( const pynr_view* view )
{
	if( view == m_view )
	{
		return;
	}

	// Retain before releasing, so re-setting the same view is not a free-then-use.
	if( view != nullptr && view->header.retain != nullptr )
	{
		view->header.retain( view->header.self );
	}
	if( m_view != nullptr && m_view->header.release != nullptr )
	{
		m_view->header.release( m_view->header.self );
	}
	m_view = view;
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
	m_step->SetRenderDevice( m_renderDevice );

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

	return true;
}

void Tr2Sprite2dNoesis::GatherSprites( Tr2Sprite2dScene* renderer )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_display )
	{
		return;
	}

	if( m_view == nullptr || !m_view->is_loaded( m_view->header.self ) )
	{
		return;
	}

	if( m_displayWidth <= 0.0f || m_displayHeight <= 0.0f )
	{
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
			const Vector2 local = renderer->InverseTransformPoint( Vector2( x, y ) );
			if( !m_pickingMask || m_pickingMask->SampleMask( local, m_translation, m_displayWidth, m_displayHeight ) )
			{
				// The rectangle is the view's, not its content's. A view over other sprites
				// -- the HUD over the space scene -- must take only the picks that land on
				// a hit-testable element, and let the rest fall through to what is beneath,
				// so the view is asked before the sprite claims the point. The point goes
				// across in the sprite's own pixels, origin at its top-left, which is the
				// convention the view's mouse entries already take.
				if( !ViewHasPoint( local - m_translation ) )
				{
					return NULL;
				}
				return this;
			}
		}
	}

	return NULL;
}

bool Tr2Sprite2dNoesis::ViewHasPoint( const Vector2& point ) const
{
	// No view is the plain sprite it always was: an opaque rectangle. A view from a
	// pynoesis older than ABI 2.1 has no hit_test slot, and Tr2NoesisTakePynrInterface
	// admits one on purpose so the two can move independently; it reads as opaque too.
	if( m_view == nullptr || !Tr2NoesisViewHasHitTest( m_view ) )
	{
		return true;
	}

	return m_view->hit_test( m_view->header.self, point.x, point.y ) != PYNR_FALSE;
}

unsigned int Tr2Sprite2dNoesis::GetVertexCount()
{
	return 0;
}
