SDK=$1
if [[ "$SDK" == 11 ]]; then
	isysroot="-isysroot $HOME/SDK/MacOS11.0.sdk"
	base=$HOME/workspace
	export MACOSX_DEPLOYMENT_TARGET=10.10
else
	base=/usr/local
fi

arch=$(uname -m)
if [[ "$arch" == arm64 ]]; then
	env=$HOME/osx-environment/arm64/11.0
else
	env=$HOME/osx-environment/x86_64/10.10
fi
sdk=$HOME/SDK

export CPPFLAGS= LDFLAGS="-L$base/lib -L$env/lib $isysroot -arch $arch"
export LINKFLAGS="-L$base/lib -L$env/lib $isysroot -arch $arch"
export CXXFLAGS="-I$base/include -I$env/include $isysroot -arch $arch"
export CFLAGS="-I$base/include -I$env/include $isysroot -arch $arch"
export PATH=$env/bin:$PATH
export PKG_CONFIG_PATH=$env/lib/pkgconfig:$base/lib/pkgconfig
