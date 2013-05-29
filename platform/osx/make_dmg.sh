#!/bin/bash

VERSION=@version@

# DMG size in megabytes
DMG_SIZE=64

dmg_name="dvdomatic-$VERSION"

mkdir -p build/platform/osx/mnt

hdiutil create -megabytes $DMG_SIZE build/platform/osx/dvdomatic.dmg
device=$(hdid -nomount build/platform/osx/dvdomatic.dmg | grep Apple_HFS | cut -f 1 -d ' ')
newfs_hfs -v "$dmg_name" "$device"
mount -t hfs "${device}" build/platform/osx/mnt
