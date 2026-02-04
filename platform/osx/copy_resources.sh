mkdir -p build/src/Resources
cp -r ../libdcp/{tags,xsd,ratings} build/src/Resources
cp fonts/*.ttf build/src/Resources
cp -r web build/src/Resources
ln -s $(which openssl) build/src/tools/openssl
