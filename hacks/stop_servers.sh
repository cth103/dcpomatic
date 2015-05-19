#!/bin/bash

dsh -m cs2-17 -m cs2-18 -m cs2-19 -m cs2-20 \
    killall dcpomatic_server_cli
