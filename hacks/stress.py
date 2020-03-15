#!/usr/bin/python3

import random
import subprocess

def make_seeks(dcp, output, seeks):
    with open(output, 'w') as f:
        # Open the DCP and start it playing
        print("O %s" % dcp, file=f)
        print("P", file=f)
        for i in range(seeks):
            # Wait a bit
            print("W %d" % random.randint(500, 60000), file=f)
	    # Seek
            print("K %d" % random.randint(0, 4095), file=f)
	    # Make sure we're still playing
            print("P", file=f)
        print("S", file=f)

def make_repeated_play(dcp, output, plays):
    length_parts = subprocess.check_output(['dcpinfo', '-o', 'total-time', dcp]).decode('utf-8').split(' ')[1].split(':')
    # Hackily ignore frames here
    length = int(length_parts[0]) * 3600 + int(length_parts[1]) * 60 + int(length_parts[2])
    with open(output, 'w') as f:
        print("O %s" % dcp, file=f)
        for i in range(0, plays):
            print("P", file=f)
            print("W %d" % (length * 1000), file=f)
            print("S", file=f)
            print("K 0", file=f)

make_seeks("/home/carl/DCP/Examples/BohemianRhapsody_TLR-7_S_DE-XX_DE_51_2K_TCFG_20180514_TM_IOP_OV/", "boho_seek", 64)
make_repeated_play("/home/carl/DCP/Examples/BohemianRhapsody_TLR-7_S_DE-XX_DE_51_2K_TCFG_20180514_TM_IOP_OV/", "boho_long", 1000)



