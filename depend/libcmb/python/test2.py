#!/usr/bin/env python
from cmb import *

total = 0
choice = 2
num = 10

items = range(1, num)
count = len(items)

config = CMB(size_min = choice, size_max = choice, show_numbers=1, debug=1)

print "Silently enumerating choose-%u from %u:" % (choice, num)
def afunc(*args):
    print `args`
    global total
    total += 1
    return 0
cmb_callback(config, len(items), items, afunc)
print "%u callbacks executed" % total
