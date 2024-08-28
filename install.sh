# based on https://github.com/tristanisham/zvm/blob/dad2862a6ebc0cd457b0f472223d874fa49ba867/install.sh

ARCH=$(uname -m)
OS=$(uname -s)

if [ $ARCH = "arm64" ]; then
    ARCH="aarch64"
fi
if [ $ARCH = "amd64" ]; then
    ARCH="x86_64"
fi

cd /tmp
wget -q --show-progress --max-redirect 5 -O openclc.tar.gz "https://github.com/e253/openclc/releases/latest/download/openclc-$ARCH-$OS.tar.gz"
mkdir -p $HOME/.openclc
tar -xvzf openclc.tar.gz -C $HOME/.openclc --strip-components 1
rm openclc.tar.gz

echo ""
echo ""
echo "Add to your .bashrc or .profile"
echo ""
echo "OPENCLC_INSTALL=$HOME/.openclc"
echo "PATH=\$PATH:\$OPENCLC_INSTALL"
echo ""
