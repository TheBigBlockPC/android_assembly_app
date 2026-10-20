#include "engine.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace armstudio;
int checks = 0;
void check(bool b, const std::string &label) {
    checks++;
    if (!b)
        throw std::runtime_error(label);
}
void compile(Engine &e, const std::string &s) {
    auto err = e.assemble(s);
    check(err.empty(), "Assemble: " + err + "\n" + s);
}
void finish(Engine &e, int limit = 1100000) {
    while (e.count < (uint64_t)limit && !e.halted && !e.paused)
        e.run(12000);
    check(e.halted && !e.status.starts_with("Fault"), "Finish: " + e.status);
}
void expectFault(const std::string &s) {
    Engine e;
    compile(e, s);
    e.run(100);
    check(e.halted && e.status.starts_with("Fault"), "Expected fault: " + e.status);
}
void reject(const std::string &s) {
    Engine e;
    check(!e.assemble(s).empty(), "Should reject: " + s);
}
int main() {
    try {
        Engine e;
        compile(e, "mov x0, #17\nmov x1, #5\nadd x2, x0, x1\nsub x3, x0, x1\nmul x4, x0, x1\nudiv "
                   "x5, x0, x1\nmov x6, #-17\nsdiv x7, x6, x1\nhlt #0");
        finish(e);
        check(e.ctx.x[2] == 22 && e.ctx.x[3] == 12 && e.ctx.x[4] == 85 && e.ctx.x[5] == 3 &&
                  int64_t(e.ctx.x[7]) == -3,
              "Native integer math");
        compile(e, "mov x0, #-1\nmov w0, #7\nmov x1, #0\nsub w2, w1, w0\nadds w3, w0, w2\ncset x4, "
                   "eq\nmov x18, #123\nadd x18, x18, #1\nmov x29, #19\nadd x29, x29, #2\nmov x30, "
                   "#29\nadd x30, x30, #2\nhlt #0");
        finish(e);
        check(e.ctx.x[0] == 7 && e.ctx.x[2] == 0xfffffff9ULL && e.ctx.x[3] == 0 && e.ctx.x[4] == 1,
              "W zero-extension and flags");
        check(e.ctx.x[18] == 124 && e.ctx.x[29] == 21 && e.ctx.x[30] == 31,
              "All guest registers including Android x18");
        compile(e, "mov x0, #0x1234\nmovk x0, #0xabcd, lsl #48\nmovz x1, #0x5678, lsl #32\nmovn "
                   "w2, #0\nmov x3, x0\nmov xzr, #123\nmov x4, xzr\nhlt #0");
        finish(e);
        check(e.ctx.x[0] == 0xabcd000000001234ULL && e.ctx.x[1] == 0x567800000000ULL &&
                  e.ctx.x[2] == 0xffffffff && e.ctx.x[3] == e.ctx.x[0] && e.ctx.x[4] == 0,
              "Wide moves and zero register");
        compile(e, "mov x0, #0xf0\nmov x1, #0x3c\nand x2, x0, x1\norr x3, x0, x1\neor x4, x0, "
                   "x1\nbic x5, x0, x1\nmvn w6, w0\nlsl x7, x0, #4\nlsr x8, x7, #4\nmov x9, "
                   "#-16\nasr x10, x9, #2\nror w11, w0, #4\nclz w12, w0\nrbit w13, w0\nrev w14, "
                   "w0\nmov x15, #4\nlsl x16, x0, x15\nlsr x17, x16, x15\nhlt #0");
        finish(e);
        check(e.ctx.x[2] == 0x30 && e.ctx.x[3] == 0xfc && e.ctx.x[4] == 0xcc &&
                  e.ctx.x[5] == 0xc0 && e.ctx.x[6] == 0xffffff0f,
              "Native logic");
        check(e.ctx.x[7] == 0xf00 && e.ctx.x[8] == 0xf0 && int64_t(e.ctx.x[10]) == -4 &&
                  e.ctx.x[11] == 15 && e.ctx.x[12] == 24 && e.ctx.x[13] == 0x0f000000 &&
                  e.ctx.x[14] == 0xf0000000 && e.ctx.x[16] == 0xf00 && e.ctx.x[17] == 0xf0,
              "Native shifts and reversal");
        compile(e,
                "mov x0, #5\nmov x1, #3\nmov x2, #20\nmadd x3, x0, x1, x2\nmsub x4, x0, x1, "
                "x2\nneg x5, x0\nneg w6, w0\ncmp x0, x1\ncsel x7, x0, x1, gt\ncset x8, lt\nhlt #0");
        finish(e);
        check(e.ctx.x[3] == 35 && e.ctx.x[4] == 5 && int64_t(e.ctx.x[5]) == -5 &&
                  e.ctx.x[6] == 0xfffffffb && e.ctx.x[7] == 5 && e.ctx.x[8] == 0,
              "Multiply-add, negate, conditions");
        compile(e, "mov x0, #0\nmov x1, #10\nloop:\nadd x0, x0, #1\nsubs x1, x1, #1\nb.ne loop\nbl "
                   "fn\nhlt #0\nfn:\nadd x0, x0, #1\nret");
        finish(e);
        check(e.ctx.x[0] == 11, "Branch, flags, call and return");
        compile(e,
                "mov x0, #0\ncbz x0, a\nmov x0, #99\na:\nmov x1, #8\ntbnz x1, #3, b\nmov x0, "
                "#99\nb:\ntbz x1, #2, c\nmov x0, #99\nc:\nadr x2, d\nbr x2\nmov x0, #99\nd:\nret");
        finish(e);
        check(e.ctx.x[0] == 0, "Compare/test and register branches");
        compile(e,
                "mov x10, #0x10000\nmov x0, #-128\nstrb w0, [x10]\nldrsb x1, [x10]\nldrb w2, "
                "[x10]\nmov w3, #0x1234\nstrh w3, [x10, #1]\nldrh w4, [x10, #1]\nstr x0, [x10, "
                "#8]!\nldr x5, [x10], #8\nstp x1, x4, [sp, #-16]!\nldp x6, x7, [sp], #16\nhlt #0");
        finish(e);
        check(int64_t(e.ctx.x[1]) == -128 && e.ctx.x[2] == 128 && e.ctx.x[4] == 0x1234 &&
                  int64_t(e.ctx.x[5]) == -128 && e.ctx.x[6] == e.ctx.x[1] && e.ctx.x[7] == 0x1234 &&
                  e.sp == RAM_END && e.ctx.x[10] == 0x10010,
              "Checked memory, extension, indexing, stack pairs");
        compile(e, "adr x10, values\nldr d0, [x10]\nldr d1, [x10, #8]\nfadd d2, d0, d1\nfsub d3, "
                   "d0, d1\nfmul d4, d0, d1\nfdiv d5, d0, d1\nfsqrt d6, d0\nfneg d7, d1\nfabs d8, "
                   "d7\nfmov d9, d8\nfcmp d9, d1\ncset x0, eq\nfcvtzs x1, d2\nmov x2, #16\nscvtf "
                   "s10, x2\nfsqrt s11, s10\nfcvtzu w3, s11\nstr d2, [x10, #16]\nldr d12, [x10, "
                   "#16]\nhlt #0\n.data\nvalues: .double 9.0, 3.0\n.space 8");
        finish(e);
        double d[32];
        std::memcpy(d, e.ctx.d, sizeof(d));
        check(d[2] == 12 && d[3] == 6 && d[4] == 27 && d[5] == 3 && d[6] == 3 && d[7] == -3 &&
                  d[8] == 3 && d[9] == 3 && d[12] == 12 && e.ctx.x[0] == 1 && e.ctx.x[1] == 12 &&
                  e.ctx.x[3] == 4,
              "Native scalar floating point");
        for (int n = 0; n < 6; n++) {
            compile(e, example(n));
            if (n == 5) {
                e.run(250000, false, false);
                bool ball = false, backdrop = false;
                for (int32_t p : e.pixels()) {
                    if (uint32_t(p) == 0xffff6688)
                        ball = true;
                    if (uint32_t(p) == 0xff203040)
                        backdrop = true;
                }
                check(ball && backdrop && !e.paused && !e.halted,
                      "Animation draws its square and keeps running");
                while (!e.halted && !e.paused && e.count < 50000000)
                    e.run(2147483647, false, true);
                check(!e.halted && !e.paused && e.ctx.x[2] <= 292 && e.ctx.x[3] <= 212,
                      "Animation bounces off every wall past 50M instructions");
            } else if (n == 2) {
                e.input(0, false, 101, 80, true);
                e.run(100);
                uint32_t pixel = 0;
                std::memcpy(&pixel, e.mem.data() + FB + (80 * 320 + 101) * 4, 4);
                check(pixel == 0x74e2bf, "Touch paints expected pixel");
            } else if (n == 3) {
                e.run(100);
                e.input('a', true, 0, 0, false);
                e.run(100);
                check(e.output.ends_with("a"), "Keyboard echo");
            } else {
                finish(e);
                if (n == 0)
                    check(e.output.find("Hello, ARM64!") != std::string::npos &&
                              (e.cells()[0] & 255) == 'H',
                          "Hello output and ANSI cells");
                if (n == 1)
                    check((uint32_t)e.pixels()[100 * 320 + 50] == 0xff3264a0, "Color field output");
                if (n == 4)
                    check(e.ctx.x[2] == 18 && e.sp == RAM_END, "Float and stack example");
            }
        }
        compile(e, "mov x0, #1\nsvc #1\nmov x0, #0xf800\nsvc #4\nsvc #3\nhlt #0");
        finish(e);
        check(uint32_t(e.pixels()[123]) == 0xffff0000, "RGB565 clear and conversion");
        compile(e, "mov x0, #1\nadd x0, x0, #1\nadd x0, x0, #1\nhlt #0");
        e.breaks = {2};
        e.run(100);
        check(e.paused && e.currentLine() == 2 && e.ctx.x[0] == 1, "Breakpoint before instruction");
        e.run(1, true);
        check(e.ctx.x[0] == 2 && e.currentLine() == 3, "Step one instruction");
        e.run(100);
        check(e.halted && e.ctx.x[0] == 3, "Resume after step");
        e.breaks.clear();
        expectFault("mov x0, #0xffffffffffffffff\nldr x1, [x0]");
        expectFault("mov x0, #0\nstr x1, [x0]");
        expectFault("mov x0, #0x150000\nstr w1, [x0]");
        expectFault("mov x0, #0xeffff\nstr x1, [x0]");
        expectFault("mov x0, #0xf0000\nldr x1, [x0]");
        expectFault("mov x0, #0x14afff\nstr w1, [x0]");
        expectFault("mov x0, #1\nbr x0");
        expectFault("mov x0, #3\nsvc #1");
        expectFault("mov x0, #0\nmov x1, #32769\nsvc #2");
        reject("svc #64");
        reject(".inst 0xd4000001");
        reject("mrs x0, tpidr_el0");
        reject("ldr x0, [x1, x2]");
        reject("add x0, x1, #4096");
        reject("lsl w0, w1, #32");
        reject("movk x0, #65536");
        reject("mov x31, #1");
        reject("str x0, [x1, #0]!, #8");
        reject("b nowhere");
        reject(".data\n.space 9999999999999");
        reject("mov x0, #1\n.data\nvalue: .ascii \"abc");
        // V2: the displayed source location must follow branches and preserve stopped/fault state.
        e.breaks.clear();
        compile(e, ".text\n_start:\nmov x0, #1\nb target\nnop\nnop\nnop\nnop\nnop\ntarget:\nadd "
                   "x0, x0, #1\nhlt #0\nnop");
        auto window = e.instructionWindow();
        check(window.size() == 7 && window[0].current && window[0].line == 3 &&
                  window[0].address == 0,
              "Context starts at the actual entry instruction");
        e.run(1);
        window = e.instructionWindow(true);
        check(window[0].current && window[0].line == 3, "Slow mode shows just-executed source");
        e.run(1);
        window = e.instructionWindow(true);
        check(window[1].current && window[1].text == "b target",
              "Slow context identifies executed branch, not target");
        e.stop();
        window = e.instructionWindow(true);
        int current = 0;
        for (const auto &row : window)
            if (row.current)
                current = row.line;
        check(current == 11 && window.size() == 7,
              "Pause switches context to the next instruction");
        e.breaks = {11};
        e.run(1);
        window = e.instructionWindow();
        bool breakpoint = false;
        for (const auto &row : window)
            if (row.current)
                breakpoint = row.line == 11 && row.breakpoint;
        check(e.paused && breakpoint && e.count == 2, "Context marks breakpoint before execution");
        e.run(1, true);
        e.run(1, true);
        window = e.instructionWindow();
        current = 0;
        for (const auto &row : window)
            if (row.current)
                current = row.line;
        check(e.halted && current == 12, "Halt in middle of code retains halted instruction");
        e.breaks.clear();
        compile(e, "mov x0, #0\nstr x1, [x0]\nnop");
        e.run(100);
        window = e.instructionWindow(true);
        check(e.status.starts_with("Fault") && window[1].current && window[1].line == 2,
              "Fault context remains at the failing instruction, not the previous instruction");
        compile(e, "nop");
        e.run(1);
        window = e.instructionWindow();
        check(e.halted && window.size() == 1 && window[0].current,
              "End-of-code context keeps last instruction");
        compile(e, "nop\nnop");
        check(e.instructionWindow(true)[0].current && e.count == 0,
              "Reset clears execution history");

        const std::string paced =
            "mov x0, #0\nmov x1, #1500\nloop: add x0, x0, #1\nsubs x1, x1, #1\nb.ne loop\nhlt #0";
        for (int budget : {1, 12000, 2147483647}) {
            compile(e, paced);
            while (!e.halted)
                e.run(budget);
            check(e.ctx.x[0] == 1500 && e.count == 4503,
                  "Slow/default/fast batches preserve identical results and ticks");
        }
        compile(e, paced);
        while (!e.halted)
            e.run(2147483647, false, false);
        check(e.ctx.x[0] == 1500 && e.count == 4503,
              "Untimed batches preserve identical results and ticks");
        compile(e, "loop: b loop");
        e.run(1000001, false, false);
        check(e.count == 1000001 && !e.paused && !e.halted,
              "Loops run past one million instructions until explicitly stopped");
        compile(e, "svc #3\nmov x0, #42\nhlt #0");
        e.run(2147483647);
        check(e.yielded && e.count == 1 && e.ctx.x[0] == 0, "Fast batches honor explicit present");

        // Fast execution must match exact dispatch, including flags, every register and ticks.
        const std::vector<std::string> fastPrograms = {
            paced,
            "mov x16, #0x123456789abcdef0\nmov x17, #-1\nmov x18, #444\nmov x29, #555\nmov x30, "
            "#666\nmov x1, #10000\nloop: add x0, x0, #3\nsubs x1, x1, #1\nb.ne loop\ncset x2, "
            "eq\nhlt #0",
            "mov x0, #9\nscvtf d0, x0\nfsqrt d1, d0\nfcmp d1, d1\nb.ne fail\nfcvtzs x1, d1\ncbz "
            "x1, fail\ntbnz x1, #0, ok\nfail: mov x2, #99\nok: tbz x1, #2, done\nmov x2, "
            "#999\ndone: hlt #0",
            "mov w0, #-1\ncbnz w0, one\nmov x1, #99\none: tbnz w0, #31, two\nmov x1, #88\ntwo: tbz "
            "x0, #63, done\nmov x1, #77\ndone: hlt #0",
            "mov x10, #0x10000\nmov x0, #100\nloop: str x0, [x10]\nsub x0, x0, #1\ncbnz x0, "
            "loop\nldr x1, [x10]\nbl fn\nhlt #0\nfn: add x1, x1, #42\nret",
            "mov x0, #7\nsvc #5\nmov x2, #4\nsvc #3\nadd x0, x0, x2\nret",
            "mov x0, #42\nmov x1, #0\nstr x0, [x1]\nnop",
            "loop: b loop"};
        for (const auto &program : fastPrograms)
            for (int budget : {1, 2, 3, 7, 64, 1024, 12000}) {
                Engine slow, fast;
                compile(slow, program);
                compile(fast, program);
                int total = program == fastPrograms.back() ? 3 : 100000;
                for (int n = 0; n < total && !slow.halted; ++n) {
                    slow.run(budget, false, false);
                    fast.run(budget, false, false, true);
                    check(slow.pc == fast.pc && slow.count == fast.count && slow.sp == fast.sp &&
                              slow.ctx.flags == fast.ctx.flags &&
                              std::memcmp(slow.ctx.x, fast.ctx.x, sizeof(slow.ctx.x)) == 0 &&
                              std::memcmp(slow.ctx.d, fast.ctx.d, sizeof(slow.ctx.d)) == 0 &&
                              slow.halted == fast.halted && slow.yielded == fast.yielded,
                          "Compiled blocks preserve exact state at each budget boundary");
                    if (slow.status.starts_with("Fault"))
                        check(fast.status == slow.status, "Precise fast fault line");
                    if (slow.yielded)
                        break;
                }
                check(slow.mem == fast.mem, "Fast mode preserves memory and instruction ticks");
            }
        compile(e, "mov x0, #1\nloop: add x0, x0, #1\nb loop");
        e.breaks = {2};
        e.run(1024, false, false, true);
        check(e.paused && e.count == 1 && e.ctx.x[0] == 1, "Fast breakpoints before execution");
        e.run(1024, false, false, true);
        check(e.paused && e.count == 3 && e.ctx.x[0] == 2, "Fast breakpoint skip exactly once");
        e.breaks.clear();

        for (int type = 1; type <= 9; ++type) {
            e.configure(320, 240, type == 1 ? std::vector<int>{1} : std::vector<int>{1, type});
            compile(e, deviceExample(type));
            e.input('z', true, 80, 60, true);
            for (int j = 0; j < 1000 && !e.halted; ++j) {
                e.run(10000, false, true, true);
                if (e.yielded && (type == 5 || type == 6))
                    break;
            }
            check(!e.status.starts_with("Fault"),
                  "Component demo " + std::to_string(type) + ": " + e.status);
            if (type != 5 && type != 6)
                check(e.halted, "Finite component demo completes");
            if (type == 1)
                check((e.cells()[0] & 255) == 'H' && (e.cells()[1] & 255) == 'i',
                      "Minimal GPU text");
            if (type == 2)
                check(uint32_t(e.pixels()[0]) == 0xff60d0b0 &&
                          uint32_t(e.pixels()[80 * 320 + 80]) == 0xffff6688,
                      "Tile and sprite layers");
            if (type == 3) {
                auto pixels = e.pixels();
                check(uint32_t(pixels[0]) == 0xff132030 &&
                          uint32_t(pixels[120 * 320 + 160]) != 0xff132030,
                      "3D triangles render");
                check(std::isfinite(e.devices.back().depth[120 * 320 + 160]),
                      "3D depth buffer populated");
            }
            if (type == 4)
                check(e.ctx.x[10] > 0, "Timer returns milliseconds");
            if (type == 5)
                check(e.ctx.x[10] == 80 && e.ctx.x[11] == 60 && e.ctx.x[12] == 1,
                      "Discoverable touch input");
            if (type == 6)
                check(e.output == "z", "Discoverable keyboard input");
            if (type == 7)
                check(e.output == "DMA copied!\n", "Asynchronous DMA copied source");
            if (type == 8)
                check(e.ctx.x[10] != e.ctx.x[11], "RNG produces changing values");
            if (type == 9) {
                auto before = e.ctx.x[10];
                compile(e, deviceExample(9));
                finish(e);
                check(e.ctx.x[10] == ((before + 1) & 255) && e.ctx.x[11] == 2,
                      "NVM survives assembly reset");
            }
        }
        e.configure(320, 640, {1, 4, 5});
        compile(e, "mov x0, #0\nsvc #1\nadr x0, msg\nmov x1, #0\nsvc #2\nhlt #0\n.data\nmsg: "
                   ".asciz \"Phone\"");
        finish(e);
        check(e.cells().size() == 3200 && (e.cells()[0] & 255) == 'P', "Tall phone text display");
        e.input(0, false, 319, 639, true);
        e.touches({3, 10, 600, 7, 319, 639});
        uint32_t touchBase = e.devices.back().base, contacts;
        std::memcpy(&contacts, e.mem.data() + touchBase + 12, 4);
        check(contacts == 2 && e.mem[touchBase + 16] == 3 && e.mem[touchBase + 32] == 7,
              "Multitouch IDs and records");
        e.configure(640, 960, {3});
        compile(e, deviceExample(3));
        finish(e);
        check(e.pixels().size() == 640 * 960, "Maximum custom display renders");
        bool invalid = false;
        try {
            e.configure(319, 240, {1});
        } catch (...) {
            invalid = true;
        }
        check(invalid, "Reject invalid display dimensions");
        invalid = false;
        try {
            e.configure(320, 240, {1, 1});
        } catch (...) {
            invalid = true;
        }
        check(invalid, "Reject duplicate components");
        e.configure(320, 240, {4});
        uint32_t timerBase = e.devices[0].base;
        e.configure(320, 240, {1, 4});
        check(e.devices[1].base != timerBase, "Bases move when machine components change");
        compile(e, "mov x0, #DEVICE_TABLE\nstr w1, [x0]");
        e.run(100, false, false, true);
        check(e.status.starts_with("Fault"), "Descriptor table is read-only");
        compile(e, "mov x0, #" + std::to_string(e.devices[1].base) + "\nstr w1, [x0]");
        e.run(100);
        check(e.status.starts_with("Fault"), "Input/timer registers are read-only");
        e.configure(320, 240, {1, 7});
        compile(e, "mov x20, #" + std::to_string(e.devices[1].base) +
                       "\nmov w0, #0\nstr w0, [x20]\nstr w0, [x20,#4]\nmov w0, #64\nstr w0, "
                       "[x20,#8]\nmov w0,#1\nstr w0,[x20,#12]\nldr w10,[x20,#16]\nhlt #0");
        finish(e);
        check(e.ctx.x[10] == 3, "DMA rejects unmapped buffers without copying");
        e.configure(320, 240, {1, 9});
        compile(e, "mov x20, #" + std::to_string(e.devices[1].base) +
                       "\nmov w0,#65535\nstr w0,[x20]\nmov w0,#1\nstr w0,[x20,#4]\nldr "
                       "w10,[x20,#8]\nhlt #0");
        finish(e);
        check(e.ctx.x[10] == 3, "NVM rejects invalid block address");

        e.configure(320, 240, {3});
        compile(e, deviceExample(3));
        finish(e);
        std::set<uint32_t> textureColors;
        for (uint32_t color : e.pixels())
            if (color == 0xffffee88 || color == 0xff4488ff || color == 0xffff4488 ||
                color == 0xff55ddaa)
                textureColors.insert(color);
        check(textureColors.size() >= 3,
              "Textured triangles sample multiple nearest texture texels");
        std::string depthProgram = "mov x20,#" + std::to_string(e.devices[0].base) + R"(
adr x0, commands
str w0,[x20,#32]
mov w0,#96
str w0,[x20,#36]
mov w0,#3
str w0,[x20,#4]
hlt #0
.data
commands:
.word 1,0,0,0,0,0,0,0
.word 4,near_triangle,0,0,0,0,0,0
.word 4,far_triangle,0,0,0,0,0,0
near_triangle:
.float -1,-1,-0.5,0,0
.word 0xff0000
.float 1,-1,-0.5,0,0
.word 0xff0000
.float 0,1,-0.5,0,0
.word 0xff0000
far_triangle:
.float -1,-1,0.5,0,0
.word 0x0000ff
.float 1,-1,0.5,0,0
.word 0x0000ff
.float 0,1,0.5,0,0
.word 0x0000ff
)";
        compile(e, depthProgram);
        finish(e);
        check(uint32_t(e.pixels()[120 * 320 + 160]) == 0xffff0000 &&
                  e.devices[0].depth[120 * 320 + 160] == -0.5f,
              "Depth buffer rejects later polygons behind visible geometry");
        const std::string nvFile = "/data/local/tmp/armstudio-v3-nvm-regression.bin";
        std::remove(nvFile.c_str());
        {
            Engine writer;
            writer.configure(320, 240, {1, 9});
            writer.storage(nvFile);
            compile(writer, deviceExample(9));
            finish(writer);
            check(writer.ctx.x[10] == 1 && writer.ctx.x[11] == 2, "NVM commits a block to disk");
        }
        {
            Engine reader;
            reader.configure(320, 240, {1, 9});
            reader.storage(nvFile);
            compile(reader, deviceExample(9));
            finish(reader);
            check(reader.ctx.x[10] == 2 && reader.ctx.x[11] == 2,
                  "NVM reloads committed state in a new engine");
        }
        std::remove(nvFile.c_str());
        // Max-size programs and long direct branches remain legal in Fast mode.
        std::string large = "mov x0,#1\nb.al last\nfirst: nop\n";
        for (int j = 0; j < 8187; ++j)
            large += "b.ne first\n";
        large += "last: mov x1,#42\nhlt #0\n";
        compile(e, large);
        e.run(100, false, false, true);
        check(e.halted && e.ctx.x[1] == 42 && e.count == 4,
              "Long native branch spans and b.al preserve results");

        // The cube stays animated and visible through a full Y rotation, without matrix drift.
        e.configure(320, 240, {3});
        compile(e, example(15));
        std::vector<int32_t> firstCubeFrame;
        for (int frame = 0; frame < 260; ++frame) {
            e.run(12000, false, false, true);
            check(!e.halted && e.yielded && !e.status.starts_with("Fault"),
                  "Spinning cube renders and yields every frame");
            auto pixels = e.pixels();
            size_t colored = 0;
            for (uint32_t pixel : pixels)
                if (pixel != 0xff111c27)
                    ++colored;
            check(colored > 1000 && colored < 320 * 240 / 2,
                  "Rotating cube remains visible and fits the display");
            if (frame == 0)
                firstCubeFrame = pixels;
            if (frame == 60)
                check(pixels != firstCubeFrame, "Cube rotation visibly changes the frame");
        }
        const auto modelValue = [&](int index) {
            float value;
            std::memcpy(&value, e.mem.data() + e.ctx.x[23] + index * 4, 4);
            return double(value);
        };
        for (int col = 0; col < 3; ++col) {
            double length = 0;
            for (int row = 0; row < 3; ++row)
                length += std::pow(modelValue(col * 4 + row), 2);
            check(std::abs(length - 1) < 1e-5, "Cube rotation preserves unit scale over long runs");
        }
        check(modelValue(14) == -4.5 && std::isfinite(e.devices[0].depth[120 * 320 + 160]),
              "Cube keeps its camera distance and writes visible depth");
        auto pausedCount = e.count;
        e.stop();
        check(e.paused && e.count == pausedCount, "Cube can be paused between frames");
        e.configure(320, 640, {3});
        compile(e, example(15));
        e.run(12000, false, false, true);
        check(e.yielded && !e.halted && e.pixels().size() == 320 * 640,
              "Cube also renders on the tall Phone display");
        float projectionX, projectionY;
        std::memcpy(&projectionX, e.mem.data() + e.ctx.x[26], 4);
        std::memcpy(&projectionY, e.mem.data() + e.ctx.x[26] + 20, 4);
        check(std::abs(projectionX * 320 - projectionY * 640) < 1e-3,
              "Cube projection preserves square pixels on tall displays");
        e.configure(320, 240, {1});
        compile(e, example(15));
        finish(e);
        check(e.output.find("Attach an Advanced GPU") != std::string::npos,
              "Cube gives an actionable message when its GPU is absent");

        auto sections = manualSections();
        check(sections.size() == 12, "Manual exposes all twelve topics");
        std::set<std::string> titles;
        for (const auto &section : sections) {
            check(titles.insert(section.title).second && !section.body.empty(),
                  "Unique nonempty manual topics");
            check(documentation().find(section.body) != std::string::npos,
                  "Manual content remains intact");
        }
        check(sections[4].title == "Memory map" &&
                  sections[4].body.find("0x00150020") != std::string::npos,
              "Complete memory map retained");
        check(sections[6].title == "VM calls" &&
                  sections[6].body.find("#5  TICKS") != std::string::npos,
              "All custom calls retained");
        // Every reference template should assemble after placeholder labels are supplied.
        std::istringstream lines(instructionCatalog());
        std::string row;
        while (std::getline(lines, row)) {
            std::vector<std::string> parts;
            size_t at = 0;
            for (int j = 0; j < 4; j++) {
                size_t next = row.find('|', at);
                parts.push_back(row.substr(at, next - at));
                at = next + 1;
            }
            compile(e,
                    parts[3] + "\nloop:\ndone:\nfunction:\nnop\n.data\nmessage: .asciz \"test\"");
        }
        std::cout << "PASS: " << checks << " checks on native ARM64\n";
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "FAIL after " << checks << " checks: " << ex.what() << "\n";
        return 1;
    }
}
