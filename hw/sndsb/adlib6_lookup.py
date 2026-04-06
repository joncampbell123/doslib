#!/usr/bin/python3

import math

min_db = 20.0 * math.log10(1.0 / 256.0)

lm = [ ]
for i in range(0,256):
    db = 20.0 * math.log10((i+1) / 256.0)
    v = 63 * (db / min_db)
    lm.append(math.floor(v + 0.5))

s = ""
c = 0
for i in lm:
    if (c&15) == 0:
        s += "  "
    s += str(i)+","
    if (c&15) == 15:
        s += "\n"
    c += 1

print(s)

