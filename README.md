# ARM Studio

An offline Android ARM64 assembly workbench implemented from [spec.md](spec.md), [specv2_improvements.md](specv2_improvements.md), and [spec_v3.md](spec_v3.md). The assembler, VM, debugger state, memory, graphics formats, ANSI parser, examples, and reference content are C++. A Java Android UI provides the editor, native controls, and framebuffer display.

## Use

Open **ARM Studio** on the phone. The default project is Hello world. Expand **Run controls** for the mode selector, **Assemble**, and **Step**. **Assemble** validates and resets the VM; **Run** executes and opens Machine; **Step** executes one instruction. Tap line numbers to set breakpoints. **Inspect** shows registers and memory. **Guide** contains searchable instructions with purpose/type filters, and the complete VM manual organized into twelve searchable topics. Memory ranges, input registers, and VM calls are displayed as individual reference cards.

The top-right menu contains sixteen examples, **Machine builder**, named project saving/opening, **Export…**, **Import…**, and separate editor/documentation font settings. Drafts persist automatically; switching projects also saves the previous source under `Autosave — <project>`. Colors follow the system light/dark setting.

## Execution modes and inspector (v2)

Expand **Run controls** to select a mode. Mode and speed preferences persist on the device; changing them keeps the current VM state.

| Mode | Behavior |
|---|---|
| **Slow** | One instruction at a time, with a **1–30 instructions/second** slider. Run opens Inspect. The actual rate may be slightly lower due to UI scheduling. |
| **Default** | Original pacing: up to 12000 instructions / approximately 8 ms per batch, followed by a 16 ms delay. |
| **Fast** | Compiled native ARM64 blocks for register instructions and direct branches, with no deliberate inter-batch delay. Context is loaded once per native quantum (up to 1024 guest instructions), rather than once per arithmetic instruction. Checked memory, indirect branches and VM calls return to C++; active breakpoints use instruction dispatch. Batches still honor the short time budget (checked every 1024 instructions), plus `svc #3`, breakpoints, and faults, so input and Pause stay responsive on endless loops; UI updates are limited to roughly 30 Hz. While a fast run is in flight the inspector hides registers, memory, and instruction context until the program pauses. |

Every mode keeps memory checks and breakpoints. The machine runs indefinitely until you pause it, the program exits with a syscall, or a fault occurs. Step always executes exactly one instruction, independent of mode.

The inspector shows up to seven nearby **assembled instructions**, with original source line numbers, virtual addresses, and breakpoint markers. During Slow execution the highlighted row is the instruction just executed. At a pause/breakpoint it is the next instruction, before execution; after a fault it is the failing instruction; on completion it remains on the last executed instruction. Nearby rows show source order, not branch history. Editing hides old context until reassembly. During a Fast run the inspector shows a placeholder instead of live data; the full context appears as soon as the program pauses.

Pausing from the execution controls and hitting a breakpoint open Inspect automatically. Run/Pause, Step, and Reset are available directly in Inspect.

## Machines, components, and portable projects (v3)

**Machine builder** offers Default (320×240 with keyboard), Phone (320×640 without keyboard), and Custom (multiples of 8, width 160–640, height 120–960). Applying a machine resets execution and keeps source. Attach any combination of the nine component types; each type can appear once. Every GPU has its own display memory. Keyboard controls appear only when attached. The editor's additional execution controls collapse by default.

The read-only discovery table at `DEVICE_TABLE` (`0x00160000`) contains a 16-byte header and 32-byte device descriptors. Search by type and load the supplied base address. Adding/removing components changes addresses. GPU framebuffer addresses come from their registers. The complete ABI, register offsets, examples and limits are available offline under **Guide → VM manual → Virtual components**.

| Type | Component | Implemented interface |
|---|---|---|
| 1 | Minimal GPU | Text, RGB565 and RGB888 framebuffer |
| 2 | Basic GPU | Wrapping 8×8 tile maps, 256-tile atlas, 128 sprites, scrolling and flips |
| 3 | Advanced GPU | Command buffer, model/view/projection matrices, colored/textured triangles, perspective UV and depth buffer |
| 4 | Timer | Monotonic host milliseconds |
| 5 | Touch controller | Primary pointer plus up to ten contact records |
| 6 | Keyboard controller | ASCII/arrows and key-down state |
| 7 | DMA | Asynchronous checked memory copy, completion/status register, overlapping buffers |
| 8 | RNG | Fresh 64-bit pseudo-random samples |
| 9 | Non volatile memory | 64 KiB per machine, 64-byte block reads/writes, atomic app-private persistence |

**Examples → Spinning cube · Advanced GPU** attaches the required GPU and animates six colored faces with perspective, two-axis rotation and depth testing. Run starts the animation; Pause stops it for matrix inspection. The source adapts projection to the configured display and normalizes rotations to avoid drift.

The 3D renderer uses opaque nearest-neighbor textures and both triangle windings. Near-plane crossing triangles are discarded rather than clipped. Each command buffer is limited to 256 records. DMA services completion after at least 1 ms while the VM runs or polls its status; reset cancels requests. These are deliberately small educational devices.

**Export…** offers source only (`.s`), machine only (`.armmachine`), and bundled machine + source + project name (`.armproject`). **Import…** accepts all three through Android's file picker. Machine/project formats use versioned JSON and validate dimensions, components and source limits before applying. Named saves and autosaves retain their machine. NVM stays on the phone and is excluded from exported files; copies of the same machine ID share its persistent state on this phone.

Pixel 10 Pro benchmark (five arithmetic-loop samples, seven graphics samples, medians): Fast executes a four-million-instruction arithmetic loop in **1.73 ms**, versus **190.18 ms** through single-instruction dispatch (~110×). A checked color-field program improves from **44.33 ms** to **16.67 ms** (~2.7×); memory-heavy workloads retain bounds-check/dispatch costs. These figures measure execution, excluding assembly and UI rendering.

## Build and install

Requirements: JDK 17 or 21 **with javac**, Android SDK 35, NDK 28.2.13676358, CMake 3.22.1. Gradle 8.12 is provided by the wrapper. Set `sdk.dir` in an untracked `local.properties` for your SDK installation.

```bash
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n dev.armstudio/.MainActivity
```

On the development machine used here, the complete JDK is:

```bash
JAVA_HOME=/home/kurama/jdk/jdk21/jdk-21.0.12.1+1 ./gradlew :app:assembleDebug
```

Package: `dev.armstudio`. Minimum Android: 11 / API 30. ABI: `arm64-v8a`. The debug APK is signed using the local Android debug key. No internet or storage permission is requested. Native libraries use 16 KiB ELF segment alignment, and executable allocations use the runtime page size, following [Android's native page-size guidance](https://developer.android.com/guide/practices/page-sizes).

## Execution architecture and supported scope

This is a documented **ARM64 subset**, not a Linux process, full GNU assembler, ELF loader, or complete AArch64 ISA implementation. The in-app instruction catalog gives the exact supported syntax. Scalar integer and floating-point arithmetic, comparisons, shifts, and bit operations run as real ARM64 instructions inside a generated trampoline. Instructions are encoded from validated operands; arbitrary machine words and system instructions are rejected.

C++ dispatches bounds-checked loads/stores, branches, virtual stack operations, large immediate/address pseudo-instructions, and custom `svc #0` through `svc #5`. Guest addresses never become host pointers. Guest SP never replaces host SP. The trampoline preserves host callee-saved registers, Android x18, SIMD state required by the ABI, and the FP environment. Generated code is writable while being constructed, then read/execute while running.

Default execution batches yield after 12000 instructions or approximately 8 ms; the deadline is checked every 1024 instructions, so a batch can run at most ~60 us past it. Fast removes the instruction-per-batch cap and inter-batch delay, checks the deadline between native quanta, and keeps `svc #3`, breakpoints, and faults as batch ends. Slow executes one instruction per interval. `svc #3` yields immediately in all modes. The machine runs indefinitely until the user pauses it, the program exits, or a fault occurs. Memory faults, invalid branches, and assembler errors are reported inside the app. Source is capped at 256 KiB, code at 8192 instructions, expressions at 1024 characters, and print calls/transcript at 32768 bytes.

The VM is an in-process educational environment; it is not claimed as an audited security boundary for hostile binaries. SIMD vector operations, atomics, arbitrary system registers, logical immediates, extended/register-offset memory forms, macros, and Linux calls are intentionally unsupported. Floating point supports scalar S/D registers. Terminal glyphs are ASCII, with ANSI color/cursor/clear support.

## Memory and VM calls

All guest memory is little-endian. Unmapped memory faults; no accesses cross region boundaries.

| Address range | Use |
|---|---|
| `00000000–00007FFF` | Maximum code capacity; only actual assembled bytes readable, no writes |
| `00008000–0000FFFF` | Unmapped |
| `00010000–000EFFFF` | 896 KiB RAM: data and descending stack |
| `000F0000–000FFFFF` | Unmapped stack guard; initial SP is `000F0000` |
| `00100000–0014AFFF` | 320×240 framebuffer, 307200 bytes |
| `0014B000–0014FFFF` | Unmapped |
| `00150000–0015001F` | Read-only key, pointer, and instruction-count registers |
| `00160000` | Read-only discovery header and descriptors |
| `00200000` onward | Configured 4096-byte device register pages, bases from descriptors |
| `00800000` onward | Additional GPU framebuffer regions, addresses from GPU registers |
| Everything else | Unmapped |

Default framebuffer modes: text cells (40×30, 4 bytes per cell: ASCII/foreground/background/reserved), RGB565 halfwords, and 32-bit words containing `0x00RRGGBB`. All modes share `0x00100000`. The UI preserves the configured aspect ratio and translates touches into its pixel coordinates. Custom/Phone GPUs have larger framebuffers discovered through registers; the legacy range above remains mapped.

Input u32 registers at `0x00150000`: key code, key-down flag, touch X, touch Y, touch-down flag, reserved zero; followed by u64 instruction ticks at offset 24. Virtual keyboard keys use ASCII; arrows are 256 up, 257 down, 258 left, 259 right.

| Call | Contract |
|---|---|
| `svc #0` | Exit; x0 = code |
| `svc #1` | Clear and set mode; x0 = 0 text, 1 RGB565, 2 RGB888 |
| `svc #2` | Print; x0 = virtual string address, x1 = byte count or 0 for NUL termination |
| `svc #3` | Present/yield to the next frame |
| `svc #4` | Clear; x0 = packed RGB color, or clear text cells |
| `svc #5` | Return the instruction count in x0 |

The comprehensive manual and exact syntax examples live in `app/src/main/cpp/content.cpp` and are accessible without leaving the app.

## Verification

The native suite is cross-compiled and executed on a connected ARM64 Android device:

```bash
./tests/run-device-tests.sh
```

Override `ANDROID_SDK_ROOT` and/or `ANDROID_NDK_HOME` if needed. The script compiles a standalone native executable, pushes it to `/data/local/tmp/armstudio-engine-tests`, and runs it.

Throughput is measured on the device with the same cross-compile toolchain; `dispatch` enforces the 8 ms time budget, `fast` enables compiled blocks (`run(budget, false, true, true)`):

```bash
NDK="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin"
"$NDK/aarch64-linux-android28-clang++" -std=c++20 -O2 -static-libstdc++ -Wl,-z,max-page-size=16384 \
    -Iapp/src/main/cpp tests/benchmark.cpp app/src/main/cpp/engine.cpp \
    app/src/main/cpp/fast.cpp app/src/main/cpp/devices.cpp app/src/main/cpp/device_content.cpp \
    app/src/main/cpp/content.cpp app/src/main/cpp/trampoline.S -o build/tests/benchmark
adb push build/tests/benchmark /data/local/tmp/armstudio-benchmark && adb shell /data/local/tmp/armstudio-benchmark
```

Device UI tests use a dependency-free Instrumentation runner. They temporarily exercise test programs and restore existing draft/project preferences in `finally`:

```bash
./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:lintDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w dev.armstudio.test/dev.armstudio.SmokeInstrumentation
```

Validated on Pixel 10 Pro:

- **70,482 native checks:** integer and float results, host/guest registers, NZCV, memory indexing and sign extension, stack/calls, all six examples (including the endless animation, wall-bounced past 50M instructions), ANSI cells, RGB565/RGB888, touch and keyboard registers, breakpoints/steps, bounds violations, forbidden calls/directives, every instruction-reference template, precise source context across branches/pauses/faults/completion, equivalent results across all three batch sizes and the untimed path, loops past one million instructions, all twelve manual topics, exact Fast/dispatch state comparisons at seven instruction budgets, all nine component demos, moved device bases, read-only descriptors/registers, multitouch records, custom display bounds, texture sampling, depth occlusion, asynchronous DMA, NVM persistence across engine restarts, maximum-size native branch graphs, and 260 cube frames checking visible rotation, stable scale, display aspect and missing-GPU diagnostics.
- **80 UI integration checks:** Run navigation and output, display aspect ratio, memory/register panels, completion insertion, valid-label and invalid-mnemonic lint, gutter breakpoints, stepping, fault recovery, real framebuffer touch events, virtual keyboard events, both guide panels, draft preservation after assembly, adjustable Slow pacing, live mode changes without state loss, Fast pause responsiveness, fast-run inspector withheld until pause, automatic breakpoint inspection, source-context invalidation after edits, manual topic navigation, cross-topic search, collapsible controls, Phone preset and keyboard omission, custom display geometry, machine/source/bundle document round trips, rejected invalid imports, Basic/Advanced GPU rendering, and cube example setup, animation and pause controls.
- Android lint: **no errors**. Remaining warnings concern English-only programmatic strings, intentional ARM64-only support, source-editor LTR layout, backup configuration guidance, and virtual-key accessibility.
- V1 screens were inspected in both system light and dark modes. V3 editor, Phone, tile/sprite, 3D and component manual screens were captured and visually inspected on the device. UI instrumentation restores the original draft/project/settings preferences after testing.
