#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  SUDO="sudo"
else
  SUDO=""
fi

OS_NAME="$(uname -s)"

if [[ "${OS_NAME}" == "Linux" ]]; then
  if command -v apt >/dev/null 2>&1; then
    ${SUDO} apt update
    ${SUDO} apt install -y build-essential cmake git ninja-build pkg-config unzip \
      libgl1-mesa-dev libglu1-mesa-dev \
      libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libxfixes-dev
    echo "Installed prerequisites using apt."
  elif command -v dnf >/dev/null 2>&1; then
    ${SUDO} dnf install -y gcc-c++ cmake git ninja-build make pkgconfig unzip \
      mesa-libGL-devel mesa-libGLU-devel libX11-devel libXext-devel libXrandr-devel libXi-devel libXcursor-devel libXinerama-devel libXfixes-devel
    echo "Installed prerequisites using dnf."
  elif command -v pacman >/dev/null 2>&1; then
    ${SUDO} pacman -Sy --noconfirm base-devel cmake git ninja pkgconf unzip mesa glu libx11 libxext libxrandr libxi libxcursor libxinerama libxfixes
    echo "Installed prerequisites using pacman."
  else
    echo "Unsupported Linux package manager. Install cmake/git/compiler manually."
    exit 1
  fi
elif [[ "${OS_NAME}" == "Darwin" ]]; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew not found. Install Homebrew first: https://brew.sh"
    exit 1
  fi
  brew install cmake git ninja
  xcode-select --install || true
  echo "Installed prerequisites using Homebrew."
else
  echo "Unsupported OS for this script: ${OS_NAME}"
  exit 1
fi
