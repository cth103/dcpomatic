#!/bin/bash
#
SYNTAX="make_dmg.sh <environment> <builddir> <type> <apple-id> <apple-password>"
# where <type> is universal or thin
#
# e.g. make_dmg.sh /Users/carl/osx-environment /Users/carl/cdist universal foo@bar.net opensesame

# Don't set -e here as egrep (used a few times) returns 1 if no matches
# were found.

version=`git describe --tags --abbrev=0 | sed -e "s/v//"`

# DMG size in megabytes
DMG_SIZE=256
ENV=$1
ROOT=$2
TYPE=$3
APPLE_ID=$4
APPLE_PASSWORD=$5

if [ "$TYPE" != "universal" -a "$TYPE" != "thin" ]; then
    echo $SYNTAX
    echo "where <type> is universal or thin"
    exit 1
fi

# This is our work area for making up the .dmgs
mkdir -p build/platform/osx
cd build/platform/osx

function copy {
    case $TYPE in
	universal)
	    for f in $1/32/$2; do
		if [ -h $f ]; then
		    ln -s $(readlink $f) "$3/`basename $f`"
		else
		    g=`echo $f | sed -e "s/\/32\//\/64\//g"`
		    mkdir -p "$3"
		    lipo -create $f $g -output "$3/`basename $f`"
		fi
	    done
	    ;;
	thin)
	    if [ -h $1/$2 ]; then
		ln -s $(readlink $1/$2) "$3/`basename $f`"
            else
	        cp $1/$2 "$3"
	    fi
	    ;;
    esac
}

function copy_lib_root {
    case $TYPE in
	universal)
	    for f in $ROOT/32/lib/$1*.dylib; do
		if [ -h $f ]; then
		    ln -s $(readlink $f) "$2/`basename $f`"
		else
		    g=`echo $f | sed -e "s/\/32\//\/64\//g"`
		    mkdir -p "$2"
		    lipo -create $f $g -output "$2/`basename $f`"
		fi
	    done
	    ;;
	thin)
	    for f in $ROOT/lib/$1*.dylib; do
		if [ -h $f ]; then
		    ln -s $(readlink $f) "$2/`basename $f`"
		else
		    mkdir -p "$2"
		    cp $f "$2"
		fi
	    done
	    ;;
    esac
    to_relink="$to_relink|$1"
}

function copy_lib_env {
    case $TYPE in
	universal)
	    for f in $ENV/32/lib/$1*.dylib; do
		if [ -h $f ]; then
		    ln -s $(readlink $f) "$2/`basename $f`"
		else
		    g=`echo $f | sed -e "s/\/32\//\/64\//g"`
		    mkdir -p "$2"
		    lipo -create $f $g -output "$2/`basename $f`"
		fi
	    done
	    ;;
	thin)
	    for f in $ENV/64/lib/$1*.dylib; do
		if [ -h $f ]; then
		    ln -s $(readlink $f) "$2/`basename $f`"
		else
		    mkdir -p "$2"
		    cp $f "$2"
		fi
	    done
	    ;;
    esac
    to_relink="$to_relink|$1"
}

# @param #1 directory to copy to
function copy_libs {
    local dest="$1"
    copy_lib_root libcxml "$dest"
    copy_lib_root libdcp-1.0 "$dest"
    copy_lib_root libasdcp-carl "$dest"
    copy_lib_root libkumu-carl "$dest"
    copy_lib_root libsub "$dest"
    copy_lib_root libopenjp2 "$dest"
    copy_lib_root libavdevice "$dest"
    copy_lib_root libavformat "$dest"
    copy_lib_root libavfilter "$dest"
    copy_lib_root libavutil "$dest"
    copy_lib_root libavcodec "$dest"
    copy_lib_root libswscale "$dest"
    copy_lib_root libpostproc "$dest"
    copy_lib_root libswresample "$dest"
    copy_lib_root liblwext4 "$dest"
    copy_lib_root libblockdev "$dest"
    copy_lib_root libleqm_nrt "$dest"
    copy $ROOT src/dcpomatic/build/src/lib/libdcpomatic2.dylib "$dest"
    copy $ROOT src/dcpomatic/build/src/wx/libdcpomatic2-wx.dylib "$dest"
    copy_lib_env libboost_system "$dest"
    copy_lib_env libboost_filesystem "$dest"
    copy_lib_env libboost_thread "$dest"
    copy_lib_env libboost_date_time "$dest"
    copy_lib_env libboost_locale "$dest"
    copy_lib_env libboost_regex "$dest"
    copy_lib_env libxml++ "$dest"
    copy_lib_env libxslt "$dest"
    copy_lib_env libxml2 "$dest"
    copy_lib_env libglibmm-2.4 "$dest"
    copy_lib_env libgobject "$dest"
    copy_lib_env libgthread "$dest"
    copy_lib_env libgmodule "$dest"
    copy_lib_env libsigc "$dest"
    copy_lib_env libglib-2 "$dest"
    copy_lib_env libintl "$dest"
    copy_lib_env libsndfile "$dest"
    copy_lib_env libssh "$dest"
    copy_lib_env libwx "$dest"
    copy_lib_env libfontconfig "$dest"
    copy_lib_env libfreetype "$dest"
    copy_lib_env libexpat "$dest"
    copy_lib_env libltdl "$dest"
    copy_lib_env libxmlsec1 "$dest"
    copy_lib_env libcurl "$dest"
    copy_lib_env libffi "$dest"
    copy_lib_env libpango "$dest"
    copy_lib_env libcairo "$dest"
    copy_lib_env libpixman "$dest"
    copy_lib_env libharfbuzz "$dest"
    copy_lib_env libsamplerate "$dest"
    copy_lib_env libicui18n "$dest"
    copy_lib_env libicudata "$dest"
    copy_lib_env libicuio "$dest"
    copy_lib_env libicule "$dest"
    copy_lib_env libiculx "$dest"
    copy_lib_env libicutest "$dest"
    copy_lib_env libicutu "$dest"
    copy_lib_env libicuuc "$dest"
    copy_lib_env libFLAC "$dest"
    copy_lib_env libvorbis "$dest"
    copy_lib_env libogg "$dest"
    copy_lib_env libxerces-c "$dest"
}

# @param #1 directory to copy to
function copy_resources {
    local dest="$1"
    case $TYPE in
	universal)
	    local prefix=$ROOT/32
	    ;;
	thin)
	    local prefix=$ROOT
	    ;;
    esac
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic_small.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_kdm.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_server.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_player.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_batch.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_playlist.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_disk.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/dcpomatic2_combiner.icns "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/defaults.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/defaults@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/kdm_email.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/kdm_email@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/email.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/email@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/servers.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/servers@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/tms.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/tms@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/keys.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/keys@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/cover_sheet.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/cover_sheet@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/notifications.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/notifications@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/sound.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/sound@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/identifiers.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/identifiers@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/general.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/general@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/advanced.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/advanced@2x.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/locations.png "$dest"
    cp $prefix/src/dcpomatic/graphics/osx/preferences/locations@2x.png "$dest"
    cp $prefix/src/dcpomatic/fonts/LiberationSans-Regular.ttf "$dest"
    cp $prefix/src/dcpomatic/fonts/LiberationSans-Italic.ttf "$dest"
    cp $prefix/src/dcpomatic/fonts/LiberationSans-Bold.ttf "$dest"
    cp $prefix/src/dcpomatic/fonts/fonts.conf.osx "$dest"/fonts.conf
    cp $prefix/src/dcpomatic/graphics/splash.png "$dest"
    cp $prefix/src/dcpomatic/graphics/zoom.png "$dest"
    cp $prefix/src/dcpomatic/graphics/zoom_all.png "$dest"
    cp $prefix/src/dcpomatic/graphics/select.png "$dest"
    cp $prefix/src/dcpomatic/graphics/snap.png "$dest"
    cp $prefix/src/dcpomatic/graphics/sequence.png "$dest"
    cp $prefix/src/dcpomatic/graphics/me.jpg "$dest"
    cp $prefix/src/dcpomatic/graphics/link.png "$dest"
    cp -r $prefix/share/libdcp/xsd "$dest"
    cp -r $prefix/share/libdcp/tags "$dest"

    # i18n: DCP-o-matic .mo files
    for lang in de_DE es_ES fr_FR it_IT sv_SE nl_NL ru_RU pl_PL da_DK pt_PT pt_BR sk_SK cs_CZ uk_UA zh_CN tr_TR; do
	mkdir -p "$dest/$lang/LC_MESSAGES"
	cp $prefix/src/dcpomatic/build/src/lib/mo/$lang/*.mo "$dest/$lang/LC_MESSAGES"
	cp $prefix/src/dcpomatic/build/src/wx/mo/$lang/*.mo "$dest/$lang/LC_MESSAGES"
	cp $prefix/src/dcpomatic/build/src/tools/mo/$lang/*.mo "$dest/$lang/LC_MESSAGES"
    done

    # i18n: wxWidgets .mo files
    for lang in de es fr it sv nl ru pl da cs; do
	mkdir "$dest/$lang"
	cp $ENV/64/share/locale/$lang/LC_MESSAGES/wxstd.mo "$dest/$lang"
    done
}

# param $1 list of things that link to other things
function relink_relative {
    to_relink=`echo $to_relink | sed -e "s/\+//g"`
    local linkers=("$@")

    for obj in "${linkers[@]}"; do
	deps=`otool -L "$obj" | awk '{print $1}' | egrep "($to_relink)" | egrep "($ENV|$ROOT|boost|libicu)"`
	changes=""
	for dep in $deps; do
	    base=`basename $dep`
	    if [ "$TYPE" == "universal" ]; then
		# $dep will be a path within 64/; make a 32/ path too
		dep32=`echo $dep | sed -e "s/\/64\//\/32\//g"`
		changes="$changes -change $dep @executable_path/../Frameworks/$base -change $dep32 @executable_path/../Frameworks/$base"
	    else
		changes="$changes -change $dep @executable_path/../Frameworks/$base"
	    fi
	done
	if test "x$changes" != "x"; then
	    install_name_tool $changes -id `basename "$obj"` "$obj"
	fi
    done
}

# param $1 directory things should be relinked into
#       $2 list of things that link to other things
function relink_absolute {
    to_relink=`echo $to_relink | sed -e "s/\+//g"`
    target=$1
    shift
    local linkers=("$@")

    for obj in "${linkers[@]}"; do
	deps=`otool -L "$obj" | awk '{print $1}' | egrep "($to_relink)" | egrep "($ENV|$ROOT|boost|libicu)"`
	for dep in $deps; do
	    base=`basename $dep`
            install_name_tool -change "$dep" "$target"/$base -id `basename "$obj"` "$obj"
	done
    done
}

function sign {
    codesign --deep --force --verify --verbose --options runtime --sign "Developer ID Application: Carl Hetherington (R82DXSR997)" "$1"
    if [ "$?" != "0" ]; then
	echo "Failed to sign $1"
	exit 1
    fi
}


# @param #1 .app directory
# @param #2 .pkg or ""
# @param #3 full name e.g. DCP-o-matic Batch Converter
# @param #4 bundle id e.g. com.dcpomatic.batch
function make_dmg {
    local appdir="$1"
    local pkg="$2"
    local full_name="$3"
    local bundle_id="$4"
    tmp_dmg=dcpomatic_tmp.dmg
    dmg="$full_name $version.dmg"
    vol_name=DCP-o-matic-$version

    sign "$appdir"

    if [ "$pkg" != "" ]; then
	productsign --sign "Developer ID Installer: Carl Hetherington (R82DXSR997)" "$pkg" "signed_temp.pkg"
	if [ "$?" != "0" ]; then
	    echo "Failed to sign .pkg"
	    exit 1
	fi
	mv signed_temp.pkg "$pkg"
    fi

    mkdir -p $vol_name
    cp -a "$appdir" $vol_name
    if [ "$pkg" != "" ]; then
        cp -a "$pkg" $vol_name
    fi
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

    if [ "$pkg" != "" ]; then
        cat<<EOF > "$vol_name/READ ME.txt"

To run this software successfully you must install $pkg before running
the .app
EOF
    fi

    if [ "$pkg" != "" ]; then
        cat<<EOF > "$vol_name/READ ME.de_DE.txt"

To run this software successfully you must install $pkg before running
the .app
EOF

    fi
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
           set the bounds of container window to {400, 200, 1160, 600}
           set the bounds of container window to {400, 200, 1160, 600}
           set the bounds of container window to {400, 200, 1160, 600}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 64
           set position of item "'$appdir'" of container window to {90, 80}
           set position of item "Applications" of container window to {265, 80}
           set position of item "READ ME.txt" of container window to {430, 80}
           set position of item "READ ME.de_DE.txt" of container window to {595, 80}
           set position of item "DCP-o-matic Disk Writer.pkg" of container window to {90, 255}
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
    xattr -c "$dmg"

    set -e
    codesign --verify --verbose --options runtime --sign "Developer ID Application: Carl Hetherington (R82DXSR997)" "$dmg"
    set +e

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

case $TYPE in
    universal)
	prefix=$ROOT/32
	;;
    thin)
	prefix=$ROOT
	;;
esac

# DCP-o-matic main
setup "DCP-o-matic 2.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2 "$approot/MacOS"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_cli "$approot/MacOS"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_create "$approot/MacOS"
copy $ROOT bin/ffprobe "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2" "$approot/MacOS/dcpomatic2_cli" "$approot/MacOS/dcpomatic2_create" "$approot/MacOS/ffprobe" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic" com.dcpomatic

# DCP-o-matic KDM Creator
setup "DCP-o-matic 2 KDM Creator.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_kdm "$approot/MacOS"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_kdm_cli "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_kdm.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_kdm" "$approot/MacOS/dcpomatic2_kdm_cli" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic KDM Creator" com.dcpomatic.kdm

# DCP-o-matic Encode Server
setup "DCP-o-matic 2 Encode Server.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_server "$approot/MacOS"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_server_cli "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_server.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_server" "$approot/MacOS/dcpomatic2_server_cli" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic Encode Server" com.dcpomatic.server

# DCP-o-matic Batch Converter
setup "DCP-o-matic 2 Batch converter.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_batch "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_batch.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_batch" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic Batch Converter" com.dcpomatic.batch

# DCP-o-matic Player
setup "DCP-o-matic 2 Player.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_player "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_player.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_player" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic Player" com.dcpomatic.player

# DCP-o-matic Playlist Editor
setup "DCP-o-matic 2 Playlist Editor.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_playlist "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_playlist.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_playlist" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic Playlist Editor" com.dcpomatic.playlist

# DCP-o-matic Combiner
setup "DCP-o-matic 2 Combiner.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_combiner "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_combiner.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_combiner" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"
make_dmg "$appdir" "" "DCP-o-matic Combiner" com.dcpomatic.combiner

# DCP-o-matic Disk Writer .app
setup "DCP-o-matic 2 Disk Writer.app"
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_disk "$approot/MacOS"
copy $ROOT src/openssl/apps/openssl "$approot/MacOS"
cp $prefix/src/dcpomatic/build/platform/osx/dcpomatic2_disk.Info.plist "$approot/Info.plist"
rl=("$approot/MacOS/dcpomatic2_disk" "$approot/Frameworks/"*.dylib)
relink_relative "${rl[@]}"

# DCP-o-matic Disk Writer daemon .pkg

pkgbase=tmp-disk-writer
rm -rf $pkgbase
mkdir $pkgbase
pkgbin=$pkgbase/bin
pkgroot=$pkgbase/root

mkdir -p $pkgroot/Library/LaunchDaemons
cat > $pkgroot/Library/LaunchDaemons/com.dcpomatic.disk.writer.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.dcpomatic.disk.writer</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Library/Application Support/com.dcpomatic/dcpomatic2_disk_writer</string>
    </array>
    <key>EnvironmentVariables</key>
    <dict>
        <key>DYLD_LIBRARY_PATH</key>
        <string><![CDATA[/Library/Application Support/com.dcpomatic]]></string>
    </dict>
    <key>StandardOutPath</key>
    <string>/var/log/dcpomatic_disk_writer_out.log</string>
    <key>StandardErrorPath</key>
    <string>/var/log/dcpomatic_disk_writer_err.log</string>
    <key>LaunchEvents</key>
    <dict>
        <key>com.apple.notifyd.matching</key>
        <dict>
            <key>com.dcpomatic.disk.writer.start</key>
            <dict>
                <key>Notification</key>
                <string>com.dcpomatic.disk.writer.start</string>
            </dict>
        </dict>
    </dict>
</dict>
</plist>
EOF

# Get the binaries together in $pkgbin then move them to the
# place with spaces in the filename to avoid some of the pain of escaping

mkdir $pkgbin
copy $ROOT src/dcpomatic/build/src/tools/dcpomatic2_disk_writer "$pkgbin"
copy_libs "$pkgbin"

rl=("$pkgbin/dcpomatic2_disk_writer" "$pkgbin/"*.dylib)
relink_absolute "/Library/Application Support/com.dcpomatic" "${rl[@]}"

mkdir $pkgbase/scripts
cat > $pkgbase/scripts/postinstall <<EOF
#!/bin/sh
/bin/launchctl unload "/Library/LaunchDaemons/com.dcpomatic.disk.writer.plist"
/bin/launchctl load "/Library/LaunchDaemons/com.dcpomatic.disk.writer.plist"
exit 0
EOF
chmod gou+x $pkgbase/scripts/postinstall

find "$pkgbin" -iname "*.dylib" -print0 | while IFS= read -r -d '' f; do
    sign "$f"
done
sign "$pkgbin/dcpomatic2_disk_writer"

mkdir -p "$pkgroot/Library/Application Support/com.dcpomatic"
mv $pkgbin/* "$pkgroot/Library/Application Support/com.dcpomatic/"
pkgbuild --root $pkgroot --identifier com.dcpomatic.disk.writer --scripts $pkgbase/scripts "DCP-o-matic Disk Writer.pkg"

make_dmg "$appdir" "DCP-o-matic Disk Writer.pkg" "DCP-o-matic Disk Writer" com.dcpomatic.disk

