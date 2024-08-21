set -eu

ROOTDIR=$(pwd)

TARGET="$1"
MCPU="$2"

ARCH_AND_OS=${TARGET%-*}
if [[ $TARGET == *"macos"* ]]; then
    ARCH_AND_OS=$TARGET
fi

RELEASE_NAME="openclc-$ARCH_AND_OS"
mkdir -p "$ROOTDIR/release/$RELEASE_NAME"

if [ $ARCH_AND_OS = "x86_64-windows" ]; then
    EXE=".exe"
else
    EXE=""
fi

cp "$ROOTDIR/out/$TARGET-$MCPU/bin/openclc$EXE" "$ROOTDIR/release/$RELEASE_NAME/openclc$EXE"
cp "$ROOTDIR/runtime/openclc_rt.c" "$ROOTDIR/release/$RELEASE_NAME/openclc_rt.c"
cp "$ROOTDIR/runtime/openclc_rt.h" "$ROOTDIR/release/$RELEASE_NAME/openclc_rt.h"

if [ $ARCH_AND_OS = "x86_64-windows" ]; then
    7z a -tzip "$ROOTDIR/release/$RELEASE_NAME.zip" "$ROOTDIR/release/$RELEASE_NAME/*"
else
    cd "$ROOTDIR/release/"
    tar czvf "$RELEASE_NAME.tar.gz" "$RELEASE_NAME"
fi

rm -r $ROOTDIR/release/$RELEASE_NAME
