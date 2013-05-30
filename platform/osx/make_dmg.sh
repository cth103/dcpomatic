#!/bin/bash

VERSION=$1
if [ "$VERSION" == "" ]; then
  echo "Syntax: $0 <version>"
  exit 1
fi

# DMG size in megabytes
DMG_SIZE=64
WORK=build/platform/osx
ENV=/Users/carl/Environments/osx/10.8
DEPS=/Users/carl/cdist

dmg_name="DVD-o-matic-$VERSION"
appdir="DVD-o-matic.app"
approot=$appdir/Contents
libs=$approot/lib
macos=$approot/MacOS

rm -rf $WORK/$appdir
mkdir -p $WORK/$macos
mkdir -p $WORK/$libs

cp build/src/tools/dvdomatic $WORK/$macos/
cp build/src/lib/libdvdomatic.dylib $WORK/$libs/
cp build/src/wx/libdvdomatic-wx.dylib $WORK/$libs/
cp $DEPS/lib/libdcp.dylib $WORK/$libs/
cp $DEPS/lib/libasdcp-libdcp.dylib $WORK/$libs/
cp $DEPS/lib/libkumu-libdcp.dylib $WORK/$libs/
cp $DEPS/lib/libopenjpeg*.dylib $WORK/$libs/
cp $DEPS/lib/libavformat*.dylib $WORK/$libs/
cp $DEPS/lib/libavfilter*.dylib $WORK/$libs/
cp $DEPS/lib/libavutil*.dylib $WORK/$libs/
cp $DEPS/lib/libavcodec*.dylib $WORK/$libs/
cp $DEPS/lib/libswscale*.dylib $WORK/$libs/
cp $DEPS/lib/libpostproc*.dylib $WORK/$libs/
cp $DEPS/lib/libswresample*.dylib $WORK/$libs/
cp $ENV/lib/libboost_system.dylib $WORK/$libs/
cp $ENV/lib/libboost_filesystem.dylib $WORK/$libs/
cp $ENV/lib/libboost_thread.dylib $WORK/$libs/
cp $ENV/lib/libboost_date_time.dylib $WORK/$libs/
cp $ENV/lib/libssl*.dylib $WORK/$libs/
cp $ENV/lib/libcrypto*.dylib $WORK/$libs/
cp $ENV/lib/libxml++-2.6*.dylib $WORK/$libs/
cp $ENV/lib/libxml2*.dylib $WORK/$libs/
cp $ENV/lib/libglibmm-2.4*.dylib $WORK/$libs/
cp $ENV/lib/libgobject*.dylib $WORK/$libs/
cp $ENV/lib/libgthread*.dylib $WORK/$libs/
cp $ENV/lib/libgmodule*.dylib $WORK/$libs/
cp $ENV/lib/libsigc*.dylib $WORK/$libs/
cp $ENV/lib/libglib-2*.dylib $WORK/$libs/
cp $ENV/lib/libintl*.dylib $WORK/$libs/
cp $ENV/lib/libsndfile*.dylib $WORK/$libs/
cp $ENV/lib/libMagick++*.dylib $WORK/$libs/
cp $ENV/lib/libMagickCore*.dylib $WORK/$libs/
cp $ENV/lib/libMagickWand*.dylib $WORK/$libs/
cp $ENV/lib/libssh*.dylib $WORK/$libs/
cp $ENV/lib/libwx*.dylib $WORK/$libs/
cp $ENV/lib/libfontconfig*.dylib $WORK/$libs/
cp $ENV/lib/libfreetype*.dylib $WORK/$libs/
cp $ENV/lib/libexpat*.dylib $WORK/$libs/

for obj in $WORK/$macos/dvdomatic $WORK/$libs/*.dylib; do
  deps=`otool -L $obj | awk '{print $1}' | egrep "(/Users/carl|libboost|libssh)"`
  changes=""
  for dep in $deps; do
    base=`basename $dep`
    changes="$changes -change $dep @executable_path/../lib/$base"
  done
  if test "x$changes" != "x"; then
    install_name_tool $changes $obj
  fi  
done


cp build/platform/osx/Info.plist $WORK/$approot

exit 0

mkdir -p $WORK/mnt

hdiutil create -megabytes $DMG_SIZE build/platform/osx/dvdomatic.dmg
device=$(hdid -nomount build/platform/osx/dvdomatic.dmg | grep Apple_HFS | cut -f 1 -d ' ')
newfs_hfs -v "$dmg_name" "$device"
mount -t hfs "$device" build/platform/osx/mnt
