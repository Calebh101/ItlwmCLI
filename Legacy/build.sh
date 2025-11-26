#!/bin/bash
TARGET=10.5

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # Root of the project
PARENT="$(dirname $ROOT)" # Root of the repo

BUILD_86="$ROOT/build/i386"
BUILD_64="$ROOT/build/x64"

[ -d "$ROOT/build" ] && rm -rf "$ROOT/build"
mkdir -p "$BUILD_86"
mkdir -p "$BUILD_64"

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # REQUIRED: osxcross and 10.5 SDK
    echo "Using osxcross to compile ItlwmCLI for OS X $TARGET..."
    o32-clang++ main.cpp -o "$BUILD_86/ItlwmCLILegacy" -mmacosx-version-min=$TARGET
    o64-clang++ main.cpp -o "$BUILD_64/ItlwmCLILegacy" -mmacosx-version-min=$TARGET
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Needs to be run on macOS 10.15
    echo "Using g++ to compile ItlwmCLI for OS X $TARGET..."
    g++ main.cpp -o "$BUILD_86/ItlwmCLILegacy" -arch i386 -mmacosx-version-min=$TARGET
    g++ main.cpp -o "$BUILD_64/ItlwmCLILegacy" -arch x86_64 -mmacosx-version-min=$TARGET
else
    echo "Unknown OS: $OSTYPE"
    exit 1
fi

echo "Copying files..."
cp "$PARENT/README.md" "$BUILD_86/README.md"
cp "$PARENT/LICENSE" "$BUILD_86/LICENSE.txt"
cp "$PARENT/README.md" "$BUILD_64/README.md"
cp "$PARENT/LICENSE" "$BUILD_64/LICENSE.txt"

echo "Job complete!"