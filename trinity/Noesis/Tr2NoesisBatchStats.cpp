// Copyright © 2026 CCP ehf.

#include "StdAfx.h"
#include "Noesis/Tr2NoesisBatchStats.h"

void Tr2NoesisBatchStats::Reset( const std::vector<const char*>& shaderNames )
{
	m_names = shaderNames;
	m_counts.assign( m_names.size(), 0 );
	m_reported.assign( m_names.size(), 0 );
	m_unwiredReported.assign( m_names.size(), false );
	m_logBatchDetail = true;
}

void Tr2NoesisBatchStats::CountBatch( uint8_t shader )
{
	if( shader < m_counts.size() )
	{
		++m_counts[shader];
	}
}

bool Tr2NoesisBatchStats::NoteUnwired( uint8_t shader )
{
	if( shader >= m_unwiredReported.size() || m_unwiredReported[shader] )
	{
		return false;
	}
	m_unwiredReported[shader] = true;
	return true;
}

std::string Tr2NoesisBatchStats::EndFrame( bool wantHistogram, uint32_t& outTotal )
{
	outTotal = 0;
	std::string histogram;

	if( wantHistogram && m_counts != m_reported )
	{
		for( size_t shader = 0; shader < m_counts.size(); ++shader )
		{
			outTotal += m_counts[shader];
			if( m_counts[shader] == 0 )
			{
				continue;
			}
			if( !histogram.empty() )
			{
				histogram += ", ";
			}
			histogram += m_names[shader] != nullptr ? m_names[shader] : "?";
			histogram += "=";
			histogram += std::to_string( m_counts[shader] );
		}
		m_reported = m_counts;

		// A frame that differs from the last one still has something to say when it drew
		// nothing, and an empty string is how the caller is told there is nothing to say.
		if( histogram.empty() )
		{
			histogram = "none";
		}
	}

	std::fill( m_counts.begin(), m_counts.end(), 0u );
	m_logBatchDetail = false;
	return histogram;
}

bool Tr2NoesisBatchStats::WantsBatchDetail() const
{
	return m_logBatchDetail;
}

void Tr2NoesisBatchStats::ClearBatchDetail()
{
	m_logBatchDetail = false;
}
