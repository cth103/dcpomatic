#!/bin/bash

checkout=$HOME/tmp/winboost
prefix=/opt/mxe/usr/x86_64-w64-mingw32.shared

docker run --rm -v $HOME:$HOME -w $checkout/src/dcpomatic \
	-e PATH=$PATH:/opt/mxe/usr/bin:$prefix/bin \
	-e CXX=/opt/mxe/usr/bin/x86_64-w64-mingw32.shared-g++ \
	-e PKG_CONFIG_PATH=$prefix/lib/pkgconfig:$checkout/lib/pkgconfig:$checkout/bin/pkgconfig \
	-e WINRC=/opt/mxe/usr/bin/x86_64-w64-mingw32.shared-windres \
	-e LINKFLAGS="-L$checkout/lib -L$checkout/lib64" \
	-e CXXFLAGS="-I$checkout/include" \
	-u carl windows_v2.20.x \
	./waf $*
