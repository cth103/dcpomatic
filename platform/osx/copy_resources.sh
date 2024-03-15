mkdir -p build/src/Resources
cp -r ../libdcp/{tags,xsd,ratings} build/src/Resources
cp graphics/{link,splash}.png build/src/Resources
cp graphics/{select,snap,sequence,zoom,zoom_all}.png build/src/Resources
cp fonts/*.ttf build/src/Resources
ln -s $(which openssl) build/src/tools/openssl
