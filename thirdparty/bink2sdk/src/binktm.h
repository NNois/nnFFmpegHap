// Copyright Epic Games, Inc. All Rights Reserved.
// Licensed under the terms of a valid Unreal Engine license agreement,
//   and the separate 'Unreal Engine End User License Agreement for Publishing'.

#ifndef BINKTMH
#define BINKTMH

// only use TM on Windows and Linux or consoles, but not on switch
#if (!defined(__RADNT__) && !defined(__RADLINUX__) && !defined(__RADCONSOLE__) ) || defined(__RADNX__)
#ifndef NTELEMETRY
  #define NTELEMETRY
#endif
#endif

#if !defined(RADUSETM3) 
#ifndef NTELEMETRY
#define NTELEMETRY
#endif
#endif

#if !defined(NTELEMETRY)

#define g_tm_api g_radtm_api

#include "rad_tm.h"

void bink_use_telemetry3( void * api, U32 mask );

RADDEFSTART
extern tm_api * g_radtm_api;
extern U32 tm_mask;
RADDEFEND

#else

#ifdef RADUSETM3
#undef RADUSETM3
#endif

#define tmRunning() 0
#define tmStartWaitForLock( ... )
#define tmStartWaitForLockEx( ... )
#define tmEndWaitForLock( ... )
#define tmAcquiredLock( ... )
#define tmAcquiredLockEx( ... )
#define tmReleasedLockEx( ... )
#define tmEnter( ... )
#define tmLeave( ... )
#define tmLockName( ... )
#define tmThreadName( ... )
#define tmBeginTimeSpan( ... )
#define tmEndTimeSpan( ... )
#define tmTick( ... )
#define tmOpen( ... )
#define tmClose( ... )
#define tmLoadLibrary( ... )
#define tmInitialize( ... )
#define bink_use_telemetry3(...)
#define tmPlot( ... )
#define tmMessage( ... )
#define tmEmitAccumulationZone(...)
#define tmEnterAccumulationZone(...)  
#define tmLeaveAccumulationZone(...)
#define tmGetAccumulationStart( ... ) 0
#define TmU64 U64
#define tmFastTime(...) 0
#define tmEnterEx(...)
#define tmLeaveEx(...)

#endif

#endif
