// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisResFile.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

namespace
{

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

namespace Tr2Noesis
{

Noesis::Ptr<Noesis::Stream> OpenResFile( const char* path, const char* providerName )
{
	Be::Clsid resFileClsid( "blue", "ResFile" );
	IResFilePtr file( resFileClsid );
	if( !file->Open( path, true ) )
	{
		// A warning rather than an error: Noesis is allowed to probe for resources that do not
		// exist, and null is the documented "not found" answer. The path is the whole point of
		// the message, since a miss without a filename is close to undebuggable.
		CCP_NOESIS_LOGWARN( "%s could not open '%s'", providerName, path );
		return nullptr;
	}
	ON_BLOCK_EXIT( [&] { file->Close(); } );

	const ssize_t size = file->GetSize();
	if( size <= 0 )
	{
		CCP_NOESIS_LOGWARN( "%s opened '%s' but it is empty", providerName, path );
		return nullptr;
	}

	const uint32_t byteCount = static_cast<uint32_t>( size );
	uint8_t* data = static_cast<uint8_t*>( CCP_MALLOC( "Tr2NoesisResFile", byteCount ) );
	if( data == nullptr )
	{
		CCP_NOESIS_LOGERR( "%s could not allocate %u bytes for '%s'", providerName, byteCount, path );
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
		CCP_NOESIS_LOGERR( "%s read %u of %u bytes from '%s'", providerName, read, byteCount, path );
		CCP_FREE( data );
		return nullptr;
	}

	if( IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "%s served '%s' (%u bytes)", providerName, path, byteCount );
	}

	return Noesis::MakePtr<Tr2NoesisResourceStream>( data, byteCount );
}

}

#endif
