// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisDataModel.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

Tr2NoesisDataModel::Tr2NoesisDataModel( IRoot* )
{
}

Tr2NoesisDataModel::~Tr2NoesisDataModel()
{
}

void Tr2NoesisDataModel::EnsureObject( const char* schemaName )
{
	if( m_object != nullptr )
	{
		return;
	}
	if( !Tr2Noesis::RequireInitialized() )
	{
		return;
	}
	m_object = Noesis::MakePtr<Tr2NoesisObject>( schemaName );
}

void Tr2NoesisDataModel::py__init__( const char* schemaName )
{
	EnsureObject( schemaName );
}

std::string Tr2NoesisDataModel::GetSchemaName() const
{
	return m_object != nullptr ? m_object->GetSchemaName() : "";
}

Tr2NoesisObject* Tr2NoesisDataModel::GetNative()
{
	EnsureObject( nullptr );
	return m_object;
}

bool Tr2NoesisDataModel::Define( const char* name, const char* type )
{
	const_cast<Tr2NoesisDataModel*>( this )->EnsureObject( nullptr );
	const Tr2NoesisPropertyType parsed = Tr2NoesisParsePropertyType( type );
	if( parsed == Tr2NoesisPropertyType::Unknown )
	{
		CCP_NOESIS_LOGERR( "Define '%s': unknown type '%s'", name ? name : "", type ? type : "" );
		return false;
	}
	return m_object->Define( name, parsed );
}

bool Tr2NoesisDataModel::DefineProperty( Noesis::Symbol name, Tr2NoesisPropertyType type )
{
	EnsureObject( nullptr );
	return m_object != nullptr && m_object->Define( name, type );
}

bool Tr2NoesisDataModel::Has( const char* name ) const
{
	return m_object != nullptr && m_object->Has( name );
}

void Tr2NoesisDataModel::SetCommand( const char* name, const BlueScriptCallback& callback )
{
	EnsureObject( nullptr );
	m_object->SetCommand( name, callback );
}

void Tr2NoesisDataModel::SetCanExecute( const char* name, const BlueScriptCallback& callback )
{
	EnsureObject( nullptr );
	m_object->SetCanExecute( name, callback );
}

void Tr2NoesisDataModel::RaiseCanExecuteChanged( const char* name )
{
	if( m_object != nullptr )
	{
		m_object->RaiseCanExecuteChanged( name );
	}
}

const BlueScriptCallback& Tr2NoesisDataModel::GetOnPropertyChanged() const
{
	static BlueScriptCallback s_empty;
	return m_object != nullptr ? m_object->GetOnPropertyChanged() : s_empty;
}

void Tr2NoesisDataModel::SetOnPropertyChanged( const BlueScriptCallback& callback )
{
	EnsureObject( nullptr );
	m_object->SetOnPropertyChanged( callback );
}

bool Tr2NoesisDataModel::SetValue( Noesis::Symbol name, Noesis::BaseComponent* value, IRoot* wrapper,
								   bool notifyScript )
{
	EnsureObject( nullptr );
	if( name.IsNull() )
	{
		return false;
	}
	if( wrapper != nullptr )
	{
		m_wrappers[name] = wrapper;
	}
	else if( !m_wrappers.empty() )
	{
		// Skipped outright for a model that only holds scalars, which is the
		// common one and writes the most often.
		m_wrappers.erase( name );
	}
	return m_object->SetValue( name, value, notifyScript );
}

Noesis::BaseComponent* Tr2NoesisDataModel::GetValue( Noesis::Symbol name ) const
{
	return m_object != nullptr ? m_object->GetValue( name ) : nullptr;
}

IRoot* Tr2NoesisDataModel::GetWrapper( Noesis::Symbol name ) const
{
	auto found = m_wrappers.find( name );
	if( found == m_wrappers.end() )
	{
		return nullptr;
	}
	return found->second;
}

#endif
