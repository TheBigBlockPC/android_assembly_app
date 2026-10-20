#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>
namespace armstudio {
constexpr uint32_t RAM = 0x10000, RAM_END = 0xf0000, FB = 0x100000, FB_END = FB + 320 * 240 * 4,
                   INPUT = 0x150000, DEVICE_TABLE = 0x160000, MEM_END = 0x1400000;
struct Context {
    uint64_t x[31]{};
    uint64_t flags = 0;
    uint64_t d[32]{};
    uint64_t executed = 0, next = 0, last = 0, remaining = 0;
};
struct Instruction {
    enum Kind {
        Native,
        Move,
        Address,
        Load,
        Store,
        PairLoad,
        PairStore,
        Branch,
        Call,
        Return,
        BranchRegister,
        CallRegister,
        CompareBranch,
        TestBranch,
        Syscall,
        Halt,
        Nop,
        StackMath
    } kind = Nop;
    uint32_t opcode = 0xd503201f;
    int line = 0, rd = 0, rn = 0, rm = 0, width = 8, cond = -1, extra = 0;
    int64_t imm = 0;
    uint64_t target = 0;
    bool pre = false, post = false, sign = false;
    std::string text;
};
struct InstructionLocation {
    uint64_t address;
    int line;
    bool current, breakpoint;
    std::string text;
};
struct ManualSection {
    std::string title, body;
};
struct Device {
    uint32_t type, base, size = 4096, vram = 0;
    std::vector<float> depth;
    std::array<std::array<float, 16>, 3> matrices{};
    uint32_t texture = 0, textureWidth = 0, textureHeight = 0;
    std::vector<uint8_t> transfer;
    uint32_t destination = 0;
    std::chrono::steady_clock::time_point ready{};
};
class Engine {
  public:
    Context ctx{};
    uint64_t sp = RAM_END, pc = 0, count = 0;
    int mode = 0;
    bool halted = true, paused = false, yielded = false;
    std::string status = "Ready", output;
    std::vector<uint8_t> mem;
    std::vector<Instruction> code;
    std::set<int> breaks;
    Engine();
    ~Engine();
    std::string assemble(const std::string &, bool executable = true);
    void run(int budget, bool single = false, bool timeLimited = true, bool fast = false);
    int screenWidth = 320, screenHeight = 240;
    uint32_t displayBase = FB;
    std::vector<Device> devices;
    void configure(int width, int height, const std::vector<int> &types);
    void storage(const std::string &path);
    void touches(const std::vector<int> &points);
    std::string deviceDocumentation() const;
    void stop();
    void input(int key, bool down, int x, int y, bool touch);
    std::string registers() const;
    std::string memory(uint32_t addr) const;
    int currentLine() const;
    std::vector<int32_t> pixels() const;
    std::vector<int32_t> cells() const;
    std::vector<InstructionLocation> instructionWindow(bool lastExecuted = false) const;

  private:
    void *fastJit = nullptr;
    size_t fastSize = 0;
    std::vector<size_t> fastEntries;
    void compileFast();
    bool runFast(int budget);
    std::vector<uint8_t> nv = std::vector<uint8_t>(65536, 0);
    std::string nvPath;
    std::mt19937_64 rng{std::random_device{}()};
    void resetDevices();
    void serviceDevices();
    Device *deviceAt(uint64_t address);
    uint64_t deviceRead(Device &, uint32_t offset, int bytes);
    void deviceWrite(Device &, uint32_t offset);
    void renderTiles(Device &);
    void render3D(Device &);
    void *jit = nullptr;
    size_t jitSize = 0, stride = 0;
    int cursorX = 0, cursorY = 0, fg = 7, bg = 0;
    std::string escape;
    bool skipBreakpoint = false;
    uint64_t lastPc = UINT64_MAX;
    uint64_t readReg(int r, int width = 8) const;
    void writeReg(int r, uint64_t v, int width = 8);
    void check(uint64_t addr, int bytes, bool write) const;
    uint64_t read(uint64_t addr, int bytes) const;
    void write(uint64_t addr, uint64_t val, int bytes);
    void execute(const Instruction &);
    void syscall(int);
    void put(char);
    void release();
};
std::string instructionCatalog();
std::string documentation();
std::vector<ManualSection> manualSections();
std::string example(int index);
std::string deviceExample(int type);
std::string cubeExample();
std::string componentDocumentation();
} // namespace armstudio
