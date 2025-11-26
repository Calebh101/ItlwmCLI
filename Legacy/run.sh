#!/bin/bash
# run.sh [version] [architecture]
set -e

VERSION=$1
ARCH=${2:-x64}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building ItlwmCLILegacy version ${VERSION:-null}..."
"$ROOT/build.sh" $VERSION

echo "Copying ItlwmCLILegacy with architecture $ARCH..."
mkdir -p $HOME/.darling/Users/$USER
cp "$ROOT/build/$ARCH/ItlwmCLILegacy" "$HOME/.darling/Users/$USER/ItlwmCLILegacy"
chmod +x "$HOME/.darling/Users/$USER/ItlwmCLILegacy"

echo -e "Running ItlwmCLILegacy as user $USER...\n"
darling shell /Users/$USER/ItlwmCLILegacy
echo -e "\nJob complete!"