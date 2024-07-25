set -eu

ROOTDIR=$(pwd)

TARGET="$1"
MCPU="$2"

OS_AND_ABI=${TARGET#*-}
OS=${OS_AND_ABI%-*}

OPENCLC_VERSION=$($ROOTDIR/out/x86_64-linux-musl-x86_64/bin/openclc --version)
RELEASE_NAME="openclc-$OPENCLC_VERSION-$TARGET"
mkdir -p "$ROOTDIR/release/$RELEASE_NAME"

if [ $OS = "windows" ]; then
    EXE=".exe"
else
    EXE=""
fi

cp "$ROOTDIR/out/$TARGET-$MCPU/bin/openclc$EXE" "$ROOTDIR/release/$RELEASE_NAME/openclc$EXE"
cp "$ROOTDIR/out/$TARGET-$MCPU/bin/spirv-as$EXE" "$ROOTDIR/release/$RELEASE_NAME/spv-as$EXE"
cp "$ROOTDIR/out/$TARGET-$MCPU/bin/spirv-dis$EXE" "$ROOTDIR/release/$RELEASE_NAME/spv-dis$EXE"
cp "$ROOTDIR/out/$TARGET-$MCPU/bin/spirv-link$EXE" "$ROOTDIR/release/$RELEASE_NAME/spv-link$EXE"
cp "$ROOTDIR/out/$TARGET-$MCPU/bin/spirv-opt$EXE" "$ROOTDIR/release/$RELEASE_NAME/spv-opt$EXE"
cp "$ROOTDIR/out/$TARGET-$MCPU/bin/llvm-spirv$EXE" "$ROOTDIR/release/$RELEASE_NAME/llvm-spirv$EXE"

if [ $OS = "windows" ]; then
    7z a -tzip "$ROOTDIR/release/$RELEASE_NAME.zip" "$ROOTDIR/release/$RELEASE_NAME/*"
else
    tar czvf "$ROOTDIR/release/$RELEASE_NAME.tar.gz" "$ROOTDIR/release/$RELEASE_NAME"
fi

rm -r $ROOTDIR/release/$RELEASE_NAME
