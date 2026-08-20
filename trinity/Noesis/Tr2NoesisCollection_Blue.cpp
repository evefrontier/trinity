// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisCollection.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisPython.h"

BLUE_DEFINE( Tr2NoesisCollection );

#if BLUE_WITH_PYTHON

static PyObject* PyGet( PyObject* self, PyObject* args )
{
	Tr2NoesisCollection* collection = BluePythonCast<Tr2NoesisCollection*>( self );
	int index = 0;
	if( !PyArg_ParseTuple( args, "i", &index ) )
	{
		return nullptr;
	}
	if( index < 0 || index >= collection->GetCount() )
	{
		PyErr_SetString( PyExc_IndexError, "collection index out of range" );
		return nullptr;
	}
	return Tr2NoesisComponentToPython(
		collection->GetItem( static_cast<uint32_t>( index ) ),
		collection->GetWrapper( static_cast<uint32_t>( index ) ) );
}

static PyObject* PySet( PyObject* self, PyObject* args )
{
	Tr2NoesisCollection* collection = BluePythonCast<Tr2NoesisCollection*>( self );
	int index = 0;
	PyObject* value = nullptr;
	if( !PyArg_ParseTuple( args, "iO", &index, &value ) )
	{
		return nullptr;
	}
	if( index < 0 || index >= collection->GetCount() )
	{
		PyErr_SetString( PyExc_IndexError, "collection index out of range" );
		return nullptr;
	}

	Noesis::Ptr<Noesis::BaseComponent> boxed;
	IRoot* wrapper = nullptr;
	if( !Tr2NoesisPythonToComponent( value, Tr2NoesisPropertyType::Unknown, boxed, wrapper ) )
	{
		return nullptr;
	}
	if( !collection->SetItem( static_cast<uint32_t>( index ), boxed, wrapper ) )
	{
		PyErr_SetString( PyExc_RuntimeError, "Set failed" );
		return nullptr;
	}
	Py_RETURN_NONE;
}

static PyObject* PyAdd( PyObject* self, PyObject* args )
{
	Tr2NoesisCollection* collection = BluePythonCast<Tr2NoesisCollection*>( self );
	PyObject* value = nullptr;
	if( !PyArg_ParseTuple( args, "O", &value ) )
	{
		return nullptr;
	}

	Noesis::Ptr<Noesis::BaseComponent> boxed;
	IRoot* wrapper = nullptr;
	if( !Tr2NoesisPythonToComponent( value, Tr2NoesisPropertyType::Unknown, boxed, wrapper ) )
	{
		return nullptr;
	}
	return Py_BuildValue( "i", collection->AddItem( boxed, wrapper ) );
}

static PyObject* PyInsert( PyObject* self, PyObject* args )
{
	Tr2NoesisCollection* collection = BluePythonCast<Tr2NoesisCollection*>( self );
	int index = 0;
	PyObject* value = nullptr;
	if( !PyArg_ParseTuple( args, "iO", &index, &value ) )
	{
		return nullptr;
	}
	if( index < 0 || index > collection->GetCount() )
	{
		PyErr_SetString( PyExc_IndexError, "collection index out of range" );
		return nullptr;
	}

	Noesis::Ptr<Noesis::BaseComponent> boxed;
	IRoot* wrapper = nullptr;
	if( !Tr2NoesisPythonToComponent( value, Tr2NoesisPropertyType::Unknown, boxed, wrapper ) )
	{
		return nullptr;
	}
	collection->InsertItem( static_cast<uint32_t>( index ), boxed, wrapper );
	Py_RETURN_NONE;
}

#endif

const Be::ClassInfo* Tr2NoesisCollection::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisCollection, "Observable list for Noesis ItemsControl. Bind ItemsSource to a collection property on a Tr2NoesisDataModel." )
		MAP_INTERFACE( Tr2NoesisCollection )

		MAP_PROPERTY_READONLY(
			"count",
			GetCount,
			"Number of items." )

		MAP_METHOD(
			"Get",
			PyGet,
			"Returns the item at index.\n"
			":param index: item index" )

		MAP_METHOD(
			"Set",
			PySet,
			"Replaces the item at index.\n"
			":param index: item index\n"
			":param value: bool, int, float, str, Tr2NoesisDataModel, or Tr2NoesisCollection" )

		MAP_METHOD(
			"Add",
			PyAdd,
			"Appends an item and returns its index.\n"
			":param value: item" )

		MAP_METHOD(
			"Insert",
			PyInsert,
			"Inserts an item at index.\n"
			":param index: insertion index\n"
			":param value: item" )

		MAP_METHOD_AND_WRAP(
			"RemoveAt",
			RemoveAt,
			"Removes the item at index. Returns False if the index is out of range.\n"
			":param index: item index\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"Clear",
			Clear,
			"Removes every item.\n"
			":rtype: None" )

	EXPOSURE_END()
}

#endif
