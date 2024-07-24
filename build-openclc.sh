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
ZIG="/home/esteere/.zvm/bin/zig"

mkdir -p "$ROOTDIR/out/build-openclc-$TARGET-$MCPU"
cd "$ROOTDIR/out/build-openclc-$TARGET-$MCPU"
cmake "$ROOTDIR/openclc" \
  -DCMAKE_INSTALL_PREFIX="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_PREFIX_PATH="$ROOTDIR/out/$TARGET-$MCPU" \
  -DCMAKE_BUILD_TYPE="$3" \
  -DCMAKE_CROSSCOMPILING=True \
  -DCMAKE_SYSTEM_NAME="$TARGET_OS_CMAKE" \
  -DCMAKE_CXX_COMPILER="$ZIG;c++;-fno-sanitize=all;-s;-target;$TARGET;-mcpu=$MCPU" \
  -DOPENCLC_ZIG_BUILD_LIB="$ZIG;build-lib;-target;$TARGET;-mcpu=$MCPU" \
  -DCMAKE_AR="/usr/bin/llvm-ar-18" \
  -DCMAKE_RANLIB="/usr/bin/llvm-ranlib-18" \
  -DCMAKE_CXX_FLAGS="-std=c++20"

if [ $TARGET_OS_CMAKE = "Windows"]; then
  cp "$ROOTDIR/out/build-openclc-$TARGET-$MCPU/compile_commands.json" "$ROOTDIR/compile_commands.json"
fi

make install
