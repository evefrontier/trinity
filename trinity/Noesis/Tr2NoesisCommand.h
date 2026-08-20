// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisCommand_H
#define Tr2NoesisCommand_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include <NsCore/BaseComponent.h>
#include <NsCore/Delegate.h>
#include <NsCore/ReflectionDeclare.h>
#include <NsGui/ICommand.h>

// --------------------------------------------------------------------------------------
// Description:
//   ICommand that forwards Execute / CanExecute to BlueScriptCallback (Python).
// --------------------------------------------------------------------------------------

class Tr2NoesisCommand : public Noesis::BaseComponent, public Noesis::ICommand
{
public:
	Tr2NoesisCommand();

	void SetExecute( const BlueScriptCallback& callback );
	void SetCanExecute( const BlueScriptCallback& callback );
	void RaiseCanExecuteChanged();

	Noesis::EventHandler& CanExecuteChanged() override;
	bool CanExecute( Noesis::BaseComponent* param ) const override;
	void Execute( Noesis::BaseComponent* param ) const override;

	NS_IMPLEMENT_INTERFACE_FIXUP

private:
	mutable BlueScriptCallback m_execute;
	mutable BlueScriptCallback m_canExecute;
	Noesis::EventHandler m_canExecuteChanged;

	NS_DECLARE_REFLECTION( Tr2NoesisCommand, BaseComponent )
};

#endif

#endif
