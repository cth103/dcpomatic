#!/bin/bash

dsh -m cs2-1  -m cs2-2  -m cs2-3  -m cs2-4  -m cs2-5  -m cs2-6  -m cs2-7  -m cs2-8 \
    "screen -dmS dcpomatic bash -c 'cd src/dcpomatic2; LD_LIBRARY_PATH=$HOME/ubuntu/lib run/dcpomatic_server_cli --verbose'"

#    -m cs2-9  -m cs2-10 -m cs2-11 -m cs2-12 -m cs2-13 -m cs2-14 -m cs2-15 -m cs2-16 \
#    -m cs2-17 -m cs2-18 -m cs2-19 -m cs2-20                               -m cs2-24 \
#    -m cs2-25 \
