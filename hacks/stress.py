#!/usr/bin/python3

import random

def make(dcp, output, seeks):
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


make("/home/carl/DCP/Examples/BohemianRhapsody_TLR-7_S_DE-XX_DE_51_2K_TCFG_20180514_TM_IOP_OV/", "boho", 64)


