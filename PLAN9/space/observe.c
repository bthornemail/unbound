/*
 * observe - the 8th, the Observer, on a running floor, with mach(2).
 *	observe pid			each character's tag and state, and the tick
 *	observe pid TAG state		interject: write that character's state
 *	observe pid TAG exact=v,ideal=v,min=v,max=v
 *		constrain, as MediaStreamTrack.applyConstraints does, without writing:
 *		of the 8 states of the character's K orbit, those that meet exact, min
 *		and max (mandatory; none meets them: OverconstrainedError naming the
 *		constraint), the nearest to ideal by bits flipped, the first on a tie;
 *		prints the constraints, the settings (the state now) and the state
 *		chosen with the ticks until the floor reaches it
 * The floor's text gives its symbols (crackhdr, syminit, lookup); its memory is
 * mapped through /proc/pid/mem (attachproc). An interjection stops the floor
 * (/proc/pid/ctl), since mem is written only while stopped, then starts it. The floor goes on stepping K from
 * whatever the Observer wrote. Ch is floor.c's: point, tag, hue, state.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

enum { NCH = 8 };

static Map *map;
static char *pid;

static void
ctl(char *msg)
{
	int fd;
	char *p;

	p = smprint("/proc/%s/ctl", pid);
	if((fd = open(p, OWRITE)) < 0 || write(fd, msg, strlen(msg)) < 0)
		sysfatal("%s %s: %r", p, msg);
	close(fd);
	free(p);
}

static uvlong
addr(char *name)
{
	Symbol s;

	if(lookup(nil, name, &s) < 0)
		sysfatal("no symbol %s", name);
	return s.value;
}

static ulong
word(uvlong a)
{
	ulong v;

	if(get4(map, a, &v) < 0)
		sysfatal("get4 %#llux: %r", a);
	return v;
}

static char*
str(uvlong a)
{
	static char buf[16];
	uchar c;
	int i;

	for(i = 0; i < sizeof buf - 1; i++){
		if(get1(map, a + i, &c, 1) < 0)
			sysfatal("get1: %r");
		if((buf[i] = c) == 0)
			break;
	}
	buf[i] = 0;
	return buf;
}

static ushort
rotl(ushort x, int n)
{
	return x<<n | x>>(16-n);
}

/* floor.c and space/src/gnomon.ts K */
static ushort
K(ushort p)
{
	return rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14) ^ 0x1D;
}

static int
flips(ushort x)
{
	int n;

	for(n = 0; x; x &= x - 1)
		n++;
	return n;
}

/* constraints: exact, ideal, min, max, each -1 when absent */
static void
constrain(char *tag, ushort now, char *spec)
{
	long c[4];
	char *f[4], *kv[2], *names[] = { "exact", "ideal", "min", "max" };
	ushort orbit[8], s;
	int i, j, n, best, bestd, d, met;

	for(j = 0; j < 4; j++)
		c[j] = -1;
	print("constraints %s %s\n", tag, spec);
	n = getfields(spec, f, nelem(f), 1, ",");
	for(i = 0; i < n; i++){
		if(getfields(f[i], kv, 2, 1, "=") != 2)
			sysfatal("constraint %s: name=value", f[i]);
		for(j = 0; j < 4; j++)
			if(strcmp(kv[0], names[j]) == 0)
				c[j] = strtoul(kv[1], nil, 16);
	}
	s = now;
	for(i = 0; i < 8; i++, s = K(s))
		orbit[i] = s;
	print("settings %s %.4uX\n", tag, now);
	best = -1;
	bestd = 17;
	for(i = 0; i < 8; i++){
		s = orbit[i];
		if((c[0] >= 0 && s != c[0]) || (c[2] >= 0 && s < c[2]) || (c[3] >= 0 && s > c[3]))
			continue;
		d = c[1] >= 0 ? flips(s ^ c[1]) : 0;
		if(d < bestd){
			best = i;
			bestd = d;
		}
	}
	if(best < 0){
		for(j = 0; j < 4; j++){
			if(j == 1 || c[j] < 0)
				continue;
			for(met = i = 0; i < 8; i++)
				met |= j == 0 ? orbit[i] == c[j] : j == 2 ? orbit[i] >= c[j] : orbit[i] <= c[j];
			if(!met)
				break;
		}
		print("OverconstrainedError %s: no state of %s's orbit meets it\n", j < 4 ? names[j] : "min,max", tag);
		return;
	}
	print("chosen %s %.4uX in %d ticks (%d bits from ideal)\n", tag, orbit[best], best, bestd);
}

void
main(int argc, char **argv)
{
	Fhdr f;
	int fd, mem, i, sz;
	uvlong ch, e;
	ushort st;
	char *tag, *p;

	if(argc != 2 && argc != 4){
		fprint(2, "usage: observe pid [TAG state]\n");
		exits("usage");
	}
	pid = argv[1];
	p = smprint("/proc/%s/text", argv[1]);
	if((fd = open(p, OREAD)) < 0)
		sysfatal("%s: %r", p);
	if(crackhdr(fd, &f) == 0)
		sysfatal("crackhdr: %r");
	machbytype(f.type);
	if(syminit(fd, &f) < 0)
		sysfatal("syminit: %r");
	p = smprint("/proc/%s/mem", argv[1]);
	if((mem = open(p, ORDWR)) < 0)
		sysfatal("%s: %r", p);
	if((map = attachproc(atoi(argv[1]), 0, mem, &f)) == nil)
		sysfatal("attachproc: %r");

	ch = addr("ch");
	sz = 16;	/* int point; char *tag; int hue; ushort state, padded */
	for(i = 0; i < NCH; i++){
		e = ch + i*sz;
		tag = str(word(e + 4));
		if(argc == 4 && strcmp(tag, argv[2]) == 0){
			if(strchr(argv[3], '=') != nil){
				if(get2(map, e + 12, &st) < 0)
					sysfatal("get2: %r");
				constrain(tag, st, argv[3]);
				continue;
			}
			st = strtoul(argv[3], nil, 16);
			ctl("stop");
			if(put2(map, e + 12, st) < 0)
				sysfatal("put2: %r");
			ctl("start");
			print("interjected %s %.4uX\n", tag, st);
		}
		if(get2(map, e + 12, &st) < 0)
			sysfatal("get2: %r");
		print("%-8s %.4uX\n", tag, st);
	}
	print("tick %lud\n", word(addr("tick")));
	exits(nil);
}
