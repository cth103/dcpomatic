#!/bin/bash

m=""
for n in "$@"; do
  m="$m -m cs2-$n"
done

dsh $m "screen -dmS dcpomatic bash -c 'cd src/dcpomatic2; LD_LIBRARY_PATH=$HOME/ubuntu/lib run/dcpomatic_server_cli --verbose'"
