#!/bin/bash
set -e

TARGET=10.5
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # Root of the project
PARENT="$(dirname $ROOT)" # Root of the repo

BUILD_86="$ROOT/build/i386"
BUILD_64="$ROOT/build/x64"

BUILD_WITHOUT_VERSION=false
OPTIONS_STRING_RAW=""

while getopts "vf:" opt; do
    case $opt in
        v) BUILD_WITHOUT_VERSION=true ;;
        *)
            echo "Unknown option: -$OPTARG"
            exit 1
            ;;
    esac

    OPTIONS_STRING_RAW+="$opt"
done


if [[ -n "$OPTIONS_STRING_RAW" ]]; then
    OPTIONS_STRING="-$OPTIONS_STRING_RAW"
else
    OPTIONS_STRING=""
fi

shift $((OPTIND - 1)) # Remove options from positional arguments
VERSION=$1

[ -d "$ROOT/build" ] && rm -rf "$ROOT/build"
mkdir -p "$BUILD_86"
mkdir -p "$BUILD_64"

TARGET_FILES="$ROOT/main.cpp -x c -include unistd.h $PARENT/HeliPort/ClientKit/Api.c"
EXTRAFLAGS="-I$ROOT/include -I$PARENT/HeliPort/ClientKit -framework CoreFoundation -framework IOKit"

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Needs osxcross and 10.5 SDK
    echo "Using osxcross to compile ItlwmCLI for OS X $TARGET with version ${VERSION:-null}..."
    o32-clang++ $TARGET_FILES -o "$BUILD_86/ItlwmCLILegacy" $EXTRAFLAGS -mmacosx-version-min=$TARGET
    o64-clang++ $TARGET_FILES -o "$BUILD_64/ItlwmCLILegacy" $EXTRAFLAGS -mmacosx-version-min=$TARGET
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Needs to be run on macOS 10.5
    echo "Using g++ to compile ItlwmCLI for OS X $TARGET with version ${VERSION:-null}..."
    g++ $TARGET_FILES -o "$BUILD_86/ItlwmCLILegacy" -arch i386 $EXTRAFLAGS -mmacosx-version-min=$TARGET
    g++ $TARGET_FILES -o "$BUILD_64/ItlwmCLILegacy" -arch x86_64 $EXTRAFLAGS -mmacosx-version-min=$TARGET
else
    echo "Unknown OS: $OSTYPE"
    exit 1
fi

echo "Copying files..."
cp "$PARENT/README.md" "$BUILD_86/README.md"
cp "$ROOT/README.md" "$BUILD_86/README-too.md"
cp "$PARENT/LICENSE" "$BUILD_86/LICENSE.txt"
cp "$PARENT/README.md" "$BUILD_64/README.md"
cp "$ROOT/README.md" "$BUILD_64/README-too.md"
cp "$PARENT/LICENSE" "$BUILD_64/LICENSE.txt"

if [[ -n "$VERSION" ]]; then
    echo "$VERSION" > "$BUILD_86/VERSION.txt"
    echo "$VERSION" > "$BUILD_64/VERSION.txt"
fi

if [[ "$BUILD_WITHOUT_VERSION" == "false" && -z "$VERSION" ]]; then
    echo "A version should be provided (as positional argument 1) when building."
    exit 1
fi

PKG_NAME_32="$ROOT/build/ItlwmCLI-Legacy-OSX-i386-${VERSION:-build}.zip"
PKG_NAME_64="$ROOT/build/ItlwmCLI-Legacy-OSX-x64-${VERSION:-build}.zip"

echo "Building package archives with version ${VERSION:-null}..."
zip -rj "$PKG_NAME_32" "$ROOT/build/i386"
zip -rj "$PKG_NAME_64" "$ROOT/build/x64"

echo "Job complete! Make sure you updated the version/beta in main.cpp!"