#include "engine.h"
namespace armstudio {
std::string componentDocumentation() {
    return R"DOC(MACHINE BUILDER

Open the menu → Machine builder. Default is 320×240 with a virtual keyboard. Phone is 320×640 with touch and no keyboard. Custom accepts multiples of 8 from 160–640 wide and 120–960 high. Select the components to attach. Applying a configuration resets execution, while keeping source. Each GPU has its own display memory; the last configured GPU is initially displayed. Writing a GPU mode or command selects its display. With no GPU, the console still works.

Export offers source (.s), machine (.armmachine), and bundled project (.armproject). Import accepts all three. Named projects include their machine. NVM is private device-local persistent state, excluded from exports. Copies of the same machine ID share its NVM on this phone.

DISCOVERY TABLE

DEVICE_TABLE = 0x00160000 is the only fixed discovery address. All values are little endian. Header: u32 magic 0x41524D33, u32 version 1, u32 count, u32 entry size 32. Entries start at +16. Each entry has eight u32 fields: type, instance ID, register base, register bytes, ABI version (1), screen width, screen height, reserved. Find a type, load its base, then use offsets below. Never hardcode component bases: removing components changes subsequent bases. DEVICE_TABLE is read-only. SCREEN_WIDTH, SCREEN_HEIGHT and FRAMEBUFFER are assembler constants for the configured display. Legacy INPUT aliases remain available for older programs.

Register writes are aligned 32-bit stores. NVM's data buffer also supports byte/halfword/64-bit stores. CPU instruction counts and debugging behave identically in every execution mode. Fast compiles register arithmetic and direct branches into native blocks, with checkpoints every 64 instructions or branch boundary and a maximum 1024 instructions before returning to C++. Memory, indirect branches, breakpoints and system calls keep checked dispatch. Native speed therefore depends on the workload; memory-heavy code still pays for VM checks.

1 · MINIMAL GPU

+0 status (2 done, 3 error), +4 command, +8 framebuffer address, +12 width, +16 height, +20 mode (0 text, 1 RGB565, 2 RGB888), +24 clear color. Write command 1 to clear to color and switch to RGB888; 2 to present/yield. Text cells are 8×8 pixels, four bytes per cell: ASCII, foreground, background, reserved. RGB888 pixels are u32 0x00RRGGBB. Legacy svc calls target the selected GPU. Display memory capacity is width×height×4 bytes.

2 · BASIC GPU · TILES & SPRITES

Includes the minimal GPU interface. Set +32 tile map pointer, +36 tile atlas pointer, +40 map width in tiles, +44 map height, +48 sprite pointer, +52 sprite count, +56 signed scroll X, +60 signed scroll Y. Command 3 renders tile map then sprites into RGB888. Tile map: u16 indices, row-major, up to 128×128 tiles, wrapping on both axes. Atlas: exactly 256 8×8 tiles of u32 ARGB pixels (65536 bytes). Sprite: four u32 fields (signed x, signed y, tile index, flags). Flags: bit 0 visible, bit 1 flip X, bit 2 flip Y. Up to 128 sprites; later sprites cover earlier ones, alpha zero is transparent and nonzero is opaque. A zero map size disables the map; clear color fills the background. Atlas, maps and sprites must remain valid mapped memory.

3 · ADVANCED GPU · COMMAND BUFFER

Includes minimal GPU registers. +32 command buffer pointer, +36 byte size. Write command 3 to execute. Buffer: 32-byte records, at most 8192 bytes (256 records). Eight u32 fields: opcode, a, b, c, d, e, f, g. Opcodes: 0 stop; 1 clear color=a and reset depth to infinity; 2 set matrix (a=0 model, 1 view, 2 projection; b=pointer to 16 float32 values); 3 bind texture (a=pixel pointer, b=width, c=height, each 1–256); 4 draw triangle (a=vertex pointer, b bit 0=textured). Unused fields are zero. Matrices are column-major and initially identity. Transform order: projection × view × model × position.

A triangle has three vertices, each 24 bytes: float32 x,y,z,u,v then u32 RGB color. Coordinates transform into clip space; screen coordinates come from perspective division with Y upward. NDC depth is -1..1, smaller values win in the depth buffer. Colored triangles interpolate vertex RGB. Textured triangles use perspective-correct UV, clamp to 0..1 and nearest-neighbor RGB sampling. Both windings render. Near-plane crossing triangles are discarded; this minimal renderer does not clip them. Depth persists until clear. Output is opaque. All guest pointers and command counts are validated.

4 · TIMER

Read u64 +0 for monotonic milliseconds since the host boot. Values are time, not executed instruction ticks. A 64-bit load gives a coherent snapshot.

5 · TOUCH INPUT CONTROLLER

Read u32 +0 X, +4 Y, +8 primary-down flag, +12 active contact count (0–10). Starting at +16 are ten 16-byte records: u32 pointer ID, X, Y, down. Coordinates use configured display pixels. Android contact IDs persist during a gesture. Primary input is also mirrored into the legacy INPUT registers.

6 · KEYBOARD INPUT CONTROLLER

Read u32 +0 key code, +4 down. ASCII keys plus 256 up, 257 down, 258 left, 259 right. The virtual keyboard appears only when this component is attached. Phone preset omits it.

7 · DMA CONTROLLER

Write u32 +0 source, +4 destination, +8 byte count, then +12=1 to submit. Read +16 status: 0 idle, 1 busy, 2 done, 3 invalid request. Copies complete asynchronously after at least 1 ms as the VM services devices; the guest can continue or poll. RAM and framebuffer buffers are accepted; overlap is safe, using a source snapshot. Size must be nonzero and at most 4 MiB, entirely within each mapped region. One request at a time. Descriptor/register memory cannot be a DMA buffer. Reset cancels pending copies.

8 · RNG

Read u64 +0 (or its low u32) to generate a fresh pseudo-random value. Seeded from the host random source. Intended for demos and games.

9 · NON VOLATILE MEMORY

64 KiB retained across assembly, resets and app restarts for each machine ID. Write u32 +0 byte address (64-byte aligned, 0–65472), then +4 command 1 to read or 2 to write a 64-byte block. +8 status: 2 success, 3 invalid address/command or storage failure. +64..127 is the 64-byte transfer buffer. Writes are atomically committed to app-private storage. Uninstalling the app removes this data. Use a bundled project to transfer machine configuration and code; it does not contain NVM contents.
)DOC";
}
std::string deviceExample(int type) {
    std::string body, data;
    switch (type) {
    case 1:
        body = R"(mov w0, #0
str w0, [x20, #20]
ldr w10, [x20, #8]
mov w11, #0x0a48
str w11, [x10]
mov w11, #0x0a69
str w11, [x10, #4]
)";
        break;
    case 2:
        body = R"(adr x0, tiles
str w0, [x20, #36]
mov x1, #64
mov w2, #0xff60d0b0
paint_tile:
str w2, [x0], #4
subs x1, x1, #1
b.ne paint_tile
adr x0, tilemap
str w0, [x20, #32]
mov w0, #1
str w0, [x20, #40]
str w0, [x20, #44]
adr x0, sprite
str w0, [x20, #48]
mov w0, #1
str w0, [x20, #52]
// Tile 1 is a contrasting sprite.
adr x0, tiles
add x0, x0, #256
mov x1, #64
mov w2, #0xffff6688
sprite_tile:
str w2, [x0], #4
subs x1, x1, #1
b.ne sprite_tile
mov w0, #3
str w0, [x20, #4]
)";
        data = "tiles: .space 65536\ntilemap: .hword 0\n.align 2\nsprite: .word 80, 80, 1, 1\n";
        break;
    case 3:
        body = R"(adr x0, commands
str w0, [x20, #32]
mov w0, #160
str w0, [x20, #36]
mov w0, #3
str w0, [x20, #4]
)";
        data = R"(commands:
.word 1, 0x132030, 0, 0, 0, 0, 0, 0
.word 2, 0, matrix, 0, 0, 0, 0, 0
.word 4, triangle, 0, 0, 0, 0, 0, 0
.word 3, texture, 2, 2, 0, 0, 0, 0
.word 4, textured, 1, 0, 0, 0, 0, 0
matrix:
.float 0.8, 0, 0, 0, 0, 0.8, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
triangle:
.float -0.9, -0.8, 0.3, 0, 0
.word 0xff6688
.float 0.9, -0.8, 0.3, 1, 0
.word 0x60dfb0
.float 0, 0.9, 0.3, 0.5, 1
.word 0x609aff
textured:
.float -0.45, -0.4, -0.3, 0, 0
.word 0xffffff
.float 0.6, -0.4, -0.3, 1, 0
.word 0xffffff
.float 0.1, 0.6, -0.3, 0.5, 1
.word 0xffffff
texture: .word 0xffee88, 0x4488ff, 0xff4488, 0x55ddaa
)";
        break;
    case 4:
        body = "ldr x0, [x20]\nmov x10, x0\n// Inspect x10: monotonic milliseconds.\n";
        break;
    case 5:
        body = R"(touch_loop:
ldr w10, [x20]
ldr w11, [x20, #4]
ldr w12, [x20, #8]
ldr w13, [x20, #12]
// Inspect x10..x13, or pause while touching the display.
mov x0, #2
svc #1
cbz w12, touch_present
mov x1, #SCREEN_WIDTH
madd x2, x11, x1, x10
lsl x2, x2, #2
mov x3, #FRAMEBUFFER
add x2, x2, x3
mov w4, #0x80e8bf
str w4, [x2]
touch_present:
svc #3
b touch_loop
)";
        break;
    case 6:
        body = R"(key_loop:
ldr w10, [x20]
ldr w11, [x20, #4]
cbz w11, key_present
cmp w10, #127
b.hi key_present
adr x0, character
strb w10, [x0]
mov x1, #1
svc #2
key_present:
svc #3
b key_loop
)";
        data = "character: .space 4\n";
        break;
    case 7:
        body = R"(adr x0, message
str w0, [x20]
adr x0, copied
str w0, [x20, #4]
mov w0, #12
str w0, [x20, #8]
mov w0, #1
str w0, [x20, #12]
wait_dma:
ldr w0, [x20, #16]
cmp w0, #1
b.eq wait_dma
adr x0, copied
mov x1, #12
svc #2
)";
        data = "message: .ascii \"DMA copied!\\n\"\ncopied: .space 12\n";
        break;
    case 8:
        body = R"(ldr x10, [x20]
ldr x11, [x20]
// Two independently generated values in x10 and x11.
mov x0, #2
svc #1
mov x0, x10
svc #4
)";
        break;
    case 9:
        body = R"(mov w0, #0
str w0, [x20]
mov w0, #1
str w0, [x20, #4]
ldrb w10, [x20, #64]
add w10, w10, #1
strb w10, [x20, #64]
mov w0, #2
str w0, [x20, #4]
// x10 is a persistent run counter (wraps at 256).
ldr w11, [x20, #8]
)";
        break;
    default:
        return "nop\n";
    }
    return "// Discover component type " + std::to_string(type) + R"( through DEVICE_TABLE.
// If absent, add it in Machine builder before running.
.text
_start:
mov w19, #)" +
           std::to_string(type) + R"(
bl discover
cbz x20, missing
)" + body + R"(
mov x0, #0
svc #0
discover:
mov x20, #DEVICE_TABLE
ldr w21, [x20, #8]
add x20, x20, #16
scan:
cbz w21, absent
ldr w22, [x20]
cmp w22, w19
b.eq found
add x20, x20, #32
sub w21, w21, #1
b scan
found:
ldr w20, [x20, #8]
ret
absent:
mov x20, #0
ret
missing:
adr x0, missing_message
mov x1, #0
svc #2
mov x0, #1
svc #0
.data
missing_message: .asciz "Add this component in Machine builder.\n"
)" + data;
}
std::string cubeExample() {
    return R"ASM(// Spinning cube - Advanced GPU command buffer demo.
// Twelve colored triangles, perspective projection and a depth buffer.
// The model rotates about X and Y. Pause to inspect its matrix.
.text
_start:
    // Discover the Advanced GPU (type 3); never hardcode its base.
    mov x20, #DEVICE_TABLE
    ldr w21, [x20, #8]
    add x20, x20, #16
find_gpu:
    cbz w21, missing_gpu
    ldr w22, [x20]
    cmp w22, #3
    b.eq gpu_found
    add x20, x20, #32
    sub w21, w21, #1
    b find_gpu
gpu_found:
    ldr w20, [x20, #8]
    adr x0, intro
    mov x1, #0
    svc #2
    adr x23, model
    adr x24, commands
    adr x25, rotation_steps
    adr x26, projection
    adr x0, initial_rotation
    ldr s0, [x0]          // cosine Y
    ldr s1, [x0, #4]      // sine Y
    ldr s2, [x0, #8]      // cosine X
    ldr s3, [x0, #12]     // sine X
    ldr s4, [x25]         // cos(0.025)
    ldr s5, [x25, #4]     // sin(0.025)
    ldr s6, [x25, #8]     // cos(0.017)
    ldr s7, [x25, #12]    // sin(0.017)

    // Keep square pixels and fit the cube in either screen orientation.
    mov x0, #SCREEN_WIDTH
    mov x1, #SCREEN_HEIGHT
    scvtf s8, x0
    scvtf s9, x1
    ldr s10, [x25, #16]   // cot(60-degree field of view / 2)
    cmp x0, x1
    b.gt landscape
    fdiv s8, s8, s9
    fmul s8, s8, s10
    str s10, [x26]
    str s8, [x26, #20]
    b projection_ready
landscape:
    fdiv s8, s9, s8
    fmul s8, s8, s10
    str s8, [x26]
    str s10, [x26, #20]
projection_ready:
    str w24, [x20, #32]   // command buffer address
    mov w0, #480          // clear + 2 matrices + 12 triangles
    str w0, [x20, #36]

frame:
    // Column-major model = translate(0,0,-4.5) * rotateY * rotateX.
    // Unchanged entries (zeros, translation and homogeneous 1) live in .data.
    str s0, [x23]
    fneg s8, s1
    str s8, [x23, #8]
    fmul s8, s1, s3
    str s8, [x23, #16]
    str s2, [x23, #20]
    fmul s8, s0, s3
    str s8, [x23, #24]
    fmul s8, s1, s2
    str s8, [x23, #32]
    fneg s8, s3
    str s8, [x23, #36]
    fmul s8, s0, s2
    str s8, [x23, #40]

    mov w0, #3
    str w0, [x20, #4]     // execute: clear depth, set matrices, draw cube
    svc #3                // present once per frame; yields to Android

    // Angle addition avoids needing sin/cos instructions in the guest ISA.
    fmul s8, s0, s4
    fmul s9, s1, s5
    fsub s8, s8, s9       // next cosine Y
    fmul s9, s1, s4
    fmul s10, s0, s5
    fadd s9, s9, s10      // next sine Y
    // Normalize the pair so long runs do not accumulate scale drift.
    fmul s10, s8, s8
    fmul s11, s9, s9
    fadd s10, s10, s11
    fsqrt s10, s10
    fdiv s0, s8, s10
    fdiv s1, s9, s10

    fmul s8, s2, s6
    fmul s9, s3, s7
    fsub s8, s8, s9       // next cosine X
    fmul s9, s3, s6
    fmul s10, s2, s7
    fadd s9, s9, s10      // next sine X
    fmul s10, s8, s8
    fmul s11, s9, s9
    fadd s10, s10, s11
    fsqrt s10, s10
    fdiv s2, s8, s10
    fdiv s3, s9, s10
    b frame

missing_gpu:
    adr x0, missing_message
    mov x1, #0
    svc #2
    mov x0, #1
    svc #0

.data
intro: .asciz "Spinning cube - Pause to inspect the model matrix.\n"
missing_message: .asciz "Attach an Advanced GPU in Machine builder.\n"
.align 2
initial_rotation: .float 0.921060994, 0.389418342, 0.968912422, -0.247403959
rotation_steps: .float 0.999687516, 0.024997396, 0.999855503, 0.016999181, 1.732050808
model:
    .float 1, 0, 0, 0
    .float 0, 1, 0, 0
    .float 0, 0, 1, 0
    .float 0, 0, -4.5, 1
// Perspective near=0.1, far=20; X/Y scales are set for this screen above.
projection:
    .float 1, 0, 0, 0
    .float 0, 1, 0, 0
    .float 0, 0, -1.010050251, -1
    .float 0, 0, -0.201005025, 0

// Commands are 32 bytes: opcode,a,b,c,d,e,f,g.
// Clear resets the depth buffer on EVERY frame to avoid trails.
commands:
    .word 1, 0x111c27, 0, 0, 0, 0, 0, 0
    .word 2, 0, model, 0, 0, 0, 0, 0
    .word 2, 2, projection, 0, 0, 0, 0, 0
    .word 4, cube+0, 0, 0, 0, 0, 0, 0
    .word 4, cube+72, 0, 0, 0, 0, 0, 0
    .word 4, cube+144, 0, 0, 0, 0, 0, 0
    .word 4, cube+216, 0, 0, 0, 0, 0, 0
    .word 4, cube+288, 0, 0, 0, 0, 0, 0
    .word 4, cube+360, 0, 0, 0, 0, 0, 0
    .word 4, cube+432, 0, 0, 0, 0, 0, 0
    .word 4, cube+504, 0, 0, 0, 0, 0, 0
    .word 4, cube+576, 0, 0, 0, 0, 0, 0
    .word 4, cube+648, 0, 0, 0, 0, 0, 0
    .word 4, cube+720, 0, 0, 0, 0, 0, 0
    .word 4, cube+792, 0, 0, 0, 0, 0, 0

// Each vertex: float x,y,z,u,v then word RGB color.
// Two triangles per face; depth resolves visibility in any draw order.
cube:
    // Front
    .float -1, -1, 1, 0, 0
    .word 0x63dec0
    .float 1, -1, 1, 0, 0
    .word 0x63dec0
    .float 1, 1, 1, 0, 0
    .word 0x63dec0
    .float -1, -1, 1, 0, 0
    .word 0x63dec0
    .float 1, 1, 1, 0, 0
    .word 0x63dec0
    .float -1, 1, 1, 0, 0
    .word 0x63dec0
    // Back
    .float 1, -1, -1, 0, 0
    .word 0x608eff
    .float -1, -1, -1, 0, 0
    .word 0x608eff
    .float -1, 1, -1, 0, 0
    .word 0x608eff
    .float 1, -1, -1, 0, 0
    .word 0x608eff
    .float -1, 1, -1, 0, 0
    .word 0x608eff
    .float 1, 1, -1, 0, 0
    .word 0x608eff
    // Left
    .float -1, -1, -1, 0, 0
    .word 0xb58bfa
    .float -1, -1, 1, 0, 0
    .word 0xb58bfa
    .float -1, 1, 1, 0, 0
    .word 0xb58bfa
    .float -1, -1, -1, 0, 0
    .word 0xb58bfa
    .float -1, 1, 1, 0, 0
    .word 0xb58bfa
    .float -1, 1, -1, 0, 0
    .word 0xb58bfa
    // Right
    .float 1, -1, 1, 0, 0
    .word 0xff7585
    .float 1, -1, -1, 0, 0
    .word 0xff7585
    .float 1, 1, -1, 0, 0
    .word 0xff7585
    .float 1, -1, 1, 0, 0
    .word 0xff7585
    .float 1, 1, -1, 0, 0
    .word 0xff7585
    .float 1, 1, 1, 0, 0
    .word 0xff7585
    // Top
    .float -1, 1, 1, 0, 0
    .word 0xffd875
    .float 1, 1, 1, 0, 0
    .word 0xffd875
    .float 1, 1, -1, 0, 0
    .word 0xffd875
    .float -1, 1, 1, 0, 0
    .word 0xffd875
    .float 1, 1, -1, 0, 0
    .word 0xffd875
    .float -1, 1, -1, 0, 0
    .word 0xffd875
    // Bottom
    .float -1, -1, -1, 0, 0
    .word 0x4dbfdf
    .float 1, -1, -1, 0, 0
    .word 0x4dbfdf
    .float 1, -1, 1, 0, 0
    .word 0x4dbfdf
    .float -1, -1, -1, 0, 0
    .word 0x4dbfdf
    .float 1, -1, 1, 0, 0
    .word 0x4dbfdf
    .float -1, -1, 1, 0, 0
    .word 0x4dbfdf
)ASM";
}
} // namespace armstudio
