#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
SDK_ROOT="${ANDROID_SDK_ROOT:-/home/kurama/android-sdk}"
NDK_ROOT="${ANDROID_NDK_HOME:-$SDK_ROOT/ndk/28.2.13676358}"
mkdir -p build/tests
"$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang++" -std=c++20 -O2 -g -static-libstdc++ -Wall -Wextra -Wl,-z,max-page-size=16384 -Iapp/src/main/cpp tests/engine_tests.cpp app/src/main/cpp/engine.cpp app/src/main/cpp/fast.cpp app/src/main/cpp/devices.cpp app/src/main/cpp/device_content.cpp app/src/main/cpp/content.cpp app/src/main/cpp/trampoline.S -o build/tests/engine-tests
adb push build/tests/engine-tests /data/local/tmp/armstudio-engine-tests
adb shell chmod 755 /data/local/tmp/armstudio-engine-tests
adb shell /data/local/tmp/armstudio-engine-tests
