#!/bin/bash
set -e

CC="/usr/bin/clang"
CXX="/usr/bin/clang++"

GENERATOR="Ninja"
BUILD_TYPE="${1:-release}"
ARCH="$(uname -m)"

case "$BUILD_TYPE" in
    Debug|debug)
        BUILD_TYPE="Debug"
        ;;
    Release|release)
        BUILD_TYPE="Release"
        ;;
    *)
        echo "Unknown build type: $BUILD_TYPE (choose Debug or Release)"
        exit 1
        ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build/$BUILD_TYPE"
CXX_FLAGS="${CXX_FLAGS:-}"
VERSION="$2"

mkdir -p "$BUILD_DIR"

if ! command -v ninja &>/dev/null; then
    echo "Ninja not found, falling back to Unix Makefiles"
    GENERATOR="Unix Makefiles"
fi

REL_FLAG="__$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')"
echo "Building for $BUILD_TYPE in $BUILD_DIR using generator: $GENERATOR (release flag: $REL_FLAG)"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_GENERATOR="$GENERATOR" -DCMAKE_COLOR_DIAGNOSTICS=ON -DCMAKE_CXX_FLAGS="$CXX_FLAGS -D$REL_FLAG"
cmake --build "$BUILD_DIR" --target all

PKG_DIR="$ROOT/build/$BUILD_TYPE-Package"
echo "Creating package at $PKG_DIR..."
mkdir -p "$PKG_DIR"

cp "$ROOT/README.md" "$PKG_DIR/README.md"
cp "$ROOT/LICENSE" "$PKG_DIR/LICENSE.txt"
cp "$BUILD_DIR/ItlwmCLI" "$PKG_DIR/ItlwmCLI"

if [[ -z "$VERSION" && "$BUILD_TYPE" == "Release" ]]; then
    echo "A version should be provided (as positional argument 2) when building in release mode."
    exit 1
fi

PKG_NAME="ItlwmCLI-$ARCH-${VERSION:-UNKNOWN}-$BUILD_TYPE.zip"
[ -f "$PKG_NAME" ] && rm "$PKG_NAME"
echo "Building package archive with version ${VERSION:-UNKNOWN} name $PKG_NAME..."
zip -rj "$ROOT/build/$PKG_NAME" "$PKG_DIR"