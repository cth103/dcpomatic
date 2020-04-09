user=cah
container=371eb645f2c3
src=/home/cah/src/dcpomatic-windows

docker exec -u $user -e src=$src -i -t $container /bin/bash -c 'export CPPFLAGS= LD=x86_64-w64-mingw32.shared-ld PKG_CONFIG_LIBDIR=/opt/mxe/usr/x86_64-w64-mingw32.shared/lib/pkgconfig CC=x86_64-w64-mingw32.shared-gcc WINRC=x86_64-w64-mingw32.shared-windres RANLIB=x86_64-w64-mingw32.shared-ranlib CXXFLAGS="-I/opt/mxe/usr/x86_64-w64-mingw32.shared/include -I$src/include" CXX=x86_64-w64-mingw32.shared-g++ LDFLAGS="-L/opt/mxe/usr/x86_64-w64-mingw32.shared/lib -L$src/lib" PKG_CONFIG_PATH=$src/lib/pkgconfig:$src/bin/pkgconfig PATH=/opt/mxe/usr/x86_64-w64-mingw32.shared/bin:/opt/mxe/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:$srcbin LINKFLAGS="-L/opt/mxe/usr/x86_64-w64-mingw32.shared/lib -L$src/lib" ; cd $src/src/dcpomatic; ./waf configure --target-windows --disable-tests'

