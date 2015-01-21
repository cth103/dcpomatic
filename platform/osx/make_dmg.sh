#!/bin/bash
#
# Syntax: make_dmg.sh <builddir>
#
# e.g. make_dmg.sh /Users/carl/cdist

# Don't set -e here as egrep (used a few times) returns 1 if no matches
# were found.

version=`cat wscript | egrep ^VERSION | awk '{print $3}' | sed -e "s/'//g"`

# DMG size in megabytes
DMG_SIZE=256
WORK=build/platform/osx
ENV=/Users/carl/Environments/osx/10.6
ROOT=$1

appdir="DCP-o-matic 2.app"
approot="$appdir/Contents"
libs="$approot/lib"
macos="$approot/MacOS"
resources="$approot/Resources"

rm -rf "$WORK/$appdir"
mkdir -p "$WORK/$macos"
mkdir -p "$WORK/$libs"
mkdir -p "$WORK/$resources"

relink="dcpomatic"

function universal_copy {
    for f in $1/32/$2; do
        if [ -h $f ]; then
	    ln -s $(readlink $f) "$3/`basename $f`"
        else
            g=`echo $f | sed -e "s/\/32\//\/64\//g"`
	    mkdir -p "$3"
            lipo -create $f $g -output "$3/`basename $f`"
        fi
    done
}

function universal_copy_lib {
    for f in $1/32/lib/$2*.dylib; do
        if [ -h $f ]; then
	    ln -s $(readlink $f) "$3/`basename $f`"
        else
            g=`echo $f | sed -e "s/\/32\//\/64\//g"`
	    mkdir -p "$3"
            lipo -create $f $g -output "$3/`basename $f`"
        fi
    done
    relink="$relink|$2"
}

universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2 "$WORK/$macos"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_cli "$WORK/$macos"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_server_cli "$WORK/$macos"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_batch "$WORK/$macos"
universal_copy $ROOT src/dcpomatic/build/src/lib/libdcpomatic2.dylib "$WORK/$libs"
universal_copy $ROOT src/dcpomatic/build/src/wx/libdcpomatic2-wx.dylib "$WORK/$libs"
universal_copy_lib $ROOT libcxml "$WORK/$libs"
universal_copy_lib $ROOT libdcp-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libasdcp-libdcp-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libkumu-libdcp-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libsub "$WORK/$libs"
universal_copy_lib $ROOT libasdcp-libsub-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libkumu-libsub-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libopenjpeg "$WORK/$libs"
universal_copy_lib $ROOT libavdevice "$WORK/$libs"
universal_copy_lib $ROOT libavformat "$WORK/$libs"
universal_copy_lib $ROOT libavfilter "$WORK/$libs"
universal_copy_lib $ROOT libavutil "$WORK/$libs"
universal_copy_lib $ROOT libavcodec "$WORK/$libs"
universal_copy_lib $ROOT libswscale "$WORK/$libs"
universal_copy_lib $ROOT libswresample "$WORK/$libs"
universal_copy_lib $ROOT libpostproc "$WORK/$libs"
universal_copy $ROOT bin/ffprobe "$WORK/$macos"
universal_copy_lib $ENV libboost_system "$WORK/$libs"
universal_copy_lib $ENV libboost_filesystem "$WORK/$libs"
universal_copy_lib $ENV libboost_thread "$WORK/$libs"
universal_copy_lib $ENV libboost_date_time "$WORK/$libs"
universal_copy_lib $ENV libboost_locale "$WORK/$libs"
universal_copy_lib $ENV libxml++ "$WORK/$libs"
universal_copy_lib $ENV libxslt "$WORK/$libs"
universal_copy_lib $ENV libxml2 "$WORK/$libs"
universal_copy_lib $ENV libglibmm-2.4 "$WORK/$libs"
universal_copy_lib $ENV libgobject "$WORK/$libs"
universal_copy_lib $ENV libgthread "$WORK/$libs"
universal_copy_lib $ENV libgmodule "$WORK/$libs"
universal_copy_lib $ENV libsigc "$WORK/$libs"
universal_copy_lib $ENV libglib-2 "$WORK/$libs"
universal_copy_lib $ENV libintl "$WORK/$libs"
universal_copy_lib $ENV libsndfile "$WORK/$libs"
universal_copy_lib $ENV libMagick++ "$WORK/$libs"
universal_copy_lib $ENV libMagickCore "$WORK/$libs"
universal_copy_lib $ENV libMagickWand "$WORK/$libs"
universal_copy_lib $ENV libssh "$WORK/$libs"
universal_copy_lib $ENV libwx "$WORK/$libs"
universal_copy_lib $ENV libfontconfig "$WORK/$libs"
universal_copy_lib $ENV libfreetype "$WORK/$libs"
universal_copy_lib $ENV libexpat "$WORK/$libs"
universal_copy_lib $ENV libltdl "$WORK/$libs"
universal_copy_lib $ENV libxmlsec1 "$WORK/$libs"
universal_copy_lib $ENV libzip "$WORK/$libs"
universal_copy_lib $ENV libquickmail "$WORK/$libs"
universal_copy_lib $ENV libcurl "$WORK/$libs"
universal_copy_lib $ENV libffi "$WORK/$libs"
universal_copy_lib $ENV libiconv "$WORK/$libs"
universal_copy_lib $ENV libpango "$WORK/$libs"
universal_copy_lib $ENV libcairo "$WORK/$libs"
universal_copy_lib $ENV libpixman "$WORK/$libs"
universal_copy_lib $ENV libharfbuzz "$WORK/$libs"

relink=`echo $relink | sed -e "s/\+//g"`

for obj in "$WORK/$macos/dcpomatic2" "$WORK/$macos/dcpomatic2_batch" "$WORK/$macos/dcpomatic2_cli" "$WORK/$macos/dcpomatic2_server_cli" "$WORK/$macos/ffprobe" "$WORK/$libs/"*.dylib; do
  deps=`otool -L "$obj" | awk '{print $1}' | egrep "($relink)" | egrep "($ENV|$ROOT|boost)"`
  changes=""
  for dep in $deps; do
      echo "Relinking $dep into $obj"
      base=`basename $dep`
      # $dep will be a path within 64/; make a 32/ path too
      dep32=`echo $dep | sed -e "s/\/64\//\/32\//g"`
      changes="$changes -change $dep @executable_path/../lib/$base -change $dep32 @executable_path/../lib/$base"
  done
  if test "x$changes" != "x"; then
    install_name_tool $changes "$obj"
  fi
done

cp $ROOT/32/src/dcpomatic/build/platform/osx/Info.plist "$WORK/$approot"
cp $ROOT/32/src/dcpomatic/icons/dcpomatic.icns "$WORK/$resources/DCP-o-matic.icns"
cp $ROOT/32/src/dcpomatic/icons/colour_conversions.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/icons/defaults.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/icons/kdm_email.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/icons/servers.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/icons/tms.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/icons/keys.png "$WORK/$resources"

# i18n: DCP-o-matic .mo files
for lang in de_DE es_ES fr_FR it_IT sv_SE nl_NL; do
  mkdir -p "$WORK/$resources/$lang/LC_MESSAGES"
  cp $ROOT/32/src/dcpomatic/build/src/lib/mo/$lang/*.mo "$WORK/$resources/$lang/LC_MESSAGES"
  cp $ROOT/32/src/dcpomatic/build/src/wx/mo/$lang/*.mo "$WORK/$resources/$lang/LC_MESSAGES"
  cp $ROOT/32/src/dcpomatic/build/src/tools/mo/$lang/*.mo "$WORK/$resources/$lang/LC_MESSAGES"
done

# i18n: wxWidgets .mo files
for lang in de es fr it sv nl; do
  mkdir "$WORK/$resources/$lang"
  cp $ENV/64/share/locale/$lang/LC_MESSAGES/wxstd.mo "$WORK/$resources/$lang"
done

tmp_dmg=$WORK/dcpomatic_tmp.dmg
dmg="$WORK/DCP-o-matic $version.dmg"
vol_name=DCP-o-matic-$version

mkdir -p $WORK/$vol_name
cp -a "$WORK/$appdir" $WORK/$vol_name
ln -s /Applications "$WORK/$vol_name/Applications"

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
           set position of item "DCP-o-matic 2.app" of container window to {90, 80}
           set position of item "Applications" of container window to {310, 80}
           close
           open
           update without registering applications
           delay 5
     end tell
   end tell
' | osascript

chmod -Rf go-w /Volumes/"$vol_name"/"$appdir"
sync

hdiutil eject $device
hdiutil convert -format UDZO $tmp_dmg -imagekey zlib-level=9 -o "$dmg"
sips -i "$WORK/$resources/DCP-o-matic.icns"
DeRez -only icns "$WORK/$resources/DCP-o-matic.icns" > "$WORK/$resources/DCP-o-matic.rsrc"
Rez -append "$WORK/$resources/DCP-o-matic.rsrc" -o "$dmg"
SetFile -a C "$dmg"
