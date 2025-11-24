#!/bin/bash
set -e

CC="/usr/bin/clang"
CXX="/usr/bin/clang++"

GENERATOR="Ninja"
BUILD_TYPE="${1:-Release}"

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

mkdir -p "$BUILD_DIR"
#export PATH="$QT_PATH/bin:$PATH"

if ! command -v ninja &>/dev/null; then
    echo "Ninja not found, falling back to Unix Makefiles"
    GENERATOR="Unix Makefiles"
fi

echo "Building for $BUILD_TYPE in $BUILD_DIR using generator: $GENERATOR"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_GENERATOR="$GENERATOR" -DCMAKE_COLOR_DIAGNOSTICS=ON -DCMAKE_CXX_FLAGS_INIT="$CXX_FLAGS"
cmake --build "$BUILD_DIR" --target all