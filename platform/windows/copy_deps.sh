#!/bin/bash

MXE=/opt/mxe/usr/x86_64-w64-mingw32.shared/bin

dir=$1
if [ "$dir" == "" ]; then
    echo "Syntax: $0 <dir>"
    exit 1
fi

cp $MXE/libgcc_s_seh-1.dll $dir
cp $MXE/libstdc++-6.dll $dir
cp $MXE/libboost*.dll $dir
cp $MXE/libglibmm*.dll $dir
cp $MXE/libcrypto*.dll $dir
cp $MXE/libwinpthread*.dll $dir
cp $MXE/wxbase*.dll $dir
cp $MXE/wxmsw*.dll $dir
cp $MXE/libcurl*.dll $dir
cp $MXE/libxml*.dll $dir
cp $MXE/libjpeg*.dll $dir
cp $MXE/zlib*.dll $dir
cp $MXE/libpng*.dll $dir
cp $MXE/libtiff*.dll $dir
cp $MXE/libssh*.dll $dir
cp $MXE/libidn*.dll $dir
cp $MXE/liblzma*.dll $dir
cp $MXE/libiconv*.dll $dir
cp $MXE/libxslt*.dll $dir
cp $MXE/libltdl*.dll $dir
cp $MXE/libintl*.dll $dir
cp $MXE/libunistring*.dll $dir
cp $MXE/libwebp*.dll $dir
cp $MXE/libgcrypt*.dll $dir
cp $MXE/libdl*.dll $dir
cp $MXE/libgpg*.dll $dir
cp $MXE/libcairo*.dll $dir
cp $MXE/libfontconfig*.dll $dir
cp $MXE/libglib*.dll $dir	
cp $MXE/icu*.dll $dir
cp $MXE/libnettle*.dll $dir
cp $MXE/libpango*.dll $dir
cp $MXE/libsamplerate*.dll $dir
cp $MXE/libzip*.dll $dir
cp $MXE/libgmodule*.dll $dir
cp $MXE/libgobject*.dll $dir
cp $MXE/libsigc*.dll $dir
cp $MXE/libpcre*.dll $dir
cp $MXE/libx264*.dll $dir
cp $MXE/libbz2*.dll $dir
cp $MXE/libexpat*.dll $dir
cp $MXE/libfreetype*.dll $dir
cp $MXE/libffi*.dll $dir
cp $MXE/libharfbuzz*.dll $dir
cp $MXE/libpixman*.dll $dir
cp $MXE/libnanomsg*.dll $dir
