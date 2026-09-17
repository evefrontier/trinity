// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisNxtInterface_H
#define Tr2NoesisNxtInterface_H

#include "nxt.h"

// --------------------------------------------------------------------------------------
// Description:
//   Takes an nxt interface out of a Python object, for the three places Python hands one
//   across: the render device's shader source, and the view on both the sprite and the
//   step.
//
//   The capsule is how the two modules pass a pointer neither can name a type for, but
//   that is plumbing and script has no use for it. So the object is what crosses, and the
//   protocol is a method on it: anything with _nxt_interface() can be handed over.
// --------------------------------------------------------------------------------------

// False with a Python error set when `object` cannot produce the named interface. True
// with a null `out` when `object` is None, which every caller treats as "clear it".
//
// The capsule is dropped before returning. That is safe because `object` is alive for the
// whole call and owns the interface -- the capsule's reference is only ever the second
// one. A caller that stores the pointer still has to retain it.
inline bool Tr2NoesisTakeNxtInterface( PyObject* object, const char* capsuleName, void*& out )
{
	out = nullptr;
	if( object == nullptr || object == Py_None )
	{
		return true;
	}

	PyObject* capsule = PyObject_CallMethod( object, "_nxt_interface", nullptr );
	if( capsule == nullptr )
	{
		// Whatever the call raised says more than anything this could add -- most often
		// that the object has no such method, which names the mistake exactly.
		return false;
	}

	void* pointer = PyCapsule_GetPointer( capsule, capsuleName );
	Py_DECREF( capsule );

	if( pointer == nullptr )
	{
		PyErr_Clear();
		PyErr_Format( PyExc_TypeError, "_nxt_interface() did not return a %s capsule",
					  capsuleName );
		return false;
	}

	const nxt_interface_header* header = static_cast<const nxt_interface_header*>( pointer );
	if( nxt_interface_usable( header ) == NXT_FALSE )
	{
		PyErr_Format( PyExc_ValueError, "the %s speaks an nxt ABI this Trinity cannot",
					  capsuleName );
		return false;
	}

	out = pointer;
	return true;
}

#endif
