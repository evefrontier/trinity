// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisDataModel_H
#define Tr2NoesisDataModel_H

#if WITH_NOESIS

#include "Noesis/Tr2NoesisObject.h"

#include <unordered_map>

// --------------------------------------------------------------------------------------
// Description:
//   Blue wrapper around Tr2NoesisObject. Python Define / Get / Set / SetCommand talk
//   to this; SetDataContext on Tr2NoesisView installs the native object.
// --------------------------------------------------------------------------------------

BLUE_DECLARE( Tr2NoesisDataModel );

class Tr2NoesisDataModel : public IRoot
{
public:
	EXPOSE_TO_BLUE();
	Tr2NoesisDataModel( IRoot* lockobj = NULL );
	~Tr2NoesisDataModel();

	void py__init__( const char* schemaName );

	std::string GetSchemaName() const;
	Tr2NoesisObject* GetNative();

	bool Define( const char* name, const char* type );
	bool Has( const char* name ) const;

	void SetCommand( const char* name, const BlueScriptCallback& callback );
	void SetCanExecute( const char* name, const BlueScriptCallback& callback );
	void RaiseCanExecuteChanged( const char* name );

	const BlueScriptCallback& GetOnPropertyChanged() const;
	void SetOnPropertyChanged( const BlueScriptCallback& callback );

	bool SetValue( const char* name, Noesis::BaseComponent* value, IRoot* wrapper );
	Noesis::BaseComponent* GetValue( const char* name ) const;
	IRoot* GetWrapper( const char* name ) const;

private:
	void EnsureObject( const char* schemaName );

	Noesis::Ptr<Tr2NoesisObject> m_object;
	std::unordered_map<std::string, IRootPtr> m_wrappers;
};

TYPEDEF_BLUECLASS( Tr2NoesisDataModel );

#endif

#endif
