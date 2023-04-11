#!/usr/bin/env bash
set -e
set -o pipefail

SEQ_INSTALL_DIR=~/.seq
OS=$(uname -s | awk '{print tolower($0)}')
ARCH="$(uname -m)"

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
curl -L https://github.com/seq-lang/seq/releases/latest/download/"$SEQ_BUILD_ARCHIVE" | tar zxvf - --strip-components=1

EXPORT_COMMAND="export PATH=$(pwd)/bin:\$PATH"
echo "PATH export command:"
echo "  $EXPORT_COMMAND"

update_profile () {
  if ! grep -F -q "$EXPORT_COMMAND" "$1"; then
    read -p "Update PATH in $1? [y/n] " -n 1 -r
    echo

    if [[ $REPLY =~ ^[Yy]$ ]]; then
      { 
        echo Updating "$1"
        echo "# Seq compiler path (added by install script)" >> "$1"
        echo "$EXPORT_COMMAND"
      }
      echo >> "$1"
    else
      echo "Skipping."
    fi
  else
    echo "PATH already updated in $1; skipping update."
  fi
}

if [[ "$SHELL" == *zsh ]]; then
  if [ -e ~/.zshenv ]; then
    update_profile ~/.zshenv
  elif [ -e ~/.zshrc ]; then
    update_profile ~/.zshrc
  else
    echo "Could not find zsh configuration file to update PATH"
  fi
elif [[ "$SHELL" == *bash ]]; then
  if [ -e ~/.bash_profile ]; then
    update_profile ~/.bash_profile
  else
    echo "Could not find bash configuration file to update PATH"
  fi
else
  echo "Don't know how to update configuration file for shell $SHELL"
fi

echo "Seq successfully installed at: $(pwd)"
echo "Open a new terminal session or update your PATH to use seqc"
