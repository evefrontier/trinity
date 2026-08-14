// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisXamlProvider.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsGui/Stream.h>
#include <NsGui/Uri.h>

namespace
{

// Noesis::MemoryStream does not own its buffer, and the resource has to outlive LoadXaml,
// so the stream owns the bytes and frees them when Noesis releases its last reference.
class Tr2NoesisResourceStream final : public Noesis::Stream
{
public:
	Tr2NoesisResourceStream( uint8_t* data, uint32_t size ) :
		m_data( data ),
		m_size( size ),
		m_position( 0 )
	{
	}

	~Tr2NoesisResourceStream()
	{
		CCP_FREE( m_data );
	}

	void SetPosition( uint32_t pos ) override
	{
		m_position = std::min( pos, m_size );
	}

	uint32_t GetPosition() const override
	{
		return m_position;
	}

	uint32_t GetLength() const override
	{
		return m_size;
	}

	uint32_t Read( void* buffer, uint32_t size ) override
	{
		const uint32_t available = std::min( size, m_size - m_position );
		memcpy( buffer, m_data + m_position, available );
		m_position += available;
		return available;
	}

	const void* GetMemoryBase() const override
	{
		return m_data;
	}

	void Close() override
	{
	}

private:
	uint8_t* m_data;
	uint32_t m_size;
	uint32_t m_position;
};

}

Noesis::Ptr<Noesis::Stream> Tr2NoesisXamlProvider::LoadXaml( const Noesis::Uri& uri )
{
	const char* path = uri.Str();

	Be::Clsid resFileClsid( "blue", "ResFile" );
	IResFilePtr file( resFileClsid );
	if( !file->Open( path, true ) )
	{
		// A warning rather than an error: Noesis is allowed to probe for resources that do not
		// exist, and null is the documented "not found" answer. The path is the whole point of
		// the message, since a XAML failure without a filename is close to undebuggable.
		CCP_NOESIS_LOGWARN( "XamlProvider could not open '%s'", path );
		return nullptr;
	}
	ON_BLOCK_EXIT( [&] { file->Close(); } );

	const ssize_t size = file->GetSize();
	if( size <= 0 )
	{
		CCP_NOESIS_LOGWARN( "XamlProvider opened '%s' but it is empty", path );
		return nullptr;
	}

	const uint32_t byteCount = static_cast<uint32_t>( size );
	uint8_t* data = static_cast<uint8_t*>( CCP_MALLOC( "Tr2NoesisXamlProvider", byteCount ) );
	if( data == nullptr )
	{
		CCP_NOESIS_LOGERR( "XamlProvider could not allocate %u bytes for '%s'", byteCount, path );
		return nullptr;
	}

	uint32_t read = 0;
	while( read < byteCount )
	{
		const ssize_t chunk = file->Read( data + read, byteCount - read );
		if( chunk <= 0 )
		{
			break;
		}
		read += static_cast<uint32_t>( chunk );
	}

	if( read != byteCount )
	{
		CCP_NOESIS_LOGERR( "XamlProvider read %u of %u bytes from '%s'", read, byteCount, path );
		CCP_FREE( data );
		return nullptr;
	}

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "XamlProvider served '%s' (%u bytes)", path, byteCount );
	}

	return Noesis::MakePtr<Tr2NoesisResourceStream>( data, byteCount );
}

#endif
