#!/bin/bash
killall dcpomatic2_server_cli || :
screen -dmS dcpomatic bash -c 'cd src/dcpomatic; LD_LIBRARY_PATH=$HOME/lib run/dcpomatic_server_cli --verbose'
