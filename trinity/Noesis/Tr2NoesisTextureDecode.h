// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisTextureDecode_H
#define Tr2NoesisTextureDecode_H

#if WITH_NOESIS

#include <NsCore/Ptr.h>
#include <NsGui/TextureProvider.h>
#include <NsRender/Texture.h>

struct IBlueStream;

namespace Noesis
{
class RenderDevice;
}

// --------------------------------------------------------------------------------------
// Description:
//   Shared ImageIO decode and CreateTexture upload used by Tr2NoesisTextureProvider
//   (res:/) and Tr2NoesisFileTextureProvider (Studio filesystem). Both providers open
//   an IBlueStream; this layer turns that stream into a Noesis texture.
// --------------------------------------------------------------------------------------
namespace Tr2Noesis
{

Noesis::TextureInfo GetTextureInfoFromStream( IBlueStream& stream, const char* path,
											  const char* providerName );

Noesis::Ptr<Noesis::Texture> CreateTextureFromStream( IBlueStream& stream, const char* path,
													  const char* providerName,
													  Noesis::RenderDevice* device );

}

#endif

#endif
