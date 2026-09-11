// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisObject_H
#define Tr2NoesisObject_H

#if WITH_NOESIS

#include "Noesis/Tr2NoesisTypes.h"

#include <NsCore/BaseComponent.h>
#include <NsCore/Delegate.h>
#include <NsCore/Ptr.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsCore/Symbol.h>
#include <NsGui/INotifyPropertyChanged.h>

#include <unordered_map>

// Runtime TypeClass for one schema name, plus the property types declared on it.
// Shared by every object created with that name; defined in Tr2NoesisObject.cpp.
struct Tr2NoesisSchema;

#if BLUE_WITH_PYTHON
#ifndef PyObject_HEAD
struct _object;
typedef struct _object PyObject;
#endif
#endif

// --------------------------------------------------------------------------------------
// Description:
//   Native-owned observable bag used as a Noesis DataContext. Properties are declared
//   on a per-schema runtime TypeClass so {Binding name} resolves, and values live in a
//   map of boxed Noesis objects so binding evaluation never calls into Python.
// --------------------------------------------------------------------------------------

class Tr2NoesisObject : public Noesis::BaseComponent, public Noesis::INotifyPropertyChanged
{
public:
	explicit Tr2NoesisObject( const char* schemaName );
	~Tr2NoesisObject();

	const char* GetSchemaName() const;

	// Symbol overloads for callers writing the same property every frame: the
	// name is interned once at the call site instead of once per lookup.
	bool Define( const char* name, Tr2NoesisPropertyType type );
	bool Define( Noesis::Symbol name, Tr2NoesisPropertyType type );
	bool Has( const char* name ) const;
	bool Has( Noesis::Symbol name ) const;
	Tr2NoesisPropertyType GetPropertyType( const char* name ) const;
	Tr2NoesisPropertyType GetPropertyType( Noesis::Symbol name ) const;

	// notifyScript false leaves the onPropertyChanged callback alone, for a
	// writer that already knows what it wrote - script setting its own
	// property. The view is told either way.
	bool SetValue( const char* name, Noesis::BaseComponent* value );
	bool SetValue( Noesis::Symbol name, Noesis::BaseComponent* value, bool notifyScript = true );
	Noesis::BaseComponent* GetValue( const char* name ) const;
	Noesis::BaseComponent* GetValue( Noesis::Symbol name ) const;

	void SetCommand( const char* name, const BlueScriptCallback& execute );
	void SetCanExecute( const char* name, const BlueScriptCallback& canExecute );
	void RaiseCanExecuteChanged( const char* name );

	void SetOnPropertyChanged( const BlueScriptCallback& callback );
	const BlueScriptCallback& GetOnPropertyChanged() const;

#if BLUE_WITH_PYTHON
	void SetPythonWrapper( PyObject* wrapper );
	PyObject* GetPythonWrapper() const;
#endif

	const Noesis::TypeClass* GetClassType() const override;
	static const Noesis::TypeClass* StaticGetClassType( Noesis::TypeTag<Tr2NoesisObject>* );

	Noesis::PropertyChangedEventHandler& PropertyChanged() override;

	NS_IMPLEMENT_INTERFACE_FIXUP

private:
	struct Slot
	{
		Tr2NoesisPropertyType type = Tr2NoesisPropertyType::Unknown;
		Noesis::Ptr<Noesis::BaseComponent> value;
	};

	typedef Tr2NoesisObject SelfClass;
	typedef Noesis::BaseComponent ParentClass;
	friend class Noesis::TypeClassCreator;
	struct Rebind_;
	static void StaticFillClassType( Noesis::TypeClassCreator& helper );

	void Notify( Noesis::Symbol name, bool notifyScript );

	Tr2NoesisSchema* m_schema;
	std::string m_schemaName;
	std::unordered_map<uint32_t, Slot> m_values;
	Noesis::PropertyChangedEventHandler m_propertyChanged;
	BlueScriptCallback m_onPropertyChanged;
#if BLUE_WITH_PYTHON
	PyObject* m_pythonWrapperWeak = nullptr;
#endif
};

#endif

#endif
