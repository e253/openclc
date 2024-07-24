#!/bin/sh
# This file is licensed under the public domain.
# https://github.com/ziglang/zig-bootstrap/commit/98bc6bf4fc4009888d33941daf6b600d20a42a56 

set -eu

TARGET="$1" # Example: riscv64-linux-gnu
MCPU="$2" # Examples: `baseline`, `native`, `generic+v7a`, or `arm1176jzf_s`

ROOTDIR="$(pwd)"
ZIG_VERSION="0.13.0"

TARGET_OS_AND_ABI=${TARGET#*-} # Example: linux-gnu

# Here we map the OS from the target triple to the value that CMake expects.
TARGET_OS_CMAKE=${TARGET_OS_AND_ABI%-*} # Example: linux
case $TARGET_OS_CMAKE in
  macos) TARGET_OS_CMAKE="Darwin";;
  freebsd) TARGET_OS_CMAKE="FreeBSD";;
  windows) TARGET_OS_CMAKE="Windows";;
  linux) TARGET_OS_CMAKE="Linux";;
  native) TARGET_OS_CMAKE="";;
esac

ZIG="$HOME/.zvm/bin/zig"
LLVM_AR="${LLVM_AR:-/usr/lib/llvm-18/bin/llvm-ar}"
LLVM_RANLIB="${LLVM_RANLIB:-/usr/lib/llvm-18/bin/llvm-ranlib}"
LLVM_TBLGEN="${LLVM_TBLGEN:-/usr/lib/llvm-18/bin/llvm-tblgen}"
CLANG_TBLGEN="${CLANG_TBLGEN:-/usr/lib/llvm-18/bin/clang-tblgen}"

mkdir -p "$ROOTDIR/out/build-llvm-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-llvm-$TARGET-$MCPU"
cmake "$ROOTDIR/third_party/llvm" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_ASM_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_AR="$LLVM_AR" \
  -DCMAKE_RANLIB="$LLVM_RANLIB" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DLLVM_ENABLE_BINDINGS=OFF \
  -DLLVM_ENABLE_LIBXML2=OFF \
  -DLLVM_ENABLE_OCAMLDOC=OFF \
  -DLLVM_ENABLE_Z3_SOLVER=OFF \
  -DLLVM_ENABLE_BACKTRACES=OFF \
  -DLLVM_ENABLE_CRASH_OVERRIDES=OFF \
  -DLLVM_ENABLE_LIBEDIT=OFF \
  -DLLVM_ENABLE_LIBPFM=OFF \
  -DLLVM_ENABLE_PLUGINS=OFF \
  -DLLVM_ENABLE_TERMINFO=OFF \
  -DLLVM_ENABLE_RUNTIMES="" \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_ENABLE_ZLIB=OFF \
  -DLLVM_ENABLE_ZSTD=OFF \
  -DLLVM_USE_STATIC_ZSTD=OFF \
  -DLLVM_INCLUDE_UTILS=ON \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_DOCS=OFF \
  -DLLVM_TOOL_LLVM_LTO2_BUILD=OFF \
  -DLLVM_TOOL_LLVM_LTO_BUILD=OFF \
  -DLLVM_TOOL_LTO_BUILD=OFF \
  -DLLVM_TOOL_REMARKS_SHLIB_BUILD=OFF \
  -DLLVM_BUILD_STATIC=ON \
  -DCLANG_INCLUDE_TESTS=OFF \
  -DCLANG_ENABLE_ARCMT=OFF \
  -DCLANG_ENABLE_STATIC_ANALYZER=OFF \
  -DCLANG_ENABLE_BOOTSTRAP=OFF \
  -DCLANG_TOOL_CLANG_IMPORT_TEST_BUILD=OFF \
  -DCLANG_TOOL_CLANG_LINKER_WRAPPER_BUILD=OFF \
  -DCLANG_TOOL_C_INDEX_TEST_BUILD=OFF \
  -DCLANG_TOOL_ARCMT_TEST_BUILD=OFF \
  -DCLANG_TOOL_C_ARCMT_TEST_BUILD=OFF \
  -DCLANG_TOOL_LIBCLANG_BUILD=OFF \
  -DLLVM_TABLEGEN="$LLVM_TBLGEN" \
  -DCLANG_TABLEGEN="$CLANG_TBLGEN"
# RPATH change breaks install becuase we have static executables
find . -type f -name "*_install.cmake" -exec sed -i '/file(RPATH_CHANGE/,+3d' {} \;
ninja install


mkdir -p "$ROOTDIR/out/build-spv-tools-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-spv-tools-$TARGET-$MCPU"
cmake "$ROOTDIR/third_party/SPIRV-Tools" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_AR="$LLVM_AR" \
  -DCMAKE_RANLIB="$LLVM_RANLIB" \
  -DSPIRV_SKIP_TESTS=ON \
  -DSPIRV-Headers_SOURCE_DIR="$ROOTDIR/third_party/SPIRV-Headers"
ninja install


mkdir -p "$ROOTDIR/out/build-spv-translator-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-spv-translator-$TARGET-$MCPU"
cmake "$ROOTDIR/third_party/SPIRV-LLVM-Translator" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_AR="$LLVM_AR" \
  -DCMAKE_RANLIB="$LLVM_RANLIB" \
  -DLLVM_DIR="$ROOTDIR/out/$TARGET-$MCPU/lib/cmake/llvm" \
  -DLLVM_SPIRV_INCLUDE_TESTS=ON \
  -DLLVM_EXTERNAL_LIT="/usr/lib/llvm-18/build/utils/lit/lit.py" \
  -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR="$ROOTDIR/third_party/SPIRV-Headers"
ninja install
