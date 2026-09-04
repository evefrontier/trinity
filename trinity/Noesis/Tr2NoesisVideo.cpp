// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisVideo.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisRenderDevice.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/String.h>
#include <NsGui/DrawingContext.h>
#include <NsGui/DynamicTextureSource.h>
#include <NsGui/Enums.h>
#include <NsGui/FrameworkPropertyMetadata.h>
#ifdef NS_HAVE_STUDIO
#include <NsGui/StudioMeta.h>
#endif
#include <NsGui/UIElementData.h>
#include <NsGui/Viewbox.h>

#include <algorithm>
#include <mutex>

namespace
{

#if BLUE_WITH_PYTHON
PyObject* s_videoFactory = nullptr;
#endif

}

class Tr2NoesisVideo::VideoRenderer final : public Noesis::DynamicTextureSource::Renderer
{
public:
	VideoRenderer() :
		m_device( nullptr )
	{
	}

	void Publish( TriTextureRes* texture )
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		if( m_publishedTexture == texture )
		{
			return;
		}

		m_publishedTexture = texture;
		m_wrappedTexture.Reset();
	}

	void Init( Noesis::RenderDevice* device ) override
	{
		m_device = device;
	}

	Noesis::Texture* Render( uint32_t, uint32_t ) override
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		if( m_wrappedTexture == nullptr && m_publishedTexture )
		{
			Tr2TextureAL* texture = m_publishedTexture->GetTexture();
			if( texture != nullptr && texture->IsValid() )
			{
				Tr2NoesisRenderDevice* renderDevice = static_cast<Tr2NoesisRenderDevice*>( m_device );
				if( renderDevice != nullptr )
				{
					m_wrappedTexture = renderDevice->WrapTexture( *texture, true );
				}
			}
		}
		return m_wrappedTexture;
	}

	void Shutdown() override
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		m_wrappedTexture.Reset();
		m_device = nullptr;
	}

private:
	std::mutex m_mutex;
	TriTextureResPtr m_publishedTexture;
	Noesis::Ptr<Noesis::Texture> m_wrappedTexture;
	Noesis::RenderDevice* m_device;
};

Tr2NoesisVideo::Tr2NoesisVideo() :
	m_videoRenderer( Noesis::MakePtr<VideoRenderer>() ),
	m_textureSource( Noesis::MakePtr<Noesis::DynamicTextureSource>( 1, 1, m_videoRenderer ) ),
	m_textureWidth( 0 ),
	m_textureHeight( 0 ),
	m_endedUpdatingIsPlaying( false )
#if BLUE_WITH_PYTHON
	, m_session( nullptr )
#endif
{
	Loaded() += Noesis::MakeDelegate( this, &Tr2NoesisVideo::OnLoaded );
	Unloaded() += Noesis::MakeDelegate( this, &Tr2NoesisVideo::OnUnloaded );
}

Tr2NoesisVideo::~Tr2NoesisVideo()
{
	Loaded() -= Noesis::MakeDelegate( this, &Tr2NoesisVideo::OnLoaded );
	Unloaded() -= Noesis::MakeDelegate( this, &Tr2NoesisVideo::OnUnloaded );
	DestroySession( false );
}

const char* Tr2NoesisVideo::GetSource() const
{
	return GetValue<Noesis::String>( SourceProperty ).Str();
}

void Tr2NoesisVideo::SetSource( const char* source )
{
	SetValue<Noesis::String>( SourceProperty, source != nullptr ? source : "" );
}

Noesis::Stretch Tr2NoesisVideo::GetStretch() const
{
	return GetValue<Noesis::Stretch>( StretchProperty );
}

void Tr2NoesisVideo::SetStretch( Noesis::Stretch value )
{
	SetValue<Noesis::Stretch>( StretchProperty, value );
}

bool Tr2NoesisVideo::GetAutoPlay() const
{
	return GetValue<bool>( AutoPlayProperty );
}

void Tr2NoesisVideo::SetAutoPlay( bool value )
{
	SetValue<bool>( AutoPlayProperty, value );
}

bool Tr2NoesisVideo::GetLoop() const
{
	return GetValue<bool>( LoopProperty );
}

void Tr2NoesisVideo::SetLoop( bool value )
{
	SetValue<bool>( LoopProperty, value );
}

bool Tr2NoesisVideo::GetIsPlaying() const
{
	return GetValue<bool>( IsPlayingProperty );
}

void Tr2NoesisVideo::SetIsPlaying( bool value )
{
	SetValue<bool>( IsPlayingProperty, value );
}

float Tr2NoesisVideo::GetVolume() const
{
	return GetValue<float>( VolumeProperty );
}

void Tr2NoesisVideo::SetVolume( float value )
{
	SetValue<float>( VolumeProperty, value );
}

bool Tr2NoesisVideo::GetIsMuted() const
{
	return GetValue<bool>( IsMutedProperty );
}

void Tr2NoesisVideo::SetIsMuted( bool value )
{
	SetValue<bool>( IsMutedProperty, value );
}

void Tr2NoesisVideo::Play()
{
	SetIsPlaying( true );
}

void Tr2NoesisVideo::Pause()
{
	SetIsPlaying( false );
}

void Tr2NoesisVideo::SetTexture( TriTextureRes* texture )
{
	uint32_t width = 0;
	uint32_t height = 0;
	bool dimensionsChanged = false;
	{
		std::lock_guard<std::mutex> lock( m_measureMutex );
		TriTextureRes* current = m_texture;
		if( current == texture )
		{
			return;
		}

		m_texture = texture;

		Tr2TextureAL* videoTexture = m_texture ? m_texture->GetTexture() : nullptr;
		if( videoTexture != nullptr && videoTexture->IsValid() )
		{
			width = videoTexture->GetWidth();
			height = videoTexture->GetHeight();
		}

		dimensionsChanged = width != m_textureWidth || height != m_textureHeight;
		m_textureWidth = width;
		m_textureHeight = height;
	}

	m_videoRenderer->Publish( texture );

	if( dimensionsChanged )
	{
		m_textureSource->Resize( std::max( width, 1u ), std::max( height, 1u ) );
		InvalidateMeasure();
	}

	// VideoPlayer updates the native texture in place. Noesis only needs a new render request
	// when the backing texture changes, not once per decoded frame.
	m_textureSource->Update();
	InvalidateRender();
}

void Tr2NoesisVideo::Ended()
{
	SetIsPlayingFromPlayer( false );
}

void Tr2NoesisVideo::OnRender( Noesis::DrawingContext* context )
{
	context->DrawImage( m_textureSource, Noesis::Rect( mRenderSize ) );
}

Noesis::Size Tr2NoesisVideo::MeasureOverride( const Noesis::Size& availableSize )
{
	return MeasureArrangeSize( availableSize );
}

Noesis::Size Tr2NoesisVideo::ArrangeOverride( const Noesis::Size& finalSize )
{
	return MeasureArrangeSize( finalSize );
}

Noesis::Size Tr2NoesisVideo::MeasureArrangeSize( const Noesis::Size& size ) const
{
	uint32_t width = 0;
	uint32_t height = 0;
	{
		std::lock_guard<std::mutex> lock( m_measureMutex );
		width = m_textureWidth;
		height = m_textureHeight;
	}

	const Noesis::Size contentSize( static_cast<float>( width ), static_cast<float>( height ) );
	const Noesis::Point scale = Noesis::Viewbox::GetStretchScale(
		contentSize, size, GetStretch(), Noesis::StretchDirection_Both );
	return Noesis::Size( contentSize.width * scale.x, contentSize.height * scale.y );
}

#if BLUE_WITH_PYTHON

Tr2NoesisVideoSink::Tr2NoesisVideoSink( IRoot* ) :
	m_video( nullptr )
{
}

Tr2NoesisVideoSink::~Tr2NoesisVideoSink()
{
	Detach();
}

void Tr2NoesisVideoSink::Attach( Tr2NoesisVideo* video )
{
	std::lock_guard<std::mutex> lock( m_mutex );
	m_video = video;
}

void Tr2NoesisVideoSink::Detach()
{
	std::lock_guard<std::mutex> lock( m_mutex );
	m_video = nullptr;
}

void Tr2NoesisVideoSink::SetTexture( TriTextureRes* texture )
{
	std::lock_guard<std::mutex> lock( m_mutex );
	if( m_video != nullptr )
	{
		m_video->SetTexture( texture );
	}
}

void Tr2NoesisVideoSink::Ended()
{
	std::lock_guard<std::mutex> lock( m_mutex );
	if( m_video != nullptr )
	{
		m_video->Ended();
	}
}

#endif

void Tr2NoesisVideo::CreateSession()
{
#if BLUE_WITH_PYTHON
	if( m_session != nullptr || !IsLoaded() || GetSource()[0] == '\0' )
#else
	if( !IsLoaded() || GetSource()[0] == '\0' )
#endif
	{
		return;
	}

#if BLUE_WITH_PYTHON
	auto gil = PyGILState_Ensure();
	ON_BLOCK_EXIT( [&gil] { PyGILState_Release( gil ); } );

	if( s_videoFactory == nullptr )
	{
		SetIsPlayingFromPlayer( false );
		return;
	}

	m_sink.Attach( new OTr2NoesisVideoSink() );
	m_sink->Attach( this );

	PyObject* sinkObject = BlueWrapObjectForPython( m_sink->GetRawRoot() );
	PyObject* args = sinkObject != nullptr ? Py_BuildValue( "(sOfOOO)",
															GetSource(),
															GetLoop() ? Py_True : Py_False,
															static_cast<double>( GetVolume() ),
															GetIsMuted() ? Py_True : Py_False,
															GetIsPlaying() ? Py_True : Py_False,
															sinkObject ) : nullptr;
	Py_XDECREF( sinkObject );

	PyObject* session = args != nullptr ? PyObject_CallObject( s_videoFactory, args ) : nullptr;
	Py_XDECREF( args );

	if( session == nullptr )
	{
		CCP_NOESIS_LOGERR( "Video factory failed for source '%s'", GetSource() );
		PyOS->PyFlushError( "Tr2NoesisVideo: video factory failed" );
		m_sink->Detach();
		m_sink = nullptr;
		SetTexture( nullptr );
		SetIsPlayingFromPlayer( false );
		return;
	}

	if( session == Py_None )
	{
		Py_DECREF( session );
		m_sink->Detach();
		m_sink = nullptr;
		SetTexture( nullptr );
		SetIsPlayingFromPlayer( false );
		return;
	}

	m_session = session;
#else
	SetIsPlayingFromPlayer( false );
#endif
}

void Tr2NoesisVideo::DestroySession( bool updateLayout )
{
#if BLUE_WITH_PYTHON
	if( m_sink != nullptr )
	{
		m_sink->Detach();
	}

	auto gil = PyGILState_Ensure();
	if( m_session != nullptr )
	{
		PyObject* session = m_session;
		m_session = nullptr;
		PyObject* result = PyObject_CallMethod( session, "destroy", nullptr );
		if( result == nullptr )
		{
			CCP_NOESIS_LOGERR( "Video session destroy() failed" );
			PyOS->PyFlushError( "Tr2NoesisVideo: session destroy failed" );
		}
		Py_XDECREF( result );
		Py_DECREF( session );
	}
	PyGILState_Release( gil );
	m_sink = nullptr;
#endif

	if( updateLayout )
	{
		SetTexture( nullptr );
	}
	else
	{
		m_videoRenderer->Publish( nullptr );
		std::lock_guard<std::mutex> lock( m_measureMutex );
		m_texture = nullptr;
		m_textureWidth = 0;
		m_textureHeight = 0;
	}
}

void Tr2NoesisVideo::CallSessionMethod( const char* method )
{
#if BLUE_WITH_PYTHON
	auto gil = PyGILState_Ensure();
	if( m_session == nullptr )
	{
		PyGILState_Release( gil );
		return;
	}

	PyObject* result = PyObject_CallMethod( m_session, method, nullptr );
	if( result == nullptr )
	{
		CCP_NOESIS_LOGERR( "Video session %s() failed", method );
		PyOS->PyFlushError( "Tr2NoesisVideo: session method failed" );
	}
	Py_XDECREF( result );
	PyGILState_Release( gil );
#else
	( void )method;
#endif
}

void Tr2NoesisVideo::CallSessionMethod( const char* method, float value )
{
#if BLUE_WITH_PYTHON
	auto gil = PyGILState_Ensure();
	if( m_session == nullptr )
	{
		PyGILState_Release( gil );
		return;
	}

	PyObject* result = PyObject_CallMethod( m_session, method, "f", static_cast<double>( value ) );
	if( result == nullptr )
	{
		CCP_NOESIS_LOGERR( "Video session %s(%g) failed", method, value );
		PyOS->PyFlushError( "Tr2NoesisVideo: session method failed" );
	}
	Py_XDECREF( result );
	PyGILState_Release( gil );
#else
	( void )method;
	( void )value;
#endif
}

void Tr2NoesisVideo::CallSessionMethod( const char* method, bool value )
{
#if BLUE_WITH_PYTHON
	auto gil = PyGILState_Ensure();
	if( m_session == nullptr )
	{
		PyGILState_Release( gil );
		return;
	}

	PyObject* result = PyObject_CallMethod( m_session, method, "O", value ? Py_True : Py_False );
	if( result == nullptr )
	{
		CCP_NOESIS_LOGERR( "Video session %s(%s) failed",
						   method, value ? "true" : "false" );
		PyOS->PyFlushError( "Tr2NoesisVideo: session method failed" );
	}
	Py_XDECREF( result );
	PyGILState_Release( gil );
#else
	( void )method;
	( void )value;
#endif
}

void Tr2NoesisVideo::CallSessionMethod( const char* method, const char* value )
{
#if BLUE_WITH_PYTHON
	auto gil = PyGILState_Ensure();
	if( m_session == nullptr )
	{
		PyGILState_Release( gil );
		return;
	}

	PyObject* result = PyObject_CallMethod( m_session, method, "s", value != nullptr ? value : "" );
	if( result == nullptr )
	{
		CCP_NOESIS_LOGERR( "Video session %s('%s') failed", method, value != nullptr ? value : "" );
		PyOS->PyFlushError( "Tr2NoesisVideo: session method failed" );
	}
	Py_XDECREF( result );
	PyGILState_Release( gil );
#else
	( void )method;
	( void )value;
#endif
}

void Tr2NoesisVideo::SetIsPlayingFromPlayer( bool value )
{
	// Ended() / factory failure write IsPlaying without resume/pause on the session.
	m_endedUpdatingIsPlaying = true;
	ON_BLOCK_EXIT( [&] { m_endedUpdatingIsPlaying = false; } );
	SetIsPlaying( value );
}

void Tr2NoesisVideo::ApplyAutoPlayOnCreate()
{
	if( GetAutoPlay() && !GetIsPlaying() )
	{
		SetIsPlaying( true );
	}
}

void Tr2NoesisVideo::OnLoaded( Noesis::BaseComponent*, const Noesis::RoutedEventArgs& )
{
	ApplyAutoPlayOnCreate();
	CreateSession();
}

void Tr2NoesisVideo::OnUnloaded( Noesis::BaseComponent*, const Noesis::RoutedEventArgs& )
{
	DestroySession( true );
}

void Tr2NoesisVideo::OnSourceChanged( Noesis::DependencyObject* object,
									   const Noesis::DependencyPropertyChangedEventArgs& )
{
	Tr2NoesisVideo* video = static_cast<Tr2NoesisVideo*>( object );
	if( video->GetSource()[0] == '\0' )
	{
		video->DestroySession( true );
		video->SetIsPlayingFromPlayer( false );
		return;
	}

#if BLUE_WITH_PYTHON
	if( video->m_session != nullptr )
	{
		video->CallSessionMethod( "set_source", video->GetSource() );
		return;
	}
#endif
	video->ApplyAutoPlayOnCreate();
	video->CreateSession();
}

void Tr2NoesisVideo::OnLoopChanged( Noesis::DependencyObject* object,
									 const Noesis::DependencyPropertyChangedEventArgs& )
{
	Tr2NoesisVideo* video = static_cast<Tr2NoesisVideo*>( object );
	video->CallSessionMethod( "set_loop", video->GetLoop() );
}

void Tr2NoesisVideo::OnIsPlayingChanged( Noesis::DependencyObject* object,
										  const Noesis::DependencyPropertyChangedEventArgs& e )
{
	Tr2NoesisVideo* video = static_cast<Tr2NoesisVideo*>( object );
	if( video->m_endedUpdatingIsPlaying )
	{
		return;
	}

	video->CallSessionMethod( e.NewValue<bool>() ? "resume" : "pause" );
}

void Tr2NoesisVideo::OnVolumeChanged( Noesis::DependencyObject* object,
									  const Noesis::DependencyPropertyChangedEventArgs& e )
{
	static_cast<Tr2NoesisVideo*>( object )->CallSessionMethod( "set_volume", e.NewValue<float>() );
}

void Tr2NoesisVideo::OnIsMutedChanged( Noesis::DependencyObject* object,
									   const Noesis::DependencyPropertyChangedEventArgs& e )
{
	static_cast<Tr2NoesisVideo*>( object )->CallSessionMethod( "set_muted", e.NewValue<bool>() );
}

bool Tr2NoesisVideo::CoerceVolume( const Noesis::DependencyObject*, const void* baseValue, void* coercedValue )
{
	const float value = *static_cast<const float*>( baseValue );
	*static_cast<float*>( coercedValue ) = std::max( 0.0f, std::min( value, 1.0f ) );
	return true;
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION( Tr2NoesisVideo, "Trinity.Video" )
{
#ifdef NS_HAVE_STUDIO
	NsMeta<Noesis::StudioOrder>( 2000, "Media" );
	NsMeta<Noesis::StudioName>( "Video" );
	NsMeta<Noesis::StudioDesc>( "Plays a video file through the carbon VideoPlayer" );
	NsMeta<Noesis::StudioIcon>( Noesis::Uri::Pack( "Media", "#Media-Icons" ), 0xE900 );

	NsProp( "Source", &Tr2NoesisVideo::GetSource, &Tr2NoesisVideo::SetSource )
		.Meta<Noesis::StudioOrder>( 0 )
		.Meta<Noesis::StudioUri>( Noesis::StudioUri::UriType::Video );
	NsProp( "Stretch", &Tr2NoesisVideo::GetStretch, &Tr2NoesisVideo::SetStretch )
		.Meta<Noesis::StudioOrder>( 1 );
	NsProp( "AutoPlay", &Tr2NoesisVideo::GetAutoPlay, &Tr2NoesisVideo::SetAutoPlay )
		.Meta<Noesis::StudioOrder>( 2 );
	NsProp( "Loop", &Tr2NoesisVideo::GetLoop, &Tr2NoesisVideo::SetLoop )
		.Meta<Noesis::StudioOrder>( 3 );
	NsProp( "IsPlaying", &Tr2NoesisVideo::GetIsPlaying, &Tr2NoesisVideo::SetIsPlaying )
		.Meta<Noesis::StudioOrder>( 4 );
	NsProp( "Volume", &Tr2NoesisVideo::GetVolume, &Tr2NoesisVideo::SetVolume )
		.Meta<Noesis::StudioOrder>( 5 )
		.Meta<Noesis::StudioRange>( 0.0f, 1.0f );
	NsProp( "IsMuted", &Tr2NoesisVideo::GetIsMuted, &Tr2NoesisVideo::SetIsMuted )
		.Meta<Noesis::StudioOrder>( 6, false );
#endif

	Noesis::UIElementData* data = NsMeta<Noesis::UIElementData>( Noesis::TypeOf<SelfClass>() );
	data->RegisterProperty<Noesis::String>(
		SourceProperty,
		"Source",
		Noesis::FrameworkPropertyMetadata::Create(
			Noesis::String(),
			Noesis::FrameworkPropertyMetadataOptions_AffectsMeasure |
				Noesis::FrameworkPropertyMetadataOptions_AffectsRender,
			Noesis::PropertyChangedCallback( OnSourceChanged ) ) );
	data->RegisterProperty<Noesis::Stretch>(
		StretchProperty,
		"Stretch",
		Noesis::FrameworkPropertyMetadata::Create(
			Noesis::Stretch_Uniform,
			Noesis::FrameworkPropertyMetadataOptions_AffectsMeasure ) );
	// AutoPlay is create-time policy only (default true). Before CreateSession
	// from OnLoaded or OnSourceChanged with no session: if AutoPlay && !IsPlaying,
	// SetIsPlaying(true). Not consulted on Loop/Source restart or runtime change.
	data->RegisterProperty<bool>(
		AutoPlayProperty,
		"AutoPlay",
		Noesis::FrameworkPropertyMetadata::Create( true ) );
	data->RegisterProperty<bool>(
		LoopProperty,
		"Loop",
		Noesis::FrameworkPropertyMetadata::Create(
			false, Noesis::PropertyChangedCallback( OnLoopChanged ) ) );
	// IsPlaying is the live play/pause command and status (default false).
	data->RegisterProperty<bool>(
		IsPlayingProperty,
		"IsPlaying",
		Noesis::FrameworkPropertyMetadata::Create(
			false,
			Noesis::FrameworkPropertyMetadataOptions_BindsTwoWayByDefault,
			Noesis::PropertyChangedCallback( OnIsPlayingChanged ) ) );
	data->RegisterProperty<float>(
		VolumeProperty,
		"Volume",
		Noesis::FrameworkPropertyMetadata::Create(
			1.0f,
			Noesis::FrameworkPropertyMetadataOptions_None,
			Noesis::PropertyChangedCallback( OnVolumeChanged ),
			Noesis::CoerceValueCallback( CoerceVolume ) ) );
	data->RegisterProperty<bool>(
		IsMutedProperty,
		"IsMuted",
		Noesis::FrameworkPropertyMetadata::Create(
			false, Noesis::PropertyChangedCallback( OnIsMutedChanged ) ) );
}

NS_END_COLD_REGION

const Noesis::DependencyProperty* Tr2NoesisVideo::SourceProperty;
const Noesis::DependencyProperty* Tr2NoesisVideo::StretchProperty;
const Noesis::DependencyProperty* Tr2NoesisVideo::AutoPlayProperty;
const Noesis::DependencyProperty* Tr2NoesisVideo::LoopProperty;
const Noesis::DependencyProperty* Tr2NoesisVideo::IsPlayingProperty;
const Noesis::DependencyProperty* Tr2NoesisVideo::VolumeProperty;
const Noesis::DependencyProperty* Tr2NoesisVideo::IsMutedProperty;

namespace Tr2Noesis
{

#if BLUE_WITH_PYTHON

void SetVideoFactory( PyObject* factory )
{
	Py_XINCREF( factory );
	Py_XDECREF( s_videoFactory );
	s_videoFactory = factory;
}

#endif

}

#endif
