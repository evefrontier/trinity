// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisTextureDecode.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

#include <NsRender/RenderDevice.h>
#include <NsRender/Texture.h>

#include <cstring>
#include <vector>

namespace
{

using namespace Tr2RenderContextEnum;

const char* FilenameLabel( const char* path )
{
	const char* label = path != nullptr ? path : "";
	const char* slash = strrchr( label, '/' );
	const char* backslash = strrchr( label, '\\' );
	if( slash != nullptr && slash + 1 > label )
	{
		label = slash + 1;
	}
	if( backslash != nullptr && backslash + 1 > label )
	{
		label = backslash + 1;
	}
	return label;
}

bool ReadBitmap( IBlueStream& stream, const char* path, const char* providerName,
				 ImageIO::HostBitmap& bitmap )
{
	const std::wstring filenameW( static_cast<const wchar_t*>( CA2W( path ) ) );
	const ImageIO::Result result =
		ImageIO::ReadImage( stream, ImageIO::LoadParameters( filenameW.c_str() ), bitmap );
	if( !result || !bitmap.IsValid() )
	{
		CCP_NOESIS_LOGWARN( "%s failed to decode '%s': %s", providerName, path,
							result.GetErrorMessage().c_str() );
		return false;
	}
	return true;
}

// Narrows R16G16B16A16_UNORM to unpremultiplied RGBA8, which is the only way out of 16 bit:
// HostBitmap::ConvertFormat has no 16-bit source case, and GetPixel - despite reading like a
// format-agnostic accessor - rejects everything except BGRA8, BGRX8, BC1 and BC3.
//
// Every 16-bit PNG lands in this one format. Tr2PngHandler maps colour type 6 (64bpp) straight
// to it and upsamples colour type 2 (48bpp) into it with alpha filled to 0xFFFF, so a source
// without an alpha channel arrives opaque. Channel order is already RGBA, so unlike the 8-bit
// path below there is no BGRA case to swizzle, and libpng byte-swaps 16-bit samples on read
// (png_set_swap whenever bit depth > 8), so these are host order.
// Cannot fail: CopyToRgba has already rejected zero dimensions and null raw data, and
// ConvertFormat leaves the bitmap untouched when it returns false.
void CopyRgba16ToRgba8( const ImageIO::HostBitmap& bitmap, std::vector<uint8_t>& rgba,
						bool& hasAlpha )
{
	const uint32_t width = bitmap.GetWidth();
	const uint32_t height = bitmap.GetHeight();
	const uint32_t srcPitch = bitmap.GetPitch();
	const uint8_t* srcBase = reinterpret_cast<const uint8_t*>( bitmap.GetRawData() );
	rgba.resize( static_cast<size_t>( width ) * height * 4 );

	hasAlpha = false;
	for( uint32_t y = 0; y < height; ++y )
	{
		const uint16_t* src = reinterpret_cast<const uint16_t*>( srcBase + y * srcPitch );
		uint8_t* dst = rgba.data() + static_cast<size_t>( y ) * width * 4;
		for( uint32_t x = 0; x < width; ++x )
		{
			for( uint32_t channel = 0; channel < 4; ++channel )
			{
				dst[channel] = static_cast<uint8_t>(
					( static_cast<uint32_t>( src[channel] ) * 255 + 32767 ) / 65535 );
			}
			// Taken from the narrowed byte, not the 16-bit sample, so hasAlpha means the same
			// thing here as it does on the 8-bit path and premultiplication stays consistent.
			hasAlpha = hasAlpha || dst[3] != 255;
			src += 4;
			dst += 4;
		}
	}
}

bool CopyToRgba( ImageIO::HostBitmap& bitmap, std::vector<uint8_t>& rgba, bool& hasAlpha,
				 const char* path, const char* providerName )
{
	uint32_t width = bitmap.GetWidth();
	uint32_t height = bitmap.GetHeight();
	if( width == 0 || height == 0 || bitmap.GetRawData() == nullptr )
	{
		return false;
	}

	uint32_t bpp = GetBytesPerPixel( bitmap.GetFormat() );
	if( bpp != 4 )
	{
		if( !bitmap.ConvertFormat( PIXEL_FORMAT_R8G8B8A8_UNORM ) )
		{
			if( bitmap.GetFormat() == PIXEL_FORMAT_R16G16B16A16_UNORM )
			{
				CCP_NOESIS_LOGWARN( "%s is narrowing 16-bit '%s' (%ux%u) to RGBA8 a pixel at a "
									"time, because Noesis has no 16-bit texture format. Re-save "
									"it as an 8-bit PNG to skip this.",
									providerName, path, width, height );
				CopyRgba16ToRgba8( bitmap, rgba, hasAlpha );
				return true;
			}
			CCP_NOESIS_LOGWARN( "%s cannot convert '%s' (format %d, %ux%u) to RGBA8",
								providerName, path, static_cast<int>( bitmap.GetFormat() ),
								width, height );
			return false;
		}
		bpp = GetBytesPerPixel( bitmap.GetFormat() );
		width = bitmap.GetWidth();
		height = bitmap.GetHeight();
		if( bpp != 4 || bitmap.GetRawData() == nullptr )
		{
			return false;
		}
	}

	const uint32_t srcPitch = bitmap.GetPitch();
	const uint8_t* srcBase = reinterpret_cast<const uint8_t*>( bitmap.GetRawData() );
	rgba.resize( static_cast<size_t>( width ) * height * 4 );

	const bool bgra = bitmap.GetFormat() == PIXEL_FORMAT_B8G8R8A8_UNORM ||
					  bitmap.GetFormat() == PIXEL_FORMAT_B8G8R8A8_UNORM_SRGB;

	hasAlpha = false;
	for( uint32_t y = 0; y < height; ++y )
	{
		const uint8_t* src = srcBase + y * srcPitch;
		uint8_t* dst = rgba.data() + static_cast<size_t>( y ) * width * 4;
		for( uint32_t x = 0; x < width; ++x )
		{
			if( bgra )
			{
				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
			}
			else
			{
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
			}
			dst[3] = src[3];
			hasAlpha = hasAlpha || src[3] != 255;
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

}

namespace Tr2Noesis
{

Noesis::TextureInfo GetTextureInfoFromStream( IBlueStream& stream, const char* path,
											  const char* providerName )
{
	ImageIO::HostBitmap bitmap;
	if( !ReadBitmap( stream, path, providerName, bitmap ) )
	{
		return {};
	}

	Noesis::TextureInfo info;
	info.width = bitmap.GetWidth();
	info.height = bitmap.GetHeight();
	return info;
}

Noesis::Ptr<Noesis::Texture> CreateTextureFromStream( IBlueStream& stream, const char* path,
													  const char* providerName,
													  Noesis::RenderDevice* device )
{
	if( device == nullptr )
	{
		return nullptr;
	}

	ImageIO::HostBitmap bitmap;
	if( !ReadBitmap( stream, path, providerName, bitmap ) )
	{
		return nullptr;
	}

	std::vector<uint8_t> rgba;
	bool hasAlpha = false;
	if( !CopyToRgba( bitmap, rgba, hasAlpha, path, providerName ) )
	{
		return nullptr;
	}

	if( hasAlpha )
	{
		Premultiply( rgba );
	}

	const void* data[1] = { rgba.data() };
	const Noesis::TextureFormat::Enum format =
		hasAlpha ? Noesis::TextureFormat::RGBA8 : Noesis::TextureFormat::RGBX8;
	Noesis::Ptr<Noesis::Texture> texture =
		device->CreateTexture( FilenameLabel( path ), bitmap.GetWidth(), bitmap.GetHeight(), 1,
							   format, data );
	if( texture == nullptr )
	{
		CCP_NOESIS_LOGERR( "%s CreateTexture failed for '%s'", providerName, path );
		return nullptr;
	}

	if( IsLogVerbose() )
	{
		CCP_NOESIS_LOG( "%s loaded '%s' (%ux%u)", providerName, path,
						bitmap.GetWidth(), bitmap.GetHeight() );
	}

	return texture;
}

}

#endif
