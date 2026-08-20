// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisCollection_H
#define Tr2NoesisCollection_H

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include <NsCore/Ptr.h>
#include <NsGui/ObservableCollection.h>

#include <vector>

// --------------------------------------------------------------------------------------
// Description:
//   Blue wrapper around ObservableCollection so Python can drive ItemsControl.ItemsSource.
// --------------------------------------------------------------------------------------

BLUE_DECLARE( Tr2NoesisCollection );

class Tr2NoesisCollection : public IRoot
{
public:
	EXPOSE_TO_BLUE();
	Tr2NoesisCollection( IRoot* lockobj = NULL );
	~Tr2NoesisCollection();

	Noesis::BaseObservableCollection* GetNative() const;

	int GetCount() const;
	void Clear();
	bool RemoveAt( int index );

	bool SetItem( uint32_t index, Noesis::BaseComponent* item, IRoot* wrapper );
	int AddItem( Noesis::BaseComponent* item, IRoot* wrapper );
	void InsertItem( uint32_t index, Noesis::BaseComponent* item, IRoot* wrapper );

	Noesis::BaseComponent* GetItem( uint32_t index ) const;
	IRoot* GetWrapper( uint32_t index ) const;

private:
	Noesis::Ptr<Noesis::ObservableCollection<Noesis::BaseComponent>> m_items;
	std::vector<IRootPtr> m_wrappers;
};

TYPEDEF_BLUECLASS( Tr2NoesisCollection );

#endif

#endif
