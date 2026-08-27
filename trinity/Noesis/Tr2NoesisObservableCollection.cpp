// Copyright © 2026 CCP ehf.

#include "StdAfx.h"

#include "Noesis/Tr2NoesisObservableCollection.h"

#if WITH_NOESIS && ( TRINITY_PLATFORM == TRINITY_DIRECTX12 )

#if BLUE_WITH_PYTHON
#include "Noesis/Tr2NoesisPythonWeak.h"
#endif

Tr2NoesisObservableCollection::Tr2NoesisObservableCollection()
{
}

Tr2NoesisObservableCollection::~Tr2NoesisObservableCollection()
{
#if BLUE_WITH_PYTHON
	Tr2NoesisClearPythonWeakRef( m_pythonWrapperWeak );
#endif
}

#if BLUE_WITH_PYTHON

void Tr2NoesisObservableCollection::SetPythonWrapper( PyObject* wrapper )
{
	Tr2NoesisSetPythonWeakRef( m_pythonWrapperWeak, wrapper );
}

PyObject* Tr2NoesisObservableCollection::GetPythonWrapper() const
{
	return Tr2NoesisGetPythonWeakRef( m_pythonWrapperWeak );
}

#endif

#endif
