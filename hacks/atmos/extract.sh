#!/bin/bash
sox ~/DCP/Examples/atmos_channel_14.wav -t s32 - | od -t d4 -w4 -v | awk '{print $2}'
