// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisPynrInterface_H
#define Tr2NoesisPynrInterface_H

#include "pynr_python.h"

// --------------------------------------------------------------------------------------
// Description:
//   Takes a pynr interface out of a Python object, for the three places Python hands one
//   across: the render device's shader source, and the view on both the sprite and the
//   step.
//
//   The capsule is how the two modules pass a pointer neither can name a type for, but
//   that is plumbing and script has no use for it. So the object is what crosses, and the
//   protocol is a method on it: anything with _pynr_interface() can be handed over.
//
//   That protocol lives in pynr_python.h, next to the ABI it carries. This is the thin
//   Trinity-side wrapper that decides how a refusal reads from Python.
// --------------------------------------------------------------------------------------

// False with a Python error set when `object` cannot produce the named interface. True
// with a null `out` when `object` is None, which every caller treats as "clear it".
//
// `expectedSize` is the sizeof of the interface being asked for, so a sender whose vtable
// stops short of what this Trinity calls is refused rather than called into.
inline bool Tr2NoesisTakePynrInterface( PyObject* object, const char* capsuleName,
									    size_t expectedSize, void*& out )
{
	switch( pynr_take_interface( object, capsuleName, expectedSize, &out ) )
	{
	case PYNR_TAKE_ERROR:
		// Whatever the helper raised names the mistake exactly.
		return false;

	case PYNR_TAKE_ABI:
		PyErr_Format( PyExc_ValueError, "the %s speaks a pynr ABI this Trinity cannot",
					  capsuleName );
		out = nullptr;
		return false;

	default:
		return true;
	}
}

// The set_view binding, shared by Tr2Sprite2dNoesis and TriStepRenderNoesis. Templated on
// the Blue class so the capsule name and the interface size are stated once.
template<typename T>
PyObject* Tr2NoesisPySetView( PyObject* self, PyObject* args )
{
	PyObject* view = nullptr;
	if( !PyArg_ParseTuple( args, "O", &view ) )
	{
		return nullptr;
	}

	void* pointer = nullptr;
	if( !Tr2NoesisTakePynrInterface( view, PYNR_CAPSULE_VIEW, sizeof( pynr_view ), pointer ) )
	{
		return nullptr;
	}

	BluePythonCast<T*>( self )->SetView( static_cast<const pynr_view*>( pointer ) );
	Py_RETURN_NONE;
}

#endif
