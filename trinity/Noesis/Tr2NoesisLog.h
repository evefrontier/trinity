// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisLog_H
#define Tr2NoesisLog_H

// Errors, warnings, and the few facts worth knowing at startup. There is no info level:
// per-frame tracing belongs in a debugger or a GPU capture, not in the log.
namespace CCP
{
inline CcpLogChannel_t& GetNoesisChannel()
{
	static CcpLogChannel_t s_moduleChannel = CCP_LOG_DEFINE_CHANNEL( "Noesis" );
	return s_moduleChannel;
}
}

#define CCP_NOESIS_LOGERR( ... ) CCP_LOGERR_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )
#define CCP_NOESIS_LOGNOTICE( ... ) CCP_LOGNOTICE_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )
#define CCP_NOESIS_LOGWARN( ... ) CCP_LOGWARN_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )

#endif
