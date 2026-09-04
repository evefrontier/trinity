// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisVideo.h"

#if WITH_NOESIS && BLUE_WITH_PYTHON

BLUE_DEFINE( Tr2NoesisVideoSink );

static PyObject* PyNoesisSetVideoFactory( PyObject*, PyObject* args )
{
	PyObject* factory = nullptr;
	if( !PyArg_ParseTuple( args, "O", &factory ) )
	{
		return nullptr;
	}
	if( factory != Py_None && !PyCallable_Check( factory ) )
	{
		PyErr_SetString( PyExc_TypeError, "video factory must be callable or None" );
		return nullptr;
	}

	Tr2Noesis::SetVideoFactory( factory == Py_None ? nullptr : factory );
	Py_RETURN_NONE;
}

MAP_FUNCTION(
	"NoesisSetVideoFactory",
	PyNoesisSetVideoFactory,
	"Sets the process-wide factory used by Trinity.Video elements. Pass None to clear it.\n"
	"The callable receives (source, loop, volume, muted, is_playing, sink) and returns a\n"
	"session exposing resume(), pause(), set_volume(float), set_muted(bool), and destroy().\n"
	"The sink exposes set_texture(texture_or_none) and ended(); both no-op after detach.\n"
	":rtype: None" );

static PyObject* PySetTexture( PyObject* self, PyObject* args )
{
	PyObject* textureObject = nullptr;
	if( !PyArg_ParseTuple( args, "O", &textureObject ) )
	{
		return nullptr;
	}

	TriTextureRes* texture = nullptr;
	if( textureObject != Py_None )
	{
		if( !BluePythonCast<TriTextureRes*>( textureObject ) ||
			!BlueExtractArgument( textureObject, texture, 2 ) )
		{
			PyErr_SetString( PyExc_TypeError, "texture must be a TriTextureRes or None" );
			return nullptr;
		}
	}

	Tr2NoesisVideoSink* sink = BluePythonCast<Tr2NoesisVideoSink*>( self );
	sink->SetTexture( texture );
	Py_RETURN_NONE;
}

const Be::ClassInfo* Tr2NoesisVideoSink::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisVideoSink, "Per-instance callback sink for a Trinity.Video session. Detached sinks no-op." )
		MAP_INTERFACE( Tr2NoesisVideoSink )

		MAP_METHOD(
			"set_texture",
			PySetTexture,
			"Supplies an allocated TriTextureRes for this video. None clears it.\n"
			"No-ops if the sink has been detached.\n"
			":param texture: TriTextureRes or None\n"
			":rtype: None" )

		MAP_METHOD_AND_WRAP(
			"ended",
			Ended,
			"Marks the video as ended without calling pause() back into the player.\n"
			"No-ops if the sink has been detached. Failure uses this same transition.\n"
			":rtype: None" )

	EXPOSURE_END()
}

#endif
