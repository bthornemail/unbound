/*
 * zframe - the frame as a 9P file server: eight 16-bit segments, one per
 * character of the Commons, and one global tick, f mod 60.
 *
 *	zframe [-m mtpt]
 *
 *	/n/frame/0 .. 7	a segment: its character's tag and state (hex)
 *	/n/frame/tick	the global count, mod 60
 *	/n/frame/ctl	write "tick": every segment advances once by
 *			K(p, 0x1D) = rotl(p,1) ^ rotl(p,3) ^ rotr(p,2) ^ 0x1D
 *			(tetra-gnomon-anim.c), and the count by one, mod 60
 * Each tag is first compared locally, by class (boxedPINE src/terms.mjs):
 * its consonants against YHWH and its vowels against YAHWEH's (ae), byte by
 * byte by XOR, a byte past either end 0; each comparison is folded to 16
 * bits by XORing its byte pairs, high byte first (the fold is a declared
 * choice). A segment's seed is its consonant comparison. Reading a segment
 * gives: tag, consonants, vowels, the two comparisons, and its state.
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum { Nseg = 8, Dial = 60, C = 0x1D };

static char *tag[Nseg] = { "METATRON", "SOLOMON", "SOLON", "ASABIYAH", "ENOCH", "SPEAKER", "GENESIS", "OBSERVER" };
static u16int seg[Nseg], csig[Nseg], vsig[Nseg];
static char cons[Nseg][16], vows[Nseg][16];

static int
isvowel(int c)
{
	return strchr("AEIOU", c) != nil;
}

/* the byte-wise XOR of s against base, folded to 16 bits by its byte pairs */
static u16int
fold(char *s, char *base)
{
	int n, i;
	u16int w;
	uchar x;

	n = strlen(s) > strlen(base) ? strlen(s) : strlen(base);
	for(w = 0, i = 0; i < n; i++){
		x = (i < strlen(s) ? s[i] : 0) ^ (i < strlen(base) ? base[i] : 0);
		w ^= i % 2 == 0 ? x << 8 : x;
	}
	return w;
}
static int tick;

static u16int
rotl(u16int x, int n)
{
	return x << n | x >> (16 - n);
}

static u16int
K(u16int p)
{
	return rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14) ^ C;	/* rotr(p, 2) = rotl(p, 14) */
}

static void
fsread(Req *r)
{
	char buf[128];
	int i;

	i = (int)(uintptr)r->fid->file->aux;
	if(i < Nseg)
		snprint(buf, sizeof buf, "%s %s %s ^YHWH %.4ux ^ae %.4ux state %.4ux\n", tag[i], cons[i], vows[i], csig[i], vsig[i], seg[i]);
	else if(i == Nseg)
		snprint(buf, sizeof buf, "%d\n", tick);
	else
		snprint(buf, sizeof buf, "tick\n");
	readstr(r, buf);
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	int i;

	if((int)(uintptr)r->fid->file->aux != Nseg + 1){
		respond(r, "read only");
		return;
	}
	if(r->ifcall.count < 4 || memcmp(r->ifcall.data, "tick", 4) != 0){
		respond(r, "the only control is tick");
		return;
	}
	for(i = 0; i < Nseg; i++)
		seg[i] = K(seg[i]);
	tick = (tick + 1) % Dial;
	r->ofcall.count = r->ifcall.count;
	respond(r, nil);
}

Srv fs = {
	.read = fsread,
	.write = fswrite,
};

void
main(int argc, char *argv[])
{
	char *mtpt, name[8];
	int i;

	mtpt = "/n/frame";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("usage: zframe [-m mtpt]"));
		break;
	}ARGEND
	fs.tree = alloctree(nil, nil, DMDIR|0555, nil);
	for(i = 0; i < Nseg; i++){
		{
			char *t, *c, *v;
			for(t = tag[i], c = cons[i], v = vows[i]; *t; t++)
				if(isvowel(*t))
					*v++ = *t - 'A' + 'a';	/* vowels go lower, 0x20 away */
				else
					*c++ = *t;
		}
		csig[i] = fold(cons[i], "YHWH");
		vsig[i] = fold(vows[i], "ae");
		seg[i] = csig[i];
		snprint(name, sizeof name, "%d", i);
		closefile(createfile(fs.tree->root, name, nil, 0444, (void*)(uintptr)i));
	}
	closefile(createfile(fs.tree->root, "tick", nil, 0444, (void*)(uintptr)Nseg));
	closefile(createfile(fs.tree->root, "ctl", nil, 0666, (void*)(uintptr)(Nseg + 1)));
	postmountsrv(&fs, nil, mtpt, MREPL);
	exits(nil);
}
