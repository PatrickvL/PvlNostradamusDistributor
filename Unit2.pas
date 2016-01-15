unit Unit2;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs;

type
  TForm2 = class(TForm)
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  Form2: TForm2;

implementation

{$R *.dfm}

{$OPTIMIZATION ON}
{$RANGECHECKS OFF}
{$OVERFLOWCHECKS OFF}
{$POINTERMATH ON}

type
  UIntPtr = DWord;

const LOOPS = 3000000;
const INSTR = 32;
const CINSTR = INSTR * LOOPS;
 
var vRefSpeed: ULONG = 0;
var vfClocksOnly: Boolean = False;
var vt1: ULONG = 0; // volatile

//#define CHECK(expr)         extern int foo[(int)(expr) ? 1 : -(__LINE__)]
//#define OFFSETOF(s,m)       (size_t)&(((s *)0)->m)

  qpf: Int64 = 0;

type
  TCode = Pointer;

  PDUOP_ = ^DUOP;
  PCPUSTATE = ^TSTATE;

  PFNDUOP_ = TCode; // Nodig omdat Delphi PFNDUOP variabelen interpreteert als calls

  // A handler takes a pointer to a DUOP and one of the operands
  RETTYPE = Pointer;

//  PFNDUOP = function (const pduop: PDUOP_; const ops: DWORD): PFNDUOP_; register;
  PFNDUOP = function (
    const v_TS: PCPUSTATE;
    const pduop: PDUOP_
  ): PFNDUOP_; register;


  // A DUOP contains a pointer to the handler, and various operands

  DUOP = record

    pfn: PFNDUOP_;   // pointer to handler
    uop: WORD;       // 16-bit duop opcode
    iduop: BYTE;     // index of this duop into the block
    delta_PC: BYTE;  // start of guest instruction duop corresponds to
    optional: DWORD; // optional 32-bit operand
    ops: DWORD;      // 32-bit packed operand(s) for this duop
  end;

  // Thread state
  TSTATE = record

    RegPC: DWORD;      // Program Counter
    EBP: DWORD;
    EDI: DWORD;
    EAX: DWORD;
    ECX: DWORD;
    EDX: DWORD;
    EBX: DWORD;
    RegEA: DWORD;      // Effective Address
    RegConst: DWORD;
    RegLastRes: DWORD; // Compare result
    RegDisp: Pointer;
    pduopNext: PDUOP_;
  end;

var
  v_TS: TSTATE; // global

  _rgduop_0: PDUOP_ = nil;

function PAD8(const C: UIntPtr = 0; const D: UIntPtr = 0): Pointer; inline;
begin
  Result := Pointer(C + D + $12345678);
end;

function RETURN_OPVEC(const v_TS: PCPUSTATE; const x: PDUOP_): PFNDUOP; inline;
begin
  v_TS.pduopNext := x;
  Result := x.pfn;
end;

function RETURN_OPVEC_2(const v_TS: PCPUSTATE; const x: PFNDUOP; const y: PDUOP_): PFNDUOP; inline;
begin
  v_TS.pduopNext := y;
  Result := x;
end;

//
// DUOP handlers operate on global state and may optionally use operands.
//
// Handlers must return pointer to the next handler or nil on error.
//

// RETTYPE __fastcall duopGenEA_EBP_Disp(DUOP *pduop, DWORD ops) FASTCALL;
//
function duopGenEA_EBP_Disp(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EBP + pduop.ops;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopGenEA_EDI_Disp(DUOP *pduop, DWORD ops) FASTCALL;

function duopGenEA_EDI_Disp(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EDI + pduop.ops;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopLoad_EAX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

function duopLoad_EAX_Mem32(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.EAX := PDWORD(v_TS.RegEA)^;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopLoad_ECX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

function duopLoad_ECX_Mem32(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.ECX := PDWORD(v_TS.RegEA)^;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopStore_EAX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

function duopStore_EAX_Mem32(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  PDWORD(v_TS.RegEA)^ := v_TS.EAX;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopStore_ECX_Mem32(DUOP *pduop, DWORD ops) FASTCALL;

function duopStore_ECX_Mem32(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  PDWORD(v_TS.RegEA)^ := v_TS.ECX;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopAdd_EAX_Op(DUOP *pduop, DWORD ops) FASTCALL;

function duopAdd_EAX_Op(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  Inc(v_TS.EAX, pduop.ops);
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopAdd_ECX_Op(DUOP *pduop, DWORD ops) FASTCALL;

function duopAdd_ECX_Op(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  Inc(v_TS.ECX, pduop.ops);
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopMov_EDX_EAX(DUOP *pduop, DWORD ops) FASTCALL;

function duopMov_EDX_EAX(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.EDX := v_TS.EAX;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopMov_EBX_ECX(DUOP *pduop, DWORD ops) FASTCALL;

function duopMov_EBX_ECX(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.EBX := v_TS.ECX;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopLoad_Constant(DUOP *pduop, DWORD ops) FASTCALL;

function duopLoad_Constant(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegConst := pduop.ops;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopCmp_EDX_Const(DUOP *pduop, DWORD ops) FASTCALL;

function duopCmp_EDX_Const(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EDX - v_TS.RegConst);
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopCmp_EBX_Const(DUOP *pduop, DWORD ops) FASTCALL;

function duopCmp_EBX_Const(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EBX - v_TS.RegConst);
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopCmp_EDX_Op(DUOP *pduop, DWORD ops) FASTCALL;

function duopCmp_EDX_Op(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EDX - pduop.ops);
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopCmp_EBX_Op(DUOP *pduop, DWORD ops) FASTCALL;

function duopCmp_EBX_Op(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EBX - pduop.ops);
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopLoad_Displac(DUOP *pduop, DWORD ops) FASTCALL;

function duopLoad_Displac(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegDisp := _rgduop_0;
  Result := RETURN_OPVEC(v_TS, @pduop[1]);
end;

// RETTYPE __fastcall duopFireEscape(DUOP *pduop, DWORD ops) FASTCALL;

function duopFireEscape(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  Result := @duopFireEscape;
end;

// RETTYPE __fastcall duopJeq(DUOP *pduop, DWORD ops) FASTCALL;

function duopJeq(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EBP;

  if (v_TS.RegLastRes = 0) then
  begin
    Result := RETURN_OPVEC_2(v_TS, @duopFireEscape, PDUOP_(v_TS.RegDisp));
  end
  else
  begin
    // comment next line to fall through to next uop
    Result := RETURN_OPVEC_2(v_TS, @duopFireEscape, @pduop[1]);
    // Result := RETURN_OPVEC(v_TS, @pduop[1]);
  end;
end;

// RETTYPE __fastcall duopJne(DUOP *pduop, DWORD ops) FASTCALL;

function duopJne(const v_TS: PCPUSTATE; const pduop: PDUOP_): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EBP;

  if (v_TS.RegLastRes <> 0) then
    Result := RETURN_OPVEC_2(v_TS, @duopFireEscape, PDUOP_(v_TS.RegDisp))
  else
    Result := RETURN_OPVEC_2(v_TS, @duopFireEscape, nil); // ??
end;


const
  rgduop: array [0..17] of DUOP =
  (
    (pfn:@duopGenEA_EDI_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:DWORD(-8)),
    //         [edi-8]
    (pfn:@duopLoad_ECX_Mem32;  uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // mov ecx,
    (pfn:@duopAdd_ECX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:+5),
    // add ecx, +5
    (pfn:@duopMov_EBX_ECX;     uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // mov ebx, ecx
    (pfn:@duopGenEA_EDI_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:DWORD(-8)),
    //     [edi-8]
    (pfn:@duopStore_ECX_Mem32; uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // mov        , ecx
    (pfn:@duopCmp_EBX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:DWORD(-1)),
    // cmp ebx, -1
    (pfn:@duopLoad_Displac;    uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    //    @loop
    (pfn:@duopJeq;             uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // je

    (pfn:@duopGenEA_EBP_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:DWORD(-8)),
    //          [edp-8]
    (pfn:@duopLoad_EAX_Mem32;  uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // mov eax, 
    (pfn:@duopAdd_EAX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:DWORD(-3)),   // net result is 5-3 = +2
    // add eax, -3
    (pfn:@duopMov_EDX_EAX;     uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // mov edx, eax
    (pfn:@duopGenEA_EBP_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:DWORD(-8)),
    //     [ebp-8]
    (pfn:@duopStore_EAX_Mem32; uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    // mov        , eax
    (pfn:@duopCmp_EDX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:INSTR * LOOPS),
    // cmp edx, INSTR * LOOPS
    (pfn:@duopLoad_Displac;    uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    //     @loop
    (pfn:@duopJne;             uop:0; iduop:0; delta_PC:0; optional:0; ops:0)
    // jne

//    pfn: PFNDUOP_;   // pointer to handler
//    uop: WORD;       // 16-bit duop opcode
//    iduop: BYTE;     // index of this duop into the block
//    delta_PC: BYTE;  // start of guest instruction duop corresponds to
//    optional: DWORD; // optional 32-bit operand
//    ops: DWORD;      // 32-bit packed operand(s) for this duop
  );

{$IFDEF CPUx86}
var
  _temp:  DWORD = 0;
  
procedure test_x86a;
asm
  push ebp
  push edi
@init:  
  mov _temp, 0
  
  mov edx, $FFFFFFFF;
  lea ebp, [_temp+8]
  lea edi, [_temp+8]

@sample_loop:  
  mov ecx, [edi-8] 
  add ecx, +5
  mov ebx, ecx
  mov [edi-8], ecx
  cmp ebx, -1
  je @sample_loop
  
  mov eax, [ebp-8]
  add eax, -3
  mov edx, eax
  mov [ebp-8], eax
  cmp edx, CINSTR
  jne @sample_loop

@finish:
  pop edi
  pop ebp
end;
{$ENDIF CPUx86}


// The A variant loops until a fire escape, does not dispatch on return value.
// Should be prone to misprediction.

procedure test_s86a_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  repeat
    pfn := pduop[0].pfn;

    repeat
      pfn := pduop[0].pfn;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
      Inc(pduop);
    until (pfn = @duopFireEscape);

    pduop := v_TS.pduopNext;
  until pduop = nil;
end;

// The B variant loops until a fire escape.
// Should be prone to misprediction.

procedure test_s86b_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  repeat
    pfn := pduop[0].pfn;

    repeat
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
      Inc(pduop);
    until (pfn = @duopFireEscape);

    pduop := v_TS.pduopNext;
  until pduop = nil;
end;

// The C variant is hard coded for a specific trace length.
// Can potentially have perfect branch prediction.

procedure test_s86c_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP;
begin
  v_TS.pduopNext := pduop;

  repeat
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

    pduop := v_TS.pduopNext;
  until pduop = nil;
end;

// The D variant loops of a given trace length.
// More general but will suffer from branch misprediction.

procedure test_s86d_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP;
  i: Integer;
begin
  v_TS.pduopNext := pduop;

  repeat
    pfn := pduop[0].pfn;

    for i := 0 to count - 1 do
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[i]);

    pduop := v_TS.pduopNext;
  until pduop = nil;
end;

// The E variant is similar to C variant but does not use return value.
// May suffer from branch misprediction as next handler is calculated later.

procedure test_s86e_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP;
begin
  v_TS.pduopNext := pduop;

  repeat
    pfn := pduop[0].pfn;

    PFNDUOP(pduop[0].pfn)(@v_TS, @pduop[0]);
    PFNDUOP(pduop[1].pfn)(@v_TS, @pduop[1]);
    PFNDUOP(pduop[2].pfn)(@v_TS, @pduop[2]);
    PFNDUOP(pduop[3].pfn)(@v_TS, @pduop[3]);
    PFNDUOP(pduop[4].pfn)(@v_TS, @pduop[4]);
    PFNDUOP(pduop[5].pfn)(@v_TS, @pduop[5]);
    PFNDUOP(pduop[6].pfn)(@v_TS, @pduop[6]);
    PFNDUOP(pduop[7].pfn)(@v_TS, @pduop[7]);
    PFNDUOP(pduop[8].pfn)(@v_TS, @pduop[8]);

    pduop := v_TS.pduopNext;
  until pduop = nil;
end;

// The F variant is similar to D and variants, and should have worst time.

procedure test_s86f_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP;
  i: Integer;
begin
  v_TS.pduopNext := pduop;

  repeat
    pfn := pduop[0].pfn;

    for i := 0 to count - 1 do
      PFNDUOP(pduop[i].pfn)(@v_TS, @pduop[i]);

    pduop := v_TS.pduopNext;
  until pduop = nil;
end;

// The G variant is similar to B but hard coded for trace length of 2.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

procedure test_s86g_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    Inc(pduop, 2);

    if pfn = @duopFireEscape then
      pduop := v_TS.pduopNext;
  end;
end;

// The H variant is similar to B but hard coded for trace length of 3.
// Should be prone to branch misprediction.

procedure test_s86h_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    Inc(pduop, 3);

    if pfn = @duopFireEscape then
      pduop := v_TS.pduopNext;
  end;
end;

// The I variant is similar to B but hard coded for trace length of 4.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

procedure test_s86i_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
    Inc(pduop, 4);

    if pfn = @duopFireEscape then
      pduop := v_TS.pduopNext;
  end;
end;

// The J variant is similar to B but hard coded for trace length of 5.
// Should be prone to branch misprediction and wasted dispatch to fire escape.

procedure test_s86j_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
    Inc(pduop, 5);

    if pfn = @duopFireEscape then
      pduop := v_TS.pduopNext;
  end;
end;

// The K variant is similar to B but hard coded for trace length of 6.

procedure test_s86k_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);
    Inc(pduop, 6);

    if pfn = @duopFireEscape then
      pduop := v_TS.pduopNext;
  end;
end;

// The L variant is the ideal case hard coded for the 18 DUOPs in the test.

procedure test_s86l_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

      pduop := v_TS.pduopNext;
    end;
  end;
end;

// The M variant is the ideal case hard coded for the 18 DUOPs in the test.
// Padded with NOPs to possibly increase branch prediction rate.

procedure test_s86m_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  while pduop <> nil do
  begin
    pfn := pduop[0].pfn;

    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);
    PAD8;
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);
      PAD8;
      pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

      pduop := v_TS.pduopNext;
    end;
  end;
end;

// The N variant is the general purpose case with expicit fire escape check.
// Should handle variable length blocks equally well.

procedure test_s86n_worker(pduop: PDUOP_; count: Integer);
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  // outer loop
  while pduop <> nil do
  begin
    // a long block that can terminate at any time
    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

      if (pfn <> @duopFireEscape) then
      begin
        Inc(pduop);
        pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

        if (pfn <> @duopFireEscape) then
        begin
          Inc(pduop);
          pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

          if (pfn <> @duopFireEscape) then
          begin
            Inc(pduop);
            pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

            if (pfn <> @duopFireEscape) then
            begin
              Inc(pduop);
              pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

              if (pfn <> @duopFireEscape) then
              begin
              Inc(pduop);
                pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                if (pfn <> @duopFireEscape) then
                begin
                  Inc(pduop);
                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                  if (pfn <> @duopFireEscape) then
                  begin
                    Inc(pduop);
                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                    if (pfn <> @duopFireEscape) then
                    begin
                      Inc(pduop);
                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                      if (pfn <> @duopFireEscape) then
                      begin
                        Inc(pduop);
                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                        if (pfn <> @duopFireEscape) then
                        begin
                          Inc(pduop);
                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                          if (pfn <> @duopFireEscape) then
                          begin
                            Inc(pduop);
                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                            if (pfn <> @duopFireEscape) then
                            begin
                              Inc(pduop);
                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                              if (pfn <> @duopFireEscape) then
                              begin
                                Inc(pduop);
                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                if (pfn <> @duopFireEscape) then
                                begin
                                  Inc(pduop);
                                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                  if (pfn <> @duopFireEscape) then
                                  begin
                                    Inc(pduop);
                                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                    if (pfn <> @duopFireEscape) then
                                    begin
                                      Inc(pduop);
                                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                      if (pfn <> @duopFireEscape) then
                                      begin
                                        Inc(pduop);
                                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                        if (pfn <> @duopFireEscape) then
                                        begin
                                          Inc(pduop);
                                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                          if (pfn <> @duopFireEscape) then
                                          begin
                                            Inc(pduop);
                                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                            if (pfn <> @duopFireEscape) then
                                            begin
                                              Inc(pduop);
                                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                              if (pfn <> @duopFireEscape) then
                                              begin
                                                Inc(pduop);
                                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
                                              end;
                                            end;
                                          end;
                                        end;
                                      end;
                                    end;
                                  end;
                                end;
                              end;
                            end;
                          end;
                        end;
                      end;
                    end;
                  end;
                end;
              end;
            end;
          end;
        end;
      end;
    end;
    // another long block
    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

      if (pfn <> @duopFireEscape) then
      begin
        Inc(pduop);
        pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

        if (pfn <> @duopFireEscape) then
        begin
          Inc(pduop);
          pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

          if (pfn <> @duopFireEscape) then
          begin
            Inc(pduop);
            pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

            if (pfn <> @duopFireEscape) then
            begin
              Inc(pduop);
              pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

              if (pfn <> @duopFireEscape) then
              begin
              Inc(pduop);
                pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                if (pfn <> @duopFireEscape) then
                begin
                  Inc(pduop);
                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                  if (pfn <> @duopFireEscape) then
                  begin
                    Inc(pduop);
                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                    if (pfn <> @duopFireEscape) then
                    begin
                      Inc(pduop);
                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                      if (pfn <> @duopFireEscape) then
                      begin
                        Inc(pduop);
                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                        if (pfn <> @duopFireEscape) then
                        begin
                          Inc(pduop);
                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                          if (pfn <> @duopFireEscape) then
                          begin
                            Inc(pduop);
                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                            if (pfn <> @duopFireEscape) then
                            begin
                              Inc(pduop);
                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                              if (pfn <> @duopFireEscape) then
                              begin
                                Inc(pduop);
                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                if (pfn <> @duopFireEscape) then
                                begin
                                  Inc(pduop);
                                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                  if (pfn <> @duopFireEscape) then
                                  begin
                                    Inc(pduop);
                                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                    if (pfn <> @duopFireEscape) then
                                    begin
                                      Inc(pduop);
                                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                      if (pfn <> @duopFireEscape) then
                                      begin
                                        Inc(pduop);
                                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                        if (pfn <> @duopFireEscape) then
                                        begin
                                          Inc(pduop);
                                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                          if (pfn <> @duopFireEscape) then
                                          begin
                                            Inc(pduop);
                                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                            if (pfn <> @duopFireEscape) then
                                            begin
                                              Inc(pduop);
                                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

                                              if (pfn <> @duopFireEscape) then
                                              begin
                                                Inc(pduop);
                                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);
                                              end;
                                            end;
                                          end;
                                        end;
                                      end;
                                    end;
                                  end;
                                end;
                              end;
                            end;
                          end;
                        end;
                      end;
                    end;
                  end;
                end;
              end;
            end;
          end;
        end;
      end;
    end;
  end; // while
end;

// The O variant is the general purpose case with expicit fire escape check.
// Should handle variable length blocks equally well.

procedure test_s86o_worker(pduop: PDUOP_; count: Integer); // NostradamusDistributor
var
  pfn: PFNDUOP_;
begin
  v_TS.pduopNext := pduop;

  // outer loop
  while pduop <> nil do
  begin
    // a long block that can terminate at any time

    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

      if (pfn <> @duopFireEscape) then
      begin
        pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);

        if (pfn <> @duopFireEscape) then
        begin
          pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);

          if (pfn <> @duopFireEscape) then
          begin
            pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);

            if (pfn <> @duopFireEscape) then
            begin
              pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);

              if (pfn <> @duopFireEscape) then
              begin
                pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);

                if (pfn <> @duopFireEscape) then
                begin
                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);

                  if (pfn <> @duopFireEscape) then
                  begin
                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);

                    if (pfn <> @duopFireEscape) then
                    begin
                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

                      if (pfn <> @duopFireEscape) then
                      begin
                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[9]);

                        if (pfn <> @duopFireEscape) then
                        begin
                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[10]);

                          if (pfn <> @duopFireEscape) then
                          begin
                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[11]);

                            if (pfn <> @duopFireEscape) then
                            begin
                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[12]);

                              if (pfn <> @duopFireEscape) then
                              begin
                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[13]);

                                if (pfn <> @duopFireEscape) then
                                begin
                                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[14]);

                                  if (pfn <> @duopFireEscape) then
                                  begin
                                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[15]);

                                    if (pfn <> @duopFireEscape) then
                                    begin
                                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[16]);

                                      if (pfn <> @duopFireEscape) then
                                      begin
                                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[17]);

                                        if (pfn <> @duopFireEscape) then
                                        begin
                                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[18]);

                                          if (pfn <> @duopFireEscape) then
                                          begin
                                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[19]);

                                            if (pfn <> @duopFireEscape) then
                                            begin
                                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[20]);
                                            end;
                                          end;
                                        end;
                                      end;
                                    end;
                                  end;
                                end;
                              end;
                            end;
                          end;
                        end;
                      end;
                    end;
                  end;
                end;
              end;
            end;
          end;
        end;
      end;
    end;
    // another long block
    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

      if (pfn <> @duopFireEscape) then
      begin
        pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);

        if (pfn <> @duopFireEscape) then
        begin
          pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);

          if (pfn <> @duopFireEscape) then
          begin
            pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);

            if (pfn <> @duopFireEscape) then
            begin
              pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);

              if (pfn <> @duopFireEscape) then
              begin
                pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);

                if (pfn <> @duopFireEscape) then
                begin
                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);

                  if (pfn <> @duopFireEscape) then
                  begin
                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);

                    if (pfn <> @duopFireEscape) then
                    begin
                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

                      if (pfn <> @duopFireEscape) then
                      begin
                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[9]);

                        if (pfn <> @duopFireEscape) then
                        begin
                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[10]);

                          if (pfn <> @duopFireEscape) then
                          begin
                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[11]);

                            if (pfn <> @duopFireEscape) then
                            begin
                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[12]);

                              if (pfn <> @duopFireEscape) then
                              begin
                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[13]);

                                if (pfn <> @duopFireEscape) then
                                begin
                                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[14]);

                                  if (pfn <> @duopFireEscape) then
                                  begin
                                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[15]);

                                    if (pfn <> @duopFireEscape) then
                                    begin
                                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[16]);

                                      if (pfn <> @duopFireEscape) then
                                      begin
                                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[17]);

                                        if (pfn <> @duopFireEscape) then
                                        begin
                                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[18]);

                                          if (pfn <> @duopFireEscape) then
                                          begin
                                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[19]);

                                            if (pfn <> @duopFireEscape) then
                                            begin
                                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[20]);
                                            end;
                                          end;
                                        end;
                                      end;
                                    end;
                                  end;
                                end;
                              end;
                            end;
                          end;
                        end;
                      end;
                    end;
                  end;
                end;
              end;
            end;
          end;
        end;
      end;
    end;

  end; // while
end;


function getms: ULONG;
var
  qpc: Int64;
begin
  if qpf = 0 then
    QueryPerformanceFrequency(qpf);

  QueryPerformanceCounter(qpc);

  Result := qpc div (qpf div 1000);
end;

var
  Output: string = '';

procedure printf(fmtstr: string; Args: array of const);
begin
  Output := Output + Format(fmtstr, Args);
  if Output[Length(Output)] = #13 then
  begin
    SetLength(Output, Length(Output) - 1);
    OutputDebugString(PChar(Output));
    Output := '';
  end;
end;

type
  PFNDUOPDISPATCH = procedure (pduop: PDUOP_; count: Integer);

procedure test_dispatch_duop(ppfn: PFNDUOPDISPATCH);
var
  temp: DWORD;
begin
  ZeroMemory(@v_TS, SizeOf(v_TS));
  DWORD(temp) := 0;

  v_TS.EDX    := $FFFFFFFF;
  v_TS.EBP    := 8 + UIntPtr(@temp);
  v_TS.EDI    := 8 + UIntPtr(@temp);

  ppfn(@rgduop[0], Length(rgduop));

  if (v_TS.EDX <> (INSTR * LOOPS)) then
    printf('ERROR in simulate loop, wrong value = %d'#13, [v_TS.EDX]);
end;

procedure test_s86;
begin
  test_dispatch_duop(@test_s86a_worker);
end;

procedure test_s86b;
begin
  test_dispatch_duop(@test_s86b_worker);
end;

procedure test_s86c;
begin
  test_dispatch_duop(@test_s86c_worker);
end;

procedure test_s86d;
begin
  test_dispatch_duop(@test_s86d_worker);
end;

procedure test_s86e;
begin
  test_dispatch_duop(@test_s86e_worker);
end;

procedure test_s86f;
begin
  test_dispatch_duop(@test_s86f_worker);
end;

procedure test_s86g;
begin
  test_dispatch_duop(@test_s86g_worker);
end;

procedure test_s86h;
begin
  test_dispatch_duop(@test_s86h_worker);
end;

procedure test_s86i;
begin
  test_dispatch_duop(@test_s86i_worker);
end;

procedure test_s86j;
begin
  test_dispatch_duop(@test_s86j_worker);
end;

procedure test_s86k;
begin
  test_dispatch_duop(@test_s86k_worker);
end;

procedure test_s86l;
begin
  test_dispatch_duop(@test_s86l_worker);
end;

procedure test_s86m;
begin
  test_dispatch_duop(@test_s86m_worker);
end;

procedure test_s86n;
begin
  test_dispatch_duop(@test_s86n_worker);
end;

procedure test_s86o;
begin
  test_dispatch_duop(@test_s86o_worker);
end;

//
// End Virtual CPU tests
//


(*
#if !defined(_WIN64) && !NOASM

procedure test_nop1;
begin
    ULONG cnt = LOOPS;

    while (cnt--)
        begin
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
        end;
end;


procedure test_nop2;
begin
    ULONG cnt = LOOPS;

    while (cnt--)
      __asm
        begin
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
        end;
end;


procedure test_nop3;
begin
    ULONG cnt = LOOPS;

    while (cnt--)
      __asm
        begin
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
        end;
end;

#endif
*)

procedure BenchmarkPfn(pfn: TProc; szTest: string; loops: Int64; instr: Int64);
var
  t1, t2: ULONG;
  kips: ULONG;
begin
  try
    vt1 := getms();
    repeat
      t1 := getms();
    until t1 <> vt1;
    vt1 := t1;

    pfn();

    t2 := getms();
    t1 := vt1;

    if (t2 = t1) then
      Inc(t2);

    printf('%-19s: %7d ms, %9d ps/instr, ', [szTest,
        t2-t1, ULONG((Int64(t2-t1) * 1000000000) div (LOOPS * INSTR))]);

    if (int64(loops * instr) div int64(t2-t1) div 1000 = 0) then
      printf(' 0.%03d MIPS, ', [int64(loops * instr) div int64(t2-t1)])
    else
      printf('%6d MIPS, ', [int64(loops * instr) div int64(t2-t1) div 1000]);

    kips := int64(loops * instr) div int64(t2-t1);

    if (vRefSpeed = 0) then
      vRefSpeed := kips;

    if (vfClocksOnly) then
    begin
      // always display the ratio as clocks per instruction/iteration

      printf('%7d.%d clk', [vRefSpeed div kips, ((10*vRefSpeed) div kips) mod 10]);
    end
    else if (kips >= vRefSpeed) then
    begin
      // handle the case of where the ratio is x >= 1.0

      printf('%7d.%d IPC', [kips div vRefSpeed, ((10*kips) div vRefSpeed) mod 10]);
    end
    else if ((kips div 9) >= (vRefSpeed div 10)) then
    begin
      // handle the case of where the ratio is 0.9 <= x < 1.0

      printf('      1.0 IPC', []);
    end
    else
    begin
      // handle the case of where the ratio is x < 0.9

      printf('%7d.%d clk', [vRefSpeed div kips, ((10*vRefSpeed) div kips) mod 10]);
    end;

  except
    printf('%s: FAILED!!', [szTest]);
  end;

  printf(#13, []);
//    fflush(stdout);
end;


procedure test_s86p_worker(pduop: PDUOP_; count: Integer); // Pvl Optimized NostradamusDistributor
var
  pfn: PFNDUOP_;
  local_duopFireEscape: Pointer;
begin
  local_duopFireEscape := @duopFireEscape;
  v_TS.pduopNext := pduop;

  // outer loop
  while pduop <> nil do
  begin
    // a long block that can terminate at any time

    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

      if (pfn <> local_duopFireEscape) then
      begin
        pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);

        if (pfn <> local_duopFireEscape) then
        begin
          pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);

          if (pfn <> local_duopFireEscape) then
          begin
            pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);

            if (pfn <> local_duopFireEscape) then
            begin
              pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);

              if (pfn <> local_duopFireEscape) then
              begin
                pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);

                if (pfn <> local_duopFireEscape) then
                begin
                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);

                  if (pfn <> local_duopFireEscape) then
                  begin
                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);

                    if (pfn <> local_duopFireEscape) then
                    begin
                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

                      if (pfn <> local_duopFireEscape) then
                      begin
                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[9]);

                        if (pfn <> local_duopFireEscape) then
                        begin
                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[10]);

                          if (pfn <> local_duopFireEscape) then
                          begin
                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[11]);

                            if (pfn <> local_duopFireEscape) then
                            begin
                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[12]);

                              if (pfn <> local_duopFireEscape) then
                              begin
                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[13]);

                                if (pfn <> local_duopFireEscape) then
                                begin
                                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[14]);

                                  if (pfn <> local_duopFireEscape) then
                                  begin
                                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[15]);

                                    if (pfn <> local_duopFireEscape) then
                                    begin
                                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[16]);

                                      if (pfn <> local_duopFireEscape) then
                                      begin
                                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[17]);

                                        if (pfn <> local_duopFireEscape) then
                                        begin
                                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[18]);

                                          if (pfn <> local_duopFireEscape) then
                                          begin
                                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[19]);

                                            if (pfn <> local_duopFireEscape) then
                                            begin
                                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[20]);
                                            end;
                                          end;
                                        end;
                                      end;
                                    end;
                                  end;
                                end;
                              end;
                            end;
                          end;
                        end;
                      end;
                    end;
                  end;
                end;
              end;
            end;
          end;
        end;
      end;
    end;
    // another long block
    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@v_TS, @pduop[0]);

      if (pfn <> local_duopFireEscape) then
      begin
        pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);

        if (pfn <> local_duopFireEscape) then
        begin
          pfn := PFNDUOP(pfn)(@v_TS, @pduop[2]);

          if (pfn <> local_duopFireEscape) then
          begin
            pfn := PFNDUOP(pfn)(@v_TS, @pduop[3]);

            if (pfn <> local_duopFireEscape) then
            begin
              pfn := PFNDUOP(pfn)(@v_TS, @pduop[4]);

              if (pfn <> local_duopFireEscape) then
              begin
                pfn := PFNDUOP(pfn)(@v_TS, @pduop[5]);

                if (pfn <> local_duopFireEscape) then
                begin
                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[6]);

                  if (pfn <> local_duopFireEscape) then
                  begin
                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[7]);

                    if (pfn <> local_duopFireEscape) then
                    begin
                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[8]);

                      if (pfn <> local_duopFireEscape) then
                      begin
                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[9]);

                        if (pfn <> local_duopFireEscape) then
                        begin
                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[10]);

                          if (pfn <> local_duopFireEscape) then
                          begin
                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[11]);

                            if (pfn <> local_duopFireEscape) then
                            begin
                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[12]);

                              if (pfn <> local_duopFireEscape) then
                              begin
                                pfn := PFNDUOP(pfn)(@v_TS, @pduop[13]);

                                if (pfn <> local_duopFireEscape) then
                                begin
                                  pfn := PFNDUOP(pfn)(@v_TS, @pduop[14]);

                                  if (pfn <> local_duopFireEscape) then
                                  begin
                                    pfn := PFNDUOP(pfn)(@v_TS, @pduop[15]);

                                    if (pfn <> local_duopFireEscape) then
                                    begin
                                      pfn := PFNDUOP(pfn)(@v_TS, @pduop[16]);

                                      if (pfn <> local_duopFireEscape) then
                                      begin
                                        pfn := PFNDUOP(pfn)(@v_TS, @pduop[17]);

                                        if (pfn <> local_duopFireEscape) then
                                        begin
                                          pfn := PFNDUOP(pfn)(@v_TS, @pduop[18]);

                                          if (pfn <> local_duopFireEscape) then
                                          begin
                                            pfn := PFNDUOP(pfn)(@v_TS, @pduop[19]);

                                            if (pfn <> local_duopFireEscape) then
                                            begin
                                              pfn := PFNDUOP(pfn)(@v_TS, @pduop[20]);
                                            end;
                                          end;
                                        end;
                                      end;
                                    end;
                                  end;
                                end;
                              end;
                            end;
                          end;
                        end;
                      end;
                    end;
                  end;
                end;
              end;
            end;
          end;
        end;
      end;
    end;

  end; // while
end;

procedure test_s86p;
begin
  test_dispatch_duop(@test_s86p_worker);
end;

initialization

  _rgduop_0 := @rgduop[0];

(*
Laptop Debug Output (na vertalen orginele code) :
x86 sim in C ver a :    3496 ms,     36416 ps/instr
x86 sim in C ver b :    3543 ms,     36906 ps/instr
x86 sim in C ver c :    2013 ms,     20968 ps/instr
x86 sim in C ver d :    8458 ms,     88104 ps/instr
x86 sim in C ver e :    2266 ms,     23604 ps/instr
x86 sim in C ver f :    2909 ms,     30302 ps/instr
x86 sim in C ver g :    4438 ms,     46229 ps/instr
x86 sim in C ver h :    2683 ms,     27947 ps/instr
x86 sim in C ver i :    3930 ms,     40937 ps/instr
x86 sim in C ver j :    2744 ms,     28583 ps/instr
x86 sim in C ver k :    2794 ms,     29104 ps/instr
x86 sim in C ver l :    1985 ms,     20677 ps/instr
x86 sim in C ver o :    2238 ms,     23312 ps/instr

Laptop Debug Output (na verwijderen dubbele pduopNext assignments in duop* functies) :
x86 sim in C ver a :    3449 ms,     35927 ps/instr
x86 sim in C ver b :    4328 ms,     45083 ps/instr
x86 sim in C ver c :    2006 ms,     20895 ps/instr
x86 sim in C ver d :    8251 ms,     85947 ps/instr
x86 sim in C ver e :    2165 ms,     22552 ps/instr
x86 sim in C ver f :    2862 ms,     29812 ps/instr
x86 sim in C ver g :    4009 ms,     41760 ps/instr
x86 sim in C ver h :    2405 ms,     25052 ps/instr
x86 sim in C ver i :    3140 ms,     32708 ps/instr
x86 sim in C ver j :    2503 ms,     26072 ps/instr
x86 sim in C ver k :    2746 ms,     28604 ps/instr
x86 sim in C ver l :    1998 ms,     20812 ps/instr
x86 sim in C ver o :    2206 ms,     22979 ps/instr

Desktop Debug Output (na doorgave van v_TS aan RETURN_OPVEC* ):
x86 sim in C ver a :    1891 ms,     19697 ps/instr
x86 sim in C ver b :    1709 ms,     17802 ps/instr
x86 sim in C ver c :    1361 ms,     14177 ps/instr
x86 sim in C ver d :    4755 ms,     49531 ps/instr
x86 sim in C ver e :    1366 ms,     14229 ps/instr
x86 sim in C ver f :    1883 ms,     19614 ps/instr
x86 sim in C ver g :    1874 ms,     19520 ps/instr
x86 sim in C ver h :    1490 ms,     15520 ps/instr
x86 sim in C ver i :    2669 ms,     27802 ps/instr
x86 sim in C ver j :    1621 ms,     16885 ps/instr
x86 sim in C ver k :    2429 ms,     25302 ps/instr
x86 sim in C ver l :    1334 ms,     13895 ps/instr
x86 sim in C ver o :    1564 ms,     16291 ps/instr

Desktop Debug Output (na v_TS vooraan te zetten, en muziek uit) :
x86 sim in C ver a :    1863 ms,     19406 ps/instr
x86 sim in C ver b :    1638 ms,     17062 ps/instr
x86 sim in C ver c :    1325 ms,     13802 ps/instr
x86 sim in C ver d :    4663 ms,     48572 ps/instr
x86 sim in C ver e :    1312 ms,     13666 ps/instr
x86 sim in C ver f :    1834 ms,     19104 ps/instr
x86 sim in C ver g :    1815 ms,     18906 ps/instr
x86 sim in C ver h :    1449 ms,     15093 ps/instr
x86 sim in C ver i :    2591 ms,     26989 ps/instr
x86 sim in C ver j :    1486 ms,     15479 ps/instr
x86 sim in C ver k :    2323 ms,     24197 ps/instr
x86 sim in C ver l :    1330 ms,     13854 ps/instr
x86 sim in C ver o :    1452 ms,     15125 ps/instr


Desktop Debug Output (ipv 32 bit, 64 bit build - 6 opcodes) :
x86 sim in C ver a :    4723 ms,     49197 ps/instr
x86 sim in C ver b :    4124 ms,     42958 ps/instr
x86 sim in C ver c :    3436 ms,     35791 ps/instr
x86 sim in C ver d :    6874 ms,     71604 ps/instr
x86 sim in C ver e :    3572 ms,     37208 ps/instr
x86 sim in C ver f :    5130 ms,     53437 ps/instr
x86 sim in C ver g :    4314 ms,     44937 ps/instr
x86 sim in C ver h :    4182 ms,     43562 ps/instr
x86 sim in C ver i :    5758 ms,     59979 ps/instr
x86 sim in C ver j :    3974 ms,     41395 ps/instr
x86 sim in C ver k :    5065 ms,     52760 ps/instr
x86 sim in C ver l :    1328 ms,     13833 ps/instr
x86 sim in C ver o :    1493 ms,     15552 ps/instr


Desktop Debug Output (64 bit build met local_duopFireEscape variable - 5 opcodes) :

x86 sim in C ver a :    4770 ms,     49687 ps/instr
x86 sim in C ver b :    4164 ms,     43375 ps/instr
x86 sim in C ver c :    3465 ms,     36093 ps/instr
x86 sim in C ver d :    6923 ms,     72114 ps/instr
x86 sim in C ver e :    3691 ms,     38447 ps/instr
x86 sim in C ver f :    5199 ms,     54156 ps/instr
x86 sim in C ver g :    4401 ms,     45843 ps/instr
x86 sim in C ver h :    4227 ms,     44031 ps/instr
x86 sim in C ver i :    5879 ms,     61239 ps/instr
x86 sim in C ver j :    4029 ms,     41968 ps/instr
x86 sim in C ver k :    5099 ms,     53114 ps/instr
x86 sim in C ver l :    1334 ms,     13895 ps/instr
x86 sim in C ver o :    1472 ms,     15333 ps/instr

Desktop Debug Output (32 bit build met local_duopFireEscape variable - 6 opcodes) :

x86 sim in C ver l :    1327 ms,     13822 ps/instr
x86 sim in C ver o :    1449 ms,     15093 ps/instr

  NostradamusDistributor is an optimized dispatch mechanism,
  used for interpreting opcodes.

  Source : http://www.emulators.com/docs/nx25_nostradamus.htm

  if (pfn != (PFNDUOP)duopFireEscape)
  {
    pfn = (PFNDUOP)(*pfn)(&pduop[1]);

  Which results in this compiled code for each dispatch spanning 19-byte per dispatch:

    cmp eax, OFFSET FLAT duopFireEscape
    je $L62748
    mov edx, DWORD PTR [esi+28]
    lea ecx, DWORD PTR [esi+16]
    call eax

  In my translation to Delphi, the code reads :

  if (pfn <> @duopFireEscape) then
  begin
    pfn := PFNDUOP(pfn)(@v_TS, @pduop[1]);

  Delphi's default 'register' calling convention uses EAX as both first argument AND return value,
  which results in assembly that's one instruction longer than the above code :

  32 bit :

    cmp ebx, $004a0d30 // = duopFireEscape
    jz $004a0e04
    lea eax, [esi+$20] // Delhi puts the first in EAX
    mov edx, [esi+$2c]
    call ebx
    mov ebx, eax // <- Resulting in this copy of EAX to EBX !

  32 bit (after putting duopFireEscape in a local variable) :

    cmp edi,esi // local_duopFireEscape
    jz $005d76ed
    lea edx,[ebx+$10]
    mov eax,ebp
    call esi
    mov esi,eax

  Techniques I tried :
  - Other Delphi calling convention : None use registers.
  - Force pfn into EAX by using Result : Inserted additional mov and xchg.
  - Untyped first argument : ?

  Alas this cannot be avoided, making the Delphi 32 bit version of the
  NostradamusDistributor a bit slower than it could have been.

  64 bit :

    lea rcx, [rel $fffffc35] // = duopFireEscape - alas put in a separate register
    cmp rax,rcx
    jz NostradamusDistributor + $298
    lea rcx, [rel $0004a462]
    lea rdx, [rbx+$18]
    call rax

  64 bit (after putting duopFireEscape in a local variable) :

    cmp rax,rsi
    jz NostradamusDistributor + $20C
    lea rcx,[rel $0004a44b]
    lea rdx,[rbx+$30]
    call rax

  Yeah! Delphi (XE6) with the same performance under 64 bit as C!
*)

  // TODO : Read from commandline/registry/whatever :
  vRefSpeed := 3400000;
  vfClocksOnly := True;

  printf('Intel(R) Core(TM) i7-2600 CPU @ 3.40GHz  3.40GHz, Delphi XE6 compiler:'#13, []);
  printf(#13, []);
  
{$IFDEF CPUx86}
  BenchmarkPfn(test_x86a, 'x86 native loop', LOOPS, INSTR);
{$ENDIF}
  // simulated loop?    
  BenchmarkPfn(test_s86,  'x86 sim in C ver a', LOOPS, INSTR);
  BenchmarkPfn(test_s86b, 'x86 sim in C ver b', LOOPS, INSTR);
  BenchmarkPfn(test_s86c, 'x86 sim in C ver c', LOOPS, INSTR);
  BenchmarkPfn(test_s86d, 'x86 sim in C ver d', LOOPS, INSTR);
  BenchmarkPfn(test_s86e, 'x86 sim in C ver e', LOOPS, INSTR);
  BenchmarkPfn(test_s86f, 'x86 sim in C ver f', LOOPS, INSTR);
  BenchmarkPfn(test_s86g, 'x86 sim in C ver g', LOOPS, INSTR);
  BenchmarkPfn(test_s86h, 'x86 sim in C ver h', LOOPS, INSTR);
  BenchmarkPfn(test_s86i, 'x86 sim in C ver i', LOOPS, INSTR);
  BenchmarkPfn(test_s86j, 'x86 sim in C ver j', LOOPS, INSTR);
  BenchmarkPfn(test_s86k, 'x86 sim in C ver k', LOOPS, INSTR);
  BenchmarkPfn(test_s86l, 'x86 sim in C ver l', LOOPS, INSTR);
  BenchmarkPfn(test_s86m, 'x86 sim in C ver m', LOOPS, INSTR);
  BenchmarkPfn(test_s86n, 'x86 sim in C ver n', LOOPS, INSTR);
  BenchmarkPfn(test_s86o, 'x86 sim in C ver o', LOOPS, INSTR); // original NostradamusDistributor
  BenchmarkPfn(test_s86p, 'x86 sim in C ver p', LOOPS, INSTR); // PvL improved

end.

