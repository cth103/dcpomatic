#!/bin/bash

set -e

version=`cat wscript | egrep ^VERSION | awk '{print $3}' | sed -e "s/'//g"`

# DMG size in megabytes
DMG_SIZE=256
WORK=build/platform/osx
ENV=/Users/carl/Environments/osx
ROOT=/Users/carl/cdist

appdir="DCP-o-matic.app"
approot=$appdir/Contents
libs=$approot/lib
macos=$approot/MacOS
resources=$approot/Resources

rm -rf $WORK/$appdir
mkdir -p $WORK/$macos
mkdir -p $WORK/$libs
mkdir -p $WORK/$resources

function universal_copy {
    echo $2
    for f in $1/32/$2; do
        if [ -h $f ]; then
	    ln -s $(readlink $f) $3/`basename $f`
        else
          g=`echo $f | sed -e "s/\/32\//\/64\//g"`
	  mkdir -p $3
          lipo -create $f $g -output $3/`basename $f`
        fi
    done
}

universal_copy $ROOT src/dvdomatic/build/src/tools/dcpomatic $WORK/$macos
universal_copy $ROOT src/dvdomatic/build/src/tools/dcpomatic_cli $WORK/$macos
universal_copy $ROOT src/dvdomatic/build/src/tools/dcpomatic_server_cli $WORK/$macos
universal_copy $ROOT src/dvdomatic/build/src/lib/libdcpomatic.dylib $WORK/$libs
universal_copy $ROOT src/dvdomatic/build/src/wx/libdcpomatic-wx.dylib $WORK/$libs
universal_copy $ROOT lib/libcxml.dylib $WORK/$libs
universal_copy $ROOT lib/libdcp.dylib $WORK/$libs
universal_copy $ROOT lib/libasdcp-libdcp.dylib $WORK/$libs
universal_copy $ROOT lib/libkumu-libdcp.dylib $WORK/$libs
universal_copy $ROOT lib/libopenjpeg*.dylib $WORK/$libs
universal_copy $ROOT lib/libavdevice*.dylib $WORK/$libs
universal_copy $ROOT lib/libavformat*.dylib $WORK/$libs
universal_copy $ROOT lib/libavfilter*.dylib $WORK/$libs
universal_copy $ROOT lib/libavutil*.dylib $WORK/$libs
universal_copy $ROOT lib/libavcodec*.dylib $WORK/$libs
universal_copy $ROOT lib/libswscale*.dylib $WORK/$libs
universal_copy $ROOT lib/libpostproc*.dylib $WORK/$libs
universal_copy $ROOT lib/libswresample*.dylib $WORK/$libs
universal_copy $ROOT bin/ffprobe $WORK/$macos
universal_copy $ENV lib/libboost_system.dylib $WORK/$libs
universal_copy $ENV lib/libboost_filesystem.dylib $WORK/$libs
universal_copy $ENV lib/libboost_thread.dylib $WORK/$libs
universal_copy $ENV lib/libboost_date_time.dylib $WORK/$libs
universal_copy $ENV lib/libxml++-2.6*.dylib $WORK/$libs
universal_copy $ENV lib/libxml2*.dylib $WORK/$libs
universal_copy $ENV lib/libglibmm-2.4*.dylib $WORK/$libs
universal_copy $ENV lib/libgobject*.dylib $WORK/$libs
universal_copy $ENV lib/libgthread*.dylib $WORK/$libs
universal_copy $ENV lib/libgmodule*.dylib $WORK/$libs
universal_copy $ENV lib/libsigc*.dylib $WORK/$libs
universal_copy $ENV lib/libglib-2*.dylib $WORK/$libs
universal_copy $ENV lib/libintl*.dylib $WORK/$libs
universal_copy $ENV lib/libsndfile*.dylib $WORK/$libs
universal_copy $ENV lib/libMagick++*.dylib $WORK/$libs
universal_copy $ENV lib/libMagickCore*.dylib $WORK/$libs
universal_copy $ENV lib/libMagickWand*.dylib $WORK/$libs
universal_copy $ENV lib/libssh*.dylib $WORK/$libs
universal_copy $ENV lib/libwx*.dylib $WORK/$libs
universal_copy $ENV lib/libfontconfig*.dylib $WORK/$libs
universal_copy $ENV lib/libfreetype*.dylib $WORK/$libs
universal_copy $ENV lib/libexpat*.dylib $WORK/$libs
universal_copy $ENV lib/libltdl*.dylib $WORK/$libs
universal_copy $ENV lib/libxmlsec1*.dylib $WORK/$libs
universal_copy $ENV lib/libzip*.dylib $WORK/$libs
universal_copy $ENV lib/libquickmail*.dylib $WORK/$libs

for obj in $WORK/$macos/dcpomatic $WORK/$macos/ffprobe $WORK/$libs/*.dylib; do
  deps=`otool -L $obj | awk '{print $1}' | egrep "(/Users/carl|libboost|libssh|libltdl)"`
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
cp icons/dcpomatic.icns $WORK/$resources/DCP-o-matic.icns

tmp_dmg=$WORK/dcpomatic_tmp.dmg
dmg="$WORK/DCP-o-matic $version.dmg"
vol_name=DCP-o-matic-$version

mkdir -p $WORK/$vol_name
cp -r $WORK/$appdir $WORK/$vol_name
ln -s /Applications $WORK/$vol_name/Applications

rm -f $tmp_dmg "$dmg"
hdiutil create -srcfolder $WORK/$vol_name -volname $vol_name -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size $DMG_SIZE $tmp_dmg
attach=$(hdiutil attach -readwrite -noverify -noautoopen $tmp_dmg)
device=`echo $attach | egrep '^/dev/' | sed 1q | awk '{print $5}'`
sleep 5

echo '
  tell application "Finder"
    tell disk "'$vol_name'"
           open
           set current view of container window to icon view
           set toolbar visible of container window to false
           set statusbar visible of container window to false
           set the bounds of container window to {400, 200, 790, 410}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 64
           set position of item "DCP-o-matic.app" of container window to {90, 80}
           set position of item "Applications" of container window to {310, 80}
           close
           open
           update without registering applications
           delay 5
     end tell
   end tell
' | osascript

chmod -Rf go-w /Volumes/"$vol_name"/$appdir
sync

umount -f $device
hdiutil eject $device
hdiutil convert -format UDZO $tmp_dmg -imagekey zlib-level=9 -o "$dmg"
sips -i $WORK/$resources/DCP-o-matic.icns
DeRez -only icns $WORK/$resources/DCP-o-matic.icns > $WORK/$resources/DCP-o-matic.rsrc
Rez -append $WORK/$resources/DCP-o-matic.rsrc -o "$dmg"
SetFile -a C "$dmg"

