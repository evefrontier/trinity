// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisError.h"

#if BLUE_WITH_PYTHON

#include <BlueStdResult.h>

BLUE_DEFINE_EXCEPTION( NoesisError, BlueStdRuntimeError );
BLUE_DEFINE_EXCEPTION( NoesisAbiMismatchError, NoesisError );

#endif
