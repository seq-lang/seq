#!/usr/bin/env bash
set -e
set -o pipefail

SEQ_INSTALL_DIR=~/.seq
OS=`uname -s | awk '{print tolower($0)}'`
ARCH=`uname -m`

if [ "$OS" != "linux" ] && [ "$OS" != "darwin" ]; then
  echo "error: Pre-built binaries only exist for Linux and macOS." >&2
  exit 1
fi

if [ "$ARCH" != "x86_64" ]; then
  echo "error: Pre-built binaries only exist for x86_64." >&2
  exit 1
fi

case "$OS" in
  "linux")  LIB_PATH_VAR="LD_LIBRARY_PATH" ;;
  "darwin") LIB_PATH_VAR="DYLD_LIBRARY_PATH" ;;
  *) exit 1 ;;
esac

SEQ_BUILD_ARCHIVE=seq-$OS-$ARCH.tar.gz
SEQ_STDLIB_ARCHIVE=seq-stdlib.tar.gz

mkdir -p $SEQ_INSTALL_DIR
cd $SEQ_INSTALL_DIR
wget -c https://github.com/seq-lang/seq/releases/latest/download/$SEQ_BUILD_ARCHIVE -O - | tar -xz
wget -c https://github.com/seq-lang/seq/releases/latest/download/$SEQ_STDLIB_ARCHIVE -O - | tar -xz
cp build/seqc build/libseq.* build/libseqrt.* .
rm -rf build

echo ""
echo "Seq installed at: `pwd`"
echo "Make sure to add the following lines to ~/.bash_profile:"
echo "  export PATH=\"`pwd`:\$PATH\""
echo "  export SEQ_PATH=\"`pwd`/stdlib\""
echo "  export $LIB_PATH_VAR=\"\$$LIB_PATH_VAR:`pwd`\""
