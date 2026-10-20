#include "engine.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
namespace armstudio {
extern "C" const char native_template_start[], native_template_op[], native_template_end[];
extern "C" const char fast_checkpoint_start[], fast_checkpoint_pc[], fast_checkpoint_length[],
    fast_checkpoint_exit[], fast_checkpoint_add[], fast_checkpoint_last[], fast_checkpoint_end[];
static_assert(offsetof(Context, executed) == 512 && offsetof(Context, remaining) == 536);
// Only validated register instructions enter this graph. Memory and VM calls return to C++.
void Engine::compileFast() {
    const size_t n = code.size();
    const auto supported = [](const Instruction &i) {
        return i.kind == Instruction::Native || i.kind == Instruction::Move ||
               i.kind == Instruction::Address || i.kind == Instruction::Nop ||
               i.kind == Instruction::Branch || i.kind == Instruction::CompareBranch ||
               i.kind == Instruction::TestBranch || i.kind == Instruction::Call;
    };
    const auto branch = [](const Instruction &i) {
        return i.kind == Instruction::Branch || i.kind == Instruction::CompareBranch ||
               i.kind == Instruction::TestBranch || i.kind == Instruction::Call;
    };
    std::set<size_t> starts{0, n};
    for (size_t j = 0; j < n; ++j) {
        if (!supported(code[j]) || branch(code[j]))
            starts.insert(j + 1);
        if (!supported(code[j]))
            starts.insert(j);
        if (branch(code[j]))
            starts.insert(code[j].target / 4);
    }
    // Bound native work and permit entry after stepping into a block.
    for (size_t j = 0; j < n; j += 64)
        starts.insert(j);
    struct Block {
        size_t start, end, body = 0, entry = SIZE_MAX;
    };
    std::vector<Block> blocks;
    for (auto a = starts.begin(), b = std::next(a); b != starts.end(); ++a, ++b)
        blocks.push_back({*a, *b});
    blocks.push_back({n, n});
    std::vector<uint32_t> words;
    const auto copy = [&](const char *a, const char *b) {
        size_t at = words.size();
        words.resize(at + (b - a) / 4);
        std::memcpy(words.data() + at, a, b - a);
        return at;
    };
    struct Fix {
        size_t at, target;
        uint32_t op;
        bool conditional;
    };
    std::vector<Fix> fixes;
    const size_t exitTarget = SIZE_MAX;
    const auto jump = [&](size_t target, uint32_t op = 0x14000000, bool cond = false) {
        fixes.push_back({words.size(), target, op, cond});
        words.push_back(0);
    };
    const auto constant = [&](int rd, uint64_t value, int width) {
        for (int part = 0; part < (width == 4 ? 2 : 4); ++part)
            words.push_back((width == 4 ? 0x52800000U : 0xd2800000U) | (part ? 0x20000000U : 0) |
                            (part << 21) | (((value >> (part * 16)) & 65535) << 5) | rd);
    };
    fastEntries.assign(n, SIZE_MAX);
    for (auto &b : blocks) {
        if (b.start == n || !supported(code[b.start]))
            continue;
        b.entry = words.size();
        copy(native_template_start, native_template_op);
        jump(b.start);
        fastEntries[b.start] = b.entry * 4;
    }
    for (auto &b : blocks) {
        b.body = words.size();
        if (b.start == n || !supported(code[b.start])) {
            // No guest register is touched on this exit path.
            words.push_back(0xa90c47f0); // stp x16, x17, [sp, #192]
            words.push_back(0xd53b4210); // mrs x16, nzcv
            words.push_back(0xf9005bf0); // str x16, [sp, #176]
            constant(16, b.start * 4, 8);
            words.push_back(0xf90077f0); // str x16, [sp, #232]
            jump(exitTarget);
            continue;
        }
        const size_t at = copy(fast_checkpoint_start, fast_checkpoint_end);
        const auto patch = [&](const char *label, uint32_t word) {
            words[at + (label - fast_checkpoint_start) / 4] = word;
        };
        patch(fast_checkpoint_pc, 0xd2800010 | (b.start * 4 << 5));
        patch(fast_checkpoint_length, 0xf1000210 | ((b.end - b.start) << 10));
        patch(fast_checkpoint_add, 0x91000210 | ((b.end - b.start) << 10));
        patch(fast_checkpoint_last, 0xd2800010 | ((b.end - 1) * 4 << 5));
        fixes.push_back({at + (fast_checkpoint_exit - fast_checkpoint_start) / 4 + 1, exitTarget,
                         0x14000000, false});
        for (size_t j = b.start; j < b.end; ++j) {
            const auto &i = code[j];
            switch (i.kind) {
            case Instruction::Native:
                words.push_back(i.opcode);
                break;
            case Instruction::Move:
            case Instruction::Address:
                constant(i.rd, i.target, i.width);
                break;
            case Instruction::Call:
                constant(30, j * 4 + 4, 8);
                jump(i.target / 4);
                break;
            case Instruction::Branch:
                if (i.cond >= 0 && i.cond != 14)
                    words.push_back(0x54000000 | (2 << 5) | (i.cond ^ 1));
                jump(i.target / 4);
                break;
            case Instruction::CompareBranch:
                words.push_back((i.width == 8 ? 0xb4000000U : 0x34000000U) | ((!i.extra) << 24) |
                                (2 << 5) | i.rn);
                jump(i.target / 4);
                break;
            case Instruction::TestBranch: {
                // TBZ's 14-bit displacement could overflow for a large program. Test locally,
                // then use the 26-bit B to reach the validated target.
                uint32_t bit = i.imm;
                words.push_back(0x36000000U | ((bit >> 5) << 31) | ((bit & 31) << 19) |
                                ((!i.extra) << 24) | (2 << 5) | i.rn);
                jump(i.target / 4);
                break;
            }
            default:
                break;
            }
        }
        const auto &last = code[b.end - 1];
        if (last.kind != Instruction::Call && !(last.kind == Instruction::Branch && last.cond < 0))
            jump(b.end);
    }
    const size_t exit = words.size();
    words.push_back(0xf9405bf0); // ldr x16, [sp, #176]
    words.push_back(0xd51b4210); // msr nzcv, x16
    words.push_back(0xa94c47f0); // ldp x16, x17, [sp, #192]
    copy(native_template_op + 4, native_template_end);
    for (const auto &f : fixes) {
        auto it = std::lower_bound(blocks.begin(), blocks.end(), f.target,
                                   [](const Block &b, size_t target) { return b.start < target; });
        const size_t target = f.target == exitTarget ? exit : it->body;
        int64_t delta = int64_t(target) - int64_t(f.at);
        if (f.conditional && (delta < -(1 << 18) || delta >= (1 << 18)))
            throw std::runtime_error("Native branch span exceeded");
        words[f.at] =
            f.op | (f.conditional ? (uint32_t(delta) & 0x7ffff) << 5 : uint32_t(delta) & 0x3ffffff);
    }
    size_t page = sysconf(_SC_PAGESIZE);
    fastSize = (words.size() * 4 + page - 1) & ~(page - 1);
    fastJit = mmap(nullptr, fastSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fastJit == MAP_FAILED) {
        fastJit = nullptr;
        throw std::runtime_error("Fast compiler allocation failed");
    }
    std::memcpy(fastJit, words.data(), words.size() * 4);
    __builtin___clear_cache((char *)fastJit, (char *)fastJit + fastSize);
    if (mprotect(fastJit, fastSize, PROT_READ | PROT_EXEC))
        throw std::runtime_error("Fast compiler protection failed");
}
bool Engine::runFast(int budget) {
    if (!fastJit || pc % 4 || pc / 4 >= fastEntries.size() || fastEntries[pc / 4] == SIZE_MAX)
        return false;
    ctx.remaining = budget;
    ctx.executed = 0;
    ctx.next = pc;
    ctx.last = lastPc;
    reinterpret_cast<void (*)(Context *)>((char *)fastJit + fastEntries[pc / 4])(&ctx);
    if (!ctx.executed)
        return false;
    pc = ctx.next;
    lastPc = ctx.last;
    count += ctx.executed;
    std::memcpy(mem.data() + INPUT + 24, &count, 8);
    return true;
}
} // namespace armstudio
