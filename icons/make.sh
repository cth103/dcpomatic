#!/bin/bash

width=$1
height=$2
output=$3

inkscape -z -e $output -w $width -h $height -D finish-trace.svg
