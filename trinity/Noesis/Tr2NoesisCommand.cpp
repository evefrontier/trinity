// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisCommand.h"

#if WITH_NOESIS

#include "Noesis/Tr2NoesisLog.h"

#if BLUE_WITH_PYTHON
#include "Noesis/Tr2NoesisPython.h"
#endif

#include <NsCore/ReflectionImplement.h>

Tr2NoesisCommand::Tr2NoesisCommand()
{
}

void Tr2NoesisCommand::SetExecute( const BlueScriptCallback& callback )
{
	m_execute = callback;
}

void Tr2NoesisCommand::SetCanExecute( const BlueScriptCallback& callback )
{
	m_canExecute = callback;
	RaiseCanExecuteChanged();
}

void Tr2NoesisCommand::RaiseCanExecuteChanged()
{
	m_canExecuteChanged( this, Noesis::EventArgs::Empty );
}

Noesis::EventHandler& Tr2NoesisCommand::CanExecuteChanged()
{
	return m_canExecuteChanged;
}

bool Tr2NoesisCommand::CanExecute( Noesis::BaseComponent* param ) const
{
	if( !m_canExecute )
	{
		return true;
	}

	bool result = false;
#if BLUE_WITH_PYTHON
	PyObject* pyParam = Tr2NoesisCommandParamToPython( param );
	const bool ok = static_cast<bool>( m_canExecute.Call( result, pyParam ) );
	Py_DECREF( pyParam );
#else
	const bool ok = static_cast<bool>( m_canExecute.Call( result ) );
#endif
	if( !ok )
	{
		CCP_NOESIS_LOGERR( "Tr2NoesisCommand CanExecute callback failed" );
#if BLUE_WITH_PYTHON
		PyOS->PyFlushError( "Tr2NoesisCommand: CanExecute callback failed" );
#endif
		return false;
	}
	return result;
}

void Tr2NoesisCommand::Execute( Noesis::BaseComponent* param ) const
{
	if( !m_execute )
	{
		return;
	}
#if BLUE_WITH_PYTHON
	PyObject* pyParam = Tr2NoesisCommandParamToPython( param );
	const bool ok = static_cast<bool>( m_execute.CallVoid( pyParam ) );
	Py_DECREF( pyParam );
#else
	const bool ok = static_cast<bool>( m_execute.CallVoid() );
#endif
	if( !ok )
	{
		CCP_NOESIS_LOGERR( "Tr2NoesisCommand Execute callback failed" );
#if BLUE_WITH_PYTHON
		PyOS->PyFlushError( "Tr2NoesisCommand: Execute callback failed" );
#endif
	}
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION( Tr2NoesisCommand )
{
	NsImpl<Noesis::ICommand>();
}

NS_END_COLD_REGION

#endif
