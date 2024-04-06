ABIS=(x86_64)

for ABI in "${ABIS[@]}"
do
  mkdir -p build/windows
  cd build/windows

  cmake ../.. \ 
    -DCMAKE_INSTALL_PREFIX="../../build/windows/x86_64" \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchain/mingw-w64-x86_64.toolchain.cmake" \
    -DOS=WIN32 \
    -DCMAKE_CFLAGS='-DMAB_WIN32' 

  cmake --build . --config Release
  cmake --install . --config Release
  cd ../..

  mkdir -p prebuilt/windows/$ABIS
  cp "build/windows/libcoast_audio.a" prebuilt/windows/$ABIS/libcoast_audio.a

  rm -rf build/windows
done