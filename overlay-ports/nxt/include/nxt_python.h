/* Copyright © 2026 CCP ehf.
 *
 * nxt_python.h -- how an nxt interface crosses between the two Python modules.
 *
 * nxt.h is the ABI and stays free of Python. This is the transport: the capsule
 * convention, and the one function both sides use to read an interface out of an object.
 *
 * THE PROTOCOL. An object that can hand over an interface exposes a no-argument method
 * named `_nxt_interface()` returning a PyCapsule whose name is one of the NXT_CAPSULE_*
 * strings and whose pointer is the interface struct. The object crosses, not the capsule:
 * a capsule is plumbing for a pointer neither module can name a type for, and script has
 * no use for one.
 *
 * Both modules implement both halves of this, so it lives here rather than being written
 * out once per repository and drifting.
 *
 * Include Python.h before this header.
 */

#ifndef NXT_PYTHON_H
#define NXT_PYTHON_H

#include "nxt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The method every interface-bearing object exposes. */
#define NXT_INTERFACE_METHOD "_nxt_interface"

/* Reads the named interface out of `object`.
 *
 * Three outcomes, because the caller may reasonably treat two of them differently:
 *
 *   NXT_TAKE_OK        `*out` is the interface, or null when `object` was NULL or None --
 *                      which every caller treats as "clear it".
 *   NXT_TAKE_ERROR     `object` cannot produce this interface at all. A Python error is
 *                      set; return null from the wrapper.
 *   NXT_TAKE_ABI       a well-formed capsule whose interface speaks an ABI this build
 *                      cannot. `*out` is still set, so a caller that wants to report the
 *                      mismatch in its own words -- or hand it to a setter that refuses
 *                      and logs -- can. No Python error is set: whether this is an
 *                      exception or a False is the entry point's own contract to keep.
 *                      Note that it is NOT null, precisely so that passing it on cannot
 *                      be mistaken for passing "nothing", which usually means "clear".
 *
 * `expected_size` is the sizeof of the concrete interface the caller wants, the same value
 * NXT_INTERFACE_USABLE would derive; the version check happens here so no caller can skip
 * it.
 *
 * The capsule is dropped before returning. That is safe because `object` is alive for the
 * whole call and owns the interface -- the capsule's reference is only ever the second
 * one. A caller that stores the pointer still has to retain it; see the ownership rules in
 * nxt.h. */
#define NXT_TAKE_OK    0
#define NXT_TAKE_ERROR 1
#define NXT_TAKE_ABI   2

static NXT_INLINE int nxt_take_interface( PyObject* object, const char* capsule_name,
                                          size_t expected_size, void** out )
{
    PyObject* capsule;
    void* pointer;

    *out = NULL;
    if( object == NULL || object == Py_None )
    {
        return NXT_TAKE_OK;
    }

    capsule = PyObject_CallMethod( object, NXT_INTERFACE_METHOD, NULL );
    if( capsule == NULL )
    {
        /* Whatever the call raised says more than anything added here would -- most often
         * that the object has no such method, which names the mistake exactly. */
        return NXT_TAKE_ERROR;
    }

    pointer = PyCapsule_GetPointer( capsule, capsule_name );
    Py_DECREF( capsule );

    if( pointer == NULL )
    {
        PyErr_Clear();
        PyErr_Format( PyExc_TypeError, "%s() did not return a %s capsule",
                      NXT_INTERFACE_METHOD, capsule_name );
        return NXT_TAKE_ERROR;
    }

    *out = pointer;

    if( nxt_interface_usable( (const nxt_interface_header*)pointer, expected_size ) == NXT_FALSE )
    {
        return NXT_TAKE_ABI;
    }

    return NXT_TAKE_OK;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NXT_PYTHON_H */
