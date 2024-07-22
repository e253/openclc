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

# Now we have Zig as a cross compiler.
ZIG="$HOME/.zvm/bin/zig"

# First cross compile zlib for the target, as we need the LLVM linked into
# the final zig binary to have zlib support enabled.
mkdir -p "$ROOTDIR/out/build-zlib-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-zlib-$TARGET-$MCPU"
cmake "$ROOTDIR/zlib" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_ASM_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_RC_COMPILER="/usr/bin/llvm-rc-18" \
  -DCMAKE_AR="/usr/bin/llvm-ar-18" \
  -DCMAKE_RANLIB="/usr/bin/llvm-ranlib-18"
cmake --build . --target install --parallel $(nproc)

# Same deal for zstd.
# The build system for zstd is whack so I just put all the files here.
mkdir -p "$ROOTDIR/out/$TARGET-$MCPU/lib"
cp "$ROOTDIR/zstd/lib/zstd.h" "$ROOTDIR/out/$TARGET-$MCPU/include/zstd.h"
cd "$ROOTDIR/out/$TARGET-$MCPU/lib"
$ZIG build-lib \
  --name zstd \
  -target $TARGET \
  -mcpu=$MCPU \
  -fstrip -OReleaseFast \
  -lc \
  "$ROOTDIR/zstd/lib/decompress/zstd_ddict.c" \
  "$ROOTDIR/zstd/lib/decompress/zstd_decompress.c" \
  "$ROOTDIR/zstd/lib/decompress/huf_decompress.c" \
  "$ROOTDIR/zstd/lib/decompress/huf_decompress_amd64.S" \
  "$ROOTDIR/zstd/lib/decompress/zstd_decompress_block.c" \
  "$ROOTDIR/zstd/lib/compress/zstdmt_compress.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_opt.c" \
  "$ROOTDIR/zstd/lib/compress/hist.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_ldm.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_fast.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_compress_literals.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_double_fast.c" \
  "$ROOTDIR/zstd/lib/compress/huf_compress.c" \
  "$ROOTDIR/zstd/lib/compress/fse_compress.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_lazy.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_compress.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_compress_sequences.c" \
  "$ROOTDIR/zstd/lib/compress/zstd_compress_superblock.c" \
  "$ROOTDIR/zstd/lib/deprecated/zbuff_compress.c" \
  "$ROOTDIR/zstd/lib/deprecated/zbuff_decompress.c" \
  "$ROOTDIR/zstd/lib/deprecated/zbuff_common.c" \
  "$ROOTDIR/zstd/lib/common/entropy_common.c" \
  "$ROOTDIR/zstd/lib/common/pool.c" \
  "$ROOTDIR/zstd/lib/common/threading.c" \
  "$ROOTDIR/zstd/lib/common/zstd_common.c" \
  "$ROOTDIR/zstd/lib/common/xxhash.c" \
  "$ROOTDIR/zstd/lib/common/debug.c" \
  "$ROOTDIR/zstd/lib/common/fse_decompress.c" \
  "$ROOTDIR/zstd/lib/common/error_private.c" \
  "$ROOTDIR/zstd/lib/dictBuilder/zdict.c" \
  "$ROOTDIR/zstd/lib/dictBuilder/divsufsort.c" \
  "$ROOTDIR/zstd/lib/dictBuilder/fastcover.c" \
  "$ROOTDIR/zstd/lib/dictBuilder/cover.c"

# Rebuild LLVM with Zig.
mkdir -p "$ROOTDIR/out/build-llvm-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-llvm-$TARGET-$MCPU"
cmake "$ROOTDIR/llvm" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_ASM_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_RC_COMPILER="/usr/bin/llvm-rc-18" \
  -DCMAKE_AR="/usr/bin/llvm-ar-18" \
  -DCMAKE_RANLIB="/usr/bin/llvm-ranlib-18" \
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
  -DLLVM_ENABLE_ZLIB=FORCE_ON \
  -DLLVM_ENABLE_ZSTD=FORCE_ON \
  -DLLVM_USE_STATIC_ZSTD=ON \
  -DLLVM_INCLUDE_UTILS=OFF \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_DOCS=OFF \
  -DLLVM_DEFAULT_TARGET_TRIPLE="spirv64-unknown-unknown" \
  -DLLVM_TOOL_LLVM_LTO2_BUILD=OFF \
  -DLLVM_TOOL_LLVM_LTO_BUILD=OFF \
  -DLLVM_TOOL_LTO_BUILD=OFF \
  -DLLVM_TOOL_REMARKS_SHLIB_BUILD=OFF \
  -DLLVM_BUILD_UTILS=ON \
  -DLLVM_BUILD_TOOLS=ON \
  -DLLVM_BUILD_STATIC=ON \
  -DCLANG_BUILD_TOOLS=ON \
  -DCLANG_INCLUDE_DOCS=OFF \
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
  -DLLVM_TABLEGEN="/usr/bin/llvm-tblgen-18" \
  -DCLANG_TABLEGEN="/usr/bin/clang-tblgen-18"

# RPATH install breaks install becuase we have static executables
find . -type f -name "*_install.cmake" -exec sed -i '/file(RPATH_CHANGE/,+3d' {} \;

ninja install

mkdir -p "$ROOTDIR/out/build-spv-headers"
cd "$ROOTDIR/out/build-spv-headers"
cmake "$ROOTDIR/SPIRV-Headers" \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU"
make install

mkdir -p "$ROOTDIR/out/build-spv-tools-headers"
cd "$ROOTDIR/out/build-spv-tools-headers"
cmake "$ROOTDIR/SPIRV-Tools" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_ASM_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_RC_COMPILER="/usr/bin/llvm-rc-18" \
  -DCMAKE_AR="/usr/bin/llvm-ar-18" \
  -DCMAKE_RANLIB="/usr/bin/llvm-ranlib-18" \
  -DSPIRV_SKIP_TESTS=ON \
  -DSPIRV_HEADERS_INCLUDE_DIR="$ROOTDIR/out/$TARGET-$MCPU"
  #-DSPIRV-Headers_SOURCE_DIR="$ROOTDIR/SPIRV-Headers"
ninja install


mkdir -p "$ROOTDIR/out/build-spv-translator-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-spv-translator-$TARGET-$MCPU"
cmake "$ROOTDIR/SPIRV-LLVM-Translator" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_ASM_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_RC_COMPILER="/usr/bin/llvm-rc-18" \
  -DCMAKE_AR="/usr/bin/llvm-ar-18" \
  -DCMAKE_RANLIB="/usr/bin/llvm-ranlib-18" \
  -DLLVM_DIR="$ROOTDIR/out/$TARGET-$MCPU/lib/cmake/llvm" \
  -DLLVM_SPIRV_BUILD_EXTERNAL=YES \
  -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR="$ROOTDIR/out/$TARGET-$MCPU" \
  -DZLIB_USE_STATIC_LIBS=ON \
  -DZLIB_ROOT="$ROOTDIR/out/$TARGET-$MCPU/lib" \
  -Dzstd_ROOT="$ROOTDIR/out/$TARGET-$MCPU/lib" \
  -Dzstd_INCLUDE_DIR="$ROOTDIR/out/$TARGET-$MCPU/include"
cmake --build . --target install --parallel $(nproc)

mkdir -p "$ROOTDIR/out/build-openclc-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-openclc-$TARGET-$MCPU"
cmake "$ROOTDIR/openclc" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_C_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_ASM_COMPILER="$ZIG;cc;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_RC_COMPILER="/usr/bin/llvm-rc-18" \
  -DCMAKE_AR="/usr/bin/llvm-ar-18" \
  -DCMAKE_RANLIB="/usr/bin/llvm-ranlib-18"
cp "$ROOTDIR/out/build-openclc-$TARGET-$MCPU/compile_commands.json" "$ROOTDIR/compile_commands.json"
