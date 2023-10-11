// NostradamusInterpreter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>


/***************************************************************************

    CPU_TEST_GCC.CPP (forked from CPU_TEST.C)

    - portable C++ based test of interpreter dispatch
    - builds with Microsoft VC++ 2003/2005/2008 and 2008 x64: cl -FAsc -O2 -Tp cpu_test_gcc.cpp
    - builds with Fedora 9 PPC, Mac OS X x86/PPC, Cygwin x86: g++ -save-temps -O3 cpu_test_gcc.cpp

    Copyright (C) 1991-2008 Branch Always Sofware. All Rights Reserved.

    07/07/2007  darekm  split off from CPU_TEST and strip out existing tests
    07/22/2008  darekm  add two return value tests (set DUOP_RETURN_USE_OPVEC_TYPE to 1)
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

#if _WIN64
#define CPU64
#else
#define CPU32
#endif

//#define LOOPS 3000000
//#define INSTR 32
#define LOOPS 500000
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

void QueryPerformanceFrequency(LARGE_INTEGER* pl)
{
    *pl = 1000LL;
}

void QueryPerformanceCounter(LARGE_INTEGER* pl)
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
        QueryPerformanceFrequency((LARGE_INTEGER*)&qpf);
    }

    QueryPerformanceCounter((LARGE_INTEGER*)&qpc);

    return (DWORD)(qpc / (qpf / 1000LL));
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

// Shorthand integer types

typedef int8_t s8;
typedef uint8_t u8; // BYTE
typedef int16_t s16;
typedef uint16_t u16; // WORD
typedef int32_t s32;
typedef uint32_t u32; // DWORD
typedef int64_t s64;
typedef uint64_t u64;

//
// This method used micro-ops with operands, so calling these
// DUOPs = Data (or Descriptor) Micro-Ops
//

#define DUOP_RETURN_USE_OPVEC_TYPE   0

// A handler takes a pointer to a DUOP and one of the operands

#if DUOP_RETURN_USE_OPVEC_TYPE
#define RETTYPE __int64
#else
#define RETTYPE void *
#endif

#define DUOP_ARGUMENTS struct DUOP* __restrict pduop, u32 ops

typedef RETTYPE (__fastcall FASTCALL* PFNDUOP) (DUOP_ARGUMENTS);

// A DUOP contains a pointer to the handler, and various operands

typedef struct DUOP
{
    PFNDUOP pfn;            // pointer to handler
    u16     uop;            // 16-bit duop opcode
    u8      iduop;          // index of this duop into the block
    u8      delta_PC;       // start of guest instruction duop corresponds to
    u32     optional;       // optional 32-bit operand
    s32     ops;            // 32-bit packed operand(s) for this duop // Not unsigned but signed, so we can initialize using negative values
} DUOP, * PDUOP;

// Virtual CPU state is global

typedef struct CPUState
{
    struct Registers {
        u32   PC; // unused?
        u32   EBP;
        u32   EDI;
        u32   EAX;
        u32   ECX;
        u32   EDX;
        u32   EBX;
    } Reg;
    struct ExecutionState {
        u32   EffectiveAddress;
        u32   Const;
        u32   LastResult;
        PDUOP LastDisplacement; // was void*
        PDUOP EscapeTo;
    } State;
} CPUState;

CPUState CPU;

#ifdef CPU32
#define USE_NATIVE_MEMORY
#endif

#ifdef USE_NATIVE_MEMORY
#define MEM32(addr) (*(u32*)addr)
#else
u32* g_Mem = nullptr;

#define MEM32(addr) g_Mem[addr && ~3]
#endif

#define SET_MEM32(addr, value) MEM32(addr) = value

#define REG_EBP CPU.Reg.EBP
#define REG_EDI CPU.Reg.EDI
#define REG_EAX CPU.Reg.EAX
#define REG_ECX CPU.Reg.ECX
#define REG_EDX CPU.Reg.EDX
#define REG_EBX CPU.Reg.EBX

#define SET_REG_EBP(value) REG_EBP = value
#define SET_REG_EDI(value) REG_EDI = value
#define SET_REG_EAX(value) REG_EAX = value
#define SET_REG_ECX(value) REG_ECX = value
#define SET_REG_EDX(value) REG_EDX = value
#define SET_REG_EBX(value) REG_EBX = value

#define STATE_EFFECTIVE_ADDRESS \
    CPU.State.EffectiveAddress

#define STATE_CONST \
    CPU.State.Const

#define STATE_LAST_RES \
    CPU.State.LastResult

#define STATE_DISPLACEMENT \
    CPU.State.LastDisplacement

#define STATE_ESCAPE \
    CPU.State.EscapeTo

#define SET_STATE_EFFECTIVE_ADDRESS(value) \
    STATE_EFFECTIVE_ADDRESS = value

#define SET_STATE_CONST(value) \
    STATE_CONST = value

#define SET_STATE_LAST_RES(value) \
    STATE_LAST_RES = value

#define SET_STATE_DISPLACEMENT(value) \
    STATE_DISPLACEMENT = value

#define SET_STATE_ESCAPE(value) \
    STATE_ESCAPE = value

#define DUOP_NEXT \
    &pduop[1]


#if DUOP_RETURN_USE_OPVEC_TYPE

typedef struct OPVEC
{
    union
    {
        __int64 ll;

        struct {
            PFNDUOP pfn;
            DUOP* pduop;
        };
    };

    inline operator PFNDUOP() { return pfn; }
    inline operator __int64() { return ((ULONG_PTR)(pfn)) | ((__int64)(ULONG_PTR)(pduop) << 32); }
} OPVEC;

#define DOUP_RETURN_NEXT_OPVEC(x,y) \
    { OPVEC opvec; opvec.pfn = (x); opvec.pduop = (y); return (RETTYPE)opvec; }

#else // !DUOP_RETURN_DUOP_RETURN_USE_OPVEC_TYPE_TYPE

#define OPVEC PFNDUOP

#define DOUP_RETURN_NEXT_OPVEC(x, y) \
    { SET_STATE_ESCAPE((PDUOP)(y)); return (RETTYPE)(x); }

#endif

#define DUOP_HEADER(name) \
RETTYPE __fastcall name(DUOP_ARGUMENTS) FASTCALL; /* signature */ \
RETTYPE __fastcall name(DUOP_ARGUMENTS) /* implementation */

#define DUOP_FOOTER \
    DOUP_RETURN_NEXT_OPVEC(pduop[1].pfn, &pduop[1])

#define DUOP_FOOTER_ESCAPE \
    DOUP_RETURN_NEXT_OPVEC((PFNDUOP)duopFireEscape, STATE_ESCAPE);

// Define next line to make jumps fall through to next uop
#define JUMP_FALLTHROUGH 1

#if JUMP_FALLTHROUGH
#define DUOP_FOOTER_JUMP_WHEN(condition)        \
    if (condition)                              \
        SET_STATE_ESCAPE(STATE_DISPLACEMENT);   \
    else                                        \
    {                                           \
        SET_STATE_ESCAPE(DUOP_NEXT);            \
        DUOP_FOOTER                             \
    }                                           \
                                                \
    DUOP_FOOTER_ESCAPE
#else
#define DUOP_FOOTER_JUMP_WHEN(condition)        \
    if (condition)                              \
        SET_STATE_ESCAPE(STATE_DISPLACEMENT);   \
    else                                        \
        SET_STATE_ESCAPE(NULL);                 \
                                                \
    DUOP_FOOTER_ESCAPE
#endif

#define DUOP_PROGRAM g_Program
#define DUOP_PROGRAM_START &g_Program[0]

//
// DUOP handlers operate on global state and may optionally use operands.
//
// Handlers must return pointer to the next handler or NULL on error.
//

DUOP_HEADER(duopFireEscape)
{
    DUOP_FOOTER_ESCAPE
}

DUOP_HEADER(duopGenEA_EBP_Disp)
{
    SET_STATE_EFFECTIVE_ADDRESS(REG_EBP + ops);
    DUOP_FOOTER
}

DUOP_HEADER(duopGenEA_EDI_Disp)
{
    SET_STATE_EFFECTIVE_ADDRESS(REG_EDI + ops);
    DUOP_FOOTER
}

DUOP_HEADER(duopLoad_EAX_Mem32)
{
    SET_REG_EAX(MEM32(STATE_EFFECTIVE_ADDRESS));
    DUOP_FOOTER
}

DUOP_HEADER(duopLoad_ECX_Mem32)
{
    SET_REG_ECX(MEM32(STATE_EFFECTIVE_ADDRESS));
    DUOP_FOOTER
}

DUOP_HEADER(duopStore_EAX_Mem32)
{
    SET_MEM32(STATE_EFFECTIVE_ADDRESS, REG_EAX);
    DUOP_FOOTER
}

DUOP_HEADER(duopStore_ECX_Mem32)
{
    SET_MEM32(STATE_EFFECTIVE_ADDRESS, REG_ECX);
    DUOP_FOOTER
}

DUOP_HEADER(duopAdd_EAX_Op)
{
    REG_EAX += ops;
    DUOP_FOOTER
}

DUOP_HEADER(duopAdd_ECX_Op)
{
    REG_ECX += ops;
    DUOP_FOOTER
}

DUOP_HEADER(duopMov_EDX_EAX)
{
    SET_REG_EDX(REG_EAX);
    DUOP_FOOTER
}

DUOP_HEADER(duopMov_EBX_ECX)
{
    SET_REG_EBX(REG_ECX);
    DUOP_FOOTER
}

DUOP_HEADER(duopLoad_Constant)
{
    SET_STATE_CONST(ops);
    DUOP_FOOTER
}

DUOP_HEADER(duopCmp_EDX_Const)
{
    SET_STATE_LAST_RES(REG_EDX - STATE_CONST);
    DUOP_FOOTER
}

DUOP_HEADER(duopCmp_EBX_Const)
{
    SET_STATE_LAST_RES(REG_EBX - STATE_CONST);
    DUOP_FOOTER
}

DUOP_HEADER(duopCmp_EDX_Op)
{
    SET_STATE_LAST_RES(REG_EDX - ops);
    DUOP_FOOTER
}

DUOP_HEADER(duopCmp_EBX_Op)
{
    SET_STATE_LAST_RES(REG_EBX - ops);
    DUOP_FOOTER
}

DUOP_HEADER(duopLoad_Displac)
{
    extern DUOP DUOP_PROGRAM[];

    SET_STATE_DISPLACEMENT(DUOP_PROGRAM_START);
    DUOP_FOOTER
}

DUOP_HEADER(duopJeq)
{
    SET_STATE_EFFECTIVE_ADDRESS(REG_EBP);
    DUOP_FOOTER_JUMP_WHEN(STATE_LAST_RES == 0);
}

DUOP_HEADER(duopJne)
{
    SET_STATE_EFFECTIVE_ADDRESS(REG_EBP);
    DUOP_FOOTER_JUMP_WHEN(STATE_LAST_RES != 0);
}

#define CNT INSTR * LOOPS
DUOP g_Program[] =
{
    (PFNDUOP)duopGenEA_EDI_Disp,  0, 0, 0, 0, -8, // EffectiveAddress = EDI + -8
    (PFNDUOP)duopLoad_ECX_Mem32,  0, 0, 0, 0, 0,  // ECX = MEM32(EffectiveAddress)
    (PFNDUOP)duopAdd_ECX_Op,      0, 0, 0, 0, +5, // ECX += +5
    (PFNDUOP)duopMov_EBX_ECX,     0, 0, 0, 0, 0,  // EBX = ECX
    (PFNDUOP)duopGenEA_EDI_Disp,  0, 0, 0, 0, -8, // EffectiveAddress = EDI + -8
    (PFNDUOP)duopStore_ECX_Mem32, 0, 0, 0, 0, 0,  // MEM32(EffectiveAddress) = ECX
    (PFNDUOP)duopCmp_EBX_Op,      0, 0, 0, 0, -1, // LastResult = (EBX - -1)
    (PFNDUOP)duopLoad_Displac,    0, 0, 0, 0, 0,  // LastDisplacement = DUOP_PROGRAM_START
    (PFNDUOP)duopJeq,             0, 0, 0, 0, 0,  // pduopNext = LastResult == 0 ? State.LastDisplacement : &pduop[1]

    (PFNDUOP)duopGenEA_EBP_Disp,  0, 0, 0, 0, -8, // EffectiveAddress = EBP + -8
    (PFNDUOP)duopLoad_EAX_Mem32,  0, 0, 0, 0, 0,  // EAX = MEM32(EffectiveAddress)
    (PFNDUOP)duopAdd_EAX_Op,      0, 0, 0, 0, -3, // EAX += -3 // net result is 5-3 = +2
    (PFNDUOP)duopMov_EDX_EAX,     0, 0, 0, 0, 0,  // EDX = EAX
    (PFNDUOP)duopGenEA_EBP_Disp,  0, 0, 0, 0, -8, // EffectiveAddress = EBP + -8
    (PFNDUOP)duopStore_EAX_Mem32, 0, 0, 0, 0, 0,  // MEM32(EffectiveAddress) = EAX
    (PFNDUOP)duopCmp_EDX_Op,      0, 0, 0, 0, CNT,// LastResult = (EDX - CNT)
    (PFNDUOP)duopLoad_Displac,    0, 0, 0, 0, 0,  // LastDisplacement = DUOP_PROGRAM_START
    (PFNDUOP)duopJne,             0, 0, 0, 0, 0,  // EffectiveAddress = EBP; pduopNext = State.LastResult != 0 ? State.LastDisplacement : NULL;
};


#define TEST_INITIALIZE(pduop) SET_STATE_ESCAPE(pduop)

#if !DUOP_RETURN_USE_OPVEC_TYPE

// The A variant loops until a fire escape, does not dispatch on return value.
// Should be prone to misprediction.

void __fastcall test_s86a_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    do
    {
        PFNDUOP pfn = pduop[0].pfn;

        do
        {
            pfn = pduop[0].pfn;
            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
            pduop++;
        } while (pfn != (PFNDUOP)duopFireEscape);

    } while (NULL != (pduop = STATE_ESCAPE));
}

// The B variant loops until a fire escape.
// Should be prone to misprediction.

void __fastcall test_s86b_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    do
    {
        PFNDUOP pfn = pduop[0].pfn;

        do
        {
            pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
            pduop++;

        } while (pfn != (PFNDUOP)duopFireEscape);

    } while (NULL != (pduop = STATE_ESCAPE));
}

// The C variant is hard coded for a specific trace length.
// Can potentially have perfect branch prediction.

void __fastcall test_s86c_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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

    } while (NULL != (pduop = STATE_ESCAPE));
}

// The D variant loops of a given trace length.
// More general but will suffer from branch misprediction.

void __fastcall test_s86d_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    do
    {
        PFNDUOP pfn = pduop[0].pfn;
        unsigned i;

        for (i = 0; i < count; i++)
        {
            pfn = (PFNDUOP)(*pfn)(&pduop[i], pduop[i].ops);
        }

    } while (NULL != (pduop = STATE_ESCAPE));
}

// The E variant is similar to C variant but does not use return value.
// May suffer from branch misprediction as next handler is calculated later.

void __fastcall test_s86e_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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

    } while (NULL != (pduop = STATE_ESCAPE));
}

// The F variant is similar to D and variants, and should have worst time.

void __fastcall test_s86f_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    do
    {
        PFNDUOP pfn = pduop[0].pfn;
        unsigned i;

        for (i = 0; i < count; i++)
        {
            pfn = (PFNDUOP)(*pduop[i].pfn)(&pduop[i], pduop[i].ops);
        }

    } while (NULL != (pduop = STATE_ESCAPE));
}

// The G variant is similar to B but hard coded for trace length of 2.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

void __fastcall test_s86g_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    while (NULL != pduop)
    {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pduop += 2;

        if (pfn == (PFNDUOP)duopFireEscape)
        {
            pduop = STATE_ESCAPE;
        }
    }
}

// The H variant is similar to B but hard coded for trace length of 3.
// Should be prone to branch misprediction.

void __fastcall test_s86h_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    while (NULL != pduop)
    {
        PFNDUOP pfn = pduop[0].pfn;

        pfn = (PFNDUOP)(*pfn)(&pduop[0], pduop[0].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);
        pfn = (PFNDUOP)(*pfn)(&pduop[2], pduop[2].ops);
        pduop += 3;

        if (pfn == (PFNDUOP)duopFireEscape)
        {
            pduop = STATE_ESCAPE;
        }
    }
}

// The I variant is similar to B but hard coded for trace length of 4.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

void __fastcall test_s86i_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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
            pduop = STATE_ESCAPE;
        }
    }
}

// The J variant is similar to B but hard coded for trace length of 5.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

void __fastcall test_s86j_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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
            pduop = STATE_ESCAPE;
        }
    }
}

// The K variant is similar to B but hard coded for trace length of 6.

void __fastcall test_s86k_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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
            pduop = STATE_ESCAPE;
        }
    }
}

// The L variant is the ideal case hard coded for the 18 DUOPs in the test.

void __fastcall test_s86l_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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

        if (NULL != (pduop = STATE_ESCAPE))
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

            pduop = STATE_ESCAPE;
        }
    }
}

// The M variant is the ideal case hard coded for the 18 DUOPs in the test.
// Padded with NOPs to possibly increase branch prediction rate.

void __fastcall test_s86m_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

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

        if (NULL != (pduop = STATE_ESCAPE))
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

            pduop = STATE_ESCAPE;
        }
    }
}

// The N variant is the general purpose case with expicit fire escape check.
// Should handle variable length blocks equally well.

void __fastcall test_s86n_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    // outer loop

    while (NULL != (pduop = STATE_ESCAPE))
    {
        // a long block that can terminate at any time

        if (NULL != (pduop = STATE_ESCAPE))
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

        if (NULL != (pduop = STATE_ESCAPE))
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

void __fastcall test_s86o_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    // outer loop

    while (NULL != (pduop = STATE_ESCAPE))
    {
        // a long block that can terminate at any time

        if (NULL != (pduop = STATE_ESCAPE))
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

        if (NULL != (pduop = STATE_ESCAPE))
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

#if DUOP_RETURN_USE_OPVEC_TYPE

// The A variant uses the descriptor pointer returned by the helper

void __fastcall test_s86A_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);

    OPVEC opvec;
    __int64 ll;

    opvec.pduop = pduop;

    while (NULL != opvec.pduop)
    {
        opvec.pfn = opvec.pduop->pfn;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;

        if (opvec.pfn == (PFNDUOP)duopFireEscape)
        {
            opvec.pduop = CPU.pduopNext;
        }
    }
}

// The B variant uses the descriptor pointer returned by the helper

void __fastcall test_s86B_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);
    OPVEC opvec;
    __int64 ll;

    opvec.pduop = pduop;
    opvec.pfn = opvec.pduop->pfn;

    while (NULL != opvec.pduop)
    {
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;

        if (opvec.pfn == (PFNDUOP)duopFireEscape)
        {
            opvec.pduop = CPU.pduopNext;

            if (opvec.pduop != NULL)
                opvec.pfn = opvec.pduop->pfn;
        }
    }
}

// The C variant uses the descriptor pointer returned by the helper

void __fastcall test_s86C_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);
    OPVEC opvec;
    __int64 ll;

    opvec.pduop = pduop;

    while (NULL != opvec.pduop)
    {
        opvec.pfn = opvec.pduop->pfn;

        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;

        if (opvec.pfn == (PFNDUOP)duopFireEscape)
        {
            opvec.pduop = CPU.pduopNext;
        }
    }
}

// The D variant uses the descriptor pointer returned by the helper

void __fastcall test_s86D_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);
    OPVEC opvec;
    __int64 ll;

    opvec.pduop = pduop;

    while (NULL != opvec.pduop)
    {
        opvec.pfn = opvec.pduop->pfn;

        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;

        if (opvec.pfn == (PFNDUOP)duopFireEscape)
        {
            opvec.pduop = CPU.pduopNext;
        }
    }
}

// The E variant uses the descriptor pointer returned by the helper

void __fastcall test_s86E_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);
    OPVEC opvec;
    __int64 ll;

    opvec.pduop = pduop;

    while (NULL != opvec.pduop)
    {
        opvec.pfn = opvec.pduop->pfn;

        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;

        if (opvec.pfn == (PFNDUOP)duopFireEscape)
        {
            opvec.pduop = CPU.pduopNext;
        }
    }
}

// The F variant uses the descriptor pointer returned by the helper

void __fastcall test_s86F_worker(DUOP* pduop, unsigned count)
{
    TEST_INITIALIZE(pduop);
    OPVEC opvec;
    __int64 ll;

    opvec.pduop = pduop;

    while (NULL != opvec.pduop)
    {
        opvec.pfn = opvec.pduop->pfn;

        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;
        ll = (*opvec.pfn)(opvec.pduop, opvec.pduop[0].ops);
        opvec.ll = ll;

        if (opvec.pfn == (PFNDUOP)duopFireEscape)
        {
            opvec.pduop = CPU.pduopNext;
        }
    }
}

#endif

typedef void(__fastcall* PFNDUOPDISPATCH) (
    struct DUOP* __restrict,
    unsigned count
    );

void test_dispatch_duop(PFNDUOPDISPATCH ppfn)
{
    u32 addr;
#ifdef USE_NATIVE_MEMORY
    u32 temp = 0;

    addr = (ULONG_PTR)(void*)&temp;
    #ifndef _MSC_VER	// gcc compiler
    printf("", &temp);	// gcc needs this reference to work
    #endif
#else

    addr = 0;
#endif
    MEM32(addr) = 0;
    REG_EBP = 8 + addr;
    REG_EDI = 8 + addr;
    REG_EDX = 0xFFFFFFFF;

    (*ppfn)(DUOP_PROGRAM_START, sizeof(DUOP_PROGRAM) / sizeof(DUOP));

    if (REG_EDX != (INSTR * LOOPS))
    {
        printf("ERROR in simulate loop, wrong value = %d\n", REG_EDX);
    }
}


#if !defined(_WIN64) && !NOASM

void test_x86a()
{
    // TODO ? test_dispatch_duop((PFNDUOPDISPATCH)test_s86a_worker);
}

void test_s86a()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86a_worker);
}

#endif

#if !DUOP_RETURN_USE_OPVEC_TYPE

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

#if DUOP_RETURN_USE_OPVEC_TYPE

void test_s86A()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86A_worker);
}

void test_s86B()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86B_worker);
}

void test_s86C()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86C_worker);
}
void test_s86D()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86D_worker);
}
void test_s86E()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86E_worker);
}
void test_s86F()
{
    test_dispatch_duop((PFNDUOPDISPATCH)test_s86F_worker);
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



void BenchmarkPfn(void (*pfn)(), const char* szTest, __int64 loops, __int64 instr)
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
            t2 - t1, (ULONG)((__int64)(t2 - t1) * 1000000000L / (loops * instr)));

        if ((__int64)(loops * instr) / (__int64)(t2 - t1) / 1000L == 0)
        {
            printf(" 0.%03lld MIPS, ", (__int64)(loops * instr) / (__int64)(t2 - t1));
        }
        else
        {
            printf("%6lld MIPS, ", (__int64)(loops * instr) / (__int64)(t2 - t1) / 1000L);
        }

        kips = (ULONG)((__int64)(loops * instr) / (__int64)(t2 - t1));

        if (vRefSpeed == 0)
            vRefSpeed = kips;

        if (vfClocksOnly)
        {
            // always display the ratio as clocks per instruction/iteration

            printf("%7d.%d clk", vRefSpeed / kips, ((10 * vRefSpeed) / kips) % 10);
        }
        else if (kips >= vRefSpeed)
        {
            // handle the case of where the ratio is x >= 1.0

            printf("%7d.%d IPC", kips / vRefSpeed, ((10 * kips) / vRefSpeed) % 10);
        }
        else if ((kips / 9) >= (vRefSpeed / 10))
        {
            // handle the case of where the ratio is 0.9 <= x < 1.0

            printf("      1.0 IPC");
        }
        else
        {
            // handle the case of where the ratio is x < 0.9

            printf("%7d.%d clk", vRefSpeed / kips, ((10 * vRefSpeed) / kips) % 10);
        }
    }

#if !NOASM
    __except (EXCEPTION_EXECUTE_HANDLER)
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

int main(int argc, char** argv)
{
    int i; // not cb,

    printf("\n\nDispatch Tester by Darek Mihocka. Built %s\n", __DATE__);

    // default tests

    for (i = 1; i < argc; i++)
    {
        char* pch = &argv[i][1];
        int  f;

        if ((argv[i][0] != '-') && (argv[i][0] != '/'))
        {
            printf("Invalid switch.\n");
            Usage();
            return 0;
        }

        f = argv[i][strlen(argv[i]) - 1] != '-';

        if (0 == _strnicmp("?", pch, 1))
        {
            Usage();
            return 0;
        }

        else if (0 == _strnicmp("help", pch, 4))
        {
            Usage();
            return 0;
        }

        else if (0 == _strnicmp("MHz", pch, 3))
        {
            i++;

            if ((i < argc) && argv[i])
            {
                sscanf_s(argv[i], "%d", &vRefSpeed);
            }

            if (0 == vRefSpeed)
            {
                Usage();
                return 0;
            }

            vRefSpeed *= 1000;      // convert MHz to kHz
        }

        else if (0 == _strnicmp("clk", pch, 3))
            vfClocksOnly = TRUE;

        else if (0 == _strnicmp("all", pch, 3))
        {
        }

        else if (0 == _strnicmp("none", pch, 4))
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
        u32 t0, t1;

        QueryPerformanceFrequency((LARGE_INTEGER*)&llqpf);
        QueryPerformanceCounter((LARGE_INTEGER*)&llqpc0);

        t0 = getms();
        lltsc0 = __rdtsc();

        do
        {
            QueryPerformanceCounter((LARGE_INTEGER*)&llqpc1);
        } while ((llqpc1 - llqpc0) < llqpf);

        lltsc1 = __rdtsc();
        t1 = getms();

        printf("Hardware performance frequency = %lld MHz\n", llqpf / 1000000L);
        printf("On-chip cycles clock frequency = %lld MHz\n",
            (lltsc1 - lltsc0) / 1000000L);
        printf("GetTickCount elapsed time (ms) = %04d ms\n", t1 - t0);
    }

#ifndef USE_NATIVE_MEMORY
    g_Mem = (u32*)malloc(64 * 1024);
#endif

#if !defined(_WIN64) && !NOASM
    BenchmarkPfn(test_nop1, "SSE nop 0F 18 /4  ", LOOPS, INSTR);
    BenchmarkPfn(test_nop2, "SSE nop 0F 19 /7  ", LOOPS, INSTR);
    BenchmarkPfn(test_nop3, "SSE nop 0F 1F /7  ", LOOPS, INSTR);
#endif

#if !DUOP_RETURN_USE_OPVEC_TYPE
#if !defined(_WIN64) && !NOASM
    BenchmarkPfn(test_x86a, "x86 native loop   ", LOOPS, INSTR);
    BenchmarkPfn(test_s86a, "x86 simulated loop", LOOPS, INSTR);
#endif
    BenchmarkPfn(test_s86, "x86 sim in C ver a", LOOPS, INSTR);
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

#ifndef USE_NATIVE_MEMORY
    free(g_Mem);
#endif
    return 0;
}

/* Output on ThinkPad laptop (accu)

Dispatch Tester by Darek Mihocka. Built Sep 24 2023

Measuring using a clock speed of 0 MHz
Hardware performance frequency = 10 MHz
On-chip cycles clock frequency = 2419 MHz
GetTickCount elapsed time (ms) = 1000 ms
SSE nop 0F 18 /4   :     136 ms,      1416 ps/instr,    705 MIPS,       1.0 IPC
SSE nop 0F 19 /7   :       8 ms,        83 ps/instr,  12000 MIPS,      17.0 IPC
SSE nop 0F 1F /7   :      10 ms,       104 ps/instr,   9600 MIPS,      13.6 IPC
x86 native loop    :       1 ms,        10 ps/instr,  96000 MIPS,     136.0 IPC
x86 simulated loop :   19651 ms,    204697 ps/instr,      4 MIPS,     144.4 clk
x86 sim in C ver a :   18007 ms,    187572 ps/instr,      5 MIPS,     132.4 clk
x86 sim in C ver b :   19786 ms,    206104 ps/instr,      4 MIPS,     145.5 clk
x86 sim in C ver c :   19292 ms,    200958 ps/instr,      4 MIPS,     141.8 clk
x86 sim in C ver d :   35957 ms,    374552 ps/instr,      2 MIPS,     264.4 clk
x86 sim in C ver e :   17690 ms,    184270 ps/instr,      5 MIPS,     130.0 clk
x86 sim in C ver f :   20245 ms,    210885 ps/instr,      4 MIPS,     148.8 clk
x86 sim in C ver g :   19830 ms,    206562 ps/instr,      4 MIPS,     145.8 clk
x86 sim in C ver h :   17256 ms,    179750 ps/instr,      5 MIPS,     126.8 clk
x86 sim in C ver i :   25636 ms,    267041 ps/instr,      3 MIPS,     188.5 clk
x86 sim in C ver j :   24707 ms,    257364 ps/instr,      3 MIPS,     181.6 clk
x86 sim in C ver k :   27845 ms,    290052 ps/instr,      3 MIPS,     204.7 clk
x86 sim in C ver l :   20544 ms,    214000 ps/instr,      4 MIPS,     151.0 clk
x86 sim in C ver m :   22481 ms,    234177 ps/instr,      4 MIPS,     165.3 clk
x86 sim in C ver n :   16918 ms,    176229 ps/instr,      5 MIPS,     124.4 clk
x86 sim in C ver o :   16917 ms,    176218 ps/instr,      5 MIPS,     124.4 clk
*/