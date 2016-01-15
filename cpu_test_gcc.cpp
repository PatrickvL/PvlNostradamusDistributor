
/***************************************************************************

    CPU_TEST_GCC.CPP (forked from CPU_TEST.C)

    - portable C++ based test of interpreter dispatch
    - builds with Microsoft VC++ 2003/2005/2008 and 2008 x64: cl -FAsc -O2 -Tp cpu_test_gcc.cpp
    - builds with Fedora 9 PPC, Mac OS X x86/PPC, Cygwin x86: g++ -save-temps -O3 cpu_test_gcc.cpp

    Copyright (C) 1991-2008 Branch Always Sofware. All Rights Reserved.

    07/07/2007  darekm  split off from CPU_TEST and strip out existing tests
    07/22/2008  darekm  add two return value tests (set USE_OPVEC to 1)
    09/03/2008  darekm  gcc compatibility fixes, rename to CPU_TEST_GCC.CPP

***************************************************************************/


#ifndef _MSC_VER	// gcc compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>

typedef unsigned int DWORD;
typedef unsigned int ULONG;
typedef long long __int64;
typedef long long LARGE_INTEGER;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long long ULONG_PTR;

#define strnicmp strncmp

#define TRUE 1
#define FALSE 0

#define __restrict

#define __assume

#define __fastcall

#define FASTCALL	__attribute__ ((fastcall))	

// Set NOASM to 1 to compile with GCC, 0 for MSVC
#define NOASM   1

#else	// Microsoft Visual Studio compiler

#include <windows.h>
#include <stdio.h>

#define FASTCALL

// Set NOASM to 1 to compile with GCC, 0 for MSVC
#define NOASM   0

#endif

#define LOOPS 3000000
#define INSTR 32

ULONG vRefSpeed;
ULONG vfClocksOnly;
ULONG volatile vt1;

#define CHECK(expr)         extern int foo[(int)(expr) ? 1 : -(__LINE__)]
#define OFFSETOF(s,m)       (size_t)&(((s *)0)->m)


#if !defined(_WIN64) && !NOASM

// 386 compatible
// #define PAD8 __asm lea cx,[edx+edx+0x12345678]

// Pentium III compatible
#define PAD8 \
             __asm __emit 0x66 __asm __emit 0x0f __asm __emit 0x1f \
             __asm __emit 0x80 __asm __emit 0xCC __asm __emit 0xCC \
             __asm __emit 0xCC __asm __emit 0xCC

#else

#define PAD8

#endif

//
// getms() is our implementation of GetTickCount() which uses the
// hardware timer to get more accurate millisecond timing.
//

#ifndef _MSC_VER	// gcc compiler

void QueryPerformanceFrequency(LARGE_INTEGER *pl)
{
    *pl = 1000LL;
}

void QueryPerformanceCounter(LARGE_INTEGER *pl)
{
    struct timeb t;

    ftime(&t);

    *pl = (t.time * 1000LL) + t.millitm;
}

#endif

DWORD getms()
{
    static __int64 qpf;
    __int64 qpc;

    if (!qpf)
        {
        QueryPerformanceFrequency((LARGE_INTEGER *)&qpf);
        }

    QueryPerformanceCounter((LARGE_INTEGER *)&qpc);

    return (qpc / (qpf / 1000LL));
}

#if !defined (_MSC_VER) || ( _MSC_VER < 1400 )

__int64 __rdtsc()
{
#if !NOASM
    return ReadTimeStampCounter();
#else
    return getms();
#endif
}

#endif


//
// Begin Virtual CPU tests
//

//
// This method used micro-ops with operands, so calling these
// DUOPs = Data (or Descriptor) Micro-Ops
//

#define USE_OPVEC   0

// A handler takes a pointer to a DUOP and one of the operands

#if USE_OPVEC
#define RETTYPE __int64
#else
#define RETTYPE void *
#endif

typedef RETTYPE (__fastcall FASTCALL *PFNDUOP) (
    struct DUOP  * __restrict,
    DWORD ops
);


// A DUOP contains a pointer to the handler, and various operands

typedef struct DUOP
{
    PFNDUOP pfn;            // pointer to handler
    WORD    uop;            // 16-bit duop opcode
    BYTE    iduop;          // index of this duop into the block
    BYTE    delta_PC;       // start of guest instruction duop corresponds to
    DWORD   optional;       // optional 32-bit operand
    DWORD   ops;            // 32-bit packed operand(s) for this duop
} DUOP, *PDUOP;

// Thread state is global

typedef struct TSTATE
{
    DWORD   RegPC;
    DWORD   EBP;
    DWORD   EDI;
    DWORD   EAX;
    DWORD   ECX;
    DWORD   EDX;
    DWORD   EBX;
    DWORD   RegEA;
    DWORD   RegConst;
    DWORD   RegLastRes;
    void *  RegDisp;
    PDUOP   pduopNext;
} TSTATE;

TSTATE v_TS;

#define OPVEC               PFNDUOP

#define RETURN_OPVEC(x) \
            { v_TS.pduopNext = (PDUOP)(&pduop[1]); return (void *)(x); }

#define RETURN_OPVEC_2(x,y) \
            { v_TS.pduopNext = (PDUOP)(y); return (void *)(x); }


//
// DUOP handlers operate on global state and may optionally use operands.
//
// Handlers must return pointer to the next handler or NULL on error.
//

RETTYPE __fastcall duopGenEA_EBP_Disp(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopGenEA_EBP_Disp(DUOP *pduop, DWORD ops)
{
    v_TS.RegEA = v_TS.EBP + ops;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopGenEA_EDI_Disp(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopGenEA_EDI_Disp(DUOP *pduop, DWORD ops)
{
    v_TS.RegEA = v_TS.EDI + ops;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopLoad_EAX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopLoad_EAX_Mem32(DUOP *pduop, DWORD ops)
{
    v_TS.EAX = *(DWORD *)v_TS.RegEA;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopLoad_ECX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopLoad_ECX_Mem32(DUOP *pduop, DWORD ops)
{
    v_TS.ECX = *(DWORD *)v_TS.RegEA;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopStore_EAX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopStore_EAX_Mem32(DUOP *pduop, DWORD ops)
{
    *(DWORD *)v_TS.RegEA = v_TS.EAX;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopStore_ECX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopStore_ECX_Mem32(DUOP *pduop, DWORD ops)
{
    *(DWORD *)v_TS.RegEA = v_TS.ECX;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopAdd_EAX_Op(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopAdd_EAX_Op(DUOP *pduop, DWORD ops)
{
    v_TS.EAX += ops;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopAdd_ECX_Op(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopAdd_ECX_Op(DUOP *pduop, DWORD ops)
{
    v_TS.ECX += ops;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopMov_EDX_EAX(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopMov_EDX_EAX(DUOP *pduop, DWORD ops)
{
    v_TS.EDX = v_TS.EAX;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopMov_EBX_ECX(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopMov_EBX_ECX(DUOP *pduop, DWORD ops)
{
    v_TS.EBX = v_TS.ECX;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopLoad_Constant(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopLoad_Constant(DUOP *pduop, DWORD ops)
{
    v_TS.RegConst = ops;
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopCmp_EDX_Const(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopCmp_EDX_Const(DUOP *pduop, DWORD ops)
{
    v_TS.RegLastRes = (v_TS.EDX - v_TS.RegConst);
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopCmp_EBX_Const(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopCmp_EBX_Const(DUOP *pduop, DWORD ops)
{
    v_TS.RegLastRes = (v_TS.EBX - v_TS.RegConst);
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopCmp_EDX_Op(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopCmp_EDX_Op(DUOP *pduop, DWORD ops)
{
    v_TS.RegLastRes = (v_TS.EDX - ops);
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopCmp_EBX_Op(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopCmp_EBX_Op(DUOP *pduop, DWORD ops)
{
    v_TS.RegLastRes = (v_TS.EBX - ops);
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopLoad_Displac(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopLoad_Displac(DUOP *pduop, DWORD ops)
{
    extern DUOP rgduop[];

    v_TS.RegDisp = &rgduop[0];
    RETURN_OPVEC(pduop[1].pfn)
}

RETTYPE __fastcall duopFireEscape(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopFireEscape(DUOP *pduop, DWORD ops)
{
    RETURN_OPVEC_2((PFNDUOP)duopFireEscape, v_TS.pduopNext);
}

RETTYPE __fastcall duopJeq(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopJeq(DUOP *pduop, DWORD ops)
{
    v_TS.RegEA = v_TS.EBP;

    if (v_TS.RegLastRes == 0)
        {
        v_TS.pduopNext = (PDUOP)v_TS.RegDisp;
        RETURN_OPVEC_2((PFNDUOP)duopFireEscape, v_TS.pduopNext);
        }
    else
        {
        v_TS.pduopNext = &pduop[1];

        // comment next line to fall through to next uop
        RETURN_OPVEC_2((PFNDUOP)duopFireEscape, v_TS.pduopNext);
        RETURN_OPVEC(pduop[1].pfn)
        }
}

RETTYPE __fastcall duopJne(DUOP *pduop, DWORD ops) FASTCALL;

RETTYPE __fastcall duopJne(DUOP *pduop, DWORD ops)
{
    v_TS.RegEA = v_TS.EBP;

    if (v_TS.RegLastRes != 0)
        v_TS.pduopNext = (PDUOP)v_TS.RegDisp;
    else
        v_TS.pduopNext = NULL;

    RETURN_OPVEC_2((PFNDUOP)duopFireEscape, v_TS.pduopNext);
}

DUOP rgduop[] =
{
    (PFNDUOP)duopGenEA_EDI_Disp,  0, 0, 0, 0, -8,
    (PFNDUOP)duopLoad_ECX_Mem32,  0, 0, 0, 0, 0,
    (PFNDUOP)duopAdd_ECX_Op,      0, 0, 0, 0, +5,
    (PFNDUOP)duopMov_EBX_ECX,     0, 0, 0, 0, 0,
    (PFNDUOP)duopGenEA_EDI_Disp,  0, 0, 0, 0, -8,
    (PFNDUOP)duopStore_ECX_Mem32, 0, 0, 0, 0, 0,
    (PFNDUOP)duopCmp_EBX_Op,      0, 0, 0, 0, -1,
    (PFNDUOP)duopLoad_Displac,    0, 0, 0, 0, 0,
    (PFNDUOP)duopJeq,             0, 0, 0, 0, 0,

    (PFNDUOP)duopGenEA_EBP_Disp,  0, 0, 0, 0, -8,
    (PFNDUOP)duopLoad_EAX_Mem32,  0, 0, 0, 0, 0,
    (PFNDUOP)duopAdd_EAX_Op,      0, 0, 0, 0, -3,   // net result is 5-3 = +2
    (PFNDUOP)duopMov_EDX_EAX,     0, 0, 0, 0, 0,
    (PFNDUOP)duopGenEA_EBP_Disp,  0, 0, 0, 0, -8,
    (PFNDUOP)duopStore_EAX_Mem32, 0, 0, 0, 0, 0,
    (PFNDUOP)duopCmp_EDX_Op,      0, 0, 0, 0, INSTR * LOOPS,
    (PFNDUOP)duopLoad_Displac,    0, 0, 0, 0, 0,
    (PFNDUOP)duopJne,             0, 0, 0, 0, 0,
};

#if !USE_OPVEC

// The A variant loops until a fire escape, does not dispatch on return value.
// Should be prone to misprediction.

void __fastcall test_s86a_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    do
        {
        PFNDUOP pfn = pduop[0].pfn;

        do
            {
            pfn = pduop[0].pfn;
            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
            pduop++;
            } while (pfn != (PFNDUOP)duopFireEscape);

        } while (NULL != (pduop = v_TS.pduopNext));
}

// The B variant loops until a fire escape.
// Should be prone to misprediction.

void __fastcall test_s86b_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    do
        {
        PFNDUOP pfn = pduop[0].pfn;

        do
            {
            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
            pduop++;

            } while (pfn != (PFNDUOP)duopFireEscape);

        } while (NULL != (pduop = v_TS.pduopNext));
}

// The C variant is hard coded for a specific trace length.
// Can potentially have perfect branch prediction.

void __fastcall test_s86c_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    do
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

        } while (NULL != (pduop = v_TS.pduopNext));
}

// The D variant loops of a given trace length.
// More general but will suffer from branch misprediction.

void __fastcall test_s86d_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    do
        {
        PFNDUOP pfn = pduop[0].pfn;
        unsigned i;

        for (i = 0; i < count; i++)
            {
            pfn = (PFNDUOP)(*pfn)(&pduop[i], pduop[i].ops);
            }

        } while (NULL != (pduop = v_TS.pduopNext));
}

// The E variant is similar to C variant but does not use return value.
// May suffer from branch misprediction as next handler is calculated later.

void __fastcall test_s86e_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    do
        {
        PFNDUOP pfn = pduop[0].pfn;

        (*pduop[0].pfn)(&pduop[0], pduop[0].ops);
        (*pduop[1].pfn)(&pduop[1], pduop[1].ops);
        (*pduop[2].pfn)(&pduop[2], pduop[2].ops);
        (*pduop[3].pfn)(&pduop[3], pduop[3].ops);
        (*pduop[4].pfn)(&pduop[4], pduop[4].ops);
        (*pduop[5].pfn)(&pduop[5], pduop[5].ops);
        (*pduop[6].pfn)(&pduop[6], pduop[6].ops);
        (*pduop[7].pfn)(&pduop[7], pduop[7].ops);
        (*pduop[8].pfn)(&pduop[8], pduop[8].ops);

        } while (NULL != (pduop = v_TS.pduopNext));
}

// The F variant is similar to D and variants, and should have worst time.

void __fastcall test_s86f_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    do
        {
        PFNDUOP pfn = pduop[0].pfn;
        unsigned i;

        for (i = 0; i < count; i++)
            {
            pfn = (PFNDUOP)(*pduop[i].pfn)(&pduop[i], pduop[i].ops);
            }

        } while (NULL != (pduop = v_TS.pduopNext));
}

// The G variant is similar to B but hard coded for trace length of 2.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

void __fastcall test_s86g_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pduop += 2;

        if (pfn == (PFNDUOP)duopFireEscape)
            {
            pduop = v_TS.pduopNext;
            }
        }
}

// The H variant is similar to B but hard coded for trace length of 3.
// Should be prone to branch misprediction.

void __fastcall test_s86h_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pduop += 3;

        if (pfn == (PFNDUOP)duopFireEscape)
            {
            pduop = v_TS.pduopNext;
            }
        }
}

// The I variant is similar to B but hard coded for trace length of 4.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

void __fastcall test_s86i_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
        pduop += 4;

        if (pfn == (PFNDUOP)duopFireEscape)
            {
            pduop = v_TS.pduopNext;
            }
        }
}

// The J variant is similar to B but hard coded for trace length of 5.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

void __fastcall test_s86j_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
        pduop += 5;

        if (pfn == (PFNDUOP)duopFireEscape)
            {
            pduop = v_TS.pduopNext;
            }
        }
}

// The K variant is similar to B but hard coded for trace length of 6.

void __fastcall test_s86k_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);
        pduop += 6;

        if (pfn == (PFNDUOP)duopFireEscape)
            {
            pduop = v_TS.pduopNext;
            }
        }
}

// The L variant is the ideal case hard coded for the 18 DUOPs in the test.

void __fastcall test_s86l_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

        if (NULL != (pduop = v_TS.pduopNext))
            {
            PFNDUOP pfn = pduop[0].pfn;

            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);
            pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

            pduop = v_TS.pduopNext;
            }
        }
}

// The M variant is the ideal case hard coded for the 18 DUOPs in the test.
// Padded with NOPs to possibly increase branch prediction rate.

void __fastcall test_s86m_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    while (NULL != pduop)
        {
        PFNDUOP pfn = pduop[0].pfn;

        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);
        PAD8;
        pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

        if (NULL != (pduop = v_TS.pduopNext))
            {
            PFNDUOP pfn = pduop[0].pfn;

            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);
            PAD8;
            pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

            pduop = v_TS.pduopNext;
            }
        }
}

// The N variant is the general purpose case with expicit fire escape check.
// Should handle variable length blocks equally well.

void __fastcall test_s86n_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    // outer loop

    while (NULL != (pduop = v_TS.pduopNext))
    {
    // a long block that can terminate at any time

    if (NULL != (pduop = v_TS.pduopNext))
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

        if (pfn != (PFNDUOP)duopFireEscape)
         {
         pduop++;
         pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

         if (pfn != (PFNDUOP)duopFireEscape)
          {
          pduop++;
          pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

          if (pfn != (PFNDUOP)duopFireEscape)
           {
           pduop++;
           pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

           if (pfn != (PFNDUOP)duopFireEscape)
            {
            pduop++;
            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

            if (pfn != (PFNDUOP)duopFireEscape)
             {
             pduop++;
             pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

             if (pfn != (PFNDUOP)duopFireEscape)
              {
              pduop++;
              pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

              if (pfn != (PFNDUOP)duopFireEscape)
               {
               pduop++;
               pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

               if (pfn != (PFNDUOP)duopFireEscape)
                {
                pduop++;
                pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                if (pfn != (PFNDUOP)duopFireEscape)
                 {
                 pduop++;
                 pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                 if (pfn != (PFNDUOP)duopFireEscape)
                  {
                  pduop++;
                  pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                  if (pfn != (PFNDUOP)duopFireEscape)
                   {
                   pduop++;
                   pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                   if (pfn != (PFNDUOP)duopFireEscape)
                    {
                    pduop++;
                    pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                    if (pfn != (PFNDUOP)duopFireEscape)
                     {
                     pduop++;
                     pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                     if (pfn != (PFNDUOP)duopFireEscape)
                      {
                      pduop++;
                      pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                      if (pfn != (PFNDUOP)duopFireEscape)
                       {
                       pduop++;
                       pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                       if (pfn != (PFNDUOP)duopFireEscape)
                        {
                        pduop++;
                        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                        if (pfn != (PFNDUOP)duopFireEscape)
                         {
                         pduop++;
                         pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                         if (pfn != (PFNDUOP)duopFireEscape)
                          {
                          pduop++;
                          pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                          if (pfn != (PFNDUOP)duopFireEscape)
                           {
                           pduop++;
                           pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                           if (pfn != (PFNDUOP)duopFireEscape)
                            {
                            pduop++;
                            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                            if (pfn != (PFNDUOP)duopFireEscape)
                             {
                             pduop++;
                             pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
                             }
                            }
                           }
                          }
                         }
                        }
                       }
                      }
                     }
                    }
                   }
                  }
                 }
                }
               }
              }
             }
            }
           }
          }
         }
        }

    // another long block

    if (NULL != (pduop = v_TS.pduopNext))
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

        if (pfn != (PFNDUOP)duopFireEscape)
         {
         pduop++;
         pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

         if (pfn != (PFNDUOP)duopFireEscape)
          {
          pduop++;
          pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

          if (pfn != (PFNDUOP)duopFireEscape)
           {
           pduop++;
           pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

           if (pfn != (PFNDUOP)duopFireEscape)
            {
            pduop++;
            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

            if (pfn != (PFNDUOP)duopFireEscape)
             {
             pduop++;
             pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

             if (pfn != (PFNDUOP)duopFireEscape)
              {
              pduop++;
              pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

              if (pfn != (PFNDUOP)duopFireEscape)
               {
               pduop++;
               pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

               if (pfn != (PFNDUOP)duopFireEscape)
                {
                pduop++;
                pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                if (pfn != (PFNDUOP)duopFireEscape)
                 {
                 pduop++;
                 pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                 if (pfn != (PFNDUOP)duopFireEscape)
                  {
                  pduop++;
                  pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                  if (pfn != (PFNDUOP)duopFireEscape)
                   {
                   pduop++;
                   pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                   if (pfn != (PFNDUOP)duopFireEscape)
                    {
                    pduop++;
                    pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                    if (pfn != (PFNDUOP)duopFireEscape)
                     {
                     pduop++;
                     pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                     if (pfn != (PFNDUOP)duopFireEscape)
                      {
                      pduop++;
                      pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                      if (pfn != (PFNDUOP)duopFireEscape)
                       {
                       pduop++;
                       pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                       if (pfn != (PFNDUOP)duopFireEscape)
                        {
                        pduop++;
                        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                        if (pfn != (PFNDUOP)duopFireEscape)
                         {
                         pduop++;
                         pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                         if (pfn != (PFNDUOP)duopFireEscape)
                          {
                          pduop++;
                          pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                          if (pfn != (PFNDUOP)duopFireEscape)
                           {
                           pduop++;
                           pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                           if (pfn != (PFNDUOP)duopFireEscape)
                            {
                            pduop++;
                            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

                            if (pfn != (PFNDUOP)duopFireEscape)
                             {
                             pduop++;
                             pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
                             }
                            }
                           }
                          }
                         }
                        }
                       }
                      }
                     }
                    }
                   }
                  }
                 }
                }
               }
              }
             }
            }
           }
          }
         }
        }

    }   // for
}

// The O variant is the general purpose case with expicit fire escape check.
// Should handle variable length blocks equally well.

void __fastcall test_s86o_worker(DUOP *pduop, unsigned count)
{
    v_TS.pduopNext = pduop;

    // outer loop

    while (NULL != (pduop = v_TS.pduopNext))
    {
    // a long block that can terminate at any time

    if (NULL != (pduop = v_TS.pduopNext))
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

        if (pfn != (PFNDUOP)duopFireEscape)
         {
         pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);

         if (pfn != (PFNDUOP)duopFireEscape)
          {
          pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);

          if (pfn != (PFNDUOP)duopFireEscape)
           {
           pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);

           if (pfn != (PFNDUOP)duopFireEscape)
            {
            pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);

            if (pfn != (PFNDUOP)duopFireEscape)
             {
             pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);

             if (pfn != (PFNDUOP)duopFireEscape)
              {
              pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);

              if (pfn != (PFNDUOP)duopFireEscape)
               {
               pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);

               if (pfn != (PFNDUOP)duopFireEscape)
                {
                pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

                if (pfn != (PFNDUOP)duopFireEscape)
                 {
                 pfn = (PFNDUOP)(*pfn)(&pduop[9], pduop[9].ops);

                 if (pfn != (PFNDUOP)duopFireEscape)
                  {
                  pfn = (PFNDUOP)(*pfn)(&pduop[10], pduop[10].ops);

                  if (pfn != (PFNDUOP)duopFireEscape)
                   {
                   pfn = (PFNDUOP)(*pfn)(&pduop[11], pduop[11].ops);

                   if (pfn != (PFNDUOP)duopFireEscape)
                    {
                    pfn = (PFNDUOP)(*pfn)(&pduop[12], pduop[12].ops);

                    if (pfn != (PFNDUOP)duopFireEscape)
                     {
                     pfn = (PFNDUOP)(*pfn)(&pduop[13], pduop[13].ops);

                     if (pfn != (PFNDUOP)duopFireEscape)
                      {
                      pfn = (PFNDUOP)(*pfn)(&pduop[14], pduop[14].ops);

                      if (pfn != (PFNDUOP)duopFireEscape)
                       {
                       pfn = (PFNDUOP)(*pfn)(&pduop[15], pduop[15].ops);

                       if (pfn != (PFNDUOP)duopFireEscape)
                        {
                        pfn = (PFNDUOP)(*pfn)(&pduop[16], pduop[16].ops);

                        if (pfn != (PFNDUOP)duopFireEscape)
                         {
                         pfn = (PFNDUOP)(*pfn)(&pduop[17], pduop[17].ops);

                         if (pfn != (PFNDUOP)duopFireEscape)
                          {
                          pfn = (PFNDUOP)(*pfn)(&pduop[18], pduop[18].ops);

                          if (pfn != (PFNDUOP)duopFireEscape)
                           {
                           pfn = (PFNDUOP)(*pfn)(&pduop[19], pduop[19].ops);

                           if (pfn != (PFNDUOP)duopFireEscape)
                            {
                            pfn = (PFNDUOP)(*pfn)(&pduop[20], pduop[20].ops);
                            }
                           }
                          }
                         }
                        }
                       }
                      }
                     }
                    }
                   }
                  }
                 }
                }
               }
              }
             }
            }
           }
          }
         }
        }

    // another long block

    if (NULL != (pduop = v_TS.pduopNext))
        {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);

        if (pfn != (PFNDUOP)duopFireEscape)
         {
         pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);

         if (pfn != (PFNDUOP)duopFireEscape)
          {
          pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);

          if (pfn != (PFNDUOP)duopFireEscape)
           {
           pfn = (PFNDUOP)(*pfn)(&pduop[3], pduop[3].ops);

           if (pfn != (PFNDUOP)duopFireEscape)
            {
            pfn = (PFNDUOP)(*pfn)(&pduop[4], pduop[4].ops);

            if (pfn != (PFNDUOP)duopFireEscape)
             {
             pfn = (PFNDUOP)(*pfn)(&pduop[5], pduop[5].ops);

             if (pfn != (PFNDUOP)duopFireEscape)
              {
              pfn = (PFNDUOP)(*pfn)(&pduop[6], pduop[6].ops);

              if (pfn != (PFNDUOP)duopFireEscape)
               {
               pfn = (PFNDUOP)(*pfn)(&pduop[7], pduop[7].ops);

               if (pfn != (PFNDUOP)duopFireEscape)
                {
                pfn = (PFNDUOP)(*pfn)(&pduop[8], pduop[8].ops);

                if (pfn != (PFNDUOP)duopFireEscape)
                 {
                 pfn = (PFNDUOP)(*pfn)(&pduop[9], pduop[9].ops);

                 if (pfn != (PFNDUOP)duopFireEscape)
                  {
                  pfn = (PFNDUOP)(*pfn)(&pduop[10], pduop[10].ops);

                  if (pfn != (PFNDUOP)duopFireEscape)
                   {
                   pfn = (PFNDUOP)(*pfn)(&pduop[11], pduop[11].ops);

                   if (pfn != (PFNDUOP)duopFireEscape)
                    {
                    pfn = (PFNDUOP)(*pfn)(&pduop[12], pduop[12].ops);

                    if (pfn != (PFNDUOP)duopFireEscape)
                     {
                     pfn = (PFNDUOP)(*pfn)(&pduop[13], pduop[13].ops);

                     if (pfn != (PFNDUOP)duopFireEscape)
                      {
                      pfn = (PFNDUOP)(*pfn)(&pduop[14], pduop[14].ops);

                      if (pfn != (PFNDUOP)duopFireEscape)
                       {
                       pfn = (PFNDUOP)(*pfn)(&pduop[15], pduop[15].ops);

                       if (pfn != (PFNDUOP)duopFireEscape)
                        {
                        pfn = (PFNDUOP)(*pfn)(&pduop[16], pduop[16].ops);

                        if (pfn != (PFNDUOP)duopFireEscape)
                         {
                         pfn = (PFNDUOP)(*pfn)(&pduop[17], pduop[17].ops);

                         if (pfn != (PFNDUOP)duopFireEscape)
                          {
                          pfn = (PFNDUOP)(*pfn)(&pduop[18], pduop[18].ops);

                          if (pfn != (PFNDUOP)duopFireEscape)
                           {
                           pfn = (PFNDUOP)(*pfn)(&pduop[19], pduop[19].ops);

                           if (pfn != (PFNDUOP)duopFireEscape)
                            {
                            pfn = (PFNDUOP)(*pfn)(&pduop[20], pduop[20].ops);
                            }
                           }
                          }
                         }
                        }
                       }
                      }
                     }
                    }
                   }
                  }
                 }
                }
               }
              }
             }
            }
           }
          }
         }
        }

    }   // for
}

#endif


typedef void (__fastcall *PFNDUOPDISPATCH) (
    struct DUOP  * __restrict,
    unsigned count
);

void test_dispatch_duop(PFNDUOPDISPATCH ppfn)
{
    DWORD temp = 0;

    v_TS.EDX    = 0xFFFFFFFF;
    v_TS.EBP    = 8 + (ULONG_PTR)(void *)&temp;
    v_TS.EDI    = 8 + (ULONG_PTR)(void *)&temp;

   printf("", &temp);	// gcc needs this reference to work

    (*ppfn)(&rgduop[0], sizeof(rgduop) / sizeof(DUOP));

    if (v_TS.EDX != (INSTR * LOOPS))
        {
        printf("ERROR in simulate loop, wrong value = %d\n", v_TS.EDX);
        }
}

#if !USE_OPVEC

void test_s86()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86a_worker);
}

void test_s86b()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86b_worker);
}

void test_s86c()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86c_worker);
}

void test_s86d()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86d_worker);
}

void test_s86e()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86e_worker);
}

void test_s86f()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86f_worker);
}

void test_s86g()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86g_worker);
}

void test_s86h()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86h_worker);
}

void test_s86i()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86i_worker);
}

void test_s86j()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86j_worker);
}

void test_s86k()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86k_worker);
}

void test_s86l()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86l_worker);
}

void test_s86m()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86m_worker);
}

void test_s86n()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86n_worker);
}

void test_s86o()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86o_worker);
}

#endif

//
// End Virtual CPU tests
//


#if !defined(_WIN64) && !NOASM

void test_nop1()
{
    ULONG cnt = LOOPS;

    while (cnt--)
        {
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        __asm __emit 0x0F __asm __emit 0x18  __asm __emit 0x20
        }
}


void test_nop2()
{
    ULONG cnt = LOOPS;

    while (cnt--)
      __asm
        {
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x19  __asm __emit 0x38
        }
}


void test_nop3()
{
    ULONG cnt = LOOPS;

    while (cnt--)
      __asm
        {
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        __asm __emit 0x0F __asm __emit 0x1F  __asm __emit 0x38
        }
}

#endif



void BenchmarkPfn(void (*pfn)(), char *szTest, __int64 loops, __int64 instr)
{
    ULONG t1, t2;
    ULONG kips;

#if !NOASM
    __try
#endif
    {
    do
        {
        t1 = getms();
        } while (vt1 == t1);
    vt1 = t1;
    (*pfn)();
    t2 = getms();
    t1 = vt1;

    if (t2 == t1)
        t2++;

    printf("%-19s: %7d ms, %9d ps/instr, ", szTest,
        t2-t1, (ULONG)((__int64)(t2-t1) * 1000000000L / (loops * instr)));

    if ((__int64)(loops * instr) / (__int64)(t2-t1) / 1000L == 0)
        {
        printf(" 0.%03d MIPS, ", (__int64)(loops * instr) / (__int64)(t2-t1));
        }
    else
        {
        printf("%6d MIPS, ", (__int64)(loops * instr) / (__int64)(t2-t1) / 1000L);
        }

    kips = (__int64)(loops * instr) / (__int64)(t2-t1);

    if (vRefSpeed == 0)
        vRefSpeed = kips;

    if (vfClocksOnly)
        {
        // always display the ratio as clocks per instruction/iteration

        printf("%7d.%d clk", vRefSpeed/kips, ((10*vRefSpeed)/kips) % 10);
        }
    else if (kips >= vRefSpeed)
        {
        // handle the case of where the ratio is x >= 1.0

        printf("%7d.%d IPC", kips/vRefSpeed, ((10*kips)/vRefSpeed) % 10);
        }
    else if ((kips/9) >= (vRefSpeed/10))
        {
        // handle the case of where the ratio is 0.9 <= x < 1.0

        printf("      1.0 IPC");
        }
    else
        {
        // handle the case of where the ratio is x < 0.9

        printf("%7d.%d clk", vRefSpeed/kips, ((10*vRefSpeed)/kips) % 10);
        }
    }

#if !NOASM
    __except(EXCEPTION_EXECUTE_HANDLER)
      {
      printf("%s: FAILED!!", szTest);
      }
#endif

    printf("\n");
    fflush(stdout);
}

void Usage()
{
    printf("\n\n");
    printf("Usage: cpu_test [/MHz nnnn] [/clk] [/all|/none] [options[-]]\n");
    printf("\n");
    printf("/MHz speed      - specify host CPU clock speed in MHz\n");
    printf("/clk            - force CPI/IPC column to always show clocks\n");
    printf("/all            - run all tests by default\n");
    printf("/none           - run no tests by default\n");
}

int main(int argc, char **argv)
{
    int cb, i;

    printf("\n\nDispatch Tester by Darek Mihocka. Built %s\n", __DATE__);

    // default tests

    for (i = 1; i < argc; i++)
        {
        char *pch = &argv[i][1];
        int  f;

        if ( (argv[i][0] != '-') && (argv[i][0] != '/') )
            {
            printf("Invalid switch.\n");
            Usage();
            return 0;
            }

        f = argv[i][strlen(argv[i])-1] != '-';

        if (0 == strnicmp("?", pch, 1))
            {
            Usage();
            return 0;
            }

        else if (0 == strnicmp("help", pch, 4))
            {
            Usage();
            return 0;
            }

        else if (0 == strnicmp("MHz", pch, 3))
            {
            i++;

            if ( (i < argc) && argv[i] )
                {
                sscanf(argv[i], "%d", &vRefSpeed);
                }

            if (0 == vRefSpeed)
                {
                Usage();
                return 0;
                }

            vRefSpeed *= 1000;      // convert MHz to kHz
            }

        else if (0 == strnicmp("clk", pch, 3))
            vfClocksOnly = TRUE;

        else if (0 == strnicmp("all", pch, 3))
            {
            }

        else if (0 == strnicmp("none", pch, 4))
            {
            }

        else
            {
            printf("Unknown option '%s'\n", pch);

            Usage();
            return 0;
            }
        }

    setvbuf(stdout, NULL, _IONBF, 2); // don't buffer if redirecting to file

#ifndef _MSC_VER	// gcc compiler
    if (0 == vRefSpeed)
        {
        printf("must specify /MHz reference speeed!\n");
        exit(0);
        }
#endif

    printf("\nMeasuring using a clock speed of %d MHz\n", vRefSpeed / 1000);

    {
    __int64 llqpf, llqpc0, llqpc1;
    __int64 lltsc0, lltsc1;
    DWORD t0, t1;

    QueryPerformanceFrequency((LARGE_INTEGER *)&llqpf);
    QueryPerformanceCounter((LARGE_INTEGER *)&llqpc0);

    t0 = getms();
    lltsc0 = __rdtsc();

    do
        {
        QueryPerformanceCounter((LARGE_INTEGER *)&llqpc1);
        } while ((llqpc1 - llqpc0) < llqpf);

    lltsc1 = __rdtsc();
    t1 = getms();

    printf("Hardware performance frequency = %d MHz\n", llqpf / 1000000L);
    printf("On-chip cycles clock frequency = %d MHz\n",
            (lltsc1 - lltsc0) / 1000000L);
    printf("GetTickCount elapsed time (ms) = %04d ms\n", t1 - t0);
    }

#if !defined(_WIN64) && !NOASM
    BenchmarkPfn(test_nop1, "SSE nop 0F 18 /4  ", LOOPS, INSTR);
    BenchmarkPfn(test_nop2, "SSE nop 0F 19 /7  ", LOOPS, INSTR);
    BenchmarkPfn(test_nop3, "SSE nop 0F 1F /7  ", LOOPS, INSTR);
#endif

#if !USE_OPVEC
#if !defined(_WIN64) && !NOASM
    BenchmarkPfn(test_x86a, "x86 native loop   ", LOOPS, INSTR);
    BenchmarkPfn(test_s86a, "x86 simulated loop", LOOPS, INSTR);
#endif
    BenchmarkPfn(test_s86,  "x86 sim in C ver a", LOOPS, INSTR);
    BenchmarkPfn(test_s86b, "x86 sim in C ver b", LOOPS, INSTR);
    BenchmarkPfn(test_s86c, "x86 sim in C ver c", LOOPS, INSTR);
    BenchmarkPfn(test_s86d, "x86 sim in C ver d", LOOPS, INSTR);
    BenchmarkPfn(test_s86e, "x86 sim in C ver e", LOOPS, INSTR);
    BenchmarkPfn(test_s86f, "x86 sim in C ver f", LOOPS, INSTR);
    BenchmarkPfn(test_s86g, "x86 sim in C ver g", LOOPS, INSTR);
    BenchmarkPfn(test_s86h, "x86 sim in C ver h", LOOPS, INSTR);
    BenchmarkPfn(test_s86i, "x86 sim in C ver i", LOOPS, INSTR);
    BenchmarkPfn(test_s86j, "x86 sim in C ver j", LOOPS, INSTR);
    BenchmarkPfn(test_s86k, "x86 sim in C ver k", LOOPS, INSTR);
    BenchmarkPfn(test_s86l, "x86 sim in C ver l", LOOPS, INSTR);
    BenchmarkPfn(test_s86m, "x86 sim in C ver m", LOOPS, INSTR);
    BenchmarkPfn(test_s86n, "x86 sim in C ver n", LOOPS, INSTR);
    BenchmarkPfn(test_s86o, "x86 sim in C ver o", LOOPS, INSTR);
#else
    BenchmarkPfn(test_s86A, "x86 sim in C ver A", LOOPS, INSTR);
    BenchmarkPfn(test_s86B, "x86 sim in C ver B", LOOPS, INSTR);
    BenchmarkPfn(test_s86C, "x86 sim in C ver C", LOOPS, INSTR);
    BenchmarkPfn(test_s86D, "x86 sim in C ver D", LOOPS, INSTR);
    BenchmarkPfn(test_s86E, "x86 sim in C ver E", LOOPS, INSTR);
    BenchmarkPfn(test_s86F, "x86 sim in C ver F", LOOPS, INSTR);
#endif

    return 0;
}

