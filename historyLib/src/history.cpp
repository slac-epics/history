#include <iostream>
#include <string>
#include <sstream>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include <iocsh.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <dbFldTypes.h>
#include <aSubRecord.h>
#include <dbAddr.h>
#include <dbAccess.h>

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
	epicsTimeStamp		m_TimePrior;
	epicsTimeStamp		m_BootTime;
};

extern "C" long History_Init(	aSubRecord	*	pSub	)
{
	if ( DEBUG_HIST >= 2 )
		cout	<< "History_Init: " << string(pSub->name) << endl;

	HistoryData		*	pHistoryData	= new HistoryData;
	epicsTimeGetCurrent( &pHistoryData->m_BootTime );
	epicsTimeGetCurrent( &pHistoryData->m_TimePrior );
	pSub->dpvt							= pHistoryData;
	pSub->neva							= 0;

	double		*	pOut = static_cast<double *>( pSub->vala );
	for ( epicsUInt32 i = 0; i < pSub->nova; ++i )
		*pOut++ = 0.0;
	return 0;
}


///	History_Process
/// Monitors an scalar value and records it's history
///	over the specified time period.
/// The delay between samples is determined by the time
/// period divided by the size of the output array.
///	Inputs:
///		A	- a single scalar value
///		T	- a time period in seconds
///	Outputs:
///		A	- Array with one point for each time period
///
extern "C" long History_Process( aSubRecord	*	pSub	)
{
	//
	//	Get PV values
	//
	epicsTimeStamp		curTime;
	epicsTimeGetCurrent( &curTime );

	HistoryData	*	pHistoryData	= static_cast<HistoryData *>( pSub->dpvt );
	assert( pSub->fta == DBR_DOUBLE );
	double		*	pData			= static_cast<double *>( pSub->a );
	assert( pSub->ftva == DBR_DOUBLE );
	double		*	pOut			= static_cast<double *>( pSub->vala );

	//	Check the duration since the last data point
	assert( pSub->ftt == DBR_DOUBLE );
	double		*	pTotalPeriod	= static_cast<double *>( pSub->t );
	double			samplePeriod	= *pTotalPeriod / pSub->nova;
	double			duration		= epicsTimeDiffInSeconds( &curTime, &pHistoryData->m_TimePrior );

	if ( duration < samplePeriod )
	{
		if ( DEBUG_HIST >= 5 )
		{
			cout	<< string(pSub->name)
					<< ": Dur "			<<	duration
					<< " shorter than "	<<	samplePeriod
					<< endl; 
		}

		// Return 1 to suppress output processing
		return 1;
	}

	pHistoryData->m_TimePrior = curTime;
	if ( pSub->neva < pSub->nova )
	{
		//	Add the new data point
		pOut[pSub->neva++]	= *pData;

		if ( DEBUG_HIST >= 4 )
		{
			cout	<< string(pSub->name)
					<< " now  adding: "	<<	*pData
					<< endl; 
		}
	}
	else
	{
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

	if ( DEBUG_HIST == 3 )
	{
		cout	<< string(pSub->name)
				<< ", nPts = "		<<	pSub->neva
				<< ", duration = "	<<	duration
				<< endl; 
	}

	// Note: Must return 0 or aSubRecord.c won't update outputs
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


extern "C" long RMS_Process( aSubRecord	*	pSub	)
{
	//
	//	Get PV values
	//

	//	Get data
	assert( pSub->fta == DBR_DOUBLE );
	double			rmsVal	= 0.0;
	double			minVal	= LONG_MAX;
	double			maxVal	= LONG_MIN;
	double			avgVal	= 0;
	double			stdDev	= 0;
	double		*	pData	= static_cast<double *>( pSub->a );
	for ( epicsUInt32 i = 0; i < pSub->noa; ++i )
	{
		double		dblVal	= *pData++;
		minVal	= min( dblVal, minVal );
		maxVal	= max( dblVal, maxVal );
		rmsVal += dblVal * dblVal;
		avgVal += dblVal;
		// absVal += abs(intVal);
	}
	rmsVal	=	sqrt( rmsVal / pSub->noa );
	avgVal	/=	pSub->noa;

	//	Calc stdDev relative to avgVal
	pData	= static_cast<double *>( pSub->a );
	for ( epicsUInt32 i = 0; i < pSub->noa; ++i )
	{
		double		dblVal	= *pData++ - avgVal;
		stdDev += (dblVal * dblVal);
	}
	stdDev	=	sqrt( stdDev / pSub->noa );

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

	if ( DEBUG_RMS >= 3 )
	{
		cout	<< string(pSub->name)
				<< ": rmsVal = "	<<	rmsVal
				<< ", avgVal = "	<<	avgVal
				<< ", minVal = "	<<	minVal
				<< ", maxVal = "	<<	maxVal
				<< ", stdDev = "	<<	stdDev
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

