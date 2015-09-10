#!/bin/bash
killall dcpomatic2_server_cli
screen -dmS dcpomatic bash -c 'cd src/dcpomatic2; LD_LIBRARY_PATH=$HOME/ubuntu/lib run/dcpomatic_server_cli --verbose'
