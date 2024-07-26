# based on https://github.com/tristanisham/zvm/blob/dad2862a6ebc0cd457b0f472223d874fa49ba867/install.sh

ARCH=$(uname -m)
OS=$(uname -s)

if [ $ARCH = "arm64" ]; then
    ARCH="aarch64"
fi
if [ $ARCH = "amd64" ]; then
    ARCH="x86_64"
fi

wget -q --show-progress --max-redirect 5 -O openclc.tar.gz "https://github.com/e253/openclc/releases/latest/download/openclc-$OS-$ARCH.tar.gz
tar -xvzf openclc.tar.gz -C .
rm openclc.tar.gz
