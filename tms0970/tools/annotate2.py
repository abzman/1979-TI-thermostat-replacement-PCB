import sys
L = {int(l.split(':')[0],16): l.rstrip().split(None, 2)[2] for l in open('/home/claude/work/listing.txt')}
for l in sys.stdin:
    p = l.split(None, 1)
    try: a = int(p[0], 16)
    except: print(l.rstrip()); continue
    print('%-14s %s' % (L[a], l.rstrip()))
