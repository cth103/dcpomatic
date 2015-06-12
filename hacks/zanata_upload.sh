#!/bin/bash
ZANATA=/opt/zanata-cli-3.6.0/bin/zanata-cli
dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
tmp=/tmp/zanata.$$
cd $dir/..
./waf pot
mkdir -p $tmp/source $tmp/translated
cp build/src/lib/*.pot build/src/wx/*.pot build/src/tools/*.pot $tmp/source
for f in src/lib/po/*.po; do
  cp $f $tmp/translated/lib_$f
done
for f in src/wx/po/*.po; do
  cp $f $tmp/translated/wx_$f
done
for f in src/tools/po/*.po; do
  cp $f $tmp/translated/tools_$f
done
$ZANATA push --push-type both -s $tmp/source -t $tmp/translated
rm -rf $tmp
