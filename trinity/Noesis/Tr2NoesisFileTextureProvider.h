// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisFileTextureProvider_H
#define Tr2NoesisFileTextureProvider_H

#if WITH_NOESIS && WITH_NOESIS_STUDIO

#include <NsGui/TextureProvider.h>

#include <string>

// --------------------------------------------------------------------------------------
// Description:
//   Serves textures from a filesystem folder. Studio's Options.GetTextureProvider
//   callback constructs one of these per assembly, rooted at the assembly path.
//
//   Decodes through Carbon ImageIO and uploads via RenderDevice::CreateTexture, so this
//   never touches NoesisApp.
// --------------------------------------------------------------------------------------
class Tr2NoesisFileTextureProvider : public Noesis::TextureProvider
{
public:
	explicit Tr2NoesisFileTextureProvider( const char* rootPath );

	Noesis::TextureInfo GetTextureInfo( const Noesis::Uri& uri ) override;
	Noesis::Ptr<Noesis::Texture> LoadTexture( const Noesis::Uri& uri, Noesis::RenderDevice* device ) override;

private:
	std::string m_rootPath;
};

#endif

#endif
