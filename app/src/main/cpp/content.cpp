#include "engine.h"
namespace armstudio {
std::string instructionCatalog() {
    return R"DOC(mov|Data|int32, int64|mov x0, #42|Copy a register or load a constant. Full-width immediates are an assembler pseudo-instruction.
movz|Data|int32, int64|movz x0, #65535, lsl #16|Load a 16-bit field and zero the other bits.
movk|Data|int32, int64|movk x0, #4660, lsl #16|Replace a 16-bit field, preserving the other bits.
movn|Data|int32, int64|movn x0, #0|Load the bitwise inverse of a shifted 16-bit field.
adr|Data|int64|adr x0, message|Load a virtual label address.
adrp|Data|int64|adrp x0, message|Load the 4096-byte page address of a label.
ldr|Memory|int32, int64, float32, float64|ldr x0, [x1, #8]|Read a checked virtual address. ldr x0, =label loads an address. Pre/post-index supported.
str|Memory|int32, int64, float32, float64|str w0, [x1, #4]|Write RAM or framebuffer. Unmapped/read-only addresses fault safely.
ldrb|Memory|int32|ldrb w0, [x1]|Load one unsigned byte into a W register.
strb|Memory|int32|strb w0, [x1]|Store the low byte of a W register.
ldrh|Memory|int32|ldrh w0, [x1]|Load an unsigned 16-bit halfword.
strh|Memory|int32|strh w0, [x1]|Store a 16-bit halfword. Useful for RGB565 pixels.
ldrsb|Memory|int32, int64|ldrsb x0, [x1]|Load and sign-extend one byte.
ldrsh|Memory|int32, int64|ldrsh x0, [x1]|Load and sign-extend a halfword.
ldrsw|Memory|int64|ldrsw x0, [x1]|Load and sign-extend a 32-bit word.
ldp|Memory|int32, int64|ldp x0, x1, [sp], #16|Load two same-width integer registers. SP and writeback supported.
stp|Memory|int32, int64|stp x29, x30, [sp, #-16]!|Store a pair of integer registers.
add|Math|int32, int64|add x0, x1, x2|Add registers or an unsigned 12-bit immediate. SP is supported with immediate operands.
adds|Math|int32, int64|adds x0, x1, #1|Add and update NZCV flags.
sub|Math|int32, int64|sub x0, x1, #1|Subtract a register or 0–4095 immediate.
subs|Math|int32, int64|subs x0, x0, #1|Subtract and update NZCV flags.
mul|Math|int32, int64|mul x0, x1, x2|Multiply; retain the low register-width bits.
madd|Math|int32, int64|madd x0, x1, x2, x3|x0 = x1 × x2 + x3.
msub|Math|int32, int64|msub x0, x1, x2, x3|x0 = x3 − x1 × x2.
udiv|Math|int32, int64|udiv x0, x1, x2|Unsigned division. ARM64 returns zero for division by zero.
sdiv|Math|int32, int64|sdiv x0, x1, x2|Signed division; quotient truncates toward zero.
neg|Math|int32, int64|neg x0, x1|Two's complement negation.
negs|Math|int32, int64|negs x0, x1|Negate and update NZCV.
lsl|Bits|int32, int64|lsl x0, x1, #2|Logical left shift, by immediate or same-width register.
lsr|Bits|int32, int64|lsr x0, x1, #2|Logical right shift.
asr|Bits|int32, int64|asr x0, x1, #2|Arithmetic right shift with sign extension.
ror|Bits|int32, int64|ror x0, x1, #8|Rotate right by immediate or register.
and|Bits|int32, int64|and x0, x1, x2|Bitwise AND. Register operands only.
ands|Bits|int32, int64|ands x0, x1, x2|AND and set NZCV.
orr|Bits|int32, int64|orr x0, x1, x2|Bitwise OR. Register operands only.
eor|Bits|int32, int64|eor x0, x1, x2|Bitwise exclusive OR.
bic|Bits|int32, int64|bic x0, x1, x2|Clear the bits set in x2 from x1.
mvn|Bits|int32, int64|mvn x0, x1|Bitwise NOT.
clz|Bits|int32, int64|clz x0, x1|Count leading zero bits.
rbit|Bits|int32, int64|rbit x0, x1|Reverse bit order.
rev|Bits|int32, int64|rev x0, x1|Reverse byte order within a register.
cmp|Comparison|int32, int64|cmp x0, #10|Set NZCV as a subtraction; discard the result.
cmn|Comparison|int32, int64|cmn x0, x1|Set NZCV as an addition; discard the result.
tst|Comparison|int32, int64|tst x0, x1|Set flags from bitwise AND.
csel|Comparison|int32, int64|csel x0, x1, x2, eq|Select x1 if the condition holds, else x2.
cset|Comparison|int32, int64|cset w0, eq|Write 1 when the condition holds, else 0.
b|Branching|control|b loop|Jump to a code label.
b.eq|Branching|control|b.eq done|Branch if equal (Z=1).
b.ne|Branching|control|b.ne loop|Branch if unequal (Z=0).
b.lt|Branching|control|b.lt loop|Signed less-than. Also b.le, b.gt, b.ge.
b.le|Branching|control|b.le loop|Signed less-than or equal.
b.gt|Branching|control|b.gt loop|Signed greater-than.
b.ge|Branching|control|b.ge done|Signed greater-than or equal.
b.hi|Branching|control|b.hi done|Unsigned greater-than. Also b.ls, b.hs/cs, b.lo/cc.
b.ls|Branching|control|b.ls loop|Unsigned less-than or equal.
b.hs|Branching|control|b.hs done|Unsigned greater-than or equal; alias b.cs.
b.lo|Branching|control|b.lo loop|Unsigned less-than; alias b.cc.
b.cs|Branching|control|b.cs done|Carry set.
b.cc|Branching|control|b.cc done|Carry clear.
b.mi|Branching|control|b.mi done|Negative.
b.pl|Branching|control|b.pl done|Non-negative.
b.vs|Branching|control|b.vs done|Overflow set.
b.vc|Branching|control|b.vc done|Overflow clear.
b.al|Branching|control|b.al done|Always branch.
bl|Branching|control|bl function|Call a label; write the return address to x30.
ret|Branching|control|ret|Return through x30, or an explicit X register. Top-level ret exits.
br|Branching|control|br x0|Branch to a checked virtual code address.
blr|Branching|control|blr x0|Call a checked virtual code address.
cbz|Branching|int32, int64|cbz x0, done|Branch when the register is zero.
cbnz|Branching|int32, int64|cbnz x0, loop|Branch when the register is nonzero.
tbz|Branching|int32, int64|tbz x0, #3, done|Branch if a bit is zero.
tbnz|Branching|int32, int64|tbnz x0, #3, done|Branch if a bit is one.
fadd|Floating point|float32, float64|fadd d0, d1, d2|Native IEEE-754 addition. Use S registers for float32.
fsub|Floating point|float32, float64|fsub d0, d1, d2|Floating-point subtraction.
fmul|Floating point|float32, float64|fmul d0, d1, d2|Floating-point multiplication.
fdiv|Floating point|float32, float64|fdiv d0, d1, d2|Floating-point division.
fmov|Floating point|float32, float64|fmov d0, d1|Copy same-width floating-point registers. Constants can be loaded from .float/.double data.
fneg|Floating point|float32, float64|fneg d0, d1|Negate a floating-point value.
fabs|Floating point|float32, float64|fabs d0, d1|Absolute value.
fsqrt|Floating point|float32, float64|fsqrt d0, d1|Native square root.
fcmp|Comparison|float32, float64|fcmp d0, d1|Compare floating-point registers and set NZCV.
scvtf|Floating point|int32, int64, float32, float64|scvtf d0, x0|Convert signed integer to floating point.
ucvtf|Floating point|int32, int64, float32, float64|ucvtf d0, x0|Convert unsigned integer to floating point.
fcvtzs|Floating point|int32, int64, float32, float64|fcvtzs x0, d0|Convert floating point to signed integer, toward zero.
fcvtzu|Floating point|int32, int64, float32, float64|fcvtzu x0, d0|Convert floating point to unsigned integer, toward zero.
svc|System|control|svc #2|Custom VM call only: 0 exit, 1 mode, 2 text, 3 present, 4 clear, 5 ticks.
hlt|System|control|hlt #0|Stop the program safely.
brk|System|control|brk #0|Stop the program safely.
nop|System|control|nop|No operation.)DOC";
}
std::string documentation() {
    return R"DOC(ARM STUDIO / FIELD GUIDE

A small, native ARM64 workbench.
Write a program, tap Assemble, then Run. Tap the editor's line-number gutter to toggle a breakpoint. Step executes one instruction. Run resumes a paused program; Reset assembles a fresh state. Your current draft is saved automatically, and Projects stores named programs. Export writes a .s file using Android's file picker.

EXECUTION MODES

Slow: execute one instruction at a time at 1–30 instructions per second. Select Slow above the workspace and drag the speed slider. Run opens Inspect, where the highlighted row shows the last executed instruction while running. Pausing highlights the next instruction instead. Speed changes take effect while running; Android scheduling can make the actual rate slightly lower.

Default: the original execution behavior, up to 12000 instructions per batch followed by a 16 ms delay. Each batch also has an approximately 8 ms time budget. Use this for interactive framebuffer programs.

Fast: execute continuous batches at the maximum available VM throughput, with no deliberate delay between ordinary batches. The UI is sampled about 30 times per second. Each batch retains the 8 ms time budget so input and Pause remain responsive. An explicit svc #3 still presents and waits for the next frame.

All modes preserve breakpoints and checked memory. The machine runs indefinitely until you pause it, the program exits with a syscall, or a fault occurs. Switching modes or adjusting the slider preserves registers, memory, and instruction count. Step always executes exactly one instruction. Mode and slow-speed preferences are saved on this device.

EXECUTION MODEL

This is a documented ARM64 subset, not a Linux environment or a general-purpose ELF loader. The instruction library is the exact supported surface. Unsupported instructions and directives are rejected. There is no network, filesystem, OS syscall, or raw machine-code directive available to guest programs.

Arithmetic, bit operations, comparisons, and scalar floating-point instructions execute as real ARM64 instructions on your phone. The C++ dispatcher checks and handles guest memory, branches, large immediate/address pseudo-instructions, SP arithmetic, and VM calls. Native snippets are generated from an allowlist of encodings, with host state preserved; guest addresses never become host pointers. Generated code changes from writable to read/execute before use. Guest code cannot modify it.

Execution yields to Android between bounded batches in every speed mode. The machine runs indefinitely until you pause it, the program exits with a syscall, or a fault occurs. Out-of-bounds reads/writes, bad branch addresses, invalid calls, and assembly errors become visible diagnostics. This is an in-process educational sandbox, not a security boundary for adversarial binaries.

REGISTERS & STARTUP

x0–x30: 64-bit integers; w0–w30 alias their low 32 bits. W writes clear the upper half. xzr/wzr always read zero and discard writes. lr aliases x30; fp aliases x29. SP is a separate virtual stack pointer initialized to 0x000F0000. It works in checked memory addressing and 64-bit add/sub immediate operations. Stack grows down into RAM. d0–d31 and s0–s31 provide scalar float64/float32. NZCV starts at zero. FP uses round-to-nearest with traps disabled and restores the host FP environment after each instruction.

Registers and memory start at zero except SP and x30. x30 initially points just past the code, allowing top-level ret to finish. Entry is _start when defined, otherwise the first instruction. PC is a virtual byte address; each parsed instruction occupies four bytes, including pseudo-instructions.

COMPLETE MEMORY MAP · LITTLE ENDIAN

0x00000000–0x00007FFF  Code capacity (8192 instructions). Only assembled instruction bytes are readable. Writes are forbidden. Native ALU bytes are shown; dispatched instructions use a NOP marker.
0x00008000–0x0000FFFF  Unmapped.
0x00010000–0x000EFFFF  RAM / data / descending stack, 896 KiB. .data begins at 0x00010000. .bss and .rodata also use writable RAM in this VM.
0x000F0000–0x000FFFFF  Unmapped stack guard.
0x00100000–0x0014AFFF  Framebuffer, 307200 bytes. Read/write. All modes use the same base.
0x0014B000–0x0014FFFF  Unmapped.
0x00150000–0x0015001F  Read-only input/tick registers, detailed below.
0x00150020 onward      Unmapped.

All accesses must fit wholly inside one mapped region. Unaligned transfers are supported. Memory panel shows -- for unmapped bytes. Initial RAM and unused framebuffer bytes are zero.

INPUT REGISTERS

0x00150000  KEY          u32, most recent virtual key: ASCII, 8 backspace, 10 enter, 27 escape; arrows 256 up / 257 down / 258 left / 259 right.
0x00150004  KEY_DOWN     u32, 1 while that key is held, otherwise 0. One active key.
0x00150008  TOUCH_X      u32, 0–319.
0x0015000C  TOUCH_Y      u32, 0–239.
0x00150010  TOUCH_DOWN   u32, 1 while the primary pointer touches the framebuffer.
0x00150014  Reserved     u32, always zero.
0x00150018  TICKS        u64, completed instruction count, updated after each instruction.

KEY, KEY_DOWN, TOUCH_X, TOUCH_Y, TOUCH_DOWN, TICKS, and FRAMEBUFFER are built-in address constants. Input is live and thus depends on your interaction; ticks are deterministic instruction counts, not wall-clock time. Extra touch pointers are ignored.

VM CALLS · svc #number

#0  EXIT: halt with x0 as exit code.
#1  SET MODE: x0 = 0 text, 1 RGB565, 2 RGB888 in 32 bits. Clears framebuffer and text cursor.
#2  PRINT: x0 = virtual string address; x1 = byte count, or zero for NUL termination. Maximum 32768 bytes. Writes the bounded console transcript and, in text mode, framebuffer cells.
#3  PRESENT: yield until the next UI frame. Framebuffer is also refreshed between execution batches. No arguments.
#4  CLEAR: x0 = packed pixel color for RGB modes. In text mode clears cells and resets cursor.
#5  TICKS: return the current instruction count in x0 (before this call completes).

No real Linux svc is executed. Only #0–#5 are accepted. Host clock, files, sockets, process creation, and native external functions are inaccessible.

FRAMEBUFFER FORMATS

320 × 240 fixed pixels, scaled to fit the display with no filtering. Touch coordinates use these original pixels.

Text (mode 0): 40 columns × 30 rows, each cell 8 × 8 pixels. Four bytes per cell: ASCII character, foreground palette index, background palette index, reserved. Address = FRAMEBUFFER + (row × 40 + column) × 4. Bytes beyond 4800 are unused in this mode. Direct cell writes and svc #2 share this buffer. Newline, carriage return, tab, wrapping, and scrolling are supported.

ANSI: ESC[m resets, ESC[30m–37m / 90m–97m set foreground, ESC[40m–47m / 100m–107m set background, ESC[39m / 49m restore defaults, ESC[2J clears, ESC[row;colH positions the cursor (1-based). Use \e or \x1b in strings. Palette: black, red, green, yellow, blue, magenta, cyan, white; indices 8–15 are bright versions. Text is ASCII; UTF-8 glyph shaping is not supported in the VM terminal.

RGB565 (mode 1): one little-endian halfword per pixel. Bits 15–11 red, 10–5 green, 4–0 blue. Address = FRAMEBUFFER + (y × 320 + x) × 2. Example red: 0xF800.

32-bit RGB (mode 2): one little-endian word per pixel, 0x00RRGGBB. The high byte is ignored; output is opaque. Address = FRAMEBUFFER + (y × 320 + x) × 4. Example mint: 0x0074E2BF.

ASSEMBLER SYNTAX

Case-insensitive instruction names. Use lowercase registers. Labels are case-sensitive. Comments begin with // or ;. Immediates may use #, decimal, negative values, or 0x hex. Simple label + constant and label - constant expressions are supported; general expressions/macros are not.

.text, .data, .rodata, .bss, .section, .global/.globl, .equ/.set NAME, value.
Data directives: .byte, .hword/.short, .word, .quad, .float, .double, .ascii, .asciz/.string, .space/.zero. .align uses a power-of-two exponent; .balign uses a byte alignment (power of two ≤4096). Alignment is for data sections. .equ values must reference already-defined symbols. String escapes: \n \r \t \0 \e \xNN \\ \".

Memory forms: [x1], [x1, #8], [x1, #-8]!, [x1], #8; SP can be the base. Register-offset/extended addressing, SIMD vectors, atomics, system registers, logical immediates, shifted arithmetic operands, and arbitrary .inst encodings are not implemented. The library gives valid templates for every supported mnemonic.

EXAMPLES

Hello world: ANSI colors and a custom print call.
Color field: native integer math writes all 76800 RGB pixels.
Touch canvas: poll single-pointer input and paint while touching.
Keyboard echo: poll the on-screen keys and print ASCII.
Float & stack: native float math, conversions, a function call, and stack pairs.
Animation: a bouncing square — each frame erases the old square and draws the new one.

DEBUGGING

Inspect shows up to seven assembled instructions around the highlighted row, with source line numbers, virtual addresses, and breakpoint dots. Nearby rows are source context, not a history of executed branches. While Slow mode runs, the highlight follows the instruction just executed. At a pause or breakpoint it marks the next instruction, before execution; a fault marks the failing instruction. On completion it keeps the last executed instruction visible. Pausing from the execution controls and hitting a breakpoint open Inspect automatically. Editing hides this context until the new source is assembled.

Breakpoint dots bind to source line numbers; edits clear them to avoid stopping on a different statement. A breakpoint pauses before execution. Run skips the current breakpoint once, then observes subsequent visits. Step ignores breakpoints for one instruction. Registers and memory stay inspectable after faults. Editing stops the current program and marks it for reassembly. Build errors are shown below the editor and the relevant source line is underlined in red.

Font sizes for editor and docs are independent under Settings. App colors follow Android's light/dark setting. No account or internet permission is needed.
)DOC";
}
std::vector<ManualSection> manualSections() {
    const std::vector<std::pair<std::string, std::string>> headings = {
        {"ARM STUDIO / FIELD GUIDE", "Getting started"},
        {"EXECUTION MODES", "Execution modes"},
        {"EXECUTION MODEL", "Execution model"},
        {"REGISTERS & STARTUP", "Registers & startup"},
        {"COMPLETE MEMORY MAP · LITTLE ENDIAN", "Memory map"},
        {"INPUT REGISTERS", "Input registers"},
        {"VM CALLS · svc #number", "VM calls"},
        {"FRAMEBUFFER FORMATS", "Framebuffer formats"},
        {"ASSEMBLER SYNTAX", "Assembler syntax"},
        {"EXAMPLES", "Examples"},
        {"DEBUGGING", "Debugging"}};
    const auto all = documentation();
    std::vector<ManualSection> sections;
    for (size_t n = 0; n < headings.size(); ++n) {
        const auto start = all.find(headings[n].first + "\n\n");
        if (start == std::string::npos)
            continue;
        const auto from = start + headings[n].first.size() + 2;
        const auto end =
            n + 1 == headings.size() ? all.size() : all.find(headings[n + 1].first + "\n\n", from);
        auto body = all.substr(from, end == std::string::npos ? end : end - from);
        const auto last = body.find_last_not_of("\r\n ");
        if (last != std::string::npos)
            body.resize(last + 1);
        sections.push_back({headings[n].second, body});
    }
    return sections;
}
std::string example(int index) {
    switch (index) {
    case 1:
        return R"ASM(// Color field · RGB888 in 32 bits
.text
_start:
    mov x0, #2
    svc #1
    mov x10, #FRAMEBUFFER
    mov w11, #320
    mov w12, #240
    mov w2, #0
row:
    mov w1, #0
pixel:
    lsl w3, w1, #16
    lsl w4, w2, #8
    orr w3, w3, w4
    mov w5, #160
    orr w3, w3, w5
    str w3, [x10], #4
    add w1, w1, #1
    cmp w1, w11
    b.lt pixel
    add w2, w2, #1
    cmp w2, w12
    b.lt row
    svc #3
    mov x0, #0
    svc #0
)ASM";
    case 2:
        return R"ASM(// Touch canvas · hold a finger to paint
.text
_start:
    mov x0, #2
    svc #1
    mov x0, #0x10251F
    svc #4
    mov x10, #FRAMEBUFFER
    mov x11, #TOUCH_X
    mov w12, #320
    mov w13, #0x74E2BF
frame:
    ldr w0, [x11, #8]
    cbz w0, present
    ldr w1, [x11]
    ldr w2, [x11, #4]
    madd w3, w2, w12, w1
    lsl x3, x3, #2
    add x4, x10, x3
    str w13, [x4]
present:
    svc #3
    b frame
)ASM";
    case 3:
        return R"ASM(// Keyboard echo · tap the virtual keys
.text
_start:
    adr x0, intro
    mov x1, #0
    svc #2
    mov x10, #KEY
    mov w11, #0
poll:
    ldr w2, [x10, #4]
    cbz w2, released
    cbnz w11, present
    mov w11, #1
    ldr w3, [x10]
    cmp w3, #127
    b.hi present
    adr x0, buffer
    strb w3, [x0]
    mov x1, #1
    svc #2
    b present
released:
    mov w11, #0
present:
    svc #3
    b poll
.data
intro: .asciz "\e[36mType on the virtual keyboard.\e[0m\n"
buffer: .space 4
)ASM";
    case 4:
        return R"ASM(// Float & stack · Step and inspect registers
.text
_start:
    mov x0, #9
    scvtf d0, x0
    fsqrt d1, d0
    fmul d2, d1, d1
    fcvtzs x1, d2
    bl double_it
    mov x0, #0
    svc #0
double_it:
    stp x29, x30, [sp, #-16]!
    add x2, x1, x1
    ldp x29, x30, [sp], #16
    ret
)ASM";
    case 5:
        return R"ASM(// Bouncing square · runs forever, Pause to stop
// Fill the screen once, then each frame erase the old square and draw
// the new one, so the ball is always on the display.
.text
_start:
    mov x0, #2
    svc #1
    mov x10, #FRAMEBUFFER
    mov w2, #50
    mov w3, #60
    mov w4, #3
    mov w5, #2
    mov w6, #28
    mov w7, #320
    mov w8, #292
    mov w9, #0x00ff6688
    mov w13, #0x00203040
    mov x11, x10
    mov w12, #76800
fill:
    str w13, [x11], #4
    subs w12, w12, #1
    b.ne fill
frame:
    mov w12, #0
erow:
    mov w14, #0
ecol:
    add w16, w3, w12
    madd w17, w16, w7, w2
    add w17, w17, w14
    lsl x17, x17, #2
    add x17, x17, x10
    str w13, [x17]
    add w14, w14, #1
    cmp w14, w6
    b.lt ecol
    add w12, w12, #1
    cmp w12, w6
    b.lt erow
    add w2, w2, w4
    cmp w2, #0
    b.gt ok_xl
    mov w2, #0
    neg w4, w4
ok_xl:
    add w14, w2, w6
    cmp w14, w7
    b.ls ok_xr
    mov w2, w8
    neg w4, w4
ok_xr:
    add w3, w3, w5
    cmp w3, #0
    b.gt ok_yl
    mov w3, #0
    neg w5, w5
ok_yl:
    add w14, w3, w6
    cmp w14, #240
    b.ls ok_yr
    mov w3, #212
    neg w5, w5
ok_yr:
    mov w12, #0
row:
    mov w14, #0
col:
    add w16, w3, w12
    madd w17, w16, w7, w2
    add w17, w17, w14
    lsl x17, x17, #2
    add x17, x17, x10
    str w9, [x17]
    add w14, w14, #1
    cmp w14, w6
    b.lt col
    add w12, w12, #1
    cmp w12, w6
    b.lt row
    svc #3
    b frame
)ASM";
    default:
        return R"ASM(// Hello, ARM64.
// Tap Assemble, then Run. No Linux required.
.text
.global _start
_start:
    mov x0, #0          // text mode
    svc #1
    adr x0, message
    mov x1, #0          // NUL-terminated
    svc #2              // VM print
    mov x0, #0
    svc #0              // exit

.data
message:
    .asciz "\e[92mHello, ARM64!\e[0m\n\nYour code. Your little machine.\n"
)ASM";
    }
}
} // namespace armstudio
