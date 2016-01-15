unit Unit1;

interface

implementation

end.

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

{$OPTIMIZATION ON}
{$RANGECHECKS OFF}
{$OVERFLOWCHECKS OFF}
{$POINTERMATH ON}

implementation

{$R *.dfm}

type
  UIntPtr = DWord;

const LOOPS = 3000000;
const INSTR = 32;

//var vRefSpeed: ULONG = 0;
//var vfClocksOnly: ULONG = 0;
var vt1: ULONG = 0; // volatile

//#define CHECK(expr)         extern int foo[(int)(expr) ? 1 : -(__LINE__)]
//#define OFFSETOF(s,m)       (size_t)&(((s *)0)->m)

type
  TCode = Pointer;

  PDUOP_ = ^DUOP;

  PFNDUOP_ = TCode; // Nodig omdat Delphi PFNDUOP variabelen interpreteert als calls
  PFNDUOP = function (const pduop: PDUOP_; const ops: DWORD): PFNDUOP_; register;

  // A DUOP contains a pointer to the handler, and various operands
  DUOP = record
    pfn: PFNDUOP_;   // pointer to handler
    uop: WORD;       // 16-bit duop opcode
    iduop: BYTE;     // index of this duop into the block
    delta_PC: BYTE;  // start of guest instruction duop corresponds to
    optional: DWORD; // optional 32-bit operand
    ops: Int32;      // 32-bit packed operand(s) for this duop
  end;

  // Thread state is global
  TSTATE = record
    RegPC: DWORD;
    EBP: DWORD;
    EDI: DWORD;
    EAX: DWORD;
    ECX: DWORD;
    EDX: DWORD;
    EBX: DWORD;
    RegEA: DWORD;
    RegConst: DWORD;
    RegLastRes: DWORD;
    RegDisp: Pointer;
    pduopNext: PDUOP_;
  end;

var
  v_TS: TSTATE = ();

function RETURN_OPVEC_2(const x: PFNDUOP; const y: PDUOP_): PFNDUOP; inline;
begin
  v_TS.pduopNext := y;
  Result := x;
end;

function RETURN_OPVEC(const x: PDUOP_): PFNDUOP; inline;
begin
  v_TS.pduopNext := x;
  Result := x.pfn;
end;

// DUOP handlers operate on global state and may optionally use operands.
// Handlers must return pointer to the next handler or nil on error.
function duopGenEA_EBP_Disp(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EBP + ops;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopGenEA_EDI_Disp(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EDI + ops;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopLoad_EAX_Mem32(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.EAX := PDWORD(v_TS.RegEA)^;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopLoad_ECX_Mem32(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.ECX := PDWORD(v_TS.RegEA)^;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopStore_EAX_Mem32(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  PDWORD(v_TS.RegEA)^ := v_TS.EAX;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopStore_ECX_Mem32(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  PDWORD(v_TS.RegEA)^ := v_TS.ECX;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopAdd_EAX_Op(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  Inc(v_TS.EAX, ops);
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopAdd_ECX_Op(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  Inc(v_TS.ECX, ops);
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopMov_EDX_EAX(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.EDX := v_TS.EAX;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopMov_EBX_ECX(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.EBX := v_TS.ECX;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopLoad_Constant(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegConst := ops;
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopCmp_EDX_Const(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EDX - v_TS.RegConst);
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopCmp_EBX_Const(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EBX - v_TS.RegConst);
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopCmp_EDX_Op(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EDX - ops);
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopCmp_EBX_Op(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegLastRes := (v_TS.EBX - ops);
  Result := RETURN_OPVEC(@pduop[1]);
end;

function duopLoad_Displac(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register; forward;

function duopFireEscape(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  Result := @duopFireEscape;
end;

function duopJeq(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EBP;

  if (v_TS.RegLastRes = 0) then
  begin
    Result := RETURN_OPVEC_2(@duopFireEscape, PDUOP_(v_TS.RegDisp));
  end
  else
    Result := RETURN_OPVEC_2(@duopFireEscape, @pduop[1]);
end;

function duopJne(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegEA := v_TS.EBP;

  if (v_TS.RegLastRes <> 0) then
    Result := RETURN_OPVEC_2(@duopFireEscape, PDUOP_(v_TS.RegDisp))
  else
    Result := RETURN_OPVEC_2(@duopFireEscape, nil); // ??
end;

const
  rgduop: array [0..17] of DUOP =
  (
//    pfn: PFNDUOP_;   // pointer to handler
//    uop: WORD;       // 16-bit duop opcode
//    iduop: BYTE;     // index of this duop into the block
//    delta_PC: BYTE;  // start of guest instruction duop corresponds to
//    optional: DWORD; // optional 32-bit operand
//    ops: DWORD;      // 32-bit packed operand(s) for this duop

    (pfn:@duopGenEA_EDI_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:-8),
    (pfn:@duopLoad_ECX_Mem32;  uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopAdd_ECX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:+5),
    (pfn:@duopMov_EBX_ECX;     uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopGenEA_EDI_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:-8),
    (pfn:@duopStore_ECX_Mem32; uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopCmp_EBX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:-1),
    (pfn:@duopLoad_Displac;    uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopJeq;             uop:0; iduop:0; delta_PC:0; optional:0; ops:0),

    (pfn:@duopGenEA_EBP_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:-8),
    (pfn:@duopLoad_EAX_Mem32;  uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopAdd_EAX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:-3),   // net result is 5-3 = +2
    (pfn:@duopMov_EDX_EAX;     uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopGenEA_EBP_Disp;  uop:0; iduop:0; delta_PC:0; optional:0; ops:-8),
    (pfn:@duopStore_EAX_Mem32; uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopCmp_EDX_Op;      uop:0; iduop:0; delta_PC:0; optional:0; ops:INSTR * LOOPS),
    (pfn:@duopLoad_Displac;    uop:0; iduop:0; delta_PC:0; optional:0; ops:0),
    (pfn:@duopJne;             uop:0; iduop:0; delta_PC:0; optional:0; ops:0)
  );

function duopLoad_Displac(const pduop: PDUOP_; const v_TS: PSTATE): PFNDUOP; register;
begin
  v_TS.RegDisp := @rgduop[0];
  Result := RETURN_OPVEC(@pduop[1]);
end;

var
  qpf: Int64 = 0;

function getms: ULONG;
var
  qpc: Int64;
begin
  if qpf = 0 then
    QueryPerformanceFrequency(qpf);

  QueryPerformanceCounter(qpc);

  Result := qpc div (qpf div 1000);
end;

procedure printf(fmtstr: string; Args: array of const);
begin
  OutputDebugString(PChar(Format(fmtstr, Args)));
end;

type
  PFNDUOPDISPATCH = procedure (pduop: PDUOP_; count: Integer);

procedure test_dispatch_duop(ppfn: PFNDUOPDISPATCH; const szTest: string);
var
  temp: DWORD;
  t1, t2: ULONG;
begin

  vt1 := getms();
  repeat
    t1 := getms();
  until t1 <> vt1;
//  vt1 := t1;

  begin
    temp := 0;

    ZeroMemory(@v_TS, SizeOf(v_TS));
    v_TS.EDX    := $FFFFFFFF;
    v_TS.EBP    := 8 + UIntPtr(@temp);
    v_TS.EDI    := 8 + UIntPtr(@temp);

    ppfn(@rgduop[0], Length(rgduop));

    if (v_TS.EDX <> (INSTR * LOOPS)) then
      printf('ERROR in simulate loop, wrong value = %d', [v_TS.EDX]);
  end;

  t2 := getms();
//  t1 := vt1;

  if (t2 = t1) then
    Inc(t2);

  printf('%-19s: %7d ms, %9d ps/instr, ', [szTest,
        t2-t1, ULONG((Int64(t2-t1) * 1000000000) div (INSTR * LOOPS))]);
end;


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
      pfn := PFNDUOP(pfn)(@pduop[0], @v_TS);
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
      pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
    pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
    pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
    pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
    pfn := PFNDUOP(pfn)(@pduop[5], pduop[5].ops);
    pfn := PFNDUOP(pfn)(@pduop[6], pduop[6].ops);
    pfn := PFNDUOP(pfn)(@pduop[7], pduop[7].ops);
    pfn := PFNDUOP(pfn)(@pduop[8], pduop[8].ops);

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
      pfn := PFNDUOP(pfn)(@pduop[i], pduop[i].ops);

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

    PFNDUOP(pduop[0].pfn)(@pduop[0], pduop[0].ops);
    PFNDUOP(pduop[1].pfn)(@pduop[1], pduop[1].ops);
    PFNDUOP(pduop[2].pfn)(@pduop[2], pduop[2].ops);
    PFNDUOP(pduop[3].pfn)(@pduop[3], pduop[3].ops);
    PFNDUOP(pduop[4].pfn)(@pduop[4], pduop[4].ops);
    PFNDUOP(pduop[5].pfn)(@pduop[5], pduop[5].ops);
    PFNDUOP(pduop[6].pfn)(@pduop[6], pduop[6].ops);
    PFNDUOP(pduop[7].pfn)(@pduop[7], pduop[7].ops);
    PFNDUOP(pduop[8].pfn)(@pduop[8], pduop[8].ops);

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
      PFNDUOP(pduop[i].pfn)(@pduop[i], pduop[i].ops);

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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
    pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
    pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
    pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
    pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
    pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
    pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
    pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
    pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
    pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
    pfn := PFNDUOP(pfn)(@pduop[5], pduop[5].ops);
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

    pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
    pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
    pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
    pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
    pfn := PFNDUOP(pfn)(@pduop[5], pduop[5].ops);
    pfn := PFNDUOP(pfn)(@pduop[6], pduop[6].ops);
    pfn := PFNDUOP(pfn)(@pduop[7], pduop[7].ops);
    pfn := PFNDUOP(pfn)(@pduop[8], pduop[8].ops);

    pduop := v_TS.pduopNext;
    if pduop <> nil then
    begin
      pfn := pduop[0].pfn;

      pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
      pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
      pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
      pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
      pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
      pfn := PFNDUOP(pfn)(@pduop[5], pduop[5].ops);
      pfn := PFNDUOP(pfn)(@pduop[6], pduop[6].ops);
      pfn := PFNDUOP(pfn)(@pduop[7], pduop[7].ops);
      pfn := PFNDUOP(pfn)(@pduop[8], pduop[8].ops);

      pduop := v_TS.pduopNext;
    end;
  end;
end;

// The M variant is the ideal case hard coded for the 18 DUOPs in the test.
// Padded with NOPs to possibly increase branch prediction rate.

(*
  NostradamusDistributor is an optimized dispatch mechanism,
  used for interpreting opcodes.

  Source : http://www.emulators.com/docs/nx25_nostradamus.htm

  if (pfn != (PFNDUOP)duopFireEscape)
  {
    pfn = (PFNDUOP)(*pfn)(&pduop[1], pduop[1].ops);

  Which results in this compiled code for each dispatch spanning 19-byte per dispatch:

    cmp eax, OFFSET FLAT duopFireEscape
    je $L62748
    mov edx, DWORD PTR [esi+28]
    lea ecx, DWORD PTR [esi+16]
    call eax

  In my translation to Delphi, the code reads :

  if (pfn <> @duopFireEscape) then
  begin
    pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);

  Delphi's default 'register' calling convention uses EAX as both first argument AND return value,
  which results in assembly that's one instruction longer than the above code :

    cmp ebx, $004a0d30 // = duopFireEscape
    jz $004a0e04
    lea eax, [esi+$20] // Delhi puts the first in EAX
    mov edx, [esi+$2c]
    call ebx
    mov ebx, eax // <- Resulting in this copy of EAX to EBX !

  Alas this cannot be avoided, making the Delphi version of the
  NostradamusDistributor a bit slower than it could have been.

  Techniques I tried :
  - Other Delphi calling convention : None use registers.
  - Force pfn into EAX by using Result : Inserted additional mov and xchg.
  - Untyped first argument : ?
*)
// The O variant is the general purpose case with expicit fire escape check.
// Should handle variable length blocks equally well.
procedure NostradamusDistributor(pduop: PDUOP_; count: Integer);
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
      pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
      if (pfn <> @duopFireEscape) then
      begin
        pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
        if (pfn <> @duopFireEscape) then
        begin
          pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
          if (pfn <> @duopFireEscape) then
          begin
            pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
            if (pfn <> @duopFireEscape) then
            begin
              pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
              if (pfn <> @duopFireEscape) then
              begin
                pfn := PFNDUOP(pfn)(@pduop[5], pduop[5].ops);
                if (pfn <> @duopFireEscape) then
                begin
                  pfn := PFNDUOP(pfn)(@pduop[6], pduop[6].ops);
                  if (pfn <> @duopFireEscape) then
                  begin
                    pfn := PFNDUOP(pfn)(@pduop[7], pduop[7].ops);
                    if (pfn <> @duopFireEscape) then
                    begin
                      pfn := PFNDUOP(pfn)(@pduop[8], pduop[8].ops);
                      if (pfn <> @duopFireEscape) then
                      begin
                        pfn := PFNDUOP(pfn)(@pduop[9], pduop[9].ops);
                        if (pfn <> @duopFireEscape) then
                        begin
                          pfn := PFNDUOP(pfn)(@pduop[10], pduop[10].ops);
                          if (pfn <> @duopFireEscape) then
                          begin
                            pfn := PFNDUOP(pfn)(@pduop[11], pduop[11].ops);
                            if (pfn <> @duopFireEscape) then
                            begin
                              pfn := PFNDUOP(pfn)(@pduop[12], pduop[12].ops);
                              if (pfn <> @duopFireEscape) then
                              begin
                                pfn := PFNDUOP(pfn)(@pduop[13], pduop[13].ops);
                                if (pfn <> @duopFireEscape) then
                                begin
                                  pfn := PFNDUOP(pfn)(@pduop[14], pduop[14].ops);
                                  if (pfn <> @duopFireEscape) then
                                  begin
                                    pfn := PFNDUOP(pfn)(@pduop[15], pduop[15].ops);
                                    if (pfn <> @duopFireEscape) then
                                    begin
                                      pfn := PFNDUOP(pfn)(@pduop[16], pduop[16].ops);
                                      if (pfn <> @duopFireEscape) then
                                      begin
                                        pfn := PFNDUOP(pfn)(@pduop[17], pduop[17].ops);
                                        if (pfn <> @duopFireEscape) then
                                        begin
                                          pfn := PFNDUOP(pfn)(@pduop[18], pduop[18].ops);
                                          if (pfn <> @duopFireEscape) then
                                          begin
                                            pfn := PFNDUOP(pfn)(@pduop[19], pduop[19].ops);
                                            if (pfn <> @duopFireEscape) then
                                            begin
                                              pfn := PFNDUOP(pfn)(@pduop[20], pduop[20].ops);
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
      pfn := PFNDUOP(pfn)(@pduop[0], pduop[0].ops);
      if (pfn <> @duopFireEscape) then
      begin
        pfn := PFNDUOP(pfn)(@pduop[1], pduop[1].ops);
        if (pfn <> @duopFireEscape) then
        begin
          pfn := PFNDUOP(pfn)(@pduop[2], pduop[2].ops);
          if (pfn <> @duopFireEscape) then
          begin
            pfn := PFNDUOP(pfn)(@pduop[3], pduop[3].ops);
            if (pfn <> @duopFireEscape) then
            begin
              pfn := PFNDUOP(pfn)(@pduop[4], pduop[4].ops);
              if (pfn <> @duopFireEscape) then
              begin
                pfn := PFNDUOP(pfn)(@pduop[5], pduop[5].ops);
                if (pfn <> @duopFireEscape) then
                begin
                  pfn := PFNDUOP(pfn)(@pduop[6], pduop[6].ops);
                  if (pfn <> @duopFireEscape) then
                  begin
                    pfn := PFNDUOP(pfn)(@pduop[7], pduop[7].ops);
                    if (pfn <> @duopFireEscape) then
                    begin
                      pfn := PFNDUOP(pfn)(@pduop[8], pduop[8].ops);
                      if (pfn <> @duopFireEscape) then
                      begin
                        pfn := PFNDUOP(pfn)(@pduop[9], pduop[9].ops);
                        if (pfn <> @duopFireEscape) then
                        begin
                          pfn := PFNDUOP(pfn)(@pduop[10], pduop[10].ops);
                          if (pfn <> @duopFireEscape) then
                          begin
                            pfn := PFNDUOP(pfn)(@pduop[11], pduop[11].ops);
                            if (pfn <> @duopFireEscape) then
                            begin
                              pfn := PFNDUOP(pfn)(@pduop[12], pduop[12].ops);
                              if (pfn <> @duopFireEscape) then
                              begin
                                pfn := PFNDUOP(pfn)(@pduop[13], pduop[13].ops);
                                if (pfn <> @duopFireEscape) then
                                begin
                                  pfn := PFNDUOP(pfn)(@pduop[14], pduop[14].ops);
                                  if (pfn <> @duopFireEscape) then
                                  begin
                                    pfn := PFNDUOP(pfn)(@pduop[15], pduop[15].ops);
                                    if (pfn <> @duopFireEscape) then
                                    begin
                                      pfn := PFNDUOP(pfn)(@pduop[16], pduop[16].ops);
                                      if (pfn <> @duopFireEscape) then
                                      begin
                                        pfn := PFNDUOP(pfn)(@pduop[17], pduop[17].ops);
                                        if (pfn <> @duopFireEscape) then
                                        begin
                                          pfn := PFNDUOP(pfn)(@pduop[18], pduop[18].ops);
                                          if (pfn <> @duopFireEscape) then
                                          begin
                                            pfn := PFNDUOP(pfn)(@pduop[19], pduop[19].ops);
                                            if (pfn <> @duopFireEscape) then
                                            begin
                                              pfn := PFNDUOP(pfn)(@pduop[20], pduop[20].ops);
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

(*
   sample_loop:
        mov eax,[ebp-8]
        add eax,1
        mov edx,eax
        mov [ebp-8],eax
        cmp edx,CINSTR
        jne sample_loop
*)

initialization

  test_dispatch_duop(@test_s86a_worker, 'x86 sim in C ver a');
  test_dispatch_duop(@test_s86b_worker, 'x86 sim in C ver b');
  test_dispatch_duop(@test_s86c_worker, 'x86 sim in C ver c');
  test_dispatch_duop(@test_s86d_worker, 'x86 sim in C ver d');
  test_dispatch_duop(@test_s86e_worker, 'x86 sim in C ver e');
  test_dispatch_duop(@test_s86f_worker, 'x86 sim in C ver f');
  test_dispatch_duop(@test_s86g_worker, 'x86 sim in C ver g');
  test_dispatch_duop(@test_s86h_worker, 'x86 sim in C ver h');
  test_dispatch_duop(@test_s86i_worker, 'x86 sim in C ver i');
  test_dispatch_duop(@test_s86j_worker, 'x86 sim in C ver j');
  test_dispatch_duop(@test_s86k_worker, 'x86 sim in C ver k');
  test_dispatch_duop(@test_s86l_worker, 'x86 sim in C ver l');
  test_dispatch_duop(@NostradamusDistributor, 'x86 sim in C ver o');

end.

(*
Debug Output (na vertalen orginele code) :
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

Debug Output (na verwijderen dubbele pduopNext assignments in duop* functies) :
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
*)

