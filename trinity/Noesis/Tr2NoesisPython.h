// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisPython_H
#define Tr2NoesisPython_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 ) && BLUE_WITH_PYTHON

#include "Noesis/Tr2NoesisCollection.h"
#include "Noesis/Tr2NoesisDataModel.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisObject.h"
#include "Noesis/Tr2NoesisObservableCollection.h"

#include <NsCore/Boxing.h>
#include <NsCore/DynamicCast.h>
#include <NsCore/String.h>

inline PyObject* Tr2NoesisComponentToPython( Noesis::BaseComponent* value, IRoot* wrapper )
{
	if( wrapper != nullptr )
	{
		return PyOS->WrapBlueObject( wrapper );
	}
	if( value == nullptr )
	{
		Py_RETURN_NONE;
	}
	if( Noesis::Boxing::CanUnbox<bool>( value ) )
	{
		if( Noesis::Boxing::Unbox<bool>( value ) )
		{
			Py_RETURN_TRUE;
		}
		Py_RETURN_FALSE;
	}
	if( Noesis::Boxing::CanUnbox<int>( value ) )
	{
		return Py_BuildValue( "i", Noesis::Boxing::Unbox<int>( value ) );
	}
	if( Noesis::Boxing::CanUnbox<float>( value ) )
	{
		return Py_BuildValue( "f", Noesis::Boxing::Unbox<float>( value ) );
	}
	if( Noesis::Boxing::CanUnbox<Noesis::String>( value ) )
	{
		return Py_BuildValue( "s", Noesis::Boxing::Unbox<Noesis::String>( value ).Str() );
	}
	Py_RETURN_NONE;
}

inline bool Tr2NoesisPythonToComponent( PyObject* value, Tr2NoesisPropertyType expected,
										Noesis::Ptr<Noesis::BaseComponent>& out, IRoot*& wrapper,
										Tr2NoesisPropertyType* inferred = nullptr )
{
	wrapper = nullptr;
	out.Reset();

	if( value == nullptr || value == Py_None )
	{
		if( inferred )
		{
			*inferred = expected;
		}
		return true;
	}

	auto setInferred = [&]( Tr2NoesisPropertyType type ) {
		if( inferred )
		{
			*inferred = type;
		}
	};

	if( Tr2NoesisDataModel* model = BluePythonCast<Tr2NoesisDataModel*>( value ) )
	{
		if( expected != Tr2NoesisPropertyType::Unknown && expected != Tr2NoesisPropertyType::Object )
		{
			PyErr_SetString( PyExc_TypeError, "expected an object property for a data model" );
			return false;
		}
		setInferred( Tr2NoesisPropertyType::Object );
		out.Reset( model->GetNative() );
		wrapper = model;
		return true;
	}

	if( Tr2NoesisCollection* collection = BluePythonCast<Tr2NoesisCollection*>( value ) )
	{
		if( expected != Tr2NoesisPropertyType::Unknown && expected != Tr2NoesisPropertyType::Collection )
		{
			PyErr_SetString( PyExc_TypeError, "expected a collection property for a collection" );
			return false;
		}
		setInferred( Tr2NoesisPropertyType::Collection );
		out.Reset( collection->GetNative() );
		wrapper = collection;
		return true;
	}

	if( PyBool_Check( value ) )
	{
		if( expected != Tr2NoesisPropertyType::Unknown && expected != Tr2NoesisPropertyType::Bool )
		{
			PyErr_SetString( PyExc_TypeError, "expected a bool" );
			return false;
		}
		setInferred( Tr2NoesisPropertyType::Bool );
		out = Noesis::Boxing::Box( value == Py_True );
		return true;
	}

	if( PyLong_Check( value ) )
	{
		if( expected == Tr2NoesisPropertyType::Float )
		{
			setInferred( Tr2NoesisPropertyType::Float );
			out = Noesis::Boxing::Box( static_cast<float>( PyLong_AsLong( value ) ) );
			return true;
		}
		if( expected != Tr2NoesisPropertyType::Unknown && expected != Tr2NoesisPropertyType::Integer )
		{
			PyErr_SetString( PyExc_TypeError, "expected an int" );
			return false;
		}
		setInferred( Tr2NoesisPropertyType::Integer );
		out = Noesis::Boxing::Box( static_cast<int>( PyLong_AsLong( value ) ) );
		return true;
	}

	if( PyFloat_Check( value ) )
	{
		if( expected != Tr2NoesisPropertyType::Unknown && expected != Tr2NoesisPropertyType::Float )
		{
			PyErr_SetString( PyExc_TypeError, "expected a float" );
			return false;
		}
		setInferred( Tr2NoesisPropertyType::Float );
		out = Noesis::Boxing::Box( static_cast<float>( PyFloat_AsDouble( value ) ) );
		return true;
	}

	if( PyUnicode_Check( value ) )
	{
		if( expected != Tr2NoesisPropertyType::Unknown && expected != Tr2NoesisPropertyType::String )
		{
			PyErr_SetString( PyExc_TypeError, "expected a string" );
			return false;
		}
		setInferred( Tr2NoesisPropertyType::String );
		out = Noesis::Boxing::Box( PyUnicode_AsUTF8( value ) );
		return true;
	}

	if( PyCallable_Check( value ) )
	{
		PyErr_SetString( PyExc_TypeError, "callables must be assigned with SetCommand" );
		return false;
	}

	PyErr_SetString( PyExc_TypeError, "unsupported value for Noesis data model" );
	return false;
}

inline PyObject* Tr2NoesisCommandParamToPython( Noesis::BaseComponent* param )
{
	if( param == nullptr )
	{
		Py_RETURN_NONE;
	}
	if( Tr2NoesisObject* object = Noesis::DynamicCast<Tr2NoesisObject*>( param ) )
	{
		return object->GetPythonWrapper();
	}
	if( Tr2NoesisObservableCollection* items = Noesis::DynamicCast<Tr2NoesisObservableCollection*>( param ) )
	{
		return items->GetPythonWrapper();
	}
	PyObject* py = Tr2NoesisComponentToPython( param, nullptr );
	if( py == Py_None
		&& !Noesis::Boxing::CanUnbox<bool>( param )
		&& !Noesis::Boxing::CanUnbox<int>( param )
		&& !Noesis::Boxing::CanUnbox<float>( param )
		&& !Noesis::Boxing::CanUnbox<Noesis::String>( param ) )
	{
		CCP_NOESIS_LOGWARN( "CommandParameter could not be converted to a Python value" );
	}
	return py;
}

#endif

#endif
