// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisPythonWeak_H
#define Tr2NoesisPythonWeak_H

#if WITH_NOESIS && BLUE_WITH_PYTHON

inline void Tr2NoesisSetPythonWeakRef( PyObject*& slot, PyObject* obj )
{
	Py_XDECREF( slot );
	slot = nullptr;
	if( obj == nullptr || obj == Py_None )
	{
		return;
	}
	slot = PyWeakref_NewRef( obj, nullptr );
}

inline PyObject* Tr2NoesisGetPythonWeakRef( PyObject* slot )
{
	if( slot == nullptr )
	{
		Py_RETURN_NONE;
	}
#if PY_VERSION_HEX >= 0x030D0000
	PyObject* obj = nullptr;
	const int result = PyWeakref_GetRef( slot, &obj );
	if( result < 0 )
	{
		return nullptr;
	}
	if( result == 0 )
	{
		Py_RETURN_NONE;
	}
	return obj;
#else
	PyObject* obj = PyWeakref_GetObject( slot );
	Py_INCREF( obj );
	return obj;
#endif
}

inline void Tr2NoesisClearPythonWeakRef( PyObject*& slot )
{
	if( slot == nullptr )
	{
		return;
	}
	auto gil = PyGILState_Ensure();
	Py_DECREF( slot );
	slot = nullptr;
	PyGILState_Release( gil );
}

#endif

#endif
