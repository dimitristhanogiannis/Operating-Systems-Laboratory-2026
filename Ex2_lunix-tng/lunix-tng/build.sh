#!/bin/bash
set -e

SRC_DIR="$(pwd)"
BUILD_DIR="/tmp/lunix_local_build"

echo "===> [1/4] Συγχρονισμός αρχείων στον τοπικό κατάλογο..."
mkdir -p "$BUILD_DIR"
rsync -av --delete \
    --exclude='*.ko' \
    --exclude='*.o' \
    --exclude='lunix-attach' \
    --exclude='mk-lunix-lookup' \
    --exclude='lunix-lookup.h' \
    --exclude='.*' \
    "$SRC_DIR/" "$BUILD_DIR/"

echo "===> [2/4] Μεταγλώττιση kernel module (lunix.ko)..."
cd "$BUILD_DIR"
make

echo "===> [3/4] Μεταγλώττιση lunix-attach..."
gcc -Wall -O2 -o lunix-attach lunix-attach.c

echo "===> [4/4] Αντιγραφή εκτελέσιμων πίσω στον κοινόχρηστο φάκελο..."
cp -f "$BUILD_DIR/lunix.ko" "$SRC_DIR/"
cp -f "$BUILD_DIR/lunix-attach" "$SRC_DIR/"

echo "===> Build completed successfully!"
ls -la "$SRC_DIR/lunix.ko" "$SRC_DIR/lunix-attach"
