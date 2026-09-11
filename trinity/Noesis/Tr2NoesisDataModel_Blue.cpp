// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisDataModel.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisPython.h"

BLUE_DEFINE( Tr2NoesisDataModel );

#if BLUE_WITH_PYTHON

static PyObject* PyGet( PyObject* self, PyObject* args )
{
	Tr2NoesisDataModel* model = BluePythonCast<Tr2NoesisDataModel*>( self );
	const char* name = nullptr;
	if( !PyArg_ParseTuple( args, "s", &name ) )
	{
		return nullptr;
	}
	const Noesis::Symbol symbol( name );
	return Tr2NoesisComponentToPython( model->GetValue( symbol ), model->GetWrapper( symbol ) );
}

static PyObject* PySet( PyObject* self, PyObject* args )
{
	Tr2NoesisDataModel* model = BluePythonCast<Tr2NoesisDataModel*>( self );
	const char* name = nullptr;
	PyObject* value = nullptr;
	if( !PyArg_ParseTuple( args, "sO", &name, &value ) )
	{
		return nullptr;
	}

	// Interned once here and then handed down: the type lookup, the define
	// and the write all take the symbol rather than hashing the name again.
	const Noesis::Symbol symbol( name );
	Tr2NoesisPropertyType expected = Tr2NoesisPropertyType::Unknown;
	if( model->GetNative() != nullptr )
	{
		expected = model->GetNative()->GetPropertyType( symbol );
	}

	Noesis::Ptr<Noesis::BaseComponent> boxed;
	IRoot* wrapper = nullptr;
	Tr2NoesisPropertyType inferred = Tr2NoesisPropertyType::Unknown;
	if( !Tr2NoesisPythonToComponent( value, expected, boxed, wrapper, &inferred ) )
	{
		return nullptr;
	}

	// A known type means the property is declared, so there is nothing to ask
	// the schema and nothing to define.
	if( expected == Tr2NoesisPropertyType::Unknown )
	{
		if( inferred == Tr2NoesisPropertyType::Unknown )
		{
			PyErr_SetString( PyExc_TypeError, "cannot infer a type from None; call Define first" );
			return nullptr;
		}
		if( !model->DefineProperty( symbol, inferred ) )
		{
			PyErr_SetString( PyExc_RuntimeError, "Define failed" );
			return nullptr;
		}
	}

	// Script wrote this, so script is not told about it: whoever called Set
	// already knows, and the change signal costs a round trip back into
	// Python. The view is notified either way.
	if( !model->SetValue( symbol, boxed, wrapper, false ) )
	{
		PyErr_SetString( PyExc_RuntimeError, "Set failed" );
		return nullptr;
	}
	Py_RETURN_NONE;
}

static PyObject* PyGetPythonWrapper( PyObject* self, PyObject* /*args*/ )
{
	Tr2NoesisDataModel* model = BluePythonCast<Tr2NoesisDataModel*>( self );
	Tr2NoesisObject* object = model->GetNative();
	if( object == nullptr )
	{
		Py_RETURN_NONE;
	}
	return object->GetPythonWrapper();
}

static PyObject* PySetPythonWrapper( PyObject* self, PyObject* args )
{
	Tr2NoesisDataModel* model = BluePythonCast<Tr2NoesisDataModel*>( self );
	PyObject* value = nullptr;
	if( !PyArg_ParseTuple( args, "O", &value ) )
	{
		return nullptr;
	}
	Tr2NoesisObject* object = model->GetNative();
	if( object != nullptr )
	{
		object->SetPythonWrapper( value );
	}
	Py_RETURN_NONE;
}

#endif

const Be::ClassInfo* Tr2NoesisDataModel::ExposeToBlue()
{
	EXPOSURE_BEGIN( Tr2NoesisDataModel, "Observable data model for a NoesisGUI view. Set it as the view DataContext and bind XAML to its properties." )
		MAP_INTERFACE( Tr2NoesisDataModel )

		MAP_METHOD_AND_WRAP_OPTIONAL_ARGS(
			"__init__",
			py__init__,
			1,
			"Create a data model. Instances that share schemaName share a property schema.\n"
			":param schemaName: optional schema name, for example 'Hud'" )

		MAP_PROPERTY_READONLY(
			"schemaName",
			GetSchemaName,
			"Name of the interned Noesis reflection type for this model." )

		MAP_METHOD_AND_WRAP(
			"Define",
			Define,
			"Declares a property so XAML {Binding name} can resolve it.\n"
			":param name: property name, matching the Binding Path\n"
			":param type: 'bool', 'int', 'float', 'string', 'object', 'command', or 'collection'\n"
			":rtype: bool" )

		MAP_METHOD_AND_WRAP(
			"Has",
			Has,
			"True if the schema already has this property.\n"
			":param name: property name\n"
			":rtype: bool" )

		MAP_METHOD(
			"Get",
			PyGet,
			"Returns the current value of a property.\n"
			":param name: property name" )

		MAP_METHOD(
			"Set",
			PySet,
			"Sets a property and notifies the view. Infers the type if Define was not called.\n"
			"Writing the value the property already holds does nothing at all, and a write from\n"
			"script never raises onPropertyChanged.\n"
			":param name: property name\n"
			":param value: bool, int, float, str, Tr2NoesisDataModel, or Tr2NoesisCollection" )

		MAP_METHOD_AND_WRAP(
			"SetCommand",
			SetCommand,
			"Defines name as a command and binds a Python callable. Button.Command='{Binding name}'.\n"
			":param name: command property name\n"
			":param callback: zero-argument callable" )

		MAP_METHOD_AND_WRAP(
			"SetCanExecute",
			SetCanExecute,
			"Binds a zero-argument callable that returns bool for command CanExecute.\n"
			":param name: command property name\n"
			":param callback: callable returning bool" )

		MAP_METHOD_AND_WRAP(
			"RaiseCanExecuteChanged",
			RaiseCanExecuteChanged,
			"Fires CanExecuteChanged for the named command.\n"
			":param name: command property name" )

		MAP_PROPERTY(
			"onPropertyChanged",
			GetOnPropertyChanged,
			SetOnPropertyChanged,
			"Callable(name) invoked when a two-way binding writes a property from the UI.\n"
			"Not called for a write made from script, which already knows what it wrote." )

		MAP_METHOD(
			"GetPythonWrapper",
			PyGetPythonWrapper,
			"Returns the weak Python facade installed with SetPythonWrapper, or None." )

		MAP_METHOD(
			"SetPythonWrapper",
			PySetPythonWrapper,
			"Stores a weak reference to the Python Model facade for CommandParameter round-trips.\n"
			":param wrapper: noesis.Model instance or None" )

	EXPOSURE_END()
}

#endif
