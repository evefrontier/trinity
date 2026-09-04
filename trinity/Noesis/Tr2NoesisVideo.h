// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisVideo_H
#define Tr2NoesisVideo_H

#if WITH_NOESIS

#include "Resources/TriTextureRes.h"

#include <NsCore/Ptr.h>
#include <NsGui/FrameworkElement.h>

#include <mutex>

#if BLUE_WITH_PYTHON
#ifndef PyObject_HEAD
struct _object;
typedef struct _object PyObject;
#endif
#endif

namespace Noesis
{
class DrawingContext;
class DynamicTextureSource;
enum Stretch: int32_t;
}

#if BLUE_WITH_PYTHON
BLUE_DECLARE( Tr2NoesisVideoSink );
#endif

class Tr2NoesisVideo final : public Noesis::FrameworkElement
{
public:
	Tr2NoesisVideo();
	~Tr2NoesisVideo();

	const char* GetSource() const;
	void SetSource( const char* source );

	Noesis::Stretch GetStretch() const;
	void SetStretch( Noesis::Stretch value );

	bool GetAutoPlay() const;
	void SetAutoPlay( bool value );

	bool GetLoop() const;
	void SetLoop( bool value );

	bool GetIsPlaying() const;
	void SetIsPlaying( bool value );

	float GetVolume() const;
	void SetVolume( float value );

	bool GetIsMuted() const;
	void SetIsMuted( bool value );

	void Play();
	void Pause();

	void SetTexture( TriTextureRes* texture );
	void Ended();

	static const Noesis::DependencyProperty* SourceProperty;
	static const Noesis::DependencyProperty* StretchProperty;
	static const Noesis::DependencyProperty* AutoPlayProperty;
	static const Noesis::DependencyProperty* LoopProperty;
	static const Noesis::DependencyProperty* IsPlayingProperty;
	static const Noesis::DependencyProperty* VolumeProperty;
	static const Noesis::DependencyProperty* IsMutedProperty;

protected:
	void OnRender( Noesis::DrawingContext* context ) override;
	Noesis::Size MeasureOverride( const Noesis::Size& availableSize ) override;
	Noesis::Size ArrangeOverride( const Noesis::Size& finalSize ) override;

private:
	class VideoRenderer;

	Noesis::Size MeasureArrangeSize( const Noesis::Size& size ) const;
	void ApplyAutoPlayOnCreate();
	void CreateSession();
	void DestroySession( bool updateLayout );
	void CallSessionMethod( const char* method );
	void CallSessionMethod( const char* method, float value );
	void CallSessionMethod( const char* method, bool value );
	void CallSessionMethod( const char* method, const char* value );
	void SetIsPlayingFromPlayer( bool value );
	void OnLoaded( Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& e );
	void OnUnloaded( Noesis::BaseComponent* sender, const Noesis::RoutedEventArgs& e );

	static void OnSourceChanged( Noesis::DependencyObject* object,
								 const Noesis::DependencyPropertyChangedEventArgs& e );
	static void OnLoopChanged( Noesis::DependencyObject* object,
							   const Noesis::DependencyPropertyChangedEventArgs& e );
	static void OnIsPlayingChanged( Noesis::DependencyObject* object,
									const Noesis::DependencyPropertyChangedEventArgs& e );
	static void OnVolumeChanged( Noesis::DependencyObject* object,
								 const Noesis::DependencyPropertyChangedEventArgs& e );
	static void OnIsMutedChanged( Noesis::DependencyObject* object,
								  const Noesis::DependencyPropertyChangedEventArgs& e );
	static bool CoerceVolume( const Noesis::DependencyObject* object,
							  const void* baseValue, void* coercedValue );

	Noesis::Ptr<VideoRenderer> m_videoRenderer;
	Noesis::Ptr<Noesis::DynamicTextureSource> m_textureSource;
	mutable std::mutex m_measureMutex;
	TriTextureResPtr m_texture;
	uint32_t m_textureWidth;
	uint32_t m_textureHeight;
	// Reentry guard so Ended() can write IsPlaying without calling session pause().
	// Always RAII-owned by ON_BLOCK_EXIT in SetIsPlayingFromPlayer.
	bool m_endedUpdatingIsPlaying;
#if BLUE_WITH_PYTHON
	Tr2NoesisVideoSinkPtr m_sink;
	PyObject* m_session;
#endif

	NS_DECLARE_REFLECTION( Tr2NoesisVideo, Noesis::FrameworkElement )
};

#if BLUE_WITH_PYTHON

class Tr2NoesisVideoSink : public IRoot
{
public:
	EXPOSE_TO_BLUE();
	Tr2NoesisVideoSink( IRoot* lockobj = NULL );
	~Tr2NoesisVideoSink();

	void Attach( Tr2NoesisVideo* video );
	void Detach();
	void SetTexture( TriTextureRes* texture );
	void Ended();

private:
	std::mutex m_mutex;
	Tr2NoesisVideo* m_video;
};

TYPEDEF_BLUECLASS( Tr2NoesisVideoSink );

#endif

namespace Tr2Noesis
{

#if BLUE_WITH_PYTHON
void SetVideoFactory( PyObject* factory );
#endif

}

#endif

#endif
