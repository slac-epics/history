#include <iostream>
#include <string>
#include <sstream>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#include <iocsh.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <dbFldTypes.h>
#include <aSubRecord.h>
#include <dbAddr.h>
#include <dbLink.h>
#include <dbScan.h>
#include <dbStaticLib.h>
#include <recGbl.h>
#include <alarm.h>

#include "history.h"

using namespace     std;

#ifndef		MAX_PATH
#define		MAX_PATH	100
#endif	//	MAX_PATH

///	Debug variables for history subroutine records.
/// Set non-zero to enable diag output to console
/// Larger values generate more output
epicsUInt32				DEBUG_HIST		= 0;
epicsUInt32				DEBUG_POLAR		= 0;
epicsUInt32				DEBUG_RMS		= 0;


struct	HistoryData
{
	epicsTimeStamp		m_TimeBoot;
	epicsTimeStamp		m_TimePrior;
	epicsTimeStamp		m_TimePublish;
	double			*	m_pBuffer;
	size_t				m_nValues;
};

extern "C" long History_Init(	aSubRecord	*	pSub	)
{
	if ( DEBUG_HIST >= 2 )
		cout	<< "History_Init: " << string(pSub->name) << endl;

	if ( pSub->fta != DBR_DOUBLE )
	{
		fprintf( stderr, "History_Init %s: INPA must be type DOUBLE!\n", pSub->name );
		return -1;
	}
	if ( pSub->ftp != DBR_DOUBLE )
	{
		fprintf( stderr, "History_Init %s: INPP must be type DOUBLE!\n", pSub->name );
		return -1;
	}
	if ( pSub->ftt != DBR_DOUBLE )
	{
		fprintf( stderr, "History_Init %s: INPT must be type DOUBLE!\n", pSub->name );
		return -1;
	}
	if ( pSub->ftva != DBR_DOUBLE )
	{
		fprintf( stderr, "History_Init %s: OUTA must be type DOUBLE!\n", pSub->name );
		return -1;
	}

	HistoryData		*	pHistoryData	= new HistoryData;
	epicsTimeGetCurrent( &pHistoryData->m_TimeBoot );
	pHistoryData->m_TimePrior.secPastEpoch	= 0;
	pHistoryData->m_TimePrior.nsec			= 0;
	pHistoryData->m_TimePublish.secPastEpoch= 0;
	pHistoryData->m_TimePublish.nsec		= 0;
	pHistoryData->m_nValues					= 0;

	// Allocate a history buffer
	void		*	pBuffer = callocMustSucceed( pSub->nova, sizeof(double), "HistoryInit" );
	pHistoryData->m_pBuffer = static_cast<double *>( pBuffer );

	// Initialize buffer to NAN as zero isn't appropriate for undefined values
	double		*	pOut	= pHistoryData->m_pBuffer;
	for ( epicsUInt32 i = 0; i < pSub->nova - 1; ++i )
		*pOut++	= NAN;

	// Clear number of values in output buffer
	pSub->neva	= 0;

	// Set private data
	pSub->dpvt	= pHistoryData;
	return 0;
}


///	History_Process
/// Monitors an scalar value and records it's history
///	over the specified time period.
/// The delay between samples is determined by the time
/// period divided by the size of the output array.
///	Inputs:
///		A	- a single scalar value
///		T	- a time period in seconds (zero to update on monitor)
///	Outputs:
///		A	- Array with one point for each time period
///
extern "C" long History_Process( aSubRecord	*	pSub	)
{
	// Get ptr to history data for this record
	HistoryData	*	pHistoryData	= static_cast<HistoryData *>( pSub->dpvt );

	//	Get current timestamp
	epicsTimeStamp		curTime;
	epicsTimeGetCurrent( &curTime );

	// Get input data's timestamp
	epicsTimeStamp		dataTime;
	int timeStatus	= dbGetTimeStamp( &pSub->inpa, &dataTime );
	if ( timeStatus != 0 )
	{
		// Default to current timestamp
		dataTime	= curTime;
	}

	//	Calculate our sample interval and the time delta's
	//	The aSubRecord's TSEL is $(PV).TIME, so the aSubRecord's timestamp
	//	will be updated each time from the PV.
	assert( pSub->ftt == DBR_DOUBLE );
	double		*	pTotalPeriod	= static_cast<double *>( pSub->t );
	double			sampleInterval	= *pTotalPeriod / pSub->nova;

	// Get input data's timestamp
	double			curDiff			= epicsTimeDiffInSeconds( &curTime,  &pHistoryData->m_TimePrior );
	double			pvDiff			= epicsTimeDiffInSeconds( &dataTime, &pHistoryData->m_TimePrior );

	// A zero pvDiff indicates we've already seen this sample
	bool			sameSample		= pvDiff == 0 ? true : false;

	//	Check the duration since the last data point
	// A non-zero sampleInterval indicates capture one sample per interval
	// A zero sampleInterval indicates capture each sample once
	bool			getSample		= false;
	if (	( curDiff >= sampleInterval )
		&&	( sampleInterval != 0 || !sameSample ) )
		getSample = true;

	if ( getSample )
	{
		// Get ptr to data
		assert( pSub->fta == DBR_DOUBLE );
		double		*	pData			= static_cast<double *>( pSub->a );

		// Write the data to the history buffer
		double		*	pOut	= pHistoryData->m_pBuffer;
		if ( pHistoryData->m_nValues < pSub->nova )
		{
			//	Add the new data point
			pOut[pHistoryData->m_nValues++]	= *pData;

			if ( DEBUG_HIST >= 4 )
			{
				cout	<< string(pSub->name)
						<< " now  adding: "	<<	*pData
						<< endl; 
			}
		}
		else
		{
			assert( pHistoryData->m_nValues	== pSub->nova );

			//	Shift in the new data point
			double		*	pNext	= pOut + 1;
			for ( epicsUInt32 i = 0; i < pSub->nova - 1; ++i )
			{
				*pOut++	= *pNext++;
			}
			*pOut	= *pData;

			if ( DEBUG_HIST >= 4 )
			{
				cout	<< string(pSub->name)
						<< " rotating in: "	<<	*pData
						<< endl; 
			}
		}

		// Keep prior sample time
		pHistoryData->m_TimePrior = dataTime;
	}

	// See if it's time to update the output array
	assert( pSub->ftp == DBR_DOUBLE );
	double		publishInterval	= *( static_cast<double *>( pSub->p ) );
	double		publishDiff		= epicsTimeDiffInSeconds( &curTime,  &pHistoryData->m_TimePublish );
	if ( !pSub->udf && ( publishDiff < publishInterval || !getSample ) )
	{
		// No need to update output array
		// Return 1 to suppress output processing
		recGblResetAlarms( pSub );
		return 1;
	}

	// Publish entire buffer to output array (replaces zeroes in unfilled output array w/ NAN's)
	assert( pSub->ftva == DBR_DOUBLE );
	double * pOut	= static_cast<double *>( pSub->vala );
	memcpy( pOut, pHistoryData->m_pBuffer, pSub->nova * sizeof(double) );

	// Update number of valid entries in output array
	pSub->neva	= pHistoryData->m_nValues;

	// Update publish time from latest sample time
	pHistoryData->m_TimePublish = pHistoryData->m_TimePrior;

	if ( DEBUG_HIST >= 3 )
	{
		cout	<< string(pSub->name)
				<< ", nPts = "			<<	pHistoryData->m_nValues
				<< ", pvDiff = "		<<	pvDiff
				<< ", publishDiff = "	<<	publishDiff
				<< ", publishInterval = "	<<	publishInterval
				<< endl; 
	}

	recGblResetAlarms( pSub );
	// Return 0 so aSubRecord.c will update outputs
	return 0;
}


extern "C" long Polar_Init(	aSubRecord	*	pSub	)
{
	if ( DEBUG_POLAR >= 2 )
		cout	<< "Polar_Init: " << string(pSub->name) << endl;

	return 0;
}


///	Polar_Process
/// Converts a pair of input arrays from
/// signal (I,Q) phases to polar (R,Theta) coordinates.
///	The Output arrays must be configured with the
/// same size as the input arrays.
///
/// Input Arrays (double):
///		I	= I phase of Signal 
///		Q	= Q phase of Signal 
/// Output Arrays (double):
///		R	= sqrt( I^2 + Q^2 )
///		T	= arctan( I / Q )
/// 
extern "C" long Polar_Process( aSubRecord	*	pSub	)
{
	//
	//	Get PV values
	//

	//	Get data
	assert( pSub->fti	== DBR_DOUBLE );
	assert( pSub->ftq	== DBR_DOUBLE );
	assert( pSub->ftvr	== DBR_DOUBLE );
	assert( pSub->ftvt	== DBR_DOUBLE );
	assert( pSub->noi == pSub->noq );
	assert( pSub->noi == pSub->novr );
	assert( pSub->noq == pSub->novt );
	double		*	pValueI	= static_cast<double *>( pSub->i );
	double		*	pValueQ	= static_cast<double *>( pSub->q );
	double		*	pValueR = static_cast<double *>( pSub->valr );
	double		*	pValueT = static_cast<double *>( pSub->valt );
	for ( epicsUInt32 i = 0; i < pSub->noi; ++i )
	{
		//	Get the inputs
		double	I	= *pValueI++;
		double	Q	= *pValueQ++;

		//	Compute radius
		double	R	= sqrt( I*I + Q*Q );
		//	Compute theta in degrees
		double	T	= atan2( I, Q ) * (180.0 / M_PI);

		//	Update the outputs
		*pValueR++	= static_cast<double>( R );
		*pValueT++	= static_cast<double>( T );
		if ( DEBUG_POLAR >= 4 )
		{
			cout	<< "Polar Process: I/Q ( " << I << ", " << Q
					<<	" ) => R/T ( "
					<<	R << ", " << T << ")"
					<< endl; 
		}
	}
	pSub->nevr		= pSub->noi;
	pSub->nevt		= pSub->noi;

	if ( DEBUG_POLAR >= 3 )
	{
		cout	<< "Polar Process Complete: "
				<< string(pSub->name)
				<< endl; 
	}

	// Note: Must return 0 or aSubRecord.c won't update outputs
	return 0;
}


extern "C" long RMS_Init(	aSubRecord	*	pSub	)
{
	if ( DEBUG_RMS >= 2 )
		cout	<< "RMS_Init: " << string(pSub->name) << endl;

	return 0;
}

// Computes several statistics for an input waveform
//	Slope and offset computed using least squares regression
//	Inputs:
//		A	- Waveform
//	Outputs:
//		A	- RMS
//		B	- MAX
//		C	- MIN
//		D	- MAX
//		E	- AVG
//		F	- STD
//		G	- SLOPE
//		C	- OFFSET
extern "C" long RMS_Process( aSubRecord	*	pSub	)
{
	//
	//	Get PV values
	//

	//	Get data
	assert( pSub->fta == DBR_DOUBLE );
	double			sumX	= 0.0;
	double			sumY	= 0.0;
	double			sumXX	= 0.0;
	double			sumXY	= 0.0;
	double			sumYY	= 0.0;

	double		*	pData	= static_cast<double *>( pSub->a );
	double			minVal	= NAN;
	double			maxVal	= NAN;
	double			nVal	= 0;
	for ( epicsUInt32 i = 0; i < pSub->noa; ++i )
	{
		double		dblVal	= *pData++;
		if ( isnan(dblVal) )
			continue;
		nVal++;
		minVal	= isnan(minVal) ? *pData : min( dblVal, minVal );
		maxVal	= isnan(maxVal) ? *pData : max( dblVal, maxVal );

		// Sums needed for least squares regression
		sumX	+= i;
		sumXX	+= i * i;
		sumXY	+= i * dblVal;
		sumY	+= dblVal;
		sumYY	+= dblVal * dblVal;
	}
	double			avgVal	= NAN;
	double			stdDev	= NAN;
	double			rmsVal	= NAN;
	double			slope	= NAN;
	double			offset	= NAN;
	if ( nVal != 0 )
	{
		stdDev	=	0.0;
		avgVal	=	sumY / nVal;

		//	Calc stdDev relative to avgVal
		pData	= static_cast<double *>( pSub->a );
		for ( epicsUInt32 i = 0; i < pSub->noa; ++i )
		{
			if ( isnan(*pData) )
				continue;
			double		dblVal	= *pData++;
			dblVal	-= avgVal;
			stdDev += (dblVal * dblVal);
		}
		stdDev	=	sqrt( stdDev / nVal );
		rmsVal	=	sqrt( sumYY  / nVal );

		// Calculate the least squares regression
		//	http://en.wikipedia.org/wiki/Simple_linear_regression
		slope	=	(	(nVal * sumXY - sumX * sumY)
					/	(nVal * sumXX - sumX * sumX) );
		offset	= sumY / nVal - slope * sumX / nVal;
	}

	//	Write the output
	double		*	pOut;
	if ( pSub->ftva == DBR_DOUBLE )
	{
		pSub->neva		= 1;
		pSub->nova		= 1;
		pOut			= static_cast<double *>( pSub->vala );
		*pOut			= rmsVal;
	}

	if ( pSub->ftvb == DBR_DOUBLE )
	{
		pSub->nevb		= 1;
		pSub->novb		= 1;
		pOut			= static_cast<double *>( pSub->valb );
		*pOut			= maxVal;
	}

	if ( pSub->ftvc == DBR_DOUBLE )
	{
		pSub->nevc		= 1;
		pSub->novc		= 1;
		pOut			= static_cast<double *>( pSub->valc );
		*pOut			= minVal;
	}

	if ( pSub->ftvd == DBR_DOUBLE )
	{
		pSub->nevd		= 1;
		pSub->novd		= 1;
		pOut			= static_cast<double *>( pSub->vald );
		*pOut			= avgVal;
	}

	if ( pSub->ftve == DBR_DOUBLE )
	{
		pSub->neve		= 1;
		pSub->nove		= 1;
		pOut			= static_cast<double *>( pSub->vale );
		*pOut			= stdDev;
	}

	if ( pSub->ftvf == DBR_DOUBLE )
	{
		pSub->nevf		= 1;
		pSub->novf		= 1;
		pOut			= static_cast<double *>( pSub->valf );
		*pOut			= slope;
	}

	if ( pSub->ftvg == DBR_DOUBLE )
	{
		pSub->nevg		= 1;
		pSub->novg		= 1;
		pOut			= static_cast<double *>( pSub->valg );
		*pOut			= offset;
	}

	if ( DEBUG_RMS >= 3 )
	{
		cout	<< string(pSub->name)
				<< ": rmsVal = "	<<	rmsVal
				<< ", avgVal = "	<<	avgVal
				<< ", minVal = "	<<	minVal
				<< ", maxVal = "	<<	maxVal
				<< ", stdDev = "	<<	stdDev
				<< ", slope  = "	<<	slope
				<< ", offset = "	<<	offset
				<< endl; 
	}

	// Note: Must return 0 or aSubRecord.c won't update outputs
	return 0;
}


// Register aSub functions
extern "C" {

epicsRegisterFunction( History_Init );
epicsRegisterFunction( History_Process );

epicsRegisterFunction( Polar_Init );
epicsRegisterFunction( Polar_Process );

epicsRegisterFunction( RMS_Init );
epicsRegisterFunction( RMS_Process );

}

// Register the functions
static registryFunctionRef RMS_aSub_Def[] =
{
	{	"History_Init",		(REGISTRYFUNCTION) History_Init		},
	{	"History_Process",	(REGISTRYFUNCTION) History_Process	},

	{	"Polar_Init",		(REGISTRYFUNCTION) Polar_Init		},
	{	"Polar_Process",	(REGISTRYFUNCTION) Polar_Process	},

	{	"RMS_Init",			(REGISTRYFUNCTION) RMS_Init			},
	{	"RMS_Process",		(REGISTRYFUNCTION) RMS_Process		},
};
static void	RMS_Register( void )
{
	registryFunctionRefAdd( RMS_aSub_Def, NELEMENTS( RMS_aSub_Def ) );
}
extern "C"
{
	epicsExportRegistrar( RMS_Register );
	epicsExportAddress( int, DEBUG_HIST );
	epicsExportAddress( int, DEBUG_POLAR );
	epicsExportAddress( int, DEBUG_RMS );
}

