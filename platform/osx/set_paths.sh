base=$HOME/dcpomatic
env=$HOME/osx-environment
sdk=$HOME/SDK

export CPPFLAGS= LDFLAGS="-L$base/lib -L$env/lib -isysroot $sdk/MacOSX10.9.sdk -arch x86_64"
export LINKFLAGS="-L$base/lib -L$env/lib -isysroot $sdk/MacOSX10.9.sdk -arch x86_64"
export MACOSX_DEPLOYMENT_TARGET=10.9
export CXXFLAGS="-I$base/include -I$env/include -isysroot $sdk/MacOSX10.9.sdk -arch x86_64"
export CFLAGS="-I$base/include -I$env/include -isysroot $sdk/MacOSX10.9.sdk -arch x86_64"
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$base/lib:$env/64/lib
export PATH=$env/64/bin:$PATH
export PKG_CONFIG_PATH=$env/64/lib/pkgconfig:$base/lib/pkgconfig
