import sys
L = {int(l.split(':')[0],16): l.rstrip() for l in open('/home/claude/work/listing.txt')}
for l in sys.stdin:
    a = l.split()[0]
    mark = ' <==' if '<==' in l else ''
    print(L[int(a,16)].replace('\t',' ') + mark)
