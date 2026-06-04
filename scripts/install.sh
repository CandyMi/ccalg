#!/bin/sh
# Install alg headers — header-only, no compilation needed.
# Usage: PREFIX=/usr/local sh scripts/install.sh
#        PREFIX=~/.local sh scripts/install.sh

set -e
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${PREFIX}/include/alg"

mkdir -p "$DESTDIR"
cp include/*.h "$DESTDIR"

echo "alg installed to $DESTDIR"
ls "$DESTDIR"
