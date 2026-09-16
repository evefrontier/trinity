// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisHost_H
#define Tr2NoesisHost_H

#include <nsi_blue.h>

#include <memory>

BLUE_DECLARE( Tr2RenderContext );
BLUE_DECLARE( Tr2NoesisHost );
class Tr2NoesisRenderDevice;
struct Tr2ScissorRect;

// --------------------------------------------------------------------------------------
// Description:
//   Trinity's side of the nsi.h boundary: the render device over TrinityAL, wrapped in
//   the two vtables the Noesis library calls through.
//
//   Python creates one of these and hands it to the library, which is the only way the
//   two modules are ever connected. Nothing here loads the other module or resolves a
//   symbol from it.
//
//   Both vtables live on this one object, but only the device one crosses as a Blue
//   object. The frame vtable is handed to a view render call as a plain pointer and is
//   only valid for that call: the step binds a render context around it, and every frame
//   entry point refuses to run without one. That is what stops the library reaching a
//   stale deferred context if the host ever drives the sequence out of order.
//
//   The scissor stays on this side. The step knows the parent clip rect and
//   begin_onscreen_render is a host call, so routing the clip through the ABI would end
//   where it started.
// --------------------------------------------------------------------------------------

class Tr2NoesisHost : public INsiDeviceHost
{
public:
	EXPOSE_TO_BLUE();

	Tr2NoesisHost( IRoot* lockobj = NULL );
	~Tr2NoesisHost();

	// Takes the library's shader source. Cheap and thread-agnostic: it only resolves the
	// interface, so Python can call it at startup.
	bool SetShaderSource( IRoot* shaderSource );

	// Builds the device on first call and returns whether it is usable.
	//
	// Deferred rather than done when the shader source is set, because building it needs
	// the main-thread render context: there is no device at Python-init time, and asking
	// for one there blocks. The render step calls this on its first Execute, which is on
	// the render thread with a live context by construction.
	//
	// One attempt only. A device that failed to build its shaders will not build them on
	// the next frame either, and retrying would repeat the failure every frame.
	bool EnsureDevice();

	bool IsReady() const;

	// INsiDeviceHost
	const nsi_device_host* GetNsiDeviceHost() override;

	// Trinity-internal. The frame vtable, valid only between BeginFrame and EndFrame.
	// Not exposed through Blue: it never crosses as an object.
	const nsi_frame_host* GetNsiFrameHost();

	// Binds the frame's deferred context, and drops it again. Prefer ScopedFrame: an exit
	// that skips EndFrame leaves the device holding a context the step has finished with.
	void BeginFrame( Tr2RenderContext& renderContext );
	void EndFrame();

	// The frame bracket as a scope, mirroring the library's own ScopedFrame around the
	// same frame from the other side.
	class ScopedFrame
	{
	public:
		ScopedFrame( Tr2NoesisHost& host, Tr2RenderContext& renderContext ) :
			m_host( host )
		{
			m_host.BeginFrame( renderContext );
		}
		~ScopedFrame()
		{
			m_host.EndFrame();
		}

		ScopedFrame( const ScopedFrame& ) = delete;
		ScopedFrame& operator=( const ScopedFrame& ) = delete;

	private:
		Tr2NoesisHost& m_host;
	};

	// Extra onscreen scissor in render-target pixels, intersected with the viewport that
	// is already clamped to the target. The sprite path uses this for parent clipChildren
	// so Noesis keeps its layout viewport while the GPU clips overflow; the overlay path
	// leaves it clear. Applied by begin_onscreen_render.
	void SetHostScissor( const Tr2ScissorRect& rect );
	void ClearHostScissor();

	Tr2NoesisRenderDevice* GetDevice() const;

private:
	void FillVtables();

	// A plain class, not a Blue object: it is an implementation detail of this host and
	// never crosses the boundary on its own.
	std::unique_ptr<Tr2NoesisRenderDevice> m_device;
	IRootPtr m_shaderSourceObject;
	const nsi_shader_source* m_shaderSource;
	bool m_deviceAttempted;
	nsi_device_host m_deviceApi;
	nsi_frame_host m_frameApi;
};

TYPEDEF_BLUECLASS( Tr2NoesisHost );

#endif
