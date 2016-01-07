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

# Main application
appdir="DCP-o-matic 2.app"
approot="$appdir/Contents"
libs="$approot/lib"
macos="$approot/MacOS"
resources="$approot/Resources"
rm -rf "$WORK/$appdir"
mkdir -p "$WORK/$macos"
mkdir -p "$WORK/$libs"
mkdir -p "$WORK/$resources"

# KDM creator
appdir_kdm="DCP-o-matic 2 KDM Creator.app"
approot_kdm="$appdir_kdm/Contents"
libs_kdm="$approot_kdm/lib"
macos_kdm="$approot_kdm/MacOS"
resources_kdm="$approot_kdm/Resources"
rm -rf "$WORK/$appdir_kdm"
mkdir -p "$WORK/$macos_kdm"

# Server
appdir_server="DCP-o-matic 2 Server.app"
approot_server="$appdir_server/Contents"
libs_server="$approot_server/lib"
macos_server="$approot_server/MacOS"
resources_server="$approot_server/Resources"
rm -rf "$WORK/$appdir_server"
mkdir -p "$WORK/$macos_server"

# Batch converter
appdir_batch="DCP-o-matic 2 Batch Converter.app"
approot_batch="$appdir_batch/Contents"
libs_batch="$approot_batch/lib"
macos_batch="$approot_batch/MacOS"
resources_batch="$approot_batch/Resources"
rm -rf "$WORK/$appdir_batch"
mkdir -p "$WORK/$macos_batch"

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
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_create "$WORK/$macos"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_kdm "$WORK/$macos_kdm"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_server "$WORK/$macos_server"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_batch "$WORK/$macos_batch"
universal_copy $ROOT src/dcpomatic/build/src/lib/libdcpomatic2.dylib "$WORK/$libs"
universal_copy $ROOT src/dcpomatic/build/src/wx/libdcpomatic2-wx.dylib "$WORK/$libs"
universal_copy_lib $ROOT libcxml "$WORK/$libs"
universal_copy_lib $ROOT libdcp-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libasdcp-libdcp-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libkumu-libdcp-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libsub "$WORK/$libs"
universal_copy_lib $ROOT libasdcp-libsub-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libkumu-libsub-1.0 "$WORK/$libs"
universal_copy_lib $ROOT libopenjp2 "$WORK/$libs"
universal_copy_lib $ROOT libavdevice "$WORK/$libs"
universal_copy_lib $ROOT libavformat "$WORK/$libs"
universal_copy_lib $ROOT libavfilter "$WORK/$libs"
universal_copy_lib $ROOT libavutil "$WORK/$libs"
universal_copy_lib $ROOT libavcodec "$WORK/$libs"
universal_copy_lib $ROOT libswscale "$WORK/$libs"
universal_copy_lib $ROOT libpostproc "$WORK/$libs"
universal_copy_lib $ROOT libswresample "$WORK/$libs"
universal_copy $ROOT bin/ffprobe "$WORK/$macos"
universal_copy_lib $ENV libboost_system "$WORK/$libs"
universal_copy_lib $ENV libboost_filesystem "$WORK/$libs"
universal_copy_lib $ENV libboost_thread "$WORK/$libs"
universal_copy_lib $ENV libboost_date_time "$WORK/$libs"
universal_copy_lib $ENV libboost_locale "$WORK/$libs"
universal_copy_lib $ENV libboost_regex "$WORK/$libs"
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
universal_copy_lib $ENV libsamplerate "$WORK/$libs"
universal_copy_lib $ENV libicui18n "$WORK/$libs"
universal_copy_lib $ENV libicudata "$WORK/$libs"
universal_copy_lib $ENV libicuio "$WORK/$libs"
universal_copy_lib $ENV libicule "$WORK/$libs"
universal_copy_lib $ENV libiculx "$WORK/$libs"
universal_copy_lib $ENV libicutest "$WORK/$libs"
universal_copy_lib $ENV libicutu "$WORK/$libs"
universal_copy_lib $ENV libicuuc "$WORK/$libs"
universal_copy_lib $ENV libFLAC "$WORK/$libs"
universal_copy_lib $ENV libvorbis "$WORK/$libs"
universal_copy_lib $ENV libogg "$WORK/$libs"

relink=`echo $relink | sed -e "s/\+//g"`

for obj in \
    "$WORK/$macos/dcpomatic2" \
    "$WORK/$macos/dcpomatic2_cli" \
    "$WORK/$macos/dcpomatic2_server_cli" \
    "$WORK/$macos_kdm/dcpomatic2_kdm" \
    "$WORK/$macos_server/dcpomatic2_server" \
    "$WORK/$macos_batch/dcpomatic2_batch" \
    "$WORK/$macos/ffprobe" \
    "$WORK/$libs/"*.dylib; do
  deps=`otool -L "$obj" | awk '{print $1}' | egrep "($relink)" | egrep "($ENV|$ROOT|boost|libicu)"`
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

cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2.Info.plist "$WORK/$approot/Info.plist"
cp $ROOT/32/src/dcpomatic/graphics/dcpomatic2.icns "$WORK/$resources/dcpomatic2.icns"
cp $ROOT/32/src/dcpomatic/graphics/dcpomatic2_kdm.icns "$WORK/$resources/dcpomatic2_kdm.icns"
cp $ROOT/32/src/dcpomatic/graphics/dcpomatic2_server.icns "$WORK/$resources/dcpomatic2_server.icns"
cp $ROOT/32/src/dcpomatic/graphics/dcpomatic2_batch.icns "$WORK/$resources/dcpomatic2_batch.icns"
cp $ROOT/32/src/dcpomatic/graphics/colour_conversions.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/defaults.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/kdm_email.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/servers.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/tms.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/keys.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/fonts/LiberationSans-Regular.ttf "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/fonts/LiberationSans-Italic.ttf "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/fonts/LiberationSans-Bold.ttf "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/splash.png "$WORK/$resources"
cp $ROOT/32/src/dcpomatic/graphics/dcpomatic2_server_small.png "$WORK/$resources"

# i18n: DCP-o-matic .mo files
for lang in de_DE es_ES fr_FR it_IT sv_SE nl_NL ru_RU pl_PL da_DK pt_PT sk_SK; do
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

# KDM creator
appdir_kdm="DCP-o-matic 2 KDM Creator.app"
approot_kdm="$appdir_kdm/Contents"
libs_kdm="$approot_kdm/lib"
macos_kdm="$approot_kdm/MacOS"
resources_kdm="$approot_kdm/Resources"
ln -s "../../DCP-o-matic 2.app/Contents/lib" "$WORK/$libs_kdm"
ln -s "../../DCP-o-matic 2.app/Contents/Resources" "$WORK/$resources_kdm"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_kdm.Info.plist "$WORK/$approot_kdm/Info.plist"
cp -a "$WORK/$appdir_kdm" $WORK/$vol_name

# Server
appdir_server="DCP-o-matic 2 Server.app"
approot_server="$appdir_server/Contents"
libs_server="$approot_server/lib"
macos_server="$approot_server/MacOS"
resources_server="$approot_server/Resources"
ln -s "../../DCP-o-matic 2.app/Contents/lib" "$WORK/$libs_server"
ln -s "../../DCP-o-matic 2.app/Contents/Resources" "$WORK/$resources_server"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_server.Info.plist "$WORK/$approot_server/Info.plist"
cp -a "$WORK/$appdir_server" $WORK/$vol_name

# Batch converter
appdir_batch="DCP-o-matic 2 Batch Converter.app"
approot_batch="$appdir_batch/Contents"
libs_batch="$approot_batch/lib"
macos_batch="$approot_batch/MacOS"
resources_batch="$approot_batch/Resources"
ln -s "../../DCP-o-matic 2.app/Contents/lib" "$WORK/$libs_batch"
ln -s "../../DCP-o-matic 2.app/Contents/Resources" "$WORK/$resources_batch"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_batch.Info.plist "$WORK/$approot_batch/Info.plist"
cp -a "$WORK/$appdir_batch" $WORK/$vol_name

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
           set the bounds of container window to {400, 200, 890, 610}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 64
           set position of item "DCP-o-matic 2.app" of container window to {90, 80}
           set position of item "DCP-o-matic 2 KDM Creator.app" of container window to {270, 80}
           set position of item "DCP-o-matic 2 Server.app" of container window to {90, 200}
           set position of item "DCP-o-matic 2 Batch Converter.app" of container window to {270, 200}
           set position of item "Applications" of container window to {450, 80}
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
sips -i "$WORK/$resources/dcpomatic2.icns"
DeRez -only icns "$WORK/$resources/dcpomatic2.icns" > "$WORK/$resources/DCP-o-matic.rsrc"
Rez -append "$WORK/$resources/DCP-o-matic.rsrc" -o "$dmg"
SetFile -a C "$dmg"
