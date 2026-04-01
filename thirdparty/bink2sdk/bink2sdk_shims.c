/*
 * Minimal shims for RAD SDK functions needed by expand2.c / binkcomm.c
 * when building with MinGW instead of MSVC.
 *
 * Provides:
 *  - rrAtomicAddExchange32 (used by binkcomm.c for slice management)
 *  - rrAtomicMemoryBarrierFull11 (referenced by rrAtomics.h)
 *  - rrmemset16, rrmemset32 (referenced by rrstring.h)
 *  - radmalloc / radfree (referenced by radmem.h)
 *  - ExpandBink2GlobalInit wrapper
 */

#include "egttypes.h"
#include "rrCore.h"

#include <stdlib.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* rrAtomics versioned names (must match rrAtomics.h version 11) */
RADDEFFUNC U32 RADLINK rrAtomicAddExchange3211(U32 volatile *ptr, U32 val)
{
    return (U32)InterlockedExchangeAdd((volatile LONG *)ptr, (LONG)val);
}

RADDEFFUNC void RADLINK rrAtomicMemoryBarrierFull11(void)
{
    MemoryBarrier();
}

RADDEFFUNC U32 rrAtomicCmpXchg3211(U32 volatile *pDestVal, U32 newVal, U32 compareVal)
{
    return (U32)InterlockedCompareExchange((volatile LONG *)pDestVal, (LONG)newVal, (LONG)compareVal);
}

RADDEFFUNC U64 rrAtomicCmpXchg6411(U64 volatile *pDestVal, U64 newVal, U64 compareVal)
{
    return (U64)InterlockedCompareExchange64((volatile LONG64 *)pDestVal, (LONG64)newVal, (LONG64)compareVal);
}

RADDEFFUNC U32 rrAtomicExchange3211(U32 volatile *ptr, U32 val)
{
    return (U32)InterlockedExchange((volatile LONG *)ptr, (LONG)val);
}

RADDEFFUNC U64 rrAtomicAddExchange6411(U64 volatile *ptr, U64 val)
{
    return (U64)InterlockedExchangeAdd64((volatile LONG64 *)ptr, (LONG64)val);
}

RADDEFFUNC U64 rrAtomicExchange6411(U64 volatile *ptr, U64 val)
{
    return (U64)InterlockedExchange64((volatile LONG64 *)ptr, (LONG64)val);
}

RADDEFFUNC U32 rrAtomicLoadAcquire3211(U32 const volatile *ptr)
{
    U32 val = *ptr;
    MemoryBarrier();
    return val;
}

RADDEFFUNC U64 rrAtomicLoadAcquire6411(U64 const volatile *ptr)
{
    U64 val = *ptr;
    MemoryBarrier();
    return val;
}

RADDEFFUNC void rrAtomicStoreRelease3211(U32 volatile *ptr, U32 val)
{
    MemoryBarrier();
    *ptr = val;
}

RADDEFFUNC void rrAtomicStoreRelease6411(U64 volatile *ptr, U64 val)
{
    MemoryBarrier();
    *ptr = val;
}

RADDEFFUNC rrbool rrAtomicCAS3211(U32 volatile *pDestVal, U32 *pOldVal, U32 newVal)
{
    U32 old = *pOldVal;
    U32 prev = (U32)InterlockedCompareExchange((volatile LONG *)pDestVal, (LONG)newVal, (LONG)old);
    if (prev == old)
        return 1;
    *pOldVal = prev;
    return 0;
}

RADDEFFUNC rrbool rrAtomicCAS6411(U64 volatile *pDestVal, U64 *pOldVal, U64 newVal)
{
    U64 old = *pOldVal;
    U64 prev = (U64)InterlockedCompareExchange64((volatile LONG64 *)pDestVal, (LONG64)newVal, (LONG64)old);
    if (prev == old)
        return 1;
    *pOldVal = prev;
    return 0;
}

/* rrmemset16 / rrmemset32 (from rrstring.h) */
RADDEFFUNC void rrmemset16(void *dest, U16 value, U32 count)
{
    U16 *d = (U16 *)dest;
    for (U32 i = 0; i < count; i++)
        d[i] = value;
}

RADDEFFUNC void rrmemset32(void *dest, U32 value, U32 count)
{
    U32 *d = (U32 *)dest;
    for (U32 i = 0; i < count; i++)
        d[i] = value;
}

/* radmalloc / radfree — simple wrappers */
void *radmalloc(UINTa bytes)
{
    return malloc((size_t)bytes);
}

void radfree(void *ptr)
{
    free(ptr);
}
