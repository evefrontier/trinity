// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisTextureProvider_H
#define Tr2NoesisTextureProvider_H

#if WITH_NOESIS

#include <NsGui/TextureProvider.h>

// --------------------------------------------------------------------------------------
// Description:
//   Serves textures to NoesisGUI out of Trinity's resource tree. Registered globally
//   from Tr2Noesis::EnsureInitialized.
//
//   XAML Image Source URIs are mapped the same way as font folders: a res: Uri is used
//   as a Trinity resource path verbatim, and a scheme-less Studio path such as
//   '/ui/images/icon.png' is prefixed with 'res:/'. Decoding goes through Carbon ImageIO
//   and the GPU upload through RenderDevice::CreateTexture.
//
//   ResFile is synchronous, which is what TextureProvider::GetTextureInfo / LoadTexture
//   demand. A cold disk read therefore stalls the calling frame (gap G6).
// --------------------------------------------------------------------------------------
class Tr2NoesisTextureProvider : public Noesis::TextureProvider
{
public:
	Noesis::TextureInfo GetTextureInfo( const Noesis::Uri& uri ) override;
	Noesis::Ptr<Noesis::Texture> LoadTexture( const Noesis::Uri& uri,
											  Noesis::RenderDevice* device ) override;
};

#endif

#endif
