#!/bin/bash

m=""
for n in "$@"; do
  m="$m -m cs2-$n"
done

dsh $m "killall dcpomatic2_server_cli"
