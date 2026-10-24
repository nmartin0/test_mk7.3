# extract.py <kern_descrip.c> <out.c> name... : copy each named K&R function verbatim
import sys, re
src = open(sys.argv[1]).read().split('\n'); out = []
for name in sys.argv[3:]:
    i = next((k for k, l in enumerate(src) if re.match(r'^%s\(' % re.escape(name), l)), None)
    if i is None:
        sys.exit('extract.py: %s is not in %s' % (name, sys.argv[1]))
    s = i
    while s > 0 and src[s-1].strip() and not src[s-1].rstrip().endswith((';', '}', '*/')) \
          and not src[s-1].startswith('#'):
        s -= 1
    e = next(k for k in range(i, len(src)) if src[k] == '}')
    out.append('\n'.join(src[s:e+1]))
open(sys.argv[2], 'w').write('\n\n'.join(out) + '\n')
