// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisError_H
#define Tr2NoesisError_H

#if BLUE_WITH_PYTHON

// --------------------------------------------------------------------------------------
// Description:
//   The exceptions the Noesis bindings raise, mirroring the rules the library side
//   settled on: a call that cannot do what it was asked raises, rather than returning a
//   status nobody is obliged to check, and the type says which kind of wrong it was.
//
//   trinity.NoesisError is the base, so a caller that does not care can catch one thing.
//   These are Blue exceptions, registered with every other one by
//   BlueRegisterExceptionsToModule, rather than types this file creates and attaches
//   itself.
//
//   Deliberately Trinity's own and not the library's: neither module imports the other,
//   which is the same rule that keeps nxt.h free of a shared object system.
// --------------------------------------------------------------------------------------

BLUE_DECLARE_EXCEPTION( NoesisError );

// The object is a Noesis interface, but one built against an nxt ABI this Trinity cannot
// call. Distinct because the fix is a rebuild of one side, not a change at the call site.
BLUE_DECLARE_EXCEPTION( NoesisAbiMismatchError );

#endif
#endif
