#include "engine.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace armstudio;
using Clock = std::chrono::steady_clock;
static constexpr const char *fill = R"(mov x0, #2
svc #1
mov x10, #FRAMEBUFFER
mov x11, #76800
mov w12, #0x74e2bf
loop:
str w12, [x10], #4
subs x11, x11, #1
b.ne loop
hlt #0
)";
void bench(const char *name, const std::string &code, bool fast) {
    double samples[7], builds[7];
    uint64_t steps = 0;
    for (int n = 0; n < 7; n++) {
        Engine vm;
        auto t = Clock::now();
        auto err = vm.assemble(code);
        if (!err.empty())
            throw std::runtime_error(err);
        builds[n] = std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        t = Clock::now();
        while (!vm.halted) {
            vm.run(2147483647, false, !fast, fast);
            if (vm.paused)
                throw std::runtime_error(vm.status);
        }
        samples[n] = std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        if (vm.status.starts_with("Fault"))
            throw std::runtime_error(vm.status);
        if (std::string(name) == "solid-fill") {
            auto pixels = vm.pixels();
            if (!std::all_of(pixels.begin(), pixels.end(),
                             [](int32_t p) { return uint32_t(p) == 0xff74e2bf; }))
                throw std::runtime_error("Wrong fill pixels");
        } else {
            uint32_t c;
            std::memcpy(&c, vm.mem.data() + FB + (100 * 320 + 50) * 4, 4);
            if (c != 0x3264a0)
                throw std::runtime_error("Wrong color field pixel");
        }
        steps = vm.count;
    }
    std::sort(samples, samples + 7);
    std::sort(builds, builds + 7);
    printf("%s %s: execute median %.3f ms, min %.3f ms, assemble %.3f ms, %llu instructions\n",
           name, fast ? "fast" : "dispatch", samples[3], samples[0], builds[3],
           (unsigned long long)steps);
}
void loopBench(bool fast) {
    const std::string source =
        "mov x1,#1000000\nloop: add x0,x0,#1\nadd x2,x2,x0\nsubs x1,x1,#1\nb.ne loop\nhlt #0";
    double results[5];
    uint64_t steps = 0;
    for (int j = 0; j < 5; ++j) {
        Engine vm;
        auto error = vm.assemble(source);
        if (!error.empty())
            throw std::runtime_error(error);
        auto start = Clock::now();
        while (!vm.halted)
            vm.run(2147483647, false, true, fast);
        if (vm.ctx.x[0] != 1000000 || vm.ctx.x[2] != 500000500000ULL)
            throw std::runtime_error("Wrong loop result");
        results[j] = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        steps = vm.count;
    }
    std::sort(results, results + 5);
    printf("native-loop %s: %.3f ms, %.2f M guest instructions/sec, %llu instructions\n",
           fast ? "compiled" : "dispatch", results[2], steps / results[2] / 1000,
           (unsigned long long)steps);
}
int main() {
    loopBench(false);
    loopBench(true);
    bench("solid-fill", fill, false);
    bench("color-field", example(1), false);
    bench("solid-fill", fill, true);
    bench("color-field", example(1), true);
}
