#!/bin/bash
set -e

# MXE directory - adjust this to your MXE installation path
MXE_DIR="/opt/mxe"
# Explicitly add MXE to PATH
export PATH="$PATH:$MXE_DIR/usr/bin"
# Temporarily disable ccache to prevent interfering
export CCACHE_DISABLE=1

ABIS=(x86_64)

for ABI in "${ABIS[@]}"
do
  mkdir -p build/windows/$ABI
  cd build/windows/$ABI

  # Use MXE's CMake
  ${MXE_DIR}/usr/bin/${ABI}-w64-mingw32.shared-cmake ../../.. \
    -DCMAKE_INSTALL_PREFIX="../../../build/windows/$ABI" \
    -DCMAKE_TOOLCHAIN_FILE="${MXE_DIR}/usr/${ABI}-w64-mingw32.shared/share/cmake/mxe-conf.cmake" \
    -DCMAKE_C_COMPILER="${MXE_DIR}/usr/bin/${ABI}-w64-mingw32.shared-gcc" \
    -DCMAKE_CXX_COMPILER="${MXE_DIR}/usr/bin/${ABI}-w64-mingw32.shared-g++" \
    -DOS=WIN32 \
    -DCMAKE_C_FLAGS='-DMAB_WIN32' \
    -DCMAKE_CXX_FLAGS='-DMAB_WIN32' \
    -DBUILD_SHARED_LIBS=ON

  # Build and Install
  ${MXE_DIR}/usr/bin/${ABI}-w64-mingw32.shared-cmake --build . --config Release
  ${MXE_DIR}/usr/bin/${ABI}-w64-mingw32.shared-cmake --install . --config Release

  cd ../../..
  mkdir -p prebuilt/windows/$ABI
  cp "build/windows/$ABI/libcoast_audio.dll" prebuilt/windows/$ABI/libcoast_audio.dll

  rm -rf build/windows
done