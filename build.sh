#!/bin/bash
# build.sh [options] [debug/release] [version]
set -e

CC="/usr/bin/clang"          # Your C compiler
CXX="/usr/bin/clang++"       # Your C++ compiler

GENERATOR="Ninja"            # Your C/C++ generator (falls back to Unix Makefiles)
ARCH="$(uname -m)"           # What architecture to build for

BUILD_WITHOUT_VERSION=false  # -v
NO_BUILD_PACKAGES=false      # -p

while getopts "vf:" opt; do
    case $opt in
        v) BUILD_WITHOUT_VERSION=true ;;
        p) NO_BUILD_PACKAGES=true ;;
        *)
            echo "Unknown option: -$OPTARG"
            exit 1
            ;;
    esac
done

# Remove options from positional arguments
shift $((OPTIND - 1))

BUILD_TYPE="${1:-release}"
VERSION="$2"

case "$BUILD_TYPE" in
    Debug|debug)
        BUILD_TYPE="Debug"
        ;;
    Release|release)
        BUILD_TYPE="Release"
        ;;
    *)
        echo "Unknown build type: $BUILD_TYPE (choose debug or release)"
        exit 1
        ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # Root of the project
BUILD_DIR="$ROOT/build/$BUILD_TYPE"
CXX_FLAGS="${CXX_FLAGS:-}"

# Make sure it exists
mkdir -p "$BUILD_DIR"

if ! command -v ninja &>/dev/null; then
    echo "Ninja not found, falling back to Unix Makefiles"
    GENERATOR="Unix Makefiles"
fi

REL_FLAG="__$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')" # main.cpp relies on '__DEBUG' being present to determine debug mode.
echo "Building for $BUILD_TYPE in $BUILD_DIR using generator: $GENERATOR (release flag: $REL_FLAG)"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_GENERATOR="$GENERATOR" -DCMAKE_COLOR_DIAGNOSTICS=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 -DCMAKE_CXX_FLAGS="$CXX_FLAGS -D$REL_FLAG"
cmake --build "$BUILD_DIR" --target all

if [[ "$NO_BUILD_PACKAGES" == "false" ]]; then
    PKG_DIR="$ROOT/build/$BUILD_TYPE-Package"
    echo "Creating package at $PKG_DIR..."
    mkdir -p "$PKG_DIR"

    # Copy a lot of files into the zip
    cp "$ROOT/README.md" "$PKG_DIR/README.md"
    cp "$ROOT/LICENSE" "$PKG_DIR/LICENSE.txt"
    cp "$BUILD_DIR/ItlwmCLI" "$PKG_DIR/ItlwmCLI"

    if [[ "$BUILD_WITHOUT_VERSION" == "false" && -z "$VERSION" && "$BUILD_TYPE" == "Release" ]]; then
        echo "A version should be provided (as positional argument 2) when building in release mode."
        exit 1
    fi

    PKG_NAME="ItlwmCLI-macOS-$ARCH-${VERSION:-build}-$BUILD_TYPE.zip"
    [ -f "$PKG_NAME" ] && rm "$PKG_NAME"
    echo "Building package archive with version ${VERSION:-null} and name $PKG_NAME..."
    zip -rj "$ROOT/build/$PKG_NAME" "$PKG_DIR" # Zip up the files into an archive
fi