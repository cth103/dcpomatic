#!/usr/bin/python3
#
# Return DocBook markup for all the verification warnings/errors/"ok" notes of a given
# type in verify.h

from pathlib import Path
import re
import sys
import subprocess

if len(sys.argv) < 3:
    print(f"Syntax: {sys.argv[0]} <path-to-libdcp-source-tree> <ERROR|BV21_ERROR|WARNING|OK>")
    sys.exit(1)

libdcp = Path(sys.argv[1])
type = sys.argv[2]
header = libdcp / "src" / "verify.h"

# The types of note we have
types = ("BV21_ERROR", "ERROR", "WARNING", "OK")

def find_type(name):
    """
    Search verify.cc to find out whether given error is error, Bv2.1 "error",
    warning or "this is OK" note.
    """
    started = False
    next = False
    with open(libdcp / "src" / "verify.cc") as s:
        for line in s:
            if line.find("dcp::VerificationNote::type() const") != -1:
                started = True
            if started and line.find(name) != -1:
                next = True
            elif line.find("return") != -1 and next:
                for type in types:
                    if line.find(type) != -1:
                        return type
    assert False, f"Could not find {name}"


print('<itemizedlist>')

active = False
with open(header) as h:
    for line in h:
        strip = line.strip()
        if strip == "enum class Code {":
            active = True
        elif strip == "};":
            active = False
        elif active:
            if strip.startswith('/**'):
                text = strip.replace('/**', '').replace('*/', '').strip()
            elif not strip.startswith('/*') and not strip.startswith('*') and strip.endswith(','):
                this_type = find_type(strip[:-1])
                if this_type == type:
                    text = re.sub(r"\[.*?\]", lambda m: f'(Bv2.1 {m[0][7:-1]})', text)
                    text = text.replace('<', '&lt;')
                    text = text.replace('>', '&gt;')
                    text = re.sub(r"_(.*?)_", r"<code>\1</code>", text)
                    print(f'<listitem>{text}.</listitem>')

print('</itemizedlist>')
