#include "engine.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
namespace armstudio {
extern "C" const char native_template_start[], native_template_op[], native_template_end[];
static std::string trim(std::string s) {
    auto a = s.find_first_not_of(" \t\r\n");
    return a == s.npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
static std::string lower(std::string s) {
    for (auto &c : s)
        c = std::tolower((unsigned char)c);
    return s;
}
static void require(bool yes, const std::string &why) {
    if (!yes)
        throw std::runtime_error(why);
}
static std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> v;
    std::string t;
    int depth = 0;
    bool quote = false, esc = false;
    for (char c : s) {
        if (c == '"' && !esc)
            quote = !quote;
        if (!quote) {
            if (c == '[')
                depth++;
            if (c == ']')
                depth--;
        }
        if (c == ',' && !depth && !quote) {
            v.push_back(trim(t));
            t.clear();
        } else
            t += c;
        esc = c == '\\' && !esc;
    }
    if (!trim(t).empty())
        v.push_back(trim(t));
    return v;
}
static std::string uncomment(std::string s) {
    bool q = false, e = false;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '"' && !e)
            q = !q;
        if (!q && (c == ';' || (c == '/' && i + 1 < s.size() && s[i + 1] == '/')))
            return trim(s.substr(0, i));
        e = c == '\\' && !e;
    }
    return trim(s);
}
static uint64_t number(std::string s, const std::map<std::string, uint64_t> &labels) {
    s = trim(s);
    require(s.size() <= 1024, "Expression exceeds 1024 characters");
    if (!s.empty() && (s[0] == '#' || s[0] == '='))
        s = trim(s.substr(1));
    auto it = labels.find(s);
    if (it != labels.end())
        return it->second;
    auto p = s.find_first_of("+-", 1);
    if (p != s.npos) {
        uint64_t a = number(s.substr(0, p), labels), b = number(s.substr(p + 1), labels);
        return s[p] == '+' ? a + b : a - b;
    }
    if (s.size() == 3 && s.front() == '\'' && s.back() == '\'')
        return (uint8_t)s[1];
    try {
        size_t n = 0;
        auto v = std::stoull(s, &n, 0);
        require(n == s.size(), "Invalid number or label: " + s);
        return v;
    } catch (...) {
        throw std::runtime_error("Invalid number or label: " + s);
    }
}
static int reg(std::string s, int *width = nullptr, bool allowSP = false) {
    s = lower(trim(s));
    if (s == "sp" || s == "wsp") {
        require(allowSP, "SP is only supported in memory and add/sub immediate");
        if (width)
            *width = s == "sp" ? 8 : 4;
        return 32;
    }
    if (s == "lr")
        s = "x30";
    if (s == "fp")
        s = "x29";
    require(s.size() >= 2 && (s[0] == 'x' || s[0] == 'w'), "Expected x0–x30 or w0–w30: " + s);
    if (width)
        *width = s[0] == 'x' ? 8 : 4;
    if (s.substr(1) == "zr")
        return 31;
    require(
        std::all_of(s.begin() + 1, s.end(), [](char c) { return std::isdigit((unsigned char)c); }),
        "Invalid register: " + s);
    int n = std::stoi(s.substr(1));
    require(n <= 30, "Register must be 0–30");
    return n;
}
static int freg(std::string s, int *width = nullptr) {
    s = lower(trim(s));
    require(s.size() >= 2 && (s[0] == 's' || s[0] == 'd'), "Expected s0–s31 or d0–d31");
    if (width)
        *width = s[0] == 'd' ? 8 : 4;
    require(
        std::all_of(s.begin() + 1, s.end(), [](char c) { return std::isdigit((unsigned char)c); }),
        "Invalid float register");
    int n = std::stoi(s.substr(1));
    require(n < 32, "Float register must be 0–31");
    return n;
}
static int condition(std::string s) {
    static const std::vector<std::string> c = {"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
                                               "hi", "ls", "ge", "lt", "gt", "le", "al"};
    s = lower(s);
    if (s == "hs")
        s = "cs";
    if (s == "lo")
        s = "cc";
    auto p = std::find(c.begin(), c.end(), s);
    require(p != c.end(), "Unknown condition: " + s);
    return int(p - c.begin());
}
static bool matches(int c, uint64_t flags) {
    bool n = flags >> 31 & 1, z = flags >> 30 & 1, v = flags >> 28 & 1, carry = flags >> 29 & 1;
    switch (c) {
    case 0:
        return z;
    case 1:
        return !z;
    case 2:
        return carry;
    case 3:
        return !carry;
    case 4:
        return n;
    case 5:
        return !n;
    case 6:
        return v;
    case 7:
        return !v;
    case 8:
        return carry && !z;
    case 9:
        return !carry || z;
    case 10:
        return n == v;
    case 11:
        return n != v;
    case 12:
        return !z && n == v;
    case 13:
        return z || n != v;
    default:
        return true;
    }
}
static std::string stringLiteral(std::string s) {
    s = trim(s);
    require(s.size() >= 2 && s[0] == '"' && s.back() == '"', "Expected quoted string");
    std::string out;
    for (size_t i = 1; i + 1 < s.size(); i++) {
        char c = s[i];
        if (c == '\\') {
            require(i + 2 < s.size(), "Incomplete escape");
            c = s[++i];
            switch (c) {
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case '0':
                c = 0;
                break;
            case 'e':
                c = 27;
                break;
            case 'x': {
                require(i + 3 < s.size(), "Expected two hex digits");
                auto h = s.substr(i + 1, 2);
                require(h.find_first_not_of("0123456789abcdefABCDEF") == h.npos,
                        "Invalid hex escape");
                c = std::stoi(h, nullptr, 16);
                i += 2;
                break;
            }
            default:
                break;
            }
        }
        out += c;
    }
    return out;
}
Engine::Engine() : mem(MEM_END, 0) {
    configure(320, 240, {1, 4, 5, 6, 7, 8, 9});
}
Engine::~Engine() {
    release();
}
void Engine::release() {
    if (fastJit) {
        munmap(fastJit, fastSize);
        fastJit = nullptr;
    }
    fastSize = 0;
    fastEntries.clear();
    if (jit) {
        munmap(jit, jitSize);
        jit = nullptr;
    }
    jitSize = 0;
}
std::string Engine::assemble(const std::string &source, bool executable) {
    release();
    ctx = {};
    sp = RAM_END;
    pc = count = 0;
    lastPc = UINT64_MAX;
    mode = 0;
    halted = true;
    paused = false;
    skipBreakpoint = false;
    status = "Ready";
    output.clear();
    code.clear();
    mem.assign(MEM_END, 0);
    resetDevices();
    cursorX = cursorY = bg = 0;
    fg = 7;
    escape.clear();
    int line = 0;
    try {
        require(source.size() <= 256 * 1024, "Source exceeds 256 KiB");
        std::map<std::string, uint64_t> labels = {{"DEVICE_TABLE", DEVICE_TABLE},
                                                  {"SCREEN_WIDTH", uint64_t(screenWidth)},
                                                  {"SCREEN_HEIGHT", uint64_t(screenHeight)},
                                                  {"FRAMEBUFFER", displayBase},
                                                  {"KEY", INPUT},
                                                  {"KEY_DOWN", INPUT + 4},
                                                  {"TOUCH_X", INPUT + 8},
                                                  {"TOUCH_Y", INPUT + 12},
                                                  {"TOUCH_DOWN", INPUT + 16},
                                                  {"TICKS", INPUT + 24}};
        struct Pending {
            int line;
            std::string op, args;
            uint32_t address;
        };
        std::vector<Pending> pending;
        bool data = false;
        uint32_t dp = RAM;
        std::istringstream lines(source);
        std::string s;
        while (std::getline(lines, s)) {
            line++;
            s = uncomment(s);
            if (s.empty())
                continue;
            auto colon = s.find(':');
            if (colon != s.npos && s.substr(0, colon).find_first_of(" \t\"") == s.npos) {
                auto name = trim(s.substr(0, colon));
                require(!name.empty() && !labels.count(name), "Duplicate or empty label: " + name);
                labels[name] = data ? dp : code.size() * 4;
                s = trim(s.substr(colon + 1));
                if (s.empty())
                    continue;
            }
            auto space = s.find_first_of(" \t");
            auto op = lower(s.substr(0, space));
            auto args = space == s.npos ? "" : trim(s.substr(space + 1));
            if (op == ".text") {
                data = false;
                continue;
            }
            if (op == ".data" || op == ".rodata" || op == ".bss") {
                data = true;
                continue;
            }
            if (op == ".section") {
                require(args == ".text" || args == ".data" || args == ".rodata" || args == ".bss",
                        "Unsupported section");
                data = args != ".text";
                continue;
            }
            if (op == ".global" || op == ".globl")
                continue;
            if (op == ".equ" || op == ".set") {
                auto a = split(args);
                require(a.size() == 2, ".equ NAME, value");
                require(!labels.count(a[0]), "Duplicate symbol");
                labels[a[0]] = number(a[1], labels);
                continue;
            }
            if (op == ".align" || op == ".balign") {
                require(data, "Alignment is supported in data sections");
                auto n = number(args, labels);
                if (op == ".align") {
                    require(n <= 12, "Alignment too large");
                    n = 1ULL << n;
                }
                require(n && n <= 4096 && (n & (n - 1)) == 0,
                        "Alignment must be a power of two ≤4096");
                dp = (dp + n - 1) & ~(n - 1);
                require(dp <= RAM_END, "Data exceeds RAM");
                continue;
            }
            if (!op.empty() && op[0] == '.') {
                require(data, "Data directives require .data");
                uint64_t bytes = 0;
                if (op == ".asciz" || op == ".string" || op == ".ascii") {
                    auto t = stringLiteral(args);
                    bytes = t.size() + (op != ".ascii");
                    require(bytes <= RAM_END - dp, "Data exceeds RAM");
                    std::copy(t.begin(), t.end(), mem.begin() + dp);
                } else if (op == ".space" || op == ".zero") {
                    bytes = number(args, labels);
                    require(bytes <= RAM_END - dp, "Data exceeds RAM");
                } else {
                    int w = op == ".byte"                      ? 1
                            : op == ".hword" || op == ".short" ? 2
                            : op == ".word" || op == ".float"  ? 4
                            : op == ".quad" || op == ".double" ? 8
                                                               : 0;
                    require(w, "Unsupported directive: " + op);
                    bytes = split(args).size() * w;
                    require(bytes <= RAM_END - dp, "Data exceeds RAM");
                    pending.push_back({line, op, args, dp});
                }
                dp += bytes;
                continue;
            }
            require(!data, "Instructions require .text");
            require(code.size() < 8192, "Maximum 8192 instructions");
            Instruction i;
            i.line = line;
            i.text = s;
            code.push_back(i);
            pending.push_back({line, op, args, uint32_t((code.size() - 1) * 4)});
        }
        for (const auto &p : pending) {
            line = p.line;
            auto op = p.op;
            auto a = split(p.args);
            if (op[0] == '.') {
                int w = op == ".byte"                      ? 1
                        : op == ".hword" || op == ".short" ? 2
                        : op == ".word" || op == ".float"  ? 4
                                                           : 8;
                uint32_t at = p.address;
                for (auto t : a) {
                    uint64_t v = 0;
                    if (op == ".float") {
                        size_t n;
                        float f = std::stof(t, &n);
                        require(n == t.size(), "Invalid float");
                        std::memcpy(&v, &f, 4);
                    } else if (op == ".double") {
                        size_t n;
                        double d = std::stod(t, &n);
                        require(n == t.size(), "Invalid double");
                        std::memcpy(&v, &d, 8);
                    } else
                        v = number(t, labels);
                    std::memcpy(mem.data() + at, &v, w);
                    at += w;
                }
                continue;
            }
            auto &i = code[p.address / 4];
            auto arity = [&](int n) {
                require(a.size() == size_t(n), op + " expects " + std::to_string(n) + " operands");
            };
            auto same = [&](const std::string &r) {
                int w;
                int n = reg(r, &w);
                require(w == i.width, "Register widths must match");
                return n;
            };
            auto target = [&](std::string t) {
                auto n = number(t, labels);
                require(n % 4 == 0 && n < code.size() * 4, "Branch target is outside code");
                return n;
            };
            if (op == "mov" || op == "movz" || op == "movk" || op == "movn") {
                require(a.size() == 2 || a.size() == 3, "Expected destination and value");
                i.rd = reg(a[0], &i.width);
                if (op == "mov" && a.size() == 2 &&
                    (a[1][0] == 'x' || a[1][0] == 'w' || a[1] == "lr" || a[1] == "fp")) {
                    i.rm = same(a[1]);
                    i.kind = Instruction::Native;
                    i.opcode = (i.width == 8 ? 0xaa0003e0 : 0x2a0003e0) | (i.rm << 16) | i.rd;
                } else {
                    auto n = number(a[1], labels);
                    if (op == "mov") {
                        arity(2);
                        require(i.width == 8 || n <= 0xffffffffULL || n >= 0xffffffff80000000ULL,
                                "Value exceeds 32 bits");
                        i.kind = Instruction::Move;
                        i.target = n;
                    } else {
                        require(n <= 65535, "movz/movk/movn immediate must fit 16 bits");
                        int shift = 0;
                        if (a.size() == 3) {
                            require(a[2].starts_with("lsl "), "Expected lsl #shift");
                            auto s = number(a[2].substr(4), labels);
                            require(s % 16 == 0 && s < uint64_t(i.width * 8),
                                    "Shift must be 0, 16, 32 or 48 within register width");
                            shift = s;
                        }
                        i.kind = Instruction::Native;
                        i.opcode = (op == "movz"   ? 0x52800000
                                    : op == "movk" ? 0x72800000
                                                   : 0x12800000) |
                                   (i.width == 8 ? 0x80000000 : 0) | ((shift / 16) << 21) |
                                   (n << 5) | i.rd;
                    }
                }
            } else if (op == "adr" || op == "adrp") {
                arity(2);
                i.kind = Instruction::Address;
                i.rd = reg(a[0], &i.width);
                require(i.width == 8, "adr requires X register");
                i.target = number(a[1], labels);
                if (op == "adrp")
                    i.target &= ~4095ULL;
            } else if (op == "add" || op == "adds" || op == "sub" || op == "subs" || op == "cmp" ||
                       op == "cmn") {
                bool compare = op == "cmp" || op == "cmn",
                     sub = op == "sub" || op == "subs" || op == "cmp",
                     flags = compare || op == "adds" || op == "subs";
                arity(compare ? 2 : 3);
                i.rd = compare ? 31 : reg(a[0], &i.width, true);
                int w;
                i.rn = reg(a[compare ? 0 : 1], &w, true);
                if (compare)
                    i.width = w;
                require(w == i.width, "Register widths must match");
                auto b = a[compare ? 1 : 2];
                i.kind = Instruction::Native;
                uint32_t base = (i.width == 8 ? 0x80000000 : 0) | (sub ? 0x40000000 : 0) |
                                (flags ? 0x20000000 : 0);
                if (b.starts_with("#") ||
                    (!b.empty() && (std::isdigit((unsigned char)b[0]) || b[0] == '-'))) {
                    auto n = number(b, labels);
                    require(n <= 4095, "add/sub immediate must be 0–4095");
                    i.imm = n;
                    if (i.rn == 32 || i.rd == 32) {
                        require(!flags && i.width == 8, "SP supports 64-bit add/sub without flags");
                        i.kind = Instruction::StackMath;
                        i.extra = sub;
                    } else {
                        require(i.rn != 31 && (!flags ? i.rd != 31 : true),
                                "Immediate add/sub uses SP encoding; use SP explicitly");
                        i.opcode = base | 0x11000000 | (n << 10) | (i.rn << 5) | i.rd;
                    }
                } else {
                    require(i.rn != 32 && i.rd != 32, "SP requires immediate operand");
                    i.rm = same(b);
                    i.opcode = base | 0x0b000000 | (i.rm << 16) | (i.rn << 5) | i.rd;
                }
            } else if (op == "and" || op == "orr" || op == "eor" || op == "ands" || op == "bic" ||
                       op == "tst") {
                bool tst = op == "tst";
                arity(tst ? 2 : 3);
                i.rd = tst ? 31 : reg(a[0], &i.width);
                int w;
                i.rn = reg(a[tst ? 0 : 1], &w);
                if (tst)
                    i.width = w;
                require(w == i.width, "Register widths must match");
                i.rm = same(a[tst ? 1 : 2]);
                i.kind = Instruction::Native;
                i.opcode = (i.width == 8 ? 0x80000000 : 0) |
                           (op == "orr"           ? 0x2a000000
                            : op == "eor"         ? 0x4a000000
                            : op == "ands" || tst ? 0x6a000000
                            : op == "bic"         ? 0x0a200000
                                                  : 0x0a000000) |
                           (i.rm << 16) | (i.rn << 5) | i.rd;
            } else if (op == "mul" || op == "madd" || op == "msub" || op == "udiv" ||
                       op == "sdiv") {
                arity(op == "madd" || op == "msub" ? 4 : 3);
                i.rd = reg(a[0], &i.width);
                i.rn = same(a[1]);
                i.rm = same(a[2]);
                i.kind = Instruction::Native;
                uint32_t base = op == "udiv" ? 0x1ac00800 : op == "sdiv" ? 0x1ac00c00 : 0x1b000000;
                if (op == "mul" || op == "madd" || op == "msub")
                    base |= ((op == "mul" ? 31 : same(a[3])) << 10) | (op == "msub" ? 0x8000 : 0);
                i.opcode =
                    base | (i.width == 8 ? 0x80000000 : 0) | (i.rm << 16) | (i.rn << 5) | i.rd;
            } else if (op == "lsl" || op == "lsr" || op == "asr" || op == "ror") {
                arity(3);
                i.rd = reg(a[0], &i.width);
                i.rn = same(a[1]);
                i.kind = Instruction::Native;
                if (a[2][0] == 'x' || a[2][0] == 'w') {
                    i.rm = same(a[2]);
                    i.opcode = (i.width == 8 ? 0x80000000 : 0) |
                               (op == "lsl"   ? 0x1ac02000
                                : op == "lsr" ? 0x1ac02400
                                : op == "asr" ? 0x1ac02800
                                              : 0x1ac02c00) |
                               (i.rm << 16) | (i.rn << 5) | i.rd;
                } else {
                    auto n = number(a[2], labels);
                    int bits = i.width * 8;
                    require(n < uint64_t(bits), "Shift outside register width");
                    uint32_t base = op == "asr" ? 0x13000000 : 0x53000000;
                    if (i.width == 8)
                        base |= 0x80400000;
                    if (op == "ror") {
                        base = i.width == 8 ? 0x93c00000 : 0x13800000;
                        i.opcode = base | (i.rn << 16) | (n << 10) | (i.rn << 5) | i.rd;
                    } else {
                        i.opcode = base | ((op == "lsl" ? (bits - n) % bits : n) << 16) |
                                   ((op == "lsl" ? bits - 1 - n : bits - 1) << 10) | (i.rn << 5) |
                                   i.rd;
                    }
                }
            } else if (op == "neg" || op == "negs" || op == "mvn" || op == "clz" || op == "rbit" ||
                       op == "rev") {
                arity(2);
                i.rd = reg(a[0], &i.width);
                i.rn = same(a[1]);
                i.kind = Instruction::Native;
                uint32_t base = op == "neg"    ? 0x4b0003e0
                                : op == "negs" ? 0x6b0003e0
                                : op == "mvn"  ? 0x2a2003e0
                                : op == "clz"  ? 0x5ac01000
                                : op == "rbit" ? 0x5ac00000
                                : i.width == 8 ? 0x5ac00c00
                                               : 0x5ac00800;
                i.opcode = base | (i.width == 8 ? 0x80000000 : 0) |
                           (i.rn << ((op == "neg" || op == "negs" || op == "mvn") ? 16 : 5)) | i.rd;
            } else if (op == "csel" || op == "cset") {
                arity(op == "csel" ? 4 : 2);
                i.rd = reg(a[0], &i.width);
                i.kind = Instruction::Native;
                int c = condition(a.back());
                if (op == "cset") {
                    require(c < 14, "cset requires conditional code");
                    i.opcode = (i.width == 8 ? 0x9a9f07e0 : 0x1a9f07e0) | ((c ^ 1) << 12) | i.rd;
                } else {
                    i.rn = same(a[1]);
                    i.rm = same(a[2]);
                    i.opcode = (i.width == 8 ? 0x9a800000 : 0x1a800000) | (i.rm << 16) | (c << 12) |
                               (i.rn << 5) | i.rd;
                }
            } else if (op == "fadd" || op == "fsub" || op == "fmul" || op == "fdiv" ||
                       op == "fmov" || op == "fneg" || op == "fabs" || op == "fsqrt" ||
                       op == "fcmp") {
                bool binary = op == "fadd" || op == "fsub" || op == "fmul" || op == "fdiv";
                arity(binary ? 3 : 2);
                i.rd = freg(a[0], &i.width);
                int w;
                i.rn = freg(a[1], &w);
                require(w == i.width, "Float widths must match");
                i.kind = Instruction::Native;
                uint32_t base = op == "fadd"    ? 0x1e202800
                                : op == "fsub"  ? 0x1e203800
                                : op == "fmul"  ? 0x1e200800
                                : op == "fdiv"  ? 0x1e201800
                                : op == "fmov"  ? 0x1e204000
                                : op == "fneg"  ? 0x1e214000
                                : op == "fabs"  ? 0x1e20c000
                                : op == "fsqrt" ? 0x1e21c000
                                                : 0x1e202000;
                if (binary) {
                    i.rm = freg(a[2], &w);
                    require(w == i.width, "Float widths must match");
                    i.opcode = base | (i.rm << 16) | (i.rn << 5) | i.rd;
                } else if (op == "fcmp")
                    i.opcode = base | (i.rn << 16) | (i.rd << 5);
                else
                    i.opcode = base | (i.rn << 5) | i.rd;
                if (i.width == 8)
                    i.opcode |= 0x400000;
            } else if (op == "scvtf" || op == "ucvtf" || op == "fcvtzs" || op == "fcvtzu") {
                arity(2);
                int iw, fw;
                bool tofloat = op == "scvtf" || op == "ucvtf";
                int d = tofloat ? freg(a[0], &fw) : reg(a[0], &iw);
                int n = tofloat ? reg(a[1], &iw) : freg(a[1], &fw);
                uint32_t base = tofloat ? 0x1e220000 : 0x1e380000;
                if (op == "ucvtf" || op == "fcvtzu")
                    base |= 0x10000;
                i.kind = Instruction::Native;
                i.opcode =
                    base | (iw == 8 ? 0x80000000 : 0) | (fw == 8 ? 0x400000 : 0) | (n << 5) | d;
            } else if (op == "ldr" || op == "str" || op == "ldrb" || op == "strb" || op == "ldrh" ||
                       op == "strh" || op == "ldrsw" || op == "ldrsb" || op == "ldrsh" ||
                       op == "ldp" || op == "stp") {
                bool pair = op == "ldp" || op == "stp", load = op.starts_with("ld");
                int mi = pair ? 2 : 1;
                require(a.size() == size_t(mi + 1) || a.size() == size_t(mi + 2),
                        "Invalid memory operands");
                i.extra = (!a[0].empty() && (a[0][0] == 'd' || a[0][0] == 's')) ? 1 : 0;
                i.rd = i.extra ? freg(a[0], &i.width) : reg(a[0], &i.width);
                i.kind = pair ? (load ? Instruction::PairLoad : Instruction::PairStore)
                              : (load ? Instruction::Load : Instruction::Store);
                if (pair) {
                    require(!i.extra, "Pairs use integer registers");
                    i.rm = same(a[1]);
                }
                if (op == "ldr" && a[mi].starts_with("=")) {
                    require(!i.extra && i.width == 8, "Literal address requires X register");
                    arity(2);
                    i.kind = Instruction::Address;
                    i.target = number(a[mi], labels);
                    continue;
                }
                if (op == "ldrb" || op == "strb" || op == "ldrsb")
                    i.cond = 1;
                else if (op == "ldrh" || op == "strh" || op == "ldrsh")
                    i.cond = 2;
                else if (op == "ldrsw")
                    i.cond = 4;
                else
                    i.cond = i.width;
                i.sign = op == "ldrsw" || op == "ldrsb" || op == "ldrsh";
                require(!i.extra || op == "ldr" || op == "str", "Float memory uses ldr/str");
                if (op == "ldrsw")
                    require(i.width == 8, "ldrsw requires X register");
                if (op == "ldrb" || op == "ldrh" || op == "strb" || op == "strh")
                    require(i.width == 4, "Byte/halfword transfers require W register");
                auto m = a[mi];
                i.pre = m.ends_with("!");
                if (i.pre)
                    m.pop_back();
                require(m.size() >= 3 && m.front() == '[' && m.back() == ']',
                        "Expected [xN, #offset]");
                auto parts = split(m.substr(1, m.size() - 2));
                require(parts.size() == 1 || parts.size() == 2, "Use base plus immediate offset");
                int w;
                i.rn = reg(parts[0], &w, true);
                require(w == 8 && i.rn != 31, "Memory base must be X register or SP");
                if (parts.size() == 2)
                    i.imm = number(parts[1], labels);
                if (a.size() == size_t(mi + 2)) {
                    require(!i.pre && parts.size() == 1, "Invalid post-index");
                    i.post = true;
                    i.imm = number(a.back(), labels);
                }
                if (load && (i.pre || i.post))
                    require(i.rd != i.rn && (!pair || i.rm != i.rn),
                            "Writeback cannot overlap load destination");
            } else if (op == "b" || op.starts_with("b.") || op == "bl") {
                arity(1);
                i.kind = op == "bl" ? Instruction::Call : Instruction::Branch;
                i.target = target(a[0]);
                i.cond = op.starts_with("b.") ? condition(op.substr(2)) : -1;
            } else if (op == "ret" || op == "br" || op == "blr") {
                require(a.size() == 1 || (op == "ret" && a.empty()),
                        "Expected one branch register");
                i.kind = op == "ret"  ? Instruction::Return
                         : op == "br" ? Instruction::BranchRegister
                                      : Instruction::CallRegister;
                i.rn = a.empty() ? 30 : reg(a[0], &i.width);
                require(i.width == 8, "Branch requires X register");
            } else if (op == "cbz" || op == "cbnz" || op == "tbz" || op == "tbnz") {
                bool test = op[0] == 't';
                arity(test ? 3 : 2);
                i.kind = test ? Instruction::TestBranch : Instruction::CompareBranch;
                i.rn = reg(a[0], &i.width);
                i.extra = op == "cbnz" || op == "tbnz";
                i.target = target(a.back());
                if (test) {
                    auto n = number(a[1], labels);
                    require(n < uint64_t(i.width * 8), "Bit outside register width");
                    i.imm = n;
                }
            } else if (op == "svc" || op == "hlt" || op == "brk") {
                require(a.size() == 1 || (op != "svc" && a.empty()), "Expected immediate");
                auto n = a.empty() ? 0 : number(a[0], labels);
                require(n <= 65535, "Immediate exceeds 16 bits");
                if (op == "svc")
                    require(n <= 5, "Only custom VM svc #0–#5 are allowed");
                i.kind = op == "svc" ? Instruction::Syscall : Instruction::Halt;
                i.imm = n;
            } else if (op == "nop") {
                arity(0);
                i.kind = Instruction::Nop;
            } else
                throw std::runtime_error("Unknown or unsupported instruction: " + op);
        }
        require(!code.empty(), "No instructions in .text");
        if (labels.count("_start")) {
            pc = labels["_start"];
            require(pc % 4 == 0 && pc < code.size() * 4, "_start must point into code");
        }
        // A top-level RET safely exits; BL replaces this virtual return address.
        ctx.x[30] = code.size() * 4;
        for (size_t j = 0; j < code.size(); j++)
            std::memcpy(mem.data() + j * 4, &code[j].opcode, 4);
        if (executable) {
            stride = native_template_end - native_template_start;
            size_t page = sysconf(_SC_PAGESIZE);
            jitSize = (stride * code.size() + page - 1) & ~(page - 1);
            jit =
                mmap(nullptr, jitSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (jit == MAP_FAILED) {
                jit = nullptr;
                throw std::runtime_error("Native executable memory unavailable");
            }
            size_t off = native_template_op - native_template_start;
            for (size_t j = 0; j < code.size(); j++) {
                auto ptr = (char *)jit + j * stride;
                std::memcpy(ptr, native_template_start, stride);
                if (code[j].kind == Instruction::Native)
                    std::memcpy(ptr + off, &code[j].opcode, 4);
            }
            __builtin___clear_cache((char *)jit, (char *)jit + jitSize);
            require(mprotect(jit, jitSize, PROT_READ | PROT_EXEC) == 0,
                    "Cannot protect native code");
            compileFast();
        }
        halted = false;
        status = "Assembled · " + std::to_string(code.size()) + " instructions";
        return "";
    } catch (const std::exception &e) {
        release();
        halted = true;
        status = "Line " + std::to_string(line) + ": " + e.what();
        return status;
    }
}
uint64_t Engine::readReg(int r, int width) const {
    auto v = r == 32 ? sp : r == 31 ? 0 : ctx.x[r];
    return width == 4 ? (uint32_t)v : v;
}
void Engine::writeReg(int r, uint64_t v, int width) {
    if (width == 4)
        v = (uint32_t)v;
    if (r == 32)
        sp = v;
    else if (r != 31)
        ctx.x[r] = v;
}
void Engine::check(uint64_t a, int n, bool wr) const {
    require(n > 0 && a <= MEM_END && uint64_t(n) <= MEM_END - a, "Memory access out of bounds");
    bool peripheral = false;
    for (const auto &d : devices) {
        if (a >= d.base && a + n <= d.base + d.size && n <= 8) {
            if (wr) {
                uint32_t off = a - d.base;
                bool buffer = d.type == 9 && off >= 64 && off + n <= 128;
                bool reg = n == 4 && off % 4 == 0 &&
                           ((d.type <= 3 &&
                             (off == 4 || off == 20 || off == 24 ||
                              (d.type >= 2 && off >= 32 && off <= (d.type == 2 ? 60U : 36U)))) ||
                            (d.type == 7 && off <= 12) || (d.type == 9 && off <= 4));
                require(buffer || reg, "Write to read-only or misaligned device register");
            }
            peripheral = true;
        }
        if (d.vram && a >= d.vram && a + n <= d.vram + uint64_t(screenWidth * screenHeight * 4))
            peripheral = true;
    }
    bool table = !wr && a >= DEVICE_TABLE && a + n <= DEVICE_TABLE + 16 + devices.size() * 32;
    bool ram = a >= RAM && a + n <= RAM_END, fb = a >= FB && a + n <= FB_END,
         inp = a >= INPUT && a + n <= INPUT + 32, codeRead = a + n <= code.size() * 4;
    require(ram || fb || peripheral || table || (!wr && (inp || codeRead)),
            wr ? "Write to unmapped or read-only memory" : "Read from unmapped memory");
}
uint64_t Engine::read(uint64_t a, int n) const {
    check(a, n, false);
    if (auto *d = const_cast<Engine *>(this)->deviceAt(a))
        return const_cast<Engine *>(this)->deviceRead(*d, a - d->base, n);
    uint64_t v = 0;
    std::memcpy(&v, mem.data() + a, n);
    return v;
}
void Engine::write(uint64_t a, uint64_t v, int n) {
    check(a, n, true);
    std::memcpy(mem.data() + a, &v, n);
    if (auto *d = deviceAt(a))
        deviceWrite(*d, a - d->base);
}
void Engine::execute(const Instruction &i) {
    uint64_t next = pc + 4;
    switch (i.kind) {
    case Instruction::Native:
        require(jit, "Native code not compiled");
        reinterpret_cast<void (*)(Context *)>((char *)jit + (pc / 4) * stride)(&ctx);
        break;
    case Instruction::Move:
    case Instruction::Address:
        writeReg(i.rd, i.target, i.width);
        break;
    case Instruction::StackMath:
        writeReg(i.rd, i.extra ? readReg(i.rn) - i.imm : readReg(i.rn) + i.imm);
        break;
    case Instruction::Load:
    case Instruction::Store:
    case Instruction::PairLoad:
    case Instruction::PairStore: {
        bool load = i.kind == Instruction::Load || i.kind == Instruction::PairLoad,
             pair = i.kind == Instruction::PairLoad || i.kind == Instruction::PairStore;
        uint64_t base = readReg(i.rn), addr = i.post ? base : base + i.imm;
        int n = i.cond;
        check(addr, n * (pair ? 2 : 1), !load);
        if (load) {
            uint64_t v = read(addr, n);
            if (i.sign) {
                if (n == 1)
                    v = int64_t(int8_t(v));
                else if (n == 2)
                    v = int64_t(int16_t(v));
                else
                    v = int64_t(int32_t(v));
            }
            if (i.extra)
                ctx.d[i.rd] = v;
            else
                writeReg(i.rd, v, i.width);
            if (pair)
                writeReg(i.rm, read(addr + n, n), i.width);
        } else {
            write(addr, i.extra ? ctx.d[i.rd] : readReg(i.rd), n);
            if (pair)
                write(addr + n, readReg(i.rm), n);
        }
        if (i.pre || i.post)
            writeReg(i.rn, base + i.imm);
        break;
    }
    case Instruction::Branch:
        if (i.cond < 0 || matches(i.cond, ctx.flags))
            next = i.target;
        break;
    case Instruction::Call:
        ctx.x[30] = next;
        next = i.target;
        break;
    case Instruction::Return:
    case Instruction::BranchRegister:
    case Instruction::CallRegister: {
        auto t = readReg(i.rn);
        require(t % 4 == 0 && t <= code.size() * 4, "Invalid branch address");
        if (i.kind == Instruction::CallRegister)
            ctx.x[30] = next;
        next = t;
        break;
    }
    case Instruction::CompareBranch:
        if ((readReg(i.rn, i.width) != 0) == bool(i.extra))
            next = i.target;
        break;
    case Instruction::TestBranch:
        if (bool((readReg(i.rn, i.width) >> i.imm) & 1) == bool(i.extra))
            next = i.target;
        break;
    case Instruction::Syscall:
        syscall(i.imm);
        break;
    case Instruction::Halt:
        halted = true;
        status = "Halted";
        break;
    case Instruction::Nop:
        break;
    }
    pc = next;
}
void Engine::run(int budget, bool single, bool timeLimited, bool fast) {
    if (halted)
        return;
    paused = false;
    yielded = false;
    bool skip = skipBreakpoint;
    skipBreakpoint = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8);
    try {
        serviceDevices();
        for (int j = 0; j < budget; j++) {
            if (fast && !single && breaks.empty() && runFast(std::min(1024, budget - j))) {
                j += int(ctx.executed) - 1;
                serviceDevices();
                if (timeLimited && std::chrono::steady_clock::now() >= deadline)
                    break;
                continue;
            }
            if (pc == code.size() * 4) {
                halted = true;
                status = "Finished";
                break;
            }
            require(pc % 4 == 0 && pc < code.size() * 4, "PC outside code");
            if (!single && !skip && breaks.count(currentLine())) {
                paused = true;
                skipBreakpoint = true;
                status = "Breakpoint · line " + std::to_string(currentLine());
                break;
            }
            skip = false;
            const uint64_t executingPc = pc;
            execute(code[pc / 4]);
            lastPc = executingPc;
            count++;
            std::memcpy(mem.data() + INPUT + 24, &count, 8);
            if (!halted && pc == code.size() * 4) {
                halted = true;
                status = "Finished";
            }
            if (halted)
                break;
            if (single) {
                paused = true;
                status = "Stepped · line " + std::to_string(currentLine());
                break;
            }
            if (yielded)
                break;
            // The machine runs until the user stops it or a syscall ends it; the deadline
            // only bounds how long a batch keeps the UI thread, so 1024 steps between
            // clock reads costs nothing (worst case ~60 us past the budget).
            if ((j & 1023) == 1023) {
                serviceDevices();
                if (timeLimited && std::chrono::steady_clock::now() >= deadline)
                    break;
            }
        }
        if (!halted && !paused)
            status = "Running · " + std::to_string(count) + " instructions";
    } catch (const std::exception &e) {
        halted = true;
        status = "Fault · line " + std::to_string(currentLine()) + ": " + e.what();
    }
}
void Engine::stop() {
    paused = true;
    status = "Paused · line " + std::to_string(currentLine());
}
int Engine::currentLine() const {
    return pc / 4 < code.size() ? code[pc / 4].line : 0;
}
std::vector<InstructionLocation> Engine::instructionWindow(bool lastExecuted) const {
    std::vector<InstructionLocation> rows;
    if (code.empty())
        return rows;
    const bool fault = status.starts_with("Fault");
    uint64_t focus =
        ((lastExecuted && !paused && !fault) || (halted && !fault)) && lastPc != UINT64_MAX ? lastPc
                                                                                            : pc;
    if (focus % 4 != 0 || focus / 4 >= code.size())
        return rows;
    const size_t center = focus / 4;
    // Keep up to seven actual instructions visible, even at the start/end of the program.
    size_t begin = center > 3 ? center - 3 : 0;
    size_t end = std::min(code.size(), begin + 7);
    if (end - begin < 7 && end > 7)
        begin = end - 7;
    for (size_t n = begin; n < end; ++n)
        rows.push_back(
            {n * 4, code[n].line, n == center, breaks.count(code[n].line) != 0, code[n].text});
    return rows;
}
void Engine::input(int key, bool down, int x, int y, bool touch) {
    uint32_t values[6] = {(uint32_t)key,
                          down ? 1U : 0U,
                          (uint32_t)std::clamp(x, 0, screenWidth - 1),
                          (uint32_t)std::clamp(y, 0, screenHeight - 1),
                          touch ? 1U : 0U,
                          0};
    std::memcpy(mem.data() + INPUT, values, sizeof(values));
    for (auto &d : devices) {
        if (d.type == 5) {
            std::memcpy(mem.data() + d.base, values + 2, 12);
            touches(touch ? std::vector<int>{0, x, y} : std::vector<int>{});
        }
        if (d.type == 6)
            std::memcpy(mem.data() + d.base, values, 8);
    }
}
void Engine::put(char c) {
    const auto clear = [&]() {
        std::fill(mem.begin() + displayBase,
                  mem.begin() + displayBase + (screenWidth / 8 * (screenHeight / 8) * 4), 0);
        cursorX = cursorY = 0;
    };
    if (!escape.empty()) {
        escape += c;
        if (escape.size() > 48) {
            escape.clear();
            return;
        }
        if (escape.size() == 2 && c != '[') {
            escape.clear();
            return;
        }
        if (escape.size() > 2 && std::isalpha((unsigned char)c)) {
            auto body = escape.substr(2, escape.size() - 3);
            std::vector<int> nums;
            std::stringstream ss(body);
            std::string part;
            while (std::getline(ss, part, ';')) {
                try {
                    nums.push_back(part.empty() ? 0 : std::stoi(part));
                } catch (...) {
                    nums.push_back(0);
                }
            }
            if (nums.empty())
                nums.push_back(0);
            if (c == 'm') {
                for (int n : nums) {
                    if (n == 0) {
                        fg = 7;
                        bg = 0;
                    } else if (n >= 30 && n <= 37)
                        fg = n - 30;
                    else if (n >= 90 && n <= 97)
                        fg = n - 90 + 8;
                    else if (n >= 40 && n <= 47)
                        bg = n - 40;
                    else if (n >= 100 && n <= 107)
                        bg = n - 100 + 8;
                    else if (n == 39)
                        fg = 7;
                    else if (n == 49)
                        bg = 0;
                }
            } else if (c == 'J' && nums[0] == 2)
                clear();
            else if (c == 'H') {
                cursorY = std::clamp(nums[0] - 1, 0, screenHeight / 8 - 1);
                cursorX = nums.size() > 1 ? std::clamp(nums[1] - 1, 0, screenWidth / 8 - 1) : 0;
            }
            escape.clear();
        }
        return;
    }
    if (c == 27) {
        escape = c;
        return;
    }
    if (c == '\r') {
        cursorX = 0;
        return;
    }
    if (c == '\n') {
        cursorX = 0;
        cursorY++;
    } else if (c == '\t') {
        for (int n = 4 - cursorX % 4; n > 0; n--)
            put(' ');
        return;
    } else if ((uint8_t)c >= 32) {
        if (cursorY >= screenHeight / 8) {
            std::memmove(mem.data() + displayBase, mem.data() + displayBase + (screenWidth / 8 * 4),
                         (screenWidth / 8 * (screenHeight / 8 - 1) * 4));
            std::fill(mem.begin() + displayBase + (screenWidth / 8 * (screenHeight / 8 - 1) * 4),
                      mem.begin() + displayBase + (screenWidth / 8 * (screenHeight / 8) * 4), 0);
            cursorY = screenHeight / 8 - 1;
        }
        auto a = displayBase + (cursorY * (screenWidth / 8) + cursorX) * 4;
        mem[a] = (uint8_t)c;
        mem[a + 1] = fg;
        mem[a + 2] = bg;
        if (++cursorX == screenWidth / 8) {
            cursorX = 0;
            cursorY++;
        }
    }
    if (cursorY >= screenHeight / 8) {
        std::memmove(mem.data() + displayBase, mem.data() + displayBase + (screenWidth / 8 * 4),
                     (screenWidth / 8 * (screenHeight / 8 - 1) * 4));
        std::fill(mem.begin() + displayBase + (screenWidth / 8 * (screenHeight / 8 - 1) * 4),
                  mem.begin() + displayBase + (screenWidth / 8 * (screenHeight / 8) * 4), 0);
        cursorY = screenHeight / 8 - 1;
    }
}
void Engine::syscall(int n) {
    switch (n) {
    case 0:
        halted = true;
        status = "Finished · exit " + std::to_string(ctx.x[0]);
        break;
    case 1:
        require(ctx.x[0] <= 2, "Graphics mode must be 0, 1 or 2");
        mode = ctx.x[0];
        std::fill(mem.begin() + displayBase,
                  mem.begin() + (displayBase + screenWidth * screenHeight * 4), 0);
        cursorX = cursorY = 0;
        fg = 7;
        bg = 0;
        escape.clear();
        break;
    case 2: {
        uint64_t a = ctx.x[0], len = ctx.x[1];
        require(len <= 32768, "Text call limited to 32768 bytes");
        std::string text;
        if (len) {
            check(a, len, false);
            text.assign((char *)mem.data() + a, len);
        } else {
            for (int j = 0; j < 32768; j++) {
                char c = read(a + j, 1);
                if (!c)
                    break;
                text += c;
                require(j < 32767, "Unterminated string");
            }
        }
        output += text;
        if (output.size() > 32768)
            output.erase(0, output.size() - 32768);
        if (mode == 0)
            for (char c : text)
                put(c);
        break;
    }
    case 3:
        yielded = true;
        break;
    case 4:
        if (mode == 0) {
            std::fill(mem.begin() + displayBase,
                      mem.begin() + displayBase + (screenWidth / 8 * (screenHeight / 8) * 4), 0);
            cursorX = cursorY = 0;
        } else {
            int nbytes = mode == 1 ? 2 : 4;
            for (int j = 0; j < screenWidth * screenHeight; j++)
                std::memcpy(mem.data() + displayBase + j * nbytes, &ctx.x[0], nbytes);
        }
        break;
    case 5:
        ctx.x[0] = count;
        break;
    default:
        throw std::runtime_error("Unsupported VM call");
    }
}
std::string Engine::registers() const {
    std::ostringstream o;
    o << "PC   " << std::hex << std::setw(8) << std::setfill('0') << pc << "    SP   "
      << std::setw(8) << sp << "\nNZCV " << ((ctx.flags >> 31) & 1) << ((ctx.flags >> 30) & 1)
      << ((ctx.flags >> 29) & 1) << ((ctx.flags >> 28) & 1) << "    Steps " << std::dec << count
      << "\n\n";
    for (int j = 0; j < 31; j++)
        o << "x" << std::dec << std::setfill(' ') << std::setw(2) << std::left << j << "  "
          << std::right << std::hex << std::setfill('0') << std::setw(16) << ctx.x[j] << "\n";
    o << "\nFloating point (D / S)\n";
    for (int j = 0; j < 32; j++) {
        double d;
        float f;
        std::memcpy(&d, &ctx.d[j], 8);
        std::memcpy(&f, &ctx.d[j], 4);
        o << "d" << std::dec << j << "  " << d << " / " << f << "\n";
    }
    return o.str();
}
std::string Engine::memory(uint32_t addr) const {
    std::ostringstream o;
    addr &= ~15U;
    for (int row = 0; row < 24; row++) {
        uint64_t a = uint64_t(addr) + row * 16;
        o << std::hex << std::setfill('0') << std::setw(8) << a << "  ";
        std::string ascii;
        for (int col = 0; col < 16; col++) {
            try {
                auto v = read(a + col, 1);
                o << std::setw(2) << v << ' ';
                ascii += (v >= 32 && v < 127) ? char(v) : '.';
            } catch (...) {
                o << "-- ";
                ascii += ' ';
            }
        }
        o << " " << ascii << '\n';
    }
    return o.str();
}
std::vector<int32_t> Engine::pixels() const {
    std::vector<int32_t> p(screenWidth * screenHeight);
    for (int j = 0; j < screenWidth * screenHeight; j++) {
        uint32_t c = 0;
        if (mode == 1) {
            uint16_t v;
            std::memcpy(&v, mem.data() + displayBase + j * 2, 2);
            c = (((v >> 11) * 255 / 31) << 16) | ((((v >> 5) & 63) * 255 / 63) << 8) |
                ((v & 31) * 255 / 31);
        } else
            std::memcpy(&c, mem.data() + displayBase + j * 4, 4);
        p[j] = 0xff000000 | c;
    }
    return p;
}
std::vector<int32_t> Engine::cells() const {
    std::vector<int32_t> p((screenWidth / 8 * (screenHeight / 8)));
    std::memcpy(p.data(), mem.data() + displayBase, p.size() * 4);
    return p;
}
} // namespace armstudio
