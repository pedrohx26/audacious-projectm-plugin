#!/bin/bash
# scripts/build.sh – Configure and build the plugin
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "==> Configuring (${BUILD_TYPE})..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "==> Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "==> Done.  Plugin: $BUILD_DIR/projectm-vis.so"
echo "    Run:  scripts/install.sh"
