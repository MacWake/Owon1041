#!/usr/bin/env sh

PROJECT_ROOT=$(pwd)

test -d ${PROJECT_ROOT}/build || mkdir ${PROJECT_ROOT}/build
cd ${PROJECT_ROOT}/build

../configure -DCMAKE_BUILD_TYPE=Release -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_EXAMPLES_BY_DEFAULT=OFF -DCMAKE_SYSTEM_NAME=Windows -DQT_HOST_PATH=/usr/local/qt6-mingw -static  -force-bundled-libs           -DCMAKE_BUILD_TYPE=Release -D CMAKE_SYSTEM_NAME=Windows -D CMAKE_CROSSCOMPILING=TRUE -D CMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -D CMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -D CMAKE_RC_COMPILER=x86_64-w64-mingw32-windres -D CMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static" -D CMAKE_CXX_FLAGS="-DSTATIC_BUILD" -no-feature-windeployqt

set -e
set -x

cmake -B ${PROJECT_ROOT}/build \
          -G "Unix Makefiles" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DQT_BUILD_TESTS=OFF \
    -DQT_BUILD_EXAMPLES=OFF \
    -DFEATURE_static_runtime=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local/qt6-mingw-static
          -DCMAKE_CROSSCOMPILING=TRUE \
          -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
          -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
          -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
          -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static" \
          -DCMAKE_CXX_FLAGS="-DSTATIC_BUILD"

cmake --build ${PROJECT_ROOT}/build --parallel
