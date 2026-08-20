// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisCollection.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#include "Noesis/Tr2NoesisLog.h"
#include "Noesis/Tr2NoesisSystem.h"

Tr2NoesisCollection::Tr2NoesisCollection( IRoot* )
{
	Tr2Noesis::EnsureInitialized();
	m_items = Noesis::MakePtr<Noesis::ObservableCollection<Noesis::BaseComponent>>();
}

Tr2NoesisCollection::~Tr2NoesisCollection()
{
}

Noesis::BaseObservableCollection* Tr2NoesisCollection::GetNative() const
{
	return m_items;
}

int Tr2NoesisCollection::GetCount() const
{
	return m_items != nullptr ? m_items->Count() : 0;
}

void Tr2NoesisCollection::Clear()
{
	if( m_items != nullptr )
	{
		m_items->Clear();
	}
	m_wrappers.clear();
}

bool Tr2NoesisCollection::RemoveAt( int index )
{
	if( m_items == nullptr || index < 0 || index >= m_items->Count() )
	{
		return false;
	}
	m_items->RemoveAt( static_cast<uint32_t>( index ) );
	if( static_cast<uint32_t>( index ) < m_wrappers.size() )
	{
		m_wrappers.erase( m_wrappers.begin() + index );
	}
	return true;
}

bool Tr2NoesisCollection::SetItem( uint32_t index, Noesis::BaseComponent* item, IRoot* wrapper )
{
	if( m_items == nullptr || static_cast<int>( index ) >= m_items->Count() )
	{
		CCP_NOESIS_LOGERR( "Tr2NoesisCollection.Set index %u is out of range", index );
		return false;
	}
	m_items->Set( index, item );
	if( index < m_wrappers.size() )
	{
		m_wrappers[index] = wrapper;
	}
	return true;
}

int Tr2NoesisCollection::AddItem( Noesis::BaseComponent* item, IRoot* wrapper )
{
	if( m_items == nullptr )
	{
		return -1;
	}
	const int index = m_items->Add( item );
	m_wrappers.push_back( wrapper );
	return index;
}

void Tr2NoesisCollection::InsertItem( uint32_t index, Noesis::BaseComponent* item, IRoot* wrapper )
{
	if( m_items == nullptr )
	{
		return;
	}
	m_items->Insert( index, item );
	if( index > m_wrappers.size() )
	{
		m_wrappers.resize( index );
	}
	m_wrappers.insert( m_wrappers.begin() + index, wrapper );
}

Noesis::BaseComponent* Tr2NoesisCollection::GetItem( uint32_t index ) const
{
	if( m_items == nullptr || static_cast<int>( index ) >= m_items->Count() )
	{
		return nullptr;
	}
	return m_items->Get( index );
}

IRoot* Tr2NoesisCollection::GetWrapper( uint32_t index ) const
{
	if( index >= m_wrappers.size() )
	{
		return nullptr;
	}
	return m_wrappers[index];
}

#endif
