base=$HOME/workspace
env=$HOME/osx-environment
sdk=$HOME/SDK

export CPPFLAGS= LDFLAGS="-L$base/lib -L$env/x86_64/lib -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export LINKFLAGS="-L$base/lib -L$env/x86_64/lib -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export MACOSX_DEPLOYMENT_TARGET=10.10
export CXXFLAGS="-I$base/include -I$env/x86_64/include -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export CFLAGS="-I$base/include -I$env/x86_64/include -isysroot $sdk/MacOSX11.0.sdk -arch x86_64"
export PATH=$env/x86_64/bin:$PATH
export PKG_CONFIG_PATH=$env/x86_64/lib/pkgconfig:$base/lib/pkgconfig
