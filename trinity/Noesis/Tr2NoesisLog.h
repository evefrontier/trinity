// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisLog_H
#define Tr2NoesisLog_H

// The Noesis channel carries what went wrong and the handful of one-off facts worth
// knowing at startup. There is no info-level macro on purpose: per-frame tracing is what
// a debugger and a GPU capture are for, and the machinery to gate it cost more than the
// output was worth.
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
