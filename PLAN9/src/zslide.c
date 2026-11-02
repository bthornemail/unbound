/*
 * zslide - omi-dom-stack's 240-clock and 5040 slide rule (shared/clock-sliderule.js) in Plan 9 C:
 * the clock's tick, phase (tick mod 240), cycle and colour (the codex's colour at tick/10 mod 24), a cue
 * time on the 240 grid, the slide rule's position, Fano group (position / 720), tetra block (position mod
 * 720 / 240) and the boundary at 4320, and a cue set's schedule and windows.
 *	zslide colors				the 24 of the codex: index hex band fano name
 *	zslide ticks				every tick of the rule, 0 to 5039: tick phase cycle colour hex
 *	zslide cues t ...			each time: t, its tick at 48 a second, at 30
 *	zslide clock start=N step=K|seek=K ...	the clock after each: op tick phase cycle colour
 *	zslide rule length=L position=P slide=D|seek=S ...	the rule after each: op position fano tetra past
 *	zslide sched tps=T window=W seed=a,b,...|- <cues	each cue (id, start, end, text, tab-separated):
 *					id tick phase colour left fano tetra window; then metrics
 *	zslide -n ...			the falsifier: 239 ticks a cycle in place of 240
 * The codex is dimension-pipeline's COLOR_CODEX, which clock-sliderule takes when it is there: 24 entries,
 * 18 colours (six repeat), so a colour does not name its index. Cue times are read as their decimals, the
 * window's values as integers (the seeds are).
 */
#include <u.h>
#include <libc.h>

enum { RULE = 5040, FANO = 720, BOUNDARY = 4320, NCOLOR = 24 };
static int TICKS = 240;

static char *codex[NCOLOR] = {
	"#000000", "#1a1a2e", "#16213e", "#0f3460", "#e94560", "#533483", "#e94560", "#0f3460",
	"#1a1a2e", "#16213e", "#533483", "#e94560", "#f5f5f5", "#e8e8f0", "#c0c0d0", "#a0a0b0",
	"#5b8def", "#6dcea5", "#e0b050", "#e07a7a", "#7a5be0", "#5be0c0", "#e05b8d", "#8de05b",
};

static int colorat(vlong i) { return ((i % NCOLOR) + NCOLOR) % NCOLOR; }
static int colortick(vlong t) { return colorat((t / 10) % NCOLOR); }

static char*
val(char *a, char *name)	/* name=value */
{
	int n;

	n = strlen(name);
	if(strncmp(a, name, n) == 0 && a[n] == '=')
		return a + n + 1;
	return nil;
}

static void
colors(void)
{
	int i;

	for(i = 0; i < NCOLOR; i++)
		print("%d %s %d %d codex-%d\n", i, codex[i], i % 4 + 1, i / 4 % 7, i);
}

static void
ticks(void)
{
	int k;

	for(k = 0; k < RULE; k++)
		print("%d %d %d %d %s\n", k, k % TICKS, k / TICKS, colortick(k), codex[colortick(k)]);
}

static vlong
cuetick(char *s, int tps)
{
	return (vlong)floor(strtod(s, nil) * tps);
}

static void
cues(int argc, char **argv)
{
	int i;

	for(i = 0; i < argc; i++)
		print("%s %lld %lld\n", argv[i], cuetick(argv[i], 48) % TICKS, cuetick(argv[i], 30) % TICKS);
}

static void
clock(int argc, char **argv)
{
	uvlong tick;
	vlong k, i, v;
	char *s;

	tick = 0;
	for(i = 0; i < argc; i++){
		if((s = val(argv[i], "start")) != nil){
			tick = strtoll(s, nil, 10);
			continue;
		}
		if((s = val(argv[i], "step")) != nil){
			v = strtoll(s, nil, 10);
			for(k = 0; k < v; k++)
				tick = (tick + 1) & 0xFFFFFFFFULL;	/* (tick + 1) >>> 0 */
		}else if((s = val(argv[i], "seek")) != nil){
			v = (int)strtoll(s, nil, 10);	/* t | 0 */
			tick = v < 0 ? 0 : v;
		}
		print("%s %lld %lld %lld %d\n", argv[i], (vlong)tick, (vlong)(tick % TICKS), (vlong)(tick / TICKS), colortick(tick));
	}
}

static void
rule(int argc, char **argv)
{
	vlong length, pos, v;
	int i;
	char *s;

	length = RULE;
	pos = 0;
	for(i = 0; i < argc; i++){
		if((s = val(argv[i], "length")) != nil){ length = strtoll(s, nil, 10); continue; }
		if((s = val(argv[i], "position")) != nil){ pos = strtoll(s, nil, 10); continue; }
		if((s = val(argv[i], "slide")) != nil){
			v = strtoll(s, nil, 10);
			pos = ((pos + v) % length + length) % length;
		}else if((s = val(argv[i], "seek")) != nil){
			v = strtoll(s, nil, 10);
			pos = (v % length + length) % length;
		}
		print("%s %lld %lld %lld %d\n", argv[i], pos, pos / FANO, (pos % FANO) / TICKS, pos >= BOUNDARY);
	}
}

static char*
readall(int fd)
{
	char *s, buf[8192];
	long n, len;

	s = strdup("");
	len = 0;
	while((n = read(fd, buf, sizeof buf)) > 0){
		s = realloc(s, len + n + 1);
		memmove(s + len, buf, n);
		len += n;
		s[len] = 0;
	}
	return s;
}

static void
sched(int argc, char **argv)
{
	int tps, window, nseed, i, j, ncue;
	vlong *seed, *slots, tick, left;
	char *s, *in, *l, *nl, *f[4], *p;

	tps = 48;
	window = TICKS;
	seed = nil;
	nseed = 0;
	for(i = 0; i < argc; i++){
		if((s = val(argv[i], "tps")) != nil) tps = atoi(s);
		if((s = val(argv[i], "window")) != nil) window = atoi(s);
		if((s = val(argv[i], "seed")) != nil && strcmp(s, "-") != 0)
			for(p = s; p; p = strchr(p, ',') ? strchr(p, ',') + 1 : nil){
				seed = realloc(seed, (nseed + 1) * sizeof(vlong));
				seed[nseed++] = strtoll(p, nil, 10);
			}
	}
	slots = mallocz(RULE * sizeof(vlong), 1);
	if(nseed)
		for(i = 0; i < RULE; i++)
			slots[i] = seed[i % nseed];
	in = readall(0);
	ncue = 0;
	for(l = in; *l; l = nl){
		nl = strchr(l, '\n');
		if(nl) *nl++ = 0; else nl = l + strlen(l);
		for(i = 0, p = l; i < 4; i++){
			f[i] = p;
			if(i < 3 && (p = strchr(p, '\t')) != nil) *p++ = 0;
			if(p == nil) break;
		}
		tick = cuetick(f[1], tps);
		left = tick % RULE;
		print("%s %lld %lld %d %lld %lld %lld ", f[0], tick, tick % TICKS, colortick(tick), left, left / FANO, (left % FANO) / TICKS);
		for(j = 0; j < window; j++)
			print("%s%lld", j ? "," : "", slots[(left + j) % RULE]);
		print("\n");
		ncue++;
	}
	print("metrics %d 0 0\n", ncue);
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'n':
		TICKS = 239;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zslide [-n] colors|ticks|cues|clock|rule|sched ...");
	if(strcmp(argv[0], "colors") == 0) colors();
	else if(strcmp(argv[0], "ticks") == 0) ticks();
	else if(strcmp(argv[0], "cues") == 0) cues(argc - 1, argv + 1);
	else if(strcmp(argv[0], "clock") == 0) clock(argc - 1, argv + 1);
	else if(strcmp(argv[0], "rule") == 0) rule(argc - 1, argv + 1);
	else if(strcmp(argv[0], "sched") == 0) sched(argc - 1, argv + 1);
	else sysfatal("unknown %s", argv[0]);
	exits(nil);
}
