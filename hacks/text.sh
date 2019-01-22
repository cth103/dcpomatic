#!/bin/bash
g++ -o text text.cc `pkg-config --cflags --libs cairomm-1.0 glibmm-2.4 pangomm-1.4`
docker run -u $USER -w $(pwd) -v $(pwd):$(pwd) -e PKG_CONFIG_PATH=/opt/mxe/usr/x86_64-w64-mingw32.shared/lib/pkgconfig -it windows /bin/bash text-win.sh
