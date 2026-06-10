#!/bin/sh
# Install ccalg headers — header-only, no compilation needed.
# Usage: PREFIX=/usr/local sh scripts/install.sh
#        PREFIX=~/.local sh scripts/install.sh

set -e
PREFIX="${PREFIX:-/usr/local}"
DESTDIR="${PREFIX}/include/ccalg"

mkdir -p "$DESTDIR"
cp include/*.h "$DESTDIR"

echo "ccalg installed to $DESTDIR"
ls "$DESTDIR"
