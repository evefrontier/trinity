// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisObservableCollection_H
#define Tr2NoesisObservableCollection_H

#if WITH_NOESIS

#include <NsCore/ReflectionImplementEmpty.h>
#include <NsGui/ObservableCollection.h>

#if BLUE_WITH_PYTHON
#ifndef PyObject_HEAD
struct _object;
typedef struct _object PyObject;
#endif
#endif

// --------------------------------------------------------------------------------------
// Description:
//   ObservableCollection that can hold a weak Python Collection facade so CommandParameter
//   `{Binding items}` round-trips the same Python object.
// --------------------------------------------------------------------------------------

class Tr2NoesisObservableCollection : public Noesis::ObservableCollection<Noesis::BaseComponent>
{
public:
	Tr2NoesisObservableCollection();
	~Tr2NoesisObservableCollection();

#if BLUE_WITH_PYTHON
	void SetPythonWrapper( PyObject* wrapper );
	PyObject* GetPythonWrapper() const;
#endif

private:
#if BLUE_WITH_PYTHON
	PyObject* m_pythonWrapperWeak = nullptr;
#endif

	NS_IMPLEMENT_INLINE_REFLECTION_( Tr2NoesisObservableCollection,
		Noesis::ObservableCollection<Noesis::BaseComponent>,
		"Tr2NoesisObservableCollection" )
};

#endif

#endif
