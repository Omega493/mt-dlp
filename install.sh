#!/bin/bash
red='\033[0;31m'
green='\033[0;32m'
yellow='\033[0;33m'
cyan='\033[0;36m'
reset='\033[0m'

log() {
  printf "%b%s%b\n" "$1" "$2" "$reset"
}

trap 'log "$red" "Script interrupted by user."; exit 1' INT

if [ "$(id -u)" -eq 0 ]; then
  log "$red" "Do not run this script as sudo."
  log "$yellow" "It will ask for password when necessary."
  exit 1
fi

debug_mode=false

for arg in "$@"; do
  case $arg in
    --linux-debug)
      debug_mode=true
      ;;
  esac
done

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Call build.sh with all the arguments
log "$cyan" "Running build script..."
if ! bash "${SCRIPT_DIR}/build.sh" "$@"; then
  log "$red" "Build failed"
  exit 1
fi

# Determine target preset (same logic as build.sh)
if [ -f "build.linux-debug" ]; then
  target_preset="linux-debug"
else
  target_preset="linux-release"
fi

if [ "$debug_mode" = false ] && [ "$target_preset" = "linux-release" ]; then
  log "$cyan" "Stripping executable..."
  strip build/linux/linux-release/mt-dlp
fi

log "$cyan" "Installing..."

BUILD_DIR="build/linux/${target_preset}"

if [ ! -d "$BUILD_DIR" ]; then
  log "$red" "Error: Build directory '${BUILD_DIR}' not found."
  exit 1
fi

if ! sudo cmake --install "$BUILD_DIR"; then
  log "$red" "Installation failed"
  exit 1
fi

log "$green" "Installation complete!"
exit 0
