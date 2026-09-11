// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisObject.h"

#if WITH_NOESIS

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
#include <NsCore/StringUtils.h>
#include <NsCore/Symbol.h>
#include <NsCore/TypeClassBuilder.h>
#include <NsCore/TypeOf.h>
#include <NsCore/TypeProperty.h>
#include <NsGui/INotifyPropertyChanged.h>

#include <unordered_map>

struct Tr2NoesisSchema
{
	Noesis::TypeClassBuilder* builder = nullptr;
	// Property types by symbol. On the schema rather than on the object
	// because the schema is shared by every instance created with the same
	// name: one Define is enough for all of them to know what a property is.
	std::unordered_map<uint32_t, Tr2NoesisPropertyType> types;
};

namespace
{

bool ValuesEqual( Noesis::BaseComponent* left, Noesis::BaseComponent* right )
{
	if( left == right )
	{
		return true;
	}
	if( left == nullptr || right == nullptr )
	{
		return false;
	}
	if( Noesis::Boxing::CanUnbox<float>( left ) )
	{
		return Noesis::Boxing::CanUnbox<float>( right )
			&& Noesis::Boxing::Unbox<float>( left ) == Noesis::Boxing::Unbox<float>( right );
	}
	if( Noesis::Boxing::CanUnbox<int>( left ) )
	{
		return Noesis::Boxing::CanUnbox<int>( right )
			&& Noesis::Boxing::Unbox<int>( left ) == Noesis::Boxing::Unbox<int>( right );
	}
	if( Noesis::Boxing::CanUnbox<bool>( left ) )
	{
		return Noesis::Boxing::CanUnbox<bool>( right )
			&& Noesis::Boxing::Unbox<bool>( left ) == Noesis::Boxing::Unbox<bool>( right );
	}
	if( Noesis::Boxing::CanUnbox<Noesis::String>( left ) )
	{
		return Noesis::Boxing::CanUnbox<Noesis::String>( right )
			&& Noesis::StrEquals( Noesis::Boxing::Unbox<Noesis::String>( left ).Str(),
								  Noesis::Boxing::Unbox<Noesis::String>( right ).Str() );
	}
	// Objects and collections are the same value only when they are the same
	// instance, which the pointer comparison above already settled.
	return false;
}

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
		return Noesis::Ptr<Noesis::BaseComponent>( object->GetValue( GetName() ) );
	}

	void SetComponent( void* ptr, Noesis::BaseComponent* value ) const override
	{
		if( IsReadOnly() )
		{
			return;
		}
		Tr2NoesisObject* object = static_cast<Tr2NoesisObject*>( ptr );
		object->SetValue( GetName(), value );
	}

	const void* Get( const void* ptr ) const override
	{
		const Tr2NoesisObject* object = static_cast<const Tr2NoesisObject*>( ptr );
		Noesis::BaseComponent* value = object->GetValue( GetName() );
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
		Noesis::BaseComponent* value = static_cast<const Tr2NoesisObject*>( ptr )->GetValue( GetName() );
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
			object->SetValue( GetName(), Noesis::Boxing::Box( *static_cast<const bool*>( value ) ) );
			break;
		case Tr2NoesisPropertyType::Integer:
			object->SetValue( GetName(), Noesis::Boxing::Box( *static_cast<const int*>( value ) ) );
			break;
		case Tr2NoesisPropertyType::Float:
			object->SetValue( GetName(), Noesis::Boxing::Box( *static_cast<const float*>( value ) ) );
			break;
		case Tr2NoesisPropertyType::String:
			object->SetValue( GetName(), Noesis::Boxing::Box( *static_cast<const Noesis::String*>( value ) ) );
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

Tr2NoesisSchema* GetOrCreateSchema( const char* name )
{
	static std::unordered_map<std::string, Tr2NoesisSchema*> s_schemas;

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
	Tr2NoesisSchema* schema = new Tr2NoesisSchema();
	schema->builder = builder;
	s_schemas[name] = schema;
	return schema;
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
	if( name == nullptr || name[0] == '\0' )
	{
		return false;
	}
	return Define( Noesis::Symbol( name ), type );
}

bool Tr2NoesisObject::Define( Noesis::Symbol name, Tr2NoesisPropertyType type )
{
	if( name.IsNull() || type == Tr2NoesisPropertyType::Unknown || m_schema == nullptr )
	{
		return false;
	}

	auto declared = m_schema->types.find( name );
	if( declared != m_schema->types.end() )
	{
		if( declared->second != type )
		{
			CCP_NOESIS_LOGERR( "Define '%s' type %s does not match existing %s on %s",
							   name.Str(), Tr2NoesisPropertyTypeName( type ),
							   Tr2NoesisPropertyTypeName( declared->second ), m_schemaName.c_str() );
			return false;
		}
		m_values[name].type = type;
		return true;
	}

	if( m_schema->builder->FindProperty( name ) == nullptr )
	{
		m_schema->builder->AddProperty( new Tr2NoesisTypeProperty( name, type ) );
	}
	m_schema->types[name] = type;
	m_values[name].type = type;
	return true;
}

bool Tr2NoesisObject::Has( const char* name ) const
{
	if( name == nullptr )
	{
		return false;
	}
	return Has( Noesis::Symbol( name ) );
}

bool Tr2NoesisObject::Has( Noesis::Symbol name ) const
{
	return m_schema != nullptr && m_schema->builder->FindProperty( name ) != nullptr;
}

Tr2NoesisPropertyType Tr2NoesisObject::GetPropertyType( const char* name ) const
{
	if( name == nullptr )
	{
		return Tr2NoesisPropertyType::Unknown;
	}
	return GetPropertyType( Noesis::Symbol( name ) );
}

Tr2NoesisPropertyType Tr2NoesisObject::GetPropertyType( Noesis::Symbol name ) const
{
	auto found = m_values.find( name );
	if( found != m_values.end() && found->second.type != Tr2NoesisPropertyType::Unknown )
	{
		return found->second.type;
	}
	// Not defined on this instance, but the schema is shared, so whatever
	// another instance declared holds here too.
	if( m_schema != nullptr )
	{
		auto declared = m_schema->types.find( name );
		if( declared != m_schema->types.end() )
		{
			return declared->second;
		}
	}
	return Tr2NoesisPropertyType::Unknown;
}

bool Tr2NoesisObject::SetValue( const char* name, Noesis::BaseComponent* value )
{
	if( name == nullptr || name[0] == '\0' )
	{
		return false;
	}
	return SetValue( Noesis::Symbol( name ), value );
}

bool Tr2NoesisObject::SetValue( Noesis::Symbol name, Noesis::BaseComponent* value, bool notifyScript )
{
	if( name.IsNull() )
	{
		return false;
	}

	auto found = m_values.find( name );
	if( found == m_values.end() )
	{
		if( m_schema == nullptr || m_schema->builder->FindProperty( name ) == nullptr )
		{
			CCP_NOESIS_LOGERR( "Set '%s' on %s: property is not defined", name.Str(), m_schemaName.c_str() );
			return false;
		}
		Slot& slot = m_values[name];
		auto declared = m_schema->types.find( name );
		if( declared != m_schema->types.end() )
		{
			slot.type = declared->second;
		}
		slot.value.Reset( value );
		Notify( name, notifyScript );
		return true;
	}

	if( ValuesEqual( found->second.value, value ) )
	{
		// Nothing changed, so nothing downstream has anything to do: no
		// binding invalidation, no layout, no callback.
		return true;
	}

	found->second.value.Reset( value );
	Notify( name, notifyScript );
	return true;
}

Noesis::BaseComponent* Tr2NoesisObject::GetValue( const char* name ) const
{
	if( name == nullptr )
	{
		return nullptr;
	}
	return GetValue( Noesis::Symbol( name ) );
}

Noesis::BaseComponent* Tr2NoesisObject::GetValue( Noesis::Symbol name ) const
{
	auto found = m_values.find( name );
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
	Notify( Noesis::Symbol( name ), true );
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
	return m_schema != nullptr ? m_schema->builder
							   : StaticGetClassType( (Noesis::TypeTag<Tr2NoesisObject>*)nullptr );
}

Noesis::PropertyChangedEventHandler& Tr2NoesisObject::PropertyChanged()
{
	return m_propertyChanged;
}

void Tr2NoesisObject::Notify( Noesis::Symbol name, bool notifyScript )
{
	m_propertyChanged( this, Noesis::PropertyChangedEventArgs( name ) );
	if( notifyScript && m_onPropertyChanged )
	{
		if( !m_onPropertyChanged.CallVoid( name.Str() ) )
		{
			CCP_NOESIS_LOGERR( "onPropertyChanged callback failed for '%s' on %s", name.Str(),
							   m_schemaName.c_str() );
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
