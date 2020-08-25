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

SEQ_BUILD_ARCHIVE=seq-$OS-$ARCH.tar.gz

mkdir -p $SEQ_INSTALL_DIR
cd $SEQ_INSTALL_DIR
curl -L https://github.com/seq-lang/seq/releases/latest/download/$SEQ_BUILD_ARCHIVE | tar zxvf - --strip-components=1

echo ""
echo "Seq installed at: `pwd`"
echo "Make sure to update your PATH environment variable:"
echo "  export PATH=\"`pwd`/bin:\$PATH\""
