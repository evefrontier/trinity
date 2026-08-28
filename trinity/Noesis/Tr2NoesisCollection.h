// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisCollection_H
#define Tr2NoesisCollection_H

#if WITH_NOESIS

#include <NsCore/Ptr.h>
#include <NsGui/ObservableCollection.h>

#include "Noesis/Tr2NoesisObservableCollection.h"

#include <vector>

#if BLUE_WITH_PYTHON
#ifndef PyObject_HEAD
struct _object;
typedef struct _object PyObject;
#endif
#endif

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

#if BLUE_WITH_PYTHON
	void SetPythonWrapper( PyObject* wrapper );
	PyObject* GetPythonWrapper() const;
#endif

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
	Noesis::Ptr<Tr2NoesisObservableCollection> m_items;
	std::vector<IRootPtr> m_wrappers;
};

TYPEDEF_BLUECLASS( Tr2NoesisCollection );

#endif

#endif
