#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace armstudio {
static void ensure(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
static uint32_t get32(const std::vector<uint8_t> &mem, uint32_t a) {
    uint32_t v;
    std::memcpy(&v, mem.data() + a, 4);
    return v;
}
static void set32(std::vector<uint8_t> &mem, uint32_t a, uint32_t v) {
    std::memcpy(mem.data() + a, &v, 4);
}
void Engine::configure(int width, int height, const std::vector<int> &types) {
    ensure(width >= 160 && width <= 640 && height >= 120 && height <= 960 && width % 8 == 0 &&
               height % 8 == 0,
           "Display dimensions must be multiples of 8 (160–640 × 120–960)");
    ensure(types.size() <= 9, "At most nine components");
    std::set<int> unique;
    for (int type : types)
        ensure(type >= 1 && type <= 9 && unique.insert(type).second,
               "Invalid or duplicate component");
    stop();
    halted = true;
    screenWidth = width;
    screenHeight = height;
    devices.clear();
    int gpu = 0;
    displayBase = (width == 320 && height == 240) ? FB : 0x800000;
    for (int type : types) {
        Device d{};
        d.type = type;
        d.base = 0x200000 + devices.size() * 0x10000;
        if (type <= 3) {
            d.vram = (gpu == 0 && width == 320 && height == 240) ? FB : 0x800000 + gpu * 0x400000;
            displayBase = d.vram;
            gpu++;
        }
        devices.push_back(std::move(d));
    }
    resetDevices();
}
void Engine::resetDevices() {
    set32(mem, DEVICE_TABLE, 0x41524d33);
    set32(mem, DEVICE_TABLE + 4, 1);
    set32(mem, DEVICE_TABLE + 8, devices.size());
    set32(mem, DEVICE_TABLE + 12, 32);
    for (size_t n = 0; n < devices.size(); ++n) {
        auto &d = devices[n];
        std::fill(mem.begin() + d.base, mem.begin() + d.base + d.size, 0);
        uint32_t entry[] = {d.type,
                            uint32_t(n + 1),
                            d.base,
                            d.size,
                            1,
                            uint32_t(screenWidth),
                            uint32_t(screenHeight),
                            0};
        std::memcpy(mem.data() + DEVICE_TABLE + 16 + n * 32, entry, sizeof(entry));
        d.transfer.clear();
        if (d.type <= 3) {
            set32(mem, d.base + 8, d.vram);
            set32(mem, d.base + 12, screenWidth);
            set32(mem, d.base + 16, screenHeight);
            for (auto &m : d.matrices) {
                m.fill(0);
                for (int j = 0; j < 4; ++j)
                    m[j * 5] = 1;
            }
            d.depth.assign(screenWidth * screenHeight, INFINITY);
            d.texture = d.textureWidth = d.textureHeight = 0;
        }
    }
}
void Engine::storage(const std::string &path) {
    if (nvPath == path)
        return;
    nvPath = path;
    std::fill(nv.begin(), nv.end(), 0);
    std::ifstream file(path, std::ios::binary);
    if (file)
        file.read(reinterpret_cast<char *>(nv.data()), nv.size());
}
Device *Engine::deviceAt(uint64_t a) {
    for (auto &d : devices)
        if (a >= d.base && a < d.base + d.size)
            return &d;
    return nullptr;
}
void Engine::serviceDevices() {
    for (auto &d : devices) {
        if (d.type == 7 && !d.transfer.empty() && std::chrono::steady_clock::now() >= d.ready) {
            std::memcpy(mem.data() + d.destination, d.transfer.data(), d.transfer.size());
            d.transfer.clear();
            set32(mem, d.base + 16, 2);
        }
    }
}
uint64_t Engine::deviceRead(Device &d, uint32_t offset, int bytes) {
    serviceDevices();
    if (d.type == 4 && offset < 8) {
        uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
        std::memcpy(mem.data() + d.base, &ms, 8);
    }
    if (d.type == 8 && offset < 8) {
        uint64_t value = rng();
        std::memcpy(mem.data() + d.base, &value, 8);
    }
    uint64_t v = 0;
    std::memcpy(&v, mem.data() + d.base + offset, bytes);
    return v;
}
void Engine::touches(const std::vector<int> &points) {
    for (auto &d : devices)
        if (d.type == 5) {
            uint32_t count = std::min<size_t>(10, points.size() / 3);
            set32(mem, d.base + 12, count);
            std::fill(mem.begin() + d.base + 16, mem.begin() + d.base + 176, 0);
            for (uint32_t j = 0; j < count; ++j) {
                uint32_t a = d.base + 16 + j * 16;
                set32(mem, a, points[j * 3]);
                set32(mem, a + 4, std::clamp(points[j * 3 + 1], 0, screenWidth - 1));
                set32(mem, a + 8, std::clamp(points[j * 3 + 2], 0, screenHeight - 1));
                set32(mem, a + 12, 1);
            }
        }
}
void Engine::deviceWrite(Device &d, uint32_t offset) {
    if (d.type <= 3 && offset == 20) {
        ensure(get32(mem, d.base + 20) <= 2, "GPU mode must be 0–2");
        displayBase = d.vram;
        mode = get32(mem, d.base + 20);
    }
    if (d.type <= 3 && offset == 4) {
        uint32_t command = get32(mem, d.base + 4);
        set32(mem, d.base, 0);
        displayBase = d.vram;
        if (command == 1) {
            uint32_t color = get32(mem, d.base + 24);
            for (int j = 0; j < screenWidth * screenHeight; ++j)
                set32(mem, d.vram + j * 4, color);
            std::fill(d.depth.begin(), d.depth.end(), INFINITY);
            mode = 2;
        } else if (command == 2)
            yielded = true;
        else if (command == 3 && d.type >= 2) {
            try {
                if (d.type == 2)
                    renderTiles(d);
                else
                    render3D(d);
            } catch (...) {
                set32(mem, d.base, 3);
                throw;
            }
            mode = 2;
        } else
            ensure(command == 0, "Invalid GPU command");
        set32(mem, d.base, 2);
    }
    if (d.type == 7 && offset == 12) {
        ensure(get32(mem, d.base + 12) == 1, "DMA command must be 1");
        ensure(d.transfer.empty(), "DMA is busy");
        uint32_t source = get32(mem, d.base), dest = get32(mem, d.base + 4),
                 size = get32(mem, d.base + 8);
        const auto buffer = [&](uint32_t a) {
            if (a >= RAM && uint64_t(a) + size <= RAM_END)
                return true;
            if (a >= FB && uint64_t(a) + size <= FB_END)
                return true;
            for (auto &gpu : devices)
                if (gpu.vram && a >= gpu.vram &&
                    uint64_t(a) + size <= gpu.vram + uint64_t(screenWidth * screenHeight * 4))
                    return true;
            return false;
        };
        if (!size || size > 4 * 1024 * 1024 || !buffer(source) || !buffer(dest)) {
            set32(mem, d.base + 16, 3);
            return;
        }
        d.transfer.assign(mem.begin() + source, mem.begin() + source + size);
        d.destination = dest;
        d.ready = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
        set32(mem, d.base + 16, 1);
    }
    if (d.type == 9 && offset == 4) {
        uint32_t a = get32(mem, d.base), command = get32(mem, d.base + 4);
        if (a > nv.size() - 64 || a % 64 || (command != 1 && command != 2)) {
            set32(mem, d.base + 8, 3);
            return;
        }
        if (command == 1)
            std::memcpy(mem.data() + d.base + 64, nv.data() + a, 64);
        else {
            auto updated = nv;
            std::memcpy(updated.data() + a, mem.data() + d.base + 64, 64);
            if (!nvPath.empty()) {
                std::string tmp = nvPath + ".tmp";
                std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
                file.write(reinterpret_cast<const char *>(updated.data()), updated.size());
                file.close();
                if (!file || std::rename(tmp.c_str(), nvPath.c_str())) {
                    set32(mem, d.base + 8, 3);
                    return;
                }
            }
            nv.swap(updated);
        }
        set32(mem, d.base + 8, 2);
    }
}
void Engine::renderTiles(Device &d) {
    uint32_t map = get32(mem, d.base + 32), tiles = get32(mem, d.base + 36),
             width = get32(mem, d.base + 40), height = get32(mem, d.base + 44),
             sprites = get32(mem, d.base + 48), count = get32(mem, d.base + 52);
    int sx = int32_t(get32(mem, d.base + 56)), sy = int32_t(get32(mem, d.base + 60));
    ensure(width <= 128 && height <= 128 && count <= 128, "Tile/sprite count exceeds limits");
    if (width && height)
        check(map, width * height * 2, false);
    if ((width && height) || count)
        check(tiles, 256 * 64 * 4, false);
    if (count)
        check(sprites, count * 16, false);
    const auto tilePixel = [&](uint32_t tile, int x, int y) {
        ensure(tile < 256, "Tile index must be less than 256");
        return get32(mem, tiles + (tile * 64 + y * 8 + x) * 4);
    };
    for (int y = 0; y < screenHeight; ++y)
        for (int x = 0; x < screenWidth; ++x) {
            uint32_t color = get32(mem, d.base + 24);
            if (width && height) {
                int tx = ((int64_t(x) + sx) % int(width * 8) + width * 8) % int(width * 8);
                int ty = ((int64_t(y) + sy) % int(height * 8) + height * 8) % int(height * 8);
                uint16_t tile;
                std::memcpy(&tile, mem.data() + map + (ty / 8 * width + tx / 8) * 2, 2);
                color = tilePixel(tile, tx % 8, ty % 8);
            }
            set32(mem, d.vram + (y * screenWidth + x) * 4, color);
        }
    for (uint32_t j = 0; j < count; ++j) {
        uint32_t a = sprites + j * 16, tile = get32(mem, a + 8), flags = get32(mem, a + 12);
        int x = int32_t(get32(mem, a)), y = int32_t(get32(mem, a + 4));
        if (!(flags & 1))
            continue;
        for (int py = 0; py < 8; ++py)
            for (int px = 0; px < 8; ++px) {
                if (int64_t(x) + px < 0 || int64_t(x) + px >= screenWidth || int64_t(y) + py < 0 ||
                    int64_t(y) + py >= screenHeight)
                    continue;
                uint32_t color = tilePixel(tile, flags & 2 ? 7 - px : px, flags & 4 ? 7 - py : py);
                if (color >> 24)
                    set32(mem, d.vram + ((y + py) * screenWidth + x + px) * 4, color);
            }
    }
}
void Engine::render3D(Device &d) {
    uint32_t ptr = get32(mem, d.base + 32), size = get32(mem, d.base + 36);
    ensure(size <= 8192 && size % 32 == 0,
           "3D command buffer must be 0–8192 bytes, 32-byte records");
    if (size)
        check(ptr, size, false);
    const auto f32 = [&](uint32_t a) {
        check(a, 4, false);
        float f;
        std::memcpy(&f, mem.data() + a, 4);
        ensure(std::isfinite(f) && std::abs(f) <= 1e6f, "Invalid graphics float");
        return double(f);
    };
    for (uint32_t at = ptr; at < ptr + size; at += 32) {
        uint32_t op = get32(mem, at), a = get32(mem, at + 4), b = get32(mem, at + 8),
                 c = get32(mem, at + 12);
        if (!op)
            break;
        if (op == 1) {
            for (int j = 0; j < screenWidth * screenHeight; ++j)
                set32(mem, d.vram + j * 4, a);
            std::fill(d.depth.begin(), d.depth.end(), INFINITY);
        } else if (op == 2) {
            ensure(a < 3, "Matrix slot must be 0–2");
            check(b, 64, false);
            for (int j = 0; j < 16; ++j)
                d.matrices[a][j] = f32(b + j * 4);
        } else if (op == 3) {
            ensure(b && c && b <= 256 && c <= 256, "Texture must be 1–256 pixels per side");
            check(a, b * c * 4, false);
            d.texture = a;
            d.textureWidth = b;
            d.textureHeight = c;
        } else if (op == 4) {
            check(a, 72, false);
            struct Vertex {
                double x, y, z, w, u, v;
                uint32_t color;
            } v[3];
            bool visible = true;
            for (int j = 0; j < 3; ++j) {
                uint32_t p = a + j * 24;
                double q[4] = {f32(p), f32(p + 4), f32(p + 8), 1};
                for (auto &m : d.matrices) {
                    double result[4]{};
                    for (int row = 0; row < 4; ++row)
                        for (int col = 0; col < 4; ++col)
                            result[row] += m[col * 4 + row] * q[col];
                    std::copy(result, result + 4, q);
                }
                // Triangles crossing the near plane are discarded, rather than clipped.
                if (!std::isfinite(q[3]) || q[3] <= 1e-8 || q[2] < -q[3])
                    visible = false;
                double w = q[3] > 1e-8 ? 1 / q[3] : 0;
                v[j] = {(q[0] * w + 1) * .5 * (screenWidth - 1),
                        (1 - q[1] * w) * .5 * (screenHeight - 1),
                        q[2] * w,
                        w,
                        f32(p + 12) * w,
                        f32(p + 16) * w,
                        get32(mem, p + 20)};
                if (!std::isfinite(v[j].x) || !std::isfinite(v[j].y) || std::abs(v[j].x) > 1e9 ||
                    std::abs(v[j].y) > 1e9)
                    visible = false;
            }
            if (!visible)
                continue;
            const auto edge = [](const Vertex &a, const Vertex &b, double x, double y) {
                return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
            };
            double area = edge(v[0], v[1], v[2].x, v[2].y);
            if (std::abs(area) < 1e-8)
                continue;
            int minX =
                std::clamp(int(std::floor(std::min({v[0].x, v[1].x, v[2].x}))), 0, screenWidth - 1);
            int maxX =
                std::clamp(int(std::ceil(std::max({v[0].x, v[1].x, v[2].x}))), 0, screenWidth - 1);
            int minY = std::clamp(int(std::floor(std::min({v[0].y, v[1].y, v[2].y}))), 0,
                                  screenHeight - 1);
            int maxY =
                std::clamp(int(std::ceil(std::max({v[0].y, v[1].y, v[2].y}))), 0, screenHeight - 1);
            ensure(!(b & 1) || d.textureWidth, "Bind a texture before a textured triangle");
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x) {
                    double u = edge(v[1], v[2], x + .5, y + .5) / area,
                           t = edge(v[2], v[0], x + .5, y + .5) / area, r = 1 - u - t;
                    if (u < 0 || t < 0 || r < 0)
                        continue;
                    double depth = u * v[0].z + t * v[1].z + r * v[2].z;
                    size_t index = y * screenWidth + x;
                    if (depth < -1 || depth > 1 || depth >= d.depth[index])
                        continue;
                    uint32_t color = 0;
                    if (b & 1) {
                        double w = u * v[0].w + t * v[1].w + r * v[2].w;
                        double tu = (u * v[0].u + t * v[1].u + r * v[2].u) / w,
                               tv = (u * v[0].v + t * v[1].v + r * v[2].v) / w;
                        int tx =
                            std::clamp(int(std::clamp(tu, 0.0, 1.0) * (d.textureWidth - 1) + .5), 0,
                                       int(d.textureWidth - 1));
                        int ty =
                            std::clamp(int(std::clamp(tv, 0.0, 1.0) * (d.textureHeight - 1) + .5),
                                       0, int(d.textureHeight - 1));
                        color = uint32_t(read(d.texture + (ty * d.textureWidth + tx) * 4, 4));
                    } else {
                        for (int shift : {0, 8, 16})
                            color |= uint32_t(std::clamp(u * ((v[0].color >> shift) & 255) +
                                                             t * ((v[1].color >> shift) & 255) +
                                                             r * ((v[2].color >> shift) & 255),
                                                         0.0, 255.0))
                                     << shift;
                    }
                    set32(mem, d.vram + index * 4, color);
                    d.depth[index] = depth;
                }
        } else
            throw std::runtime_error("Unknown 3D command");
    }
}
std::string Engine::deviceDocumentation() const {
    return componentDocumentation();
}
} // namespace armstudio
