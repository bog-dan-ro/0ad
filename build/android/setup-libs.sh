#!/bin/bash

set -e
set -o nounset

ANDROID=$HOME/android
NDK=$ANDROID/android-ndk
TOOLCHAIN_VERSION=4.9
ABI=arm
PLATFORM=12

BOOST_VERSION=1.55.0
BOOST_VERSION_LONG=1_55_0
BOOST_VERSION_SHORT=1_55
OPENSSL_VERSION=1.0.1j
CURL_VERSION=7.33.0
PNG_VERSION=1.6.16
JPEG_VERSION=1.3.1
XML2_VERSION=2.9.2
ENET_VERSION=1.3.12
MOZJS_VERSION=24.2.0
ICU_VERSION=51_2
GLOOX_VERSION=1.0.12
OPENAL_VERSION=1.16.0
OGG_VERSION=1.3.2
VORBIS_VERSION=1.3.4
MINIUPNPC_VERSION=1.9
SDL_VERSION=2.0.3

while getopts ":hn:a:p:" optname
do
  case "$optname" in
    "h")
      echo "Usage:
      setup-libs.sh -n NDK -p PLATFORM -a ABI
      "
      exit 1
      ;;
    "a")
      ABI="$OPTARG"
      ;;
    "n")
      NDK="$OPTARG"
      ;;
    "n")
      PLATFORM="$OPTARG"
      ;;
    "?")
      echo "Unknown option $OPTARG"
      exit 1
      ;;
    ":")
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

HOST_ARCH=`uname -m`
case "$HOST_ARCH" in
    i?86) HOST_ARCH=x86
    ;;
    amd64) HOST_ARCH=x86_64
    ;;
    powerpc) HOST_ARCH=ppc
    ;;
esac

HOST_OS=`uname -s`
case "$HOST_OS" in
    Darwin)
        HOST_OS=darwin
        ;;
    Linux)
        # note that building  32-bit binaries on x86_64 is handled later
        HOST_OS=linux
        ;;
esac

case $ABI in
  "arm")
    SYSROOT=$NDK/platforms/android-$PLATFORM/arch-arm
    TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-$TOOLCHAIN_VERSION/prebuilt/$HOST_OS-$HOST_ARCH
    TOOLCHAIN_PREFIX=arm-linux-androideabi
    CFLAGS="-mandroid -mthumb -Wno-psabi -march=armv7-a -mfloat-abi=softfp -mfpu=vfp -ffunction-sections -funwind-tables -fstack-protector -fno-short-enums -DANDROID -Wa,--noexecstack -Os -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 -I${NDK}/sources/cxx-stl/gnu-libstdc++/${TOOLCHAIN_VERSION}/libs/armeabi-v7a/include"
    LDFLAGS="-L${NDK}/sources/cxx-stl/gnu-libstdc++/${TOOLCHAIN_VERSION}/libs/armeabi-v7a"
    ;;
  *)
    echo "Unknown/Unhandled ABI $ABI"
    exit 1
    ;;
esac

INSTALL_PREFIX=$(dirname $0)
pushd $INSTALL_PREFIX

FINAL_INSTALL_PREFIX=$(cd $INSTALL_PREFIX/../../libraries && pwd)/android/$ABI
INSTALL_PREFIX=${FINAL_INSTALL_PREFIX}_temp
rm -fr $INSTALL_PREFIX
mkdir -p $INSTALL_PREFIX

build_apitrace=true
build_js=true
build_openssl=true
build_boost=true
build_curl=true
build_libpng=true
build_libjpeg=true
build_libxml2=true
build_enet=true
build_icu=true
build_gloox=true
build_ogg=true
build_vorbis=true
build_openal=true
build_miniupnpc=true
build_sdl=true

JOBS=${JOBS:="-j4"}

export PATH=$TOOLCHAIN/bin:$NDK:$PATH

mkdir -p files
pushd files

if [ ! -e openssl-$OPENSSL_VERSION.tar.gz ]; then
  wget https://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz
fi

if [ ! -e MysticTreeGames-Boost-for-Android-8fd5469.tar.gz ]; then
  wget https://github.com/MysticTreeGames/Boost-for-Android/tarball/8fd5469a492c621b604c4e85231b328f3a62c487 -O MysticTreeGames-Boost-for-Android-8fd5469.tar.gz
fi

if [ ! -e curl-$CURL_VERSION.tar.bz2 ]; then
  wget http://curl.haxx.se/download/curl-$CURL_VERSION.tar.bz2
fi

if [ ! -e libpng-$PNG_VERSION.tar.xz ]; then
  wget http://prdownloads.sourceforge.net/libpng/libpng-$PNG_VERSION.tar.xz
fi

if [ ! -e libjpeg-turbo-$JPEG_VERSION.tar.gz ]; then
  wget http://prdownloads.sourceforge.net/libjpeg-turbo/libjpeg-turbo-$JPEG_VERSION.tar.gz
fi

if [ ! -e libxml2-$XML2_VERSION.tar.gz ]; then
  wget ftp://xmlsoft.org/libxml2/libxml2-$XML2_VERSION.tar.gz
fi

if [ ! -e enet-$ENET_VERSION.tar.gz ]; then
  wget http://enet.bespin.org/download/enet-$ENET_VERSION.tar.gz
fi

if [ ! -e icu4c-$ICU_VERSION-src.tgz ]; then
  wget http://download.icu-project.org/files/icu4c/51.2/icu4c-$ICU_VERSION-src.tgz
fi

if [ ! -e gloox-$GLOOX_VERSION.tar.bz2 ]; then
  wget http://camaya.net/download/gloox-$GLOOX_VERSION.tar.bz2
fi

if [ ! -e libogg-$OGG_VERSION.tar.xz ]; then
  wget  http://downloads.xiph.org/releases/ogg/libogg-$OGG_VERSION.tar.xz
fi

if [ ! -e libvorbis-$VORBIS_VERSION.tar.xz ]; then
  wget  http://downloads.xiph.org/releases/vorbis/libvorbis-$VORBIS_VERSION.tar.xz
fi

if [ ! -e openal-soft-$OPENAL_VERSION.tar.bz2 ]; then
  wget http://www.openal-soft.org/openal-releases/openal-soft-$OPENAL_VERSION.tar.bz2
fi

if [ ! -e miniupnpc-$MINIUPNPC_VERSION.tar.gz ]; then
  wget http://miniupnp.free.fr/files/miniupnpc-$MINIUPNPC_VERSION.tar.gz
fi

if [ ! -e SDL2-$SDL_VERSION.tar.gz ]; then
  wget https://www.libsdl.org/release/SDL2-$SDL_VERSION.tar.gz
fi

if [ ! -d openal-soft ]; then
  git clone git://repo.or.cz/openal-soft.git
fi

if [ ! -d apitrace ]; then
  git clone https://github.com/apitrace/apitrace.git
fi

popd

mkdir -p temp

if [ "$build_apitrace" = "true" ]; then
  rm -rf temp/apitrace
  cp -a files/apitrace temp/
  pushd temp/apitrace
  git checkout f11d3c6
  patch -p1 < ../../apitrace-android.patch
  cmake -DCMAKE_TOOLCHAIN_FILE=../../android.toolchain.cmake -DANDROID_NDK=$NDK -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI="armeabi-v7a" \
        -DANDROID_NATIVE_API_LEVEL=$PLATFORM -DANDROID_STL=gnustl_shared -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX .
  make $JOBS
  make install
  popd
fi

if [ "$build_js" = "true" ]; then
  rm -rf temp/mozjs-$MOZJS_VERSION
  tar xvf ../../libraries/source/spidermonkey/mozjs-$MOZJS_VERSION.tar.bz2 -C temp/
  pushd temp/mozjs-$MOZJS_VERSION/js/src
  ./configure \
    --prefix=$INSTALL_PREFIX \
    --target=$TOOLCHAIN_PREFIX \
    --with-android-ndk=$NDK \
    --with-android-gnu-compiler-version=$TOOLCHAIN_VERSION \
    --with-android-version=$PLATFORM \
    --enable-android-libstdcxx \
    --disable-tests \
    --disable-shared-js \
    --with-arm-kuser

  make $JOBS JS_DISABLE_SHELL=1
  make install JS_DISABLE_SHELL=1
  popd
fi

if [ "$build_openssl" = "true" ]; then
  rm -rf temp/openssl-$OPENSSL_VERSION
  tar xvf files/openssl-$OPENSSL_VERSION.tar.gz -C temp/
  pushd temp/openssl-$OPENSSL_VERSION
  ./Configure --cross-compile-prefix="$TOOLCHAIN_PREFIX-" --prefix=$INSTALL_PREFIX -I${SYSROOT}/usr/include -L${SYSROOT}/usr/lib -lm -ldl no-shared android
  make build_libs || echo Never mind

  mkdir -p $INSTALL_PREFIX/lib
  cp *.a $INSTALL_PREFIX/lib/
  cp -Lr include $INSTALL_PREFIX/
  popd
fi

if [ "$build_boost" = "true" ]; then
  rm -rf temp/MysticTreeGames-Boost-for-Android-8fd5469
  tar xvf files/MysticTreeGames-Boost-for-Android-8fd5469.tar.gz -C temp/
  patch -p1 temp/MysticTreeGames-Boost-for-Android-8fd5469/build-android.sh < boost-android-build.patch
  pushd temp/MysticTreeGames-Boost-for-Android-8fd5469
  if [ -e ../../files/boost_$BOOST_VERSION_LONG.* ]; then
    cp ../../files/boost_$BOOST_VERSION_LONG.* .
  fi
  ./build-android.sh --boost=$BOOST_VERSION --with-libraries=system,filesystem --prefix=$INSTALL_PREFIX $NDK
  cp boost_$BOOST_VERSION_LONG.* ../../files/
  pushd $INSTALL_PREFIX/include
  ln -s boost-$BOOST_VERSION_SHORT/boost .
  popd
  pushd $INSTALL_PREFIX/lib
  ln -s libboost_system-*-$BOOST_VERSION_SHORT.a libboost_system.a
  ln -s libboost_filesystem-*-$BOOST_VERSION_SHORT.a libboost_filesystem.a
  popd
  popd
fi

export CFLAGS="${CFLAGS} --sysroot=${SYSROOT} -I${SYSROOT}/usr/include \
               -I${NDK}/sources/cxx-stl/gnu-libstdc++/${TOOLCHAIN_VERSION}/include \
               -I${INSTALL_PREFIX}/include"
export CPPFLAGS="${CFLAGS}"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="${LDFLAGS} -L${SYSROOT}/usr/lib -L${INSTALL_PREFIX}/lib -lm -ldl"

function compileAutoConf {
  rm -rf temp/$1
  tar xvf files/$1.$2 -C temp/

  if [  $# = 4 ]; then
      patch -p1 < $4
  fi

  if [  $# = 5 ]; then
      patch $5 < $4
  fi

  pushd temp/$1

  if [ $# -gt 2 ] && [ "0" != "$3" ]; then
    #very old config.sub?, copy a newer one from png
    cp ../libpng-$PNG_VERSION/config.sub .
  fi

  ./configure --host=$TOOLCHAIN_PREFIX --with-sysroot=$SYSROOT --prefix=$INSTALL_PREFIX \
              --disable-shared --enable-static --without-python --without-tests --without-examples \
              --with-openssl=$INSTALL_PREFIX --with-ssl=$INSTALL_PREFIX --with-gnutls=no
  make $JOBS
  make install
  popd
}

if [ "$build_curl" = "true" ]; then
  compileAutoConf curl-$CURL_VERSION tar.bz2
fi

if [ "$build_libpng" = "true" ]; then
  compileAutoConf libpng-$PNG_VERSION tar.xz
fi

if [ "$build_libjpeg" = "true" ]; then
  compileAutoConf libjpeg-turbo-$JPEG_VERSION tar.gz 1
fi

if [ "$build_libxml2" = "true" ]; then
  compileAutoConf libxml2-$XML2_VERSION tar.gz 0 libxml2-android-build.patch temp/libxml2-$XML2_VERSION/Makefile.in
fi

if [ "$build_enet" = "true" ]; then
  compileAutoConf enet-$ENET_VERSION tar.gz
fi

if [ "$build_icu" = "true" ]; then
  rm -rf temp/icu
  tar xvf files/icu4c-$ICU_VERSION-src.tgz -C temp/
  pushd temp/icu/source
  patch Makefile.in < ../../../icu-android-build.patch
  mkdir host_build
  pushd host_build
  OLD_CFLAGS=$CFLAGS
  OLD_CPPFLAGS=$CPPFLAGS
  OLD_CXXFLAGS=$CXXFLAGS
  OLD_LDFLAGS=$LDFLAGS
  unset CFLAGS
  unset CPPFLAGS
  unset CXXFLAGS
  unset LDFLAGS
  ../runConfigureICU Linux/gcc --prefix=$PWD --disable-tests --disable-samples
  make $JOBS
  export CFLAGS="$OLD_CFLAGS -I${NDK}/sources/android/support/include"
  export CPPFLAGS="$OLD_CPPFLAGS -I${NDK}/sources/android/support/include"
  export CXXFLAGS="$OLD_CXXFLAGS -I${NDK}/sources/android/support/include"
  export LDFLAGS=$OLD_LDFLAGS
  popd
  ./configure --host=$TOOLCHAIN_PREFIX --with-cross-build=$PWD/host_build --with-sysroot=$SYSROOT \
              --prefix=$INSTALL_PREFIX --enable-static --disable-shared --disable-extras \
              --disable-tests --disable-samples --with-data-packaging=static

  make $JOBS
  make install
  export CFLAGS=$OLD_CFLAGS
  export CPPFLAGS=$OLD_CPPFLAGS
  export CXXFLAGS=$OLD_CXXFLAGS
  export LDFLAGS=$OLD_LDFLAGS
  popd
fi

if [ "$build_gloox" = "true" ]; then
  compileAutoConf gloox-$GLOOX_VERSION tar.bz2
fi

if [ "$build_ogg" = "true" ]; then
  compileAutoConf libogg-$OGG_VERSION tar.xz
fi

if [ "$build_vorbis" = "true" ]; then
  compileAutoConf libvorbis-$VORBIS_VERSION tar.xz
fi

if [ "$build_openal" = "true" ]; then
  rm -rf temp/openal-soft
  cp -a files/openal-soft temp/
  pushd temp/openal-soft
  git checkout 93b6958
  cmake -DCMAKE_TOOLCHAIN_FILE=../../android.toolchain.cmake -DANDROID_NDK=$NDK -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI="armeabi-v7a" \
        -DANDROID_NATIVE_API_LEVEL=$PLATFORM -DALSOFT_UTILS=OFF -DALSOFT_NO_CONFIG_UTIL=ON -DALSOFT_EXAMPLES=OFF -DLIBTYPE=STATIC \
        -DANDROID_STL=gnustl_shared -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX .
  make $JOBS
  make install
  cp libcommon.a $INSTALL_PREFIX/lib
  popd
fi

if [ "$build_miniupnpc" = "true" ]; then
  rm -rf temp/icu
  tar xvf files/miniupnpc-$MINIUPNPC_VERSION.tar.gz -C temp/
  pushd temp/miniupnpc-$MINIUPNPC_VERSION
  patch miniwget.c < ../../miniupnpc-android.patch
  cmake -DCMAKE_TOOLCHAIN_FILE=../../android.toolchain.cmake -DANDROID_NDK=$NDK -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI="armeabi-v7a" \
        -DANDROID_NATIVE_API_LEVEL=$PLATFORM -DUPNPC_BUILD_STATIC=ON -DUPNPC_BUILD_SHARED=OFF -DUPNPC_BUILD_TESTS=OFF \
        -DANDROID_STL=gnustl_shared -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX .
  make $JOBS
  make install
  cp portlistingparse.h $INSTALL_PREFIX/include
  cp miniupnpctypes.h $INSTALL_PREFIX/include
  popd
fi

if [ "$build_sdl" = "true" ]; then
  rm -rf temp/SDL2-$SDL_VERSION
  tar xvf files/SDL2-$SDL_VERSION.tar.gz -C temp/
  mv temp/SDL2-$SDL_VERSION $INSTALL_PREFIX/SDL2
  ln -s ../SDL2/include $INSTALL_PREFIX/include/SDL2
fi

rm -fr temp
rm -fr $FINAL_INSTALL_PREFIX
mv $INSTALL_PREFIX $FINAL_INSTALL_PREFIX
popd
