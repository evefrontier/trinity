// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisFileTextureProvider.h"

#if WITH_NOESIS && WITH_NOESIS_STUDIO

#include "Noesis/Tr2NoesisFilePath.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsRender/RenderDevice.h>
#include <NsRender/Texture.h>
#include <NsGui/Uri.h>

#include <cstring>
#include <vector>

namespace
{

bool CopyToRgba( const ImageIO::HostBitmap& bitmap, std::vector<uint8_t>& rgba, bool& hasAlpha )
{
	const uint32_t width = bitmap.GetWidth();
	const uint32_t height = bitmap.GetHeight();
	const uint32_t bpp = Tr2RenderContextEnum::GetBytesPerPixel( bitmap.GetFormat() );
	if( width == 0 || height == 0 || bpp != 4 || bitmap.GetRawData() == nullptr )
	{
		return false;
	}

	const uint32_t srcPitch = bitmap.GetPitch();
	const uint8_t* srcBase = reinterpret_cast<const uint8_t*>( bitmap.GetRawData() );
	rgba.resize( static_cast<size_t>( width ) * height * 4 );

	const bool bgra = bitmap.GetFormat() == Tr2RenderContextEnum::PIXEL_FORMAT_B8G8R8A8_UNORM ||
					  bitmap.GetFormat() == Tr2RenderContextEnum::PIXEL_FORMAT_B8G8R8A8_UNORM_SRGB;

	hasAlpha = false;
	for( uint32_t y = 0; y < height; ++y )
	{
		const uint8_t* src = srcBase + y * srcPitch;
		uint8_t* dst = rgba.data() + static_cast<size_t>( y ) * width * 4;
		for( uint32_t x = 0; x < width; ++x )
		{
			const uint8_t b = src[0];
			const uint8_t g = src[1];
			const uint8_t r = src[2];
			const uint8_t a = src[3];
			if( bgra )
			{
				dst[0] = r;
				dst[1] = g;
				dst[2] = b;
			}
			else
			{
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
			}
			dst[3] = a;
			hasAlpha = hasAlpha || a != 255;
			src += 4;
			dst += 4;
		}
	}

	return true;
}

void Premultiply( std::vector<uint8_t>& rgba )
{
	for( size_t i = 0; i + 3 < rgba.size(); i += 4 )
	{
		const uint8_t a = rgba[i + 3];
		rgba[i + 0] = static_cast<uint8_t>( ( static_cast<uint32_t>( rgba[i + 0] ) * a ) / 255 );
		rgba[i + 1] = static_cast<uint8_t>( ( static_cast<uint32_t>( rgba[i + 1] ) * a ) / 255 );
		rgba[i + 2] = static_cast<uint8_t>( ( static_cast<uint32_t>( rgba[i + 2] ) * a ) / 255 );
	}
}

bool LoadBitmapFile( const std::string& rootPath, const Noesis::Uri& uri, ImageIO::HostBitmap& bitmap,
					 std::string& filename )
{
	filename = Tr2NoesisJoinFilePath( rootPath.c_str(), uri );
	if( filename.empty() )
	{
		return false;
	}

	const std::wstring filenameW( static_cast<const wchar_t*>( CA2W( filename.c_str() ) ) );
	IBlueStreamPtr stream;
	if( !BeIsSuccess( BePaths->GetFileContentsWithYield( filenameW.c_str(), &stream ) ) || !stream )
	{
		CCP_NOESIS_LOGWARN( "FileTextureProvider could not open '%s'", filename.c_str() );
		return false;
	}

	const ImageIO::Result result = ImageIO::ReadImage( *stream, ImageIO::LoadParameters( filenameW.c_str() ), bitmap );
	if( !result || !bitmap.IsValid() )
	{
		CCP_NOESIS_LOGWARN( "FileTextureProvider failed to decode '%s'", filename.c_str() );
		return false;
	}

	return true;
}

}

Tr2NoesisFileTextureProvider::Tr2NoesisFileTextureProvider( const char* rootPath ) :
	m_rootPath( rootPath != nullptr ? rootPath : "" )
{
}

Noesis::TextureInfo Tr2NoesisFileTextureProvider::GetTextureInfo( const Noesis::Uri& uri )
{
	ImageIO::HostBitmap bitmap;
	std::string filename;
	if( !LoadBitmapFile( m_rootPath, uri, bitmap, filename ) )
	{
		return {};
	}

	Noesis::TextureInfo info;
	info.width = bitmap.GetWidth();
	info.height = bitmap.GetHeight();
	return info;
}

Noesis::Ptr<Noesis::Texture> Tr2NoesisFileTextureProvider::LoadTexture( const Noesis::Uri& uri,
																		Noesis::RenderDevice* device )
{
	if( device == nullptr )
	{
		return nullptr;
	}

	ImageIO::HostBitmap bitmap;
	std::string filename;
	if( !LoadBitmapFile( m_rootPath, uri, bitmap, filename ) )
	{
		return nullptr;
	}

	std::vector<uint8_t> rgba;
	bool hasAlpha = false;
	if( !CopyToRgba( bitmap, rgba, hasAlpha ) )
	{
		CCP_NOESIS_LOGWARN( "FileTextureProvider cannot convert '%s' (format %d, %ux%u) to RGBA8",
							filename.c_str(), static_cast<int>( bitmap.GetFormat() ),
							bitmap.GetWidth(), bitmap.GetHeight() );
		return nullptr;
	}

	if( hasAlpha )
	{
		Premultiply( rgba );
	}

	const void* data[1] = { rgba.data() };
	const char* slash = strrchr( filename.c_str(), '/' );
	const char* backslash = strrchr( filename.c_str(), '\\' );
	const char* label = filename.c_str();
	if( slash != nullptr && slash + 1 > label )
	{
		label = slash + 1;
	}
	if( backslash != nullptr && backslash + 1 > label )
	{
		label = backslash + 1;
	}

	const Noesis::TextureFormat::Enum format =
		hasAlpha ? Noesis::TextureFormat::RGBA8 : Noesis::TextureFormat::RGBX8;
	Noesis::Ptr<Noesis::Texture> texture =
		device->CreateTexture( label, bitmap.GetWidth(), bitmap.GetHeight(), 1, format, data );
	if( texture == nullptr )
	{
		CCP_NOESIS_LOGERR( "FileTextureProvider CreateTexture failed for '%s'", filename.c_str() );
		return nullptr;
	}

	if( Tr2Noesis::IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "FileTextureProvider loaded '%s' (%ux%u)", filename.c_str(),
						bitmap.GetWidth(), bitmap.GetHeight() );
	}

	return texture;
}

#endif
