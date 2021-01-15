mkdir -p build/src/Resources
cp -r ../libdcp/{tags,xsd} build/src/Resources
cp graphics/{link,splash}.png build/src/Resources
cp graphics/osx/preferences/*.png build/src/Resources
ln -s $(which openssl) build/src/tools/openssl
