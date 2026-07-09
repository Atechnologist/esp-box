#!/bin/bash

set -e

echo "======================================="
echo " ESP-BOX OS Build System"
echo "======================================="

ROOT=$(pwd)
PATCHES=$ROOT/patches

echo ""
echo "Cleaning..."

rm -rf "$PATCHES"

mkdir -p "$PATCHES"

mkdir -p "$PATCHES/core"
mkdir -p "$PATCHES/hardware"
mkdir -p "$PATCHES/services"
mkdir -p "$PATCHES/ai"

echo ""
echo "Generating core..."

mkdir -p "$PATCHES/core/espnow_manager"
mkdir -p "$PATCHES/core/command_dispatcher"
mkdir -p "$PATCHES/core/node_registry"
mkdir -p "$PATCHES/core/logger"

echo ""
echo "Generating hardware..."

mkdir -p "$PATCHES/hardware/relay_manager"
mkdir -p "$PATCHES/hardware/audio_manager"
mkdir -p "$PATCHES/hardware/light_manager"
mkdir -p "$PATCHES/hardware/touch_manager"

echo ""
echo "Generating services..."

mkdir -p "$PATCHES/services/webui"
mkdir -p "$PATCHES/services/websocket"
mkdir -p "$PATCHES/services/ota"

echo ""
echo "Generating AI..."

mkdir -p "$PATCHES/ai/offline"
mkdir -p "$PATCHES/ai/online"

echo ""
echo "Done."

find "$PATCHES" -maxdepth 3
