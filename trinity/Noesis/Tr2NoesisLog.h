// Copyright © 2026 CCP ehf.

#pragma once
#ifndef Tr2NoesisLog_H
#define Tr2NoesisLog_H

#if WITH_NOESIS

namespace CCP
{
inline CcpLogChannel_t& GetNoesisChannel()
{
	static CcpLogChannel_t s_moduleChannel = CCP_LOG_DEFINE_CHANNEL( "Noesis" );
	return s_moduleChannel;
}
}

#define CCP_NOESIS_LOG( ... ) CCP_LOG_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )
#define CCP_NOESIS_LOGERR( ... ) CCP_LOGERR_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )
#define CCP_NOESIS_LOGNOTICE( ... ) CCP_LOGNOTICE_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )
#define CCP_NOESIS_LOGWARN( ... ) CCP_LOGWARN_CH( CCP::GetNoesisChannel(), __VA_ARGS__ )

namespace Tr2Noesis
{

// True when /noesisLogVerbose was set at startup. The library keeps its own copy: a
// static cannot be shared across two modules, and neither side should have to call the
// other just to decide whether to log.
inline bool IsLogVerbose()
{
	static const bool s_verbose = []() {
		const auto arg = BeOS->GetStartupArgValue( L"noesisLogVerbose" );
		return !arg.empty() && arg != L"0";
	}();
	return s_verbose;
}

}

#endif

#endif
