#!/usr/bin/python3
#
# assuming 48kHz

import sys
from enum import Enum

samples = []
with open('ints', 'r') as f:
    for l in f.readlines():
        l = l.strip()
        if l == "":
            continue
        x = float(l) / pow(2, 31)
        if abs(x - 0.038) < 0.002:
            samples.append(1)
        elif abs(x - 0.092) < 0.002:
            samples.append(2)
        elif abs(x + 0.092) < 0.002:
            samples.append(3)
        elif abs(x + 0.038) < 0.002:
            samples.append(4)
        elif abs(x - 0.071) < 0.002:
            samples.append(5)
        elif abs(x + 0.071) < 0.002:
            samples.append(6)
        else:
            print("Unknown sample %f" % x)
            sys.exit(1)


class State(Enum):
    QUIESCENT = 0
    GOT_SYNC_FIRST = 1
    GOT_SYNC = 2
    AWAIT_UUID_SUB0 = 3
    AWAIT_UUID_SUB1 = 4
    AWAIT_UUID_SUB2 = 5
    AWAIT_UUID_SUB3 = 6
    AWAIT_EUI0 = 7
    AWAIT_EUI1 = 8
    AWAIT_EUI2 = 9
    AWAIT_CRC0 = 10
    AWAIT_CRC1 = 11
    AWAIT_REMBITS = 12

i = 0
bits = []
while True:
    four = samples[i:i+4]
    if four == [1, 2, 2, 1] or four == [4, 3, 3, 4]:
        bits.append(0)
    elif four == [5, 5, 6, 6] or four == [6, 6, 5, 5]:
        bits.append(1)
    elif len(four) == 0:
        break
    else:
        print("Unknown symbol %s" % four)
        sys.exit(1)
    i += 4


def to_int(bits):
    i = 0
    for b in bits:
        i = i << 1
        if b:
            i |= 1
    return i


i = 0
while True:
    sync_word = to_int(bits[i:i+16])
    if sync_word == 0x4d56:
        print("Sync")
    else:
        print("Out of sync")
    i += 16

    edit_rate = to_int(bits[i:i+4])
    if edit_rate == 0:
        print("24fps")
    else:
        print("Unknown edit rate")
    i += 4

    # Reserved
    i += 2

    print("UUID sub index: %d" % to_int(bits[i:i+2]))
    i += 2

    print("UUID sub: %8x" % to_int(bits[i:i+32]))
    i += 32

    # Edit unit index
    print("Edit unit index: %d" % to_int(bits[i:i+24]))
    i += 24

    # CRC
    i += 16

    # Reserved
    i += 4

    # RemBits
    if edit_rate == 0:
        i += 25



