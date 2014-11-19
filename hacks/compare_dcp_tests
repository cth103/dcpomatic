#!/bin/bash

name=$1
if [ "$name" == "" ]; then
  echo "Syntax $0 <name>"
  exit 1
fi

mkdir -p tmp/ref
cp test/data/$name/*video.mxf tmp/ref
cd tmp/ref
asdcp-unwrap *.mxf
for f in *.j2c; do 
  j2k_to_image -i $f -o $f.png
done
cd ../..

mkdir -p tmp/new
for d in `find build/test/$name -mindepth 1 -maxdepth 1 -type d`; do
  b=`basename $d`
  echo $d, $b
  if [ $b != "info" -a $b != "video" ]; then
      cp $d/*video.mxf tmp/new
  fi
done

cd tmp/new
asdcp-unwrap *.mxf
for f in *.j2c; do 
  j2k_to_image -i $f -o $f.png
done
cd ../..

eog tmp/ref/*.png &
eog tmp/new/*.png 

