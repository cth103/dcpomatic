import shlex
import subprocess
import sys

if len(sys.argv) != 7:
    print("Syntax: %s <in-pdf> <out-pdf> <left> <bottom> <right> <top>" % sys.argv[0])
    sys.exit(1)

in_pdf = sys.argv[1]
out_pdf = sys.argv[2]

cp = subprocess.run(shlex.split('gs -dNOPAUSE -dBATCH -sDEVICE=bbox %s' % in_pdf), capture_output=True)
if cp.returncode != 0:
    print("gs call failed", file=sys.stderr)
    sys.exit(1)
for line in cp.stderr.splitlines():
    print(line)
    if line.startswith(b"%%BoundingBox"):
        old_bbox = line.split()[1:]
        new_bbox = []
        for i in range(0, 2):
            new_bbox.append(int(old_bbox[i]) + int(sys.argv[i+3]))
        for i in range(2, 4):
            new_bbox.append(int(old_bbox[i]) - int(sys.argv[i+3]))
        print(new_bbox)
        cp = subprocess.run(shlex.split('pdfcrop --bbox "%d %d %d %d" %s %s' % (new_bbox[0], new_bbox[1], new_bbox[2], new_bbox[3], in_pdf, out_pdf)))

