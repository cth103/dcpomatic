#!/bin/bash
#
# Syntax: make_dmg.sh <builddir>
#
# e.g. make_dmg.sh /Users/carl/cdist

# Don't set -e here as egrep (used a few times) returns 1 if no matches
# were found.

version=`git describe --tags --abbrev=0 | sed -e "s/v//"`

# DMG size in megabytes
DMG_SIZE=256
ENV=/Users/carl/Environments/dcpomatic
ROOT=$1

# This is our work area for making up the .dmgs
mkdir -p build/platform/osx
cd build/platform/osx

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
    to_relink="$to_relink|$2"
}

# @param #1 directory to copy to
function copy_libs {
    local dest="$1"
    universal_copy_lib $ROOT libcxml "$dest"
    universal_copy_lib $ROOT libdcp-1.0 "$dest"
    universal_copy_lib $ROOT libasdcp-cth "$dest"
    universal_copy_lib $ROOT libkumu-cth "$dest"
    universal_copy_lib $ROOT libsub "$dest"
    universal_copy_lib $ROOT libopenjp2 "$dest"
    universal_copy_lib $ROOT libavdevice "$dest"
    universal_copy_lib $ROOT libavformat "$dest"
    universal_copy_lib $ROOT libavfilter "$dest"
    universal_copy_lib $ROOT libavutil "$dest"
    universal_copy_lib $ROOT libavcodec "$dest"
    universal_copy_lib $ROOT libswscale "$dest"
    universal_copy_lib $ROOT libpostproc "$dest"
    universal_copy_lib $ROOT libswresample "$dest"
    universal_copy $ROOT src/dcpomatic/build/src/lib/libdcpomatic2.dylib "$dest"
    universal_copy $ROOT src/dcpomatic/build/src/wx/libdcpomatic2-wx.dylib "$dest"
    universal_copy_lib $ENV libboost_system "$dest"
    universal_copy_lib $ENV libboost_filesystem "$dest"
    universal_copy_lib $ENV libboost_thread "$dest"
    universal_copy_lib $ENV libboost_date_time "$dest"
    universal_copy_lib $ENV libboost_locale "$dest"
    universal_copy_lib $ENV libboost_regex "$dest"
    universal_copy_lib $ENV libxml++ "$dest"
    universal_copy_lib $ENV libxslt "$dest"
    universal_copy_lib $ENV libxml2 "$dest"
    universal_copy_lib $ENV libglibmm-2.4 "$dest"
    universal_copy_lib $ENV libgobject "$dest"
    universal_copy_lib $ENV libgthread "$dest"
    universal_copy_lib $ENV libgmodule "$dest"
    universal_copy_lib $ENV libsigc "$dest"
    universal_copy_lib $ENV libglib-2 "$dest"
    universal_copy_lib $ENV libintl "$dest"
    universal_copy_lib $ENV libsndfile "$dest"
    universal_copy_lib $ENV libMagick++ "$dest"
    universal_copy_lib $ENV libMagickCore "$dest"
    universal_copy_lib $ENV libMagickWand "$dest"
    universal_copy_lib $ENV libssh "$dest"
    universal_copy_lib $ENV libwx "$dest"
    universal_copy_lib $ENV libfontconfig "$dest"
    universal_copy_lib $ENV libfreetype "$dest"
    universal_copy_lib $ENV libexpat "$dest"
    universal_copy_lib $ENV libltdl "$dest"
    universal_copy_lib $ENV libxmlsec1 "$dest"
    universal_copy_lib $ENV libcurl "$dest"
    universal_copy_lib $ENV libffi "$dest"
    universal_copy_lib $ENV libpango "$dest"
    universal_copy_lib $ENV libcairo "$dest"
    universal_copy_lib $ENV libpixman "$dest"
    universal_copy_lib $ENV libharfbuzz "$dest"
    universal_copy_lib $ENV libsamplerate "$dest"
    universal_copy_lib $ENV libicui18n "$dest"
    universal_copy_lib $ENV libicudata "$dest"
    universal_copy_lib $ENV libicuio "$dest"
    universal_copy_lib $ENV libicule "$dest"
    universal_copy_lib $ENV libiculx "$dest"
    universal_copy_lib $ENV libicutest "$dest"
    universal_copy_lib $ENV libicutu "$dest"
    universal_copy_lib $ENV libicuuc "$dest"
    universal_copy_lib $ENV libFLAC "$dest"
    universal_copy_lib $ENV libvorbis "$dest"
    universal_copy_lib $ENV libogg "$dest"
}

# @param #1 directory to copy to
function copy_resources {
    local dest="$1"
    cp $ROOT/32/src/dcpomatic/graphics/osx/dcpomatic_small.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/dcpomatic2.icns "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/dcpomatic2_kdm.icns "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/dcpomatic2_server.icns "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/dcpomatic2_player.icns "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/dcpomatic2_batch.icns "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/colour_conversions.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/defaults.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/kdm_email.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/email.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/servers.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/tms.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/keys.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/cover_sheet.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/osx/preferences/notifications.png "$dest"
    cp $ROOT/32/src/dcpomatic/fonts/LiberationSans-Regular.ttf "$dest"
    cp $ROOT/32/src/dcpomatic/fonts/LiberationSans-Italic.ttf "$dest"
    cp $ROOT/32/src/dcpomatic/fonts/LiberationSans-Bold.ttf "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/splash.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/zoom.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/zoom_all.png "$dest"
    cp $ROOT/32/src/dcpomatic/graphics/select.png "$dest"

    # i18n: DCP-o-matic .mo files
    for lang in de_DE es_ES fr_FR it_IT sv_SE nl_NL ru_RU pl_PL da_DK pt_PT pt_BR sk_SK cs_CZ uk_UA zh_CN ar_LB fi_FI el_GR; do
	mkdir -p "$dest/$lang/LC_MESSAGES"
	cp $ROOT/32/src/dcpomatic/build/src/lib/mo/$lang/*.mo "$dest/$lang/LC_MESSAGES"
	cp $ROOT/32/src/dcpomatic/build/src/wx/mo/$lang/*.mo "$dest/$lang/LC_MESSAGES"
	cp $ROOT/32/src/dcpomatic/build/src/tools/mo/$lang/*.mo "$dest/$lang/LC_MESSAGES"
    done

    # i18n: wxWidgets .mo files
    for lang in de es fr it sv nl ru pl da cs; do
	mkdir "$dest/$lang"
	cp $ENV/64/share/locale/$lang/LC_MESSAGES/wxstd.mo "$dest/$lang"
    done
}

# param $1 list of things that link to other things
function relink {
    to_relink=`echo $to_relink | sed -e "s/\+//g"`
    local linkers=("$@")

    for obj in "${linkers[@]}"; do
	deps=`otool -L "$obj" | awk '{print $1}' | egrep "($to_relink)" | egrep "($ENV|$ROOT|boost|libicu)"`
	changes=""
	for dep in $deps; do
	    base=`basename $dep`
	    # $dep will be a path within 64/; make a 32/ path too
	    dep32=`echo $dep | sed -e "s/\/64\//\/32\//g"`
	    changes="$changes -change $dep @executable_path/../Frameworks/$base -change $dep32 @executable_path/../Frameworks/$base"
	done
	if test "x$changes" != "x"; then
	    install_name_tool $changes -id `basename "$obj"` "$obj"
	fi
    done
}

# @param #1 .app directory
# @param #2 full name e.g. DCP-o-matic Batch Converter
function make_dmg {
    local appdir="$1"
    local full_name="$2"
    tmp_dmg=dcpomatic_tmp.dmg
    dmg="$full_name $version.dmg"
    vol_name=DCP-o-matic-$version

    codesign --deep --force --verify --verbose --sign "Developer ID Application: Carl Hetherington (R82DXSR997)" "$appdir"
    if [ "$?" != "0" ]; then
	echo "Failed to sign .app"
	exit 1
    fi

    mkdir -p $vol_name
    cp -a "$appdir" $vol_name
    ln -s /Applications "$vol_name/Applications"
    cat<<EOF > "$vol_name/READ ME.txt"
Welcome to DCP-o-matic!  The first time you run the program there may be
a long (several-minute) delay while OS X checks the code for viruses and
other malware.  Please be patient!
EOF
    cat<<EOF > "$vol_name/READ ME.de_DE.txt"
Beim erstmaligen Start der DCP-o-matic Anwendungen kann ein längerer
Verifikationsvorgang auftreten.  Dies ist von der OS X Sicherheitsumgebung
'Gatekeeper' verursacht.  Dieser je nach Rechner teils minutenlange
Verifikationsvorgang ist gegenwärtig normal und nicht zu umgehen,
es ist kein Programmfehler.  Warten sie die Verifikation für jede der
DCP-o-matic Anwendungen ab, bei weiteren Programmstarts wird sie nicht
mehr auftreten.
EOF

    rm -f $tmp_dmg "$dmg"
    hdiutil create -srcfolder $vol_name -volname $vol_name -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size $DMG_SIZE $tmp_dmg
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
           set the bounds of container window to {400, 200, 940, 300}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 64
           set position of item "'$appdir'" of container window to {90, 80}
           set position of item "Applications" of container window to {265, 80}
           set position of item "READ ME.txt" of container window to {430, 80}
           set position of item "READ ME.de_DE.txt" of container window to {595, 80}
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
    sips -i "$appdir/Contents/Resources/dcpomatic2.icns"
    DeRez -only icns "$appdir/Contents/Resources/dcpomatic2.icns" > "$appdir/Contents/Resources/DCP-o-matic.rsrc"
    Rez -append "$appdir/Contents/Resources/DCP-o-matic.rsrc" -o "$dmg"
    SetFile -a C "$dmg"
    codesign --verify --verbose --sign "Developer ID Application: Carl Hetherington (R82DXSR997)" "$dmg"
    if [ "$?" != "0" ]; then
	echo "Failed to sign .dmg"
	exit 1
    fi
    rm $tmp_dmg
    rm -rf $vol_name
}

# @param #1 appdir
function setup {
    appdir="$1"
    approot="$appdir/Contents"
    rm -rf "$appdir"
    mkdir -p "$approot/MacOS"
    mkdir -p "$approot/Frameworks"
    mkdir -p "$approot/Resources"

    to_relink="dcpomatic"
    copy_libs "$approot/Frameworks"
    copy_resources "$approot/Resources"
}

# DCP-o-matic main
setup "DCP-o-matic 2.app"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2 "$approot/MacOS"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_cli "$approot/MacOS"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_create "$approot/MacOS"
universal_copy $ROOT bin/ffprobe "$approot/MacOS"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2" "$approot/MacOS/dcpomatic2_cli" "$approot/MacOS/dcpomatic2_create" "$approot/MacOS/ffprobe" "$approot/Frameworks/"*.dylib)
relink "${rl[@]}"
make_dmg "$appdir" "DCP-o-matic"

# DCP-o-matic KDM Creator
setup "DCP-o-matic 2 KDM Creator.app"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_kdm "$approot/MacOS"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_kdm_cli "$approot/MacOS"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_kdm.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_kdm" "$approot/MacOS/dcpomatic2_kdm_cli" "$approot/Frameworks/"*.dylib)
relink "${rl[@]}"
make_dmg "$appdir" "DCP-o-matic KDM Creator"

# DCP-o-matic Encode Server
setup "DCP-o-matic 2 Encode Server.app"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_server "$approot/MacOS"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_server_cli "$approot/MacOS"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_server.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_server" "$approot/MacOS/dcpomatic2_server_cli" "$approot/Frameworks/"*.dylib)
relink "${rl[@]}"
make_dmg "$appdir" "DCP-o-matic Encode Server"

# DCP-o-matic Batch Converter
setup "DCP-o-matic 2 Batch converter.app"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_batch "$approot/MacOS"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_batch.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_batch" "$approot/Frameworks/"*.dylib)
relink "${rl[@]}"
make_dmg "$appdir" "DCP-o-matic Batch Converter"

# DCP-o-matic Player
setup "DCP-o-matic 2 Player.app"
universal_copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_player "$approot/MacOS"
cp $ROOT/32/src/dcpomatic/build/platform/osx/dcpomatic2_player.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_player" "$approot/Frameworks/"*.dylib)
relink "${rl[@]}"
make_dmg "$appdir" "DCP-o-matic Player"
