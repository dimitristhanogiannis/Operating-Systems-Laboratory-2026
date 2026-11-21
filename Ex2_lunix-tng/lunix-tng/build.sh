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
    --exclude='lunix-ioctl-compare' \
    --exclude='lunix-logger' \
    --exclude='mmap_use_case' \
    --exclude='mk-lunix-lookup' \
    --exclude='lunix-lookup.h' \
    --exclude='.*' \
    "$SRC_DIR/" "$BUILD_DIR/"

echo "===> [2/4] Μεταγλώττιση kernel module (lunix.ko)..."
cd "$BUILD_DIR"
make

echo "===> [3/4] Μεταγλώττιση εκτελέσιμων και testing προγραμμάτων..."
gcc -Wall -O2 -o lunix-attach lunix-attach.c

if [ -f "lunix-ioctl-compare.c" ]; then
    echo "     -> Μεταγλώττιση lunix-ioctl-compare..."
    gcc -Wall -O2 -o lunix-ioctl-compare lunix-ioctl-compare.c
fi

if [ -f "lunix-logger.c" ]; then
    echo "     -> Μεταγλώττιση lunix-logger..."
    gcc -Wall -O2 -o lunix-logger lunix-logger.c
fi

if [ -f "mmap_use_case.c" ]; then
    echo "     -> Μεταγλώττιση mmap_use_case..."
    gcc -Wall -O2 -o mmap_use_case mmap_use_case.c
fi

echo "===> [4/4] Αντιγραφή εκτελέσιμων πίσω στον κοινόχρηστο φάκελο..."
cp -f "$BUILD_DIR/lunix.ko" "$SRC_DIR/"
cp -f "$BUILD_DIR/lunix-attach" "$SRC_DIR/"

[ -f "$BUILD_DIR/lunix-ioctl-compare" ] && cp -f "$BUILD_DIR/lunix-ioctl-compare" "$SRC_DIR/"
[ -f "$BUILD_DIR/lunix-logger" ] && cp -f "$BUILD_DIR/lunix-logger" "$SRC_DIR/"
[ -f "$BUILD_DIR/mmap_use_case" ] && cp -f "$BUILD_DIR/mmap_use_case" "$SRC_DIR/"

echo "===> Build completed successfully!"
ls -la "$SRC_DIR/lunix.ko" \
       "$SRC_DIR/lunix-attach" \
       "$SRC_DIR/lunix-ioctl-compare" \
       "$SRC_DIR/lunix-logger" \
       "$SRC_DIR/mmap_use_case" 2>/dev/null || true
