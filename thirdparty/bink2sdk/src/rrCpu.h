// Copyright Epic Games, Inc. All Rights Reserved.
// Licensed under the terms of a valid Unreal Engine license agreement,
//   and the separate 'Unreal Engine End User License Agreement for Publishing'.

#include "rrCore.h"

#if defined(__RADNT__) || defined(__RADMAC__) || defined (__RADLINUX__) || defined(__RADEMSCRIPTEN__)
  #define RR_MAX_CPUS 64
#elif defined (__RADIPHONE__)
  #define RR_MAX_CPUS 8
#elif defined (__RADANDROID__) || defined (__RADQNX__)
  #define RR_MAX_CPUS 16
#elif defined(RR_MAX_CPUS)
  // already set?
#else
  #error Have not thought about your rrcpus yet!
#endif

#ifndef rrGetTotalCPUs
  // returns the total number of CPUS, full speed or otherwise
  RADDEFFUNC S32 RADLINK rrGetTotalCPUs( void );
#endif

#ifndef rrGetSpinCount
  RADDEFFUNC S32 RADLINK rrGetSpinCount( void );
#endif

// these functions are for handling hyperthreaded cores
//   on windows.  it is really only necessary on XP,
//   where windows scheduled HT cores poorly.
#if defined(__RADNT__) && !defined(__RADARM__)

// returns the number of slower CPUS (hyperthreaded cores).
//   someday, when we have asymetrical cores, these would also
//   be in the slow list.
RADDEFFUNC S32 RADLINK rrGetTotalSlowerCPUs( void );

// returns the core index to use, in order of speed.  If you ask
//   for the 0th index, you will get the fastest CPU. 1st index
//   will be the second fastest, etc.  You can use these core
//   numbers to set the affinity to place the most important
//   threads on the fastest cores.  Usually this will be:
//   fast cores, and then all the hyperthreaded cores (in
//   HT alias order - so, if index 3 was the final real core,
//   then index 4 would be the first HT core that aliases
//   with index 3).  There is also logic here to keep
//   consecutive indexes together on the same core, so if
//   caching across threads is important, then use neighboring
//   indexes.
RADDEFFUNC S32 RADLINK rrGetCPUCoreByPrecedence( S32 index );

// returns the core index to use, in order of speed, starting
//   in the slow core section of the precedence array.  This
//   is useful when you have a non-CPU intensive task that you
//   can stick on a hyperthreaded core.  Eventually this list
//   wraps around to real cores (obviously when there are no
//   slow cores this happens immediately).
RADDEFFUNC S32 RADLINK rrGetSlowerCPUCoreByPrecedence( S32 index );

#else
  
  #define rrGetTotalSlowerCPUs() 0
  #define rrGetSlowerCPUCoreByPrecedence(i) (i)
  #define rrGetCPUCoreByPrecedence(i) (i)

#endif

#if defined(__RADNT__) || (defined(__RADWINRT__) && defined(__RADX86__))

// feature checks
RADDEFFUNC rrbool rrCPU_HasSSE2( void );
RADDEFFUNC rrbool rrCPU_HasSSE4( void );

typedef struct
{
   U32 sse2:1;
   U32 ssse3:1;
   U32 sse4_1:1;
   U32 sse4_2:1;
   U32 popcnt:1;
   U32 avx:1;
   U32 avx2:1;
   U32 avx512:1;
   U32 bmi1:1;
   U32 bmi2:1;
   U32 padding:16; // enough to push it over 24 bits
   U32 initialized:1;
} rrcpu_features;

RADDEFFUNC rrcpu_features rrCPU_Features( void );

#endif


/* @cdep pre

  $unless($NoAutoInclude, 
    $when($BuildingLinux$BuildingMac$BuildingiOS$BuildingAndroid,$requires( $clipfilename($file)/posix/posix_rrCpu.c ))
    $when($BuildingWin64$BuildingWin32,$requires( $clipfilename($file)/win32/win32_rrCpu.c ))
    $when($BuildingWin64ARM,$requires( $clipfilename($file)/winrt/winrt_rrCpu.cpp ))
  )
*/
  
