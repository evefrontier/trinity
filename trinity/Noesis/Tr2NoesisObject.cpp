// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisObject.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisCommand.h"
#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"
#if BLUE_WITH_PYTHON
#include "Noesis/Tr2NoesisPythonWeak.h"
#endif

#include <NsCore/Boxing.h>
#include <NsCore/DynamicCast.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/String.h>
#include <NsCore/Symbol.h>
#include <NsCore/TypeClassBuilder.h>
#include <NsCore/TypeOf.h>
#include <NsCore/TypeProperty.h>
#include <NsGui/INotifyPropertyChanged.h>

#include <unordered_map>

namespace
{

const Noesis::Type* ContentTypeFor( Tr2NoesisPropertyType type )
{
	switch( type )
	{
	case Tr2NoesisPropertyType::Bool:
		return Noesis::TypeOf<bool>();
	case Tr2NoesisPropertyType::Integer:
		return Noesis::TypeOf<int>();
	case Tr2NoesisPropertyType::Float:
		return Noesis::TypeOf<float>();
	case Tr2NoesisPropertyType::String:
		return Noesis::TypeOf<Noesis::String>();
	default:
		return Noesis::TypeOf<Noesis::BaseComponent>();
	}
}

class Tr2NoesisTypeProperty : public Noesis::TypeProperty
{
public:
	Tr2NoesisTypeProperty( Noesis::Symbol name, Tr2NoesisPropertyType type )
		: TypeProperty( name, ContentTypeFor( type ) )
		, m_type( type )
	{
	}

	void* GetContent( const void* ptr ) const override
	{
		return const_cast<void*>( Get( ptr ) );
	}

	bool IsReadOnly() const override
	{
		return m_type == Tr2NoesisPropertyType::Command;
	}

	Noesis::Ptr<Noesis::BaseComponent> GetComponent( const void* ptr ) const override
	{
		const Tr2NoesisObject* object = static_cast<const Tr2NoesisObject*>( ptr );
		return Noesis::Ptr<Noesis::BaseComponent>( object->GetValue( GetName().Str() ) );
	}

	void SetComponent( void* ptr, Noesis::BaseComponent* value ) const override
	{
		if( IsReadOnly() )
		{
			return;
		}
		Tr2NoesisObject* object = static_cast<Tr2NoesisObject*>( ptr );
		object->SetValue( GetName().Str(), value );
	}

	const void* Get( const void* ptr ) const override
	{
		const Tr2NoesisObject* object = static_cast<const Tr2NoesisObject*>( ptr );
		Noesis::BaseComponent* value = object->GetValue( GetName().Str() );
		if( value == nullptr )
		{
			return nullptr;
		}
		if( Noesis::BoxedValue* boxed = Noesis::DynamicCast<Noesis::BoxedValue*>( value ) )
		{
			return boxed->GetValuePtr();
		}
		return value;
	}

	void Get( const void* ptr, void* dest ) const override
	{
		Noesis::BaseComponent* value = static_cast<const Tr2NoesisObject*>( ptr )->GetValue( GetName().Str() );
		if( value == nullptr || dest == nullptr )
		{
			return;
		}
		switch( m_type )
		{
		case Tr2NoesisPropertyType::Bool:
			if( Noesis::Boxing::CanUnbox<bool>( value ) )
			{
				*static_cast<bool*>( dest ) = Noesis::Boxing::Unbox<bool>( value );
			}
			break;
		case Tr2NoesisPropertyType::Integer:
			if( Noesis::Boxing::CanUnbox<int>( value ) )
			{
				*static_cast<int*>( dest ) = Noesis::Boxing::Unbox<int>( value );
			}
			break;
		case Tr2NoesisPropertyType::Float:
			if( Noesis::Boxing::CanUnbox<float>( value ) )
			{
				*static_cast<float*>( dest ) = Noesis::Boxing::Unbox<float>( value );
			}
			break;
		case Tr2NoesisPropertyType::String:
			if( Noesis::Boxing::CanUnbox<Noesis::String>( value ) )
			{
				*static_cast<Noesis::String*>( dest ) = Noesis::Boxing::Unbox<Noesis::String>( value );
			}
			break;
		default:
			break;
		}
	}

	void Set( void* ptr, const void* value ) const override
	{
		if( IsReadOnly() || value == nullptr )
		{
			return;
		}
		Tr2NoesisObject* object = static_cast<Tr2NoesisObject*>( ptr );
		switch( m_type )
		{
		case Tr2NoesisPropertyType::Bool:
			object->SetValue( GetName().Str(), Noesis::Boxing::Box( *static_cast<const bool*>( value ) ) );
			break;
		case Tr2NoesisPropertyType::Integer:
			object->SetValue( GetName().Str(), Noesis::Boxing::Box( *static_cast<const int*>( value ) ) );
			break;
		case Tr2NoesisPropertyType::Float:
			object->SetValue( GetName().Str(), Noesis::Boxing::Box( *static_cast<const float*>( value ) ) );
			break;
		case Tr2NoesisPropertyType::String:
			object->SetValue( GetName().Str(), Noesis::Boxing::Box( *static_cast<const Noesis::String*>( value ) ) );
			break;
		default:
			break;
		}
	}

private:
	Tr2NoesisPropertyType m_type;
};

uint32_t NotifyOffset()
{
	Tr2NoesisObject* ptr = reinterpret_cast<Tr2NoesisObject*>( 0x10000000 );
	uint8_t* classPtr = reinterpret_cast<uint8_t*>( ptr );
	uint8_t* ifacePtr = reinterpret_cast<uint8_t*>( static_cast<Noesis::INotifyPropertyChanged*>( ptr ) );
	return static_cast<uint32_t>( ifacePtr - classPtr );
}

Noesis::TypeClassBuilder* GetOrCreateSchema( const char* name )
{
	static std::unordered_map<std::string, Noesis::TypeClassBuilder*> s_schemas;

	Tr2NoesisObject::StaticGetClassType( (Noesis::TypeTag<Tr2NoesisObject>*)nullptr );

	auto found = s_schemas.find( name );
	if( found != s_schemas.end() )
	{
		return found->second;
	}

	Noesis::TypeClassBuilder* builder = new Noesis::TypeClassBuilder( Noesis::Symbol( name ), false );
	builder->AddBase( Tr2NoesisObject::StaticGetClassType( (Noesis::TypeTag<Tr2NoesisObject>*)nullptr ) );
	builder->AddInterface(
		Noesis::INotifyPropertyChanged::StaticGetClassType( (Noesis::TypeTag<Noesis::INotifyPropertyChanged>*)nullptr ),
		NotifyOffset() );
	Noesis::Reflection::RegisterType( builder );
	s_schemas[name] = builder;
	return builder;
}

std::string MakeAnonymousSchemaName()
{
	static uint32_t s_next = 0;
	char buffer[64];
	sprintf_s( buffer, "Tr2NoesisModel.Anonymous.%u", ++s_next );
	return buffer;
}

std::string MakeSchemaName( const char* schemaName )
{
	if( schemaName == nullptr || schemaName[0] == '\0' )
	{
		return MakeAnonymousSchemaName();
	}
	if( strncmp( schemaName, "Tr2NoesisModel.", 15 ) == 0 )
	{
		return schemaName;
	}
	std::string name( "Tr2NoesisModel." );
	name += schemaName;
	return name;
}

} // namespace

Tr2NoesisObject::Tr2NoesisObject( const char* schemaName )
	: m_schema( nullptr )
{
	if( !Tr2Noesis::RequireInitialized() )
	{
		return;
	}
	m_schemaName = MakeSchemaName( schemaName );
	m_schema = GetOrCreateSchema( m_schemaName.c_str() );
}

Tr2NoesisObject::~Tr2NoesisObject()
{
#if BLUE_WITH_PYTHON
	Tr2NoesisClearPythonWeakRef( m_pythonWrapperWeak );
#endif
}

const char* Tr2NoesisObject::GetSchemaName() const
{
	return m_schemaName.c_str();
}

bool Tr2NoesisObject::Define( const char* name, Tr2NoesisPropertyType type )
{
	if( name == nullptr || name[0] == '\0' || type == Tr2NoesisPropertyType::Unknown || m_schema == nullptr )
	{
		return false;
	}

	const Noesis::Symbol symbol( name );
	Noesis::TypeClassBuilder* builder = static_cast<Noesis::TypeClassBuilder*>( m_schema );
	const Noesis::TypeProperty* existing = builder->FindProperty( symbol );
	if( existing != nullptr )
	{
		Slot& slot = m_values[symbol];
		if( slot.type == Tr2NoesisPropertyType::Unknown )
		{
			slot.type = type;
		}
		else if( slot.type != type )
		{
			CCP_NOESIS_LOGERR( "Define '%s' type %s does not match existing %s on %s",
							   name, Tr2NoesisPropertyTypeName( type ), Tr2NoesisPropertyTypeName( slot.type ),
							   m_schemaName.c_str() );
			return false;
		}
		return true;
	}

	builder->AddProperty( new Tr2NoesisTypeProperty( symbol, type ) );
	Slot& slot = m_values[symbol];
	slot.type = type;
	return true;
}

bool Tr2NoesisObject::Has( const char* name ) const
{
	if( name == nullptr || m_schema == nullptr )
	{
		return false;
	}
	return m_schema->FindProperty( Noesis::Symbol( name ) ) != nullptr;
}

Tr2NoesisPropertyType Tr2NoesisObject::GetPropertyType( const char* name ) const
{
	if( name == nullptr )
	{
		return Tr2NoesisPropertyType::Unknown;
	}
	auto found = m_values.find( Noesis::Symbol( name ) );
	if( found == m_values.end() )
	{
		return Tr2NoesisPropertyType::Unknown;
	}
	return found->second.type;
}

bool Tr2NoesisObject::SetValue( const char* name, Noesis::BaseComponent* value )
{
	if( name == nullptr || name[0] == '\0' )
	{
		return false;
	}

	const Noesis::Symbol symbol( name );
	auto found = m_values.find( symbol );
	if( found == m_values.end() )
	{
		if( m_schema == nullptr || m_schema->FindProperty( symbol ) == nullptr )
		{
			CCP_NOESIS_LOGERR( "Set '%s' on %s: property is not defined", name, m_schemaName.c_str() );
			return false;
		}
		Slot& slot = m_values[symbol];
		slot.value.Reset( value );
		Notify( name );
		return true;
	}

	found->second.value.Reset( value );
	Notify( name );
	return true;
}

Noesis::BaseComponent* Tr2NoesisObject::GetValue( const char* name ) const
{
	if( name == nullptr )
	{
		return nullptr;
	}
	auto found = m_values.find( Noesis::Symbol( name ) );
	if( found == m_values.end() )
	{
		return nullptr;
	}
	return found->second.value;
}

void Tr2NoesisObject::SetCommand( const char* name, const BlueScriptCallback& execute )
{
	if( !Define( name, Tr2NoesisPropertyType::Command ) )
	{
		return;
	}
	Noesis::Ptr<Tr2NoesisCommand> command = Noesis::DynamicPtrCast<Tr2NoesisCommand>(
		Noesis::Ptr<Noesis::BaseComponent>( GetValue( name ) ) );
	if( command == nullptr )
	{
		command = Noesis::MakePtr<Tr2NoesisCommand>();
		m_values[Noesis::Symbol( name )].value = command;
	}
	command->SetExecute( execute );
	Notify( name );
}

void Tr2NoesisObject::SetCanExecute( const char* name, const BlueScriptCallback& canExecute )
{
	if( !Define( name, Tr2NoesisPropertyType::Command ) )
	{
		return;
	}
	Noesis::Ptr<Tr2NoesisCommand> command = Noesis::DynamicPtrCast<Tr2NoesisCommand>(
		Noesis::Ptr<Noesis::BaseComponent>( GetValue( name ) ) );
	if( command == nullptr )
	{
		command = Noesis::MakePtr<Tr2NoesisCommand>();
		m_values[Noesis::Symbol( name )].value = command;
	}
	command->SetCanExecute( canExecute );
}

void Tr2NoesisObject::RaiseCanExecuteChanged( const char* name )
{
	Noesis::Ptr<Tr2NoesisCommand> command = Noesis::DynamicPtrCast<Tr2NoesisCommand>(
		Noesis::Ptr<Noesis::BaseComponent>( GetValue( name ) ) );
	if( command != nullptr )
	{
		command->RaiseCanExecuteChanged();
	}
}

void Tr2NoesisObject::SetOnPropertyChanged( const BlueScriptCallback& callback )
{
	m_onPropertyChanged = callback;
}

const BlueScriptCallback& Tr2NoesisObject::GetOnPropertyChanged() const
{
	return m_onPropertyChanged;
}

#if BLUE_WITH_PYTHON

void Tr2NoesisObject::SetPythonWrapper( PyObject* wrapper )
{
	Tr2NoesisSetPythonWeakRef( m_pythonWrapperWeak, wrapper );
}

PyObject* Tr2NoesisObject::GetPythonWrapper() const
{
	return Tr2NoesisGetPythonWeakRef( m_pythonWrapperWeak );
}

#endif

const Noesis::TypeClass* Tr2NoesisObject::GetClassType() const
{
	return m_schema != nullptr ? m_schema : StaticGetClassType( (Noesis::TypeTag<Tr2NoesisObject>*)nullptr );
}

Noesis::PropertyChangedEventHandler& Tr2NoesisObject::PropertyChanged()
{
	return m_propertyChanged;
}

void Tr2NoesisObject::Notify( const char* name )
{
	m_propertyChanged( this, Noesis::PropertyChangedEventArgs( Noesis::Symbol( name ) ) );
	if( m_onPropertyChanged )
	{
		if( !m_onPropertyChanged.CallVoid( name ) )
		{
			CCP_NOESIS_LOGERR( "onPropertyChanged callback failed for '%s' on %s", name, m_schemaName.c_str() );
#if BLUE_WITH_PYTHON
			PyOS->PyFlushError( "Tr2NoesisObject: onPropertyChanged callback failed" );
#endif
		}
	}
}

NS_NO_INLINE const Noesis::TypeClass* Tr2NoesisObject::StaticGetClassType( Noesis::TypeTag<Tr2NoesisObject>* )
{
	static const Noesis::TypeClass* type;
	if( NS_UNLIKELY( type == 0 ) )
	{
		type = static_cast<const Noesis::TypeClass*>( Noesis::Reflection::RegisterType(
			"Tr2NoesisObject",
			Noesis::TypeClassCreator::Create<SelfClass>,
			Noesis::TypeClassCreator::Fill<SelfClass, ParentClass> ) );
	}
	return type;
}

NS_COLD_FUNC void Tr2NoesisObject::StaticFillClassType( Noesis::TypeClassCreator& helper )
{
	helper.Impl<Tr2NoesisObject, Noesis::INotifyPropertyChanged>();
}

#endif
