// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisCommand.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisLog.h"

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

bool Tr2NoesisCommand::CanExecute( Noesis::BaseComponent* ) const
{
	if( !m_canExecute )
	{
		return true;
	}

	bool result = false;
	if( !m_canExecute.Call( result ) )
	{
		CCP_NOESIS_LOGERR( "Tr2NoesisCommand CanExecute callback failed" );
#if BLUE_WITH_PYTHON
		PyOS->PyFlushError( "Tr2NoesisCommand: CanExecute callback failed" );
#endif
		return false;
	}
	return result;
}

void Tr2NoesisCommand::Execute( Noesis::BaseComponent* ) const
{
	if( !m_execute )
	{
		return;
	}
	if( !m_execute.CallVoid() )
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
