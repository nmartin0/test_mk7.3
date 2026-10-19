#!/usr/bin/env python3
#
# bbclockcheck.py -- check the i386 kernel's battery-clock arithmetic
# on the host (docs/deferred.md, S-16).
#
#   python3 docs/tools/bbclockcheck.py [bbclock.c]
#
# Takes bbc_gettime(), bbc_settime(), yeartoday(), hexdectodec(),
# dectohexdec() and month[] out of osfmk/src/mach_kernel/i386/AT386/
# bbclock.c as they stand, compiles them with the host's cc around a
# model of the MC146818's registers, and for every day from 1 January
# 1970 to 18 January 2038 -- where the kernel's 32-bit time_t ends --
# sets the clock, checks the chip holds that calendar date in BCD,
# reads it back and checks the same second returns.  Prints the count
# of days and of failed checks; exits 1 if any failed.
import os, re, subprocess, sys, tempfile

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    '../../osfmk/src/mach_kernel/i386/AT386/bbclock.c')
src = open(path).read()

def func(name):
    m = re.search(r'\n(kern_return_t|int|char)\n' + name + r'\(.*?\n\}\n',
                  src, re.S)
    if not m:
        sys.exit('no %s in %s' % (name, path))
    return m.group(0)

prog = r'''#include <stdio.h>
#include <time.h>
typedef int spl_t; typedef void *thread_t; typedef int kern_return_t;
typedef struct { long tv_sec; long tv_nsec; } tvalspec_t;
struct rtc_st { char rtc_sec, rtc_asec, rtc_min, rtc_amin, rtc_hr,
		rtc_ahr, rtc_dow, rtc_dom, rtc_mon, rtc_yr; };
#define KERN_SUCCESS 0
#define LOCK_BBC() 0
#define UNLOCK_BBC(s) (void)(s)
#define NCPUS 1
static struct rtc_st chip;
static int rtcget(struct rtc_st *r) { *r = chip; return 0; }
static void rtcput(struct rtc_st *r) { chip = *r; }
int yeartoday(int); int hexdectodec(char); char dectohexdec(int);
'''
prog += re.search(r'static\s+int\s+month\[12\].*?;\n', src).group(0)
for n in ('bbc_gettime', 'bbc_settime', 'yeartoday', 'hexdectodec',
          'dectohexdec'):
    prog += func(n)
prog += r'''
int main(void)
{
	long t, days = 0, bad = 0;

	for (t = 12*3600 + 34*60 + 56; t < 2147483647L - 86400; t += 86400) {
		time_t tt = t;
		struct tm *g = gmtime(&tt);
		tvalspec_t in = { t, 0 }, out;

		bbc_settime(&in);
		if (hexdectodec(chip.rtc_yr) != g->tm_year % 100 ||
		    hexdectodec(chip.rtc_mon) != g->tm_mon + 1 ||
		    hexdectodec(chip.rtc_dom) != g->tm_mday)
			bad++;
		bbc_gettime(&out);
		if (out.tv_sec != t)
			bad++;
		days++;
	}
	printf("%ld days, %ld failed checks\n", days, bad);
	return bad != 0;
}
'''
d = tempfile.mkdtemp()
c, exe = os.path.join(d, 'bbc.c'), os.path.join(d, 'bbc')
open(c, 'w').write(prog)
subprocess.check_call(['cc', '-w', '-o', exe, c])
sys.exit(subprocess.call([exe]))
