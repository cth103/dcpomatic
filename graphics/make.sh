#!/bin/bash

width=$1
height=$2
output=$3

inkscape -C -z -e $output -w $width -h $height -D logo.svg
