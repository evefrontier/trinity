// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisObject_H
#define Tr2NoesisObject_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisTypes.h"

#include <NsCore/BaseComponent.h>
#include <NsCore/Delegate.h>
#include <NsCore/Ptr.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsGui/INotifyPropertyChanged.h>

#include <unordered_map>

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

	bool Define( const char* name, Tr2NoesisPropertyType type );
	bool Has( const char* name ) const;
	Tr2NoesisPropertyType GetPropertyType( const char* name ) const;

	bool SetValue( const char* name, Noesis::BaseComponent* value );
	Noesis::BaseComponent* GetValue( const char* name ) const;

	void SetCommand( const char* name, const BlueScriptCallback& execute );
	void SetCanExecute( const char* name, const BlueScriptCallback& canExecute );
	void RaiseCanExecuteChanged( const char* name );

	void SetOnPropertyChanged( const BlueScriptCallback& callback );
	const BlueScriptCallback& GetOnPropertyChanged() const;

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

	void Notify( const char* name );

	Noesis::TypeClass* m_schema;
	std::string m_schemaName;
	std::unordered_map<uint32_t, Slot> m_values;
	Noesis::PropertyChangedEventHandler m_propertyChanged;
	BlueScriptCallback m_onPropertyChanged;
};

#endif

#endif
