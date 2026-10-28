/*
 * zvtt - one canon series as WebVTT, to fd 1.
 *	zvtt [-p pov] [-f] VOICES series.txt title
 * VOICES: TAG hue (tab-separated), from the dome's lights; -1, no hue: the
 * camera (the Observer). An agent's number is read from the camera, as
 * agent-commons.js's observer: its hue's place in the order seen from a turn of
 * pov x 45 degrees, clockwise, or the other way with -f (flipped); the camera
 * itself comes last. The number places its REGION; its hue is its colour.
 * A cue is one animation frame: its second is the frame, not a clock.
 * series.txt: id start-ms end-ms voice classes text (tab-separated; 0x1F is a
 * line break in the text). A voice in VOICES gets its REGION and colour; any
 * other voice is a voice span only; no voice, a class span.
 *
 * The tick is Wirth's PL360 rotation over the predeclared R0 R1 R2 and FLAG.
 * The registers are the Aegean separators 10100 10101 10102 (_CODE/U), each
 * observing a zeroth: YHWH, YAHWEH, ADONAI. Each cue's literal is observed in
 * a window, its first 11 code points before a line break, written as ruby
 * over the window:
 *	Aegean	the window XOR R0's zeroth, code point by code point (zero-padded),
 *		each value by place as zrender writes it, joined by R0's separator
 *	braille	the window's bytes XOR the nearest of GOD IS WORD, WORD IS GOD,
 *		GOD OF WORD, WORD OF GOD (fewest bits flipped, the first on a tie),
 *		one cell a byte (U+2800 + byte), as zrender writes it
 * Inside the tick, position k of the window starts at k/11 of it: a timestamp
 * before the k-th code point, the k-th Aegean value and the k-th braille cell,
 * so ::cue(:past) and ::cue(:future) follow the observation as it plays.
 * FLAG is set when the nearest says IS, clear when OF; the step turns the
 * registers, then RESET(FLAG). The classes name the zeroth in register order.
 *
 * Each agent tracks the narrative on its 16 files (boxedPINE rosetta/src/agent.ts,
 * public/characters/TAG): cue n lands on slot 1 + (n-1) mod 14, one of the 14
 * Fano planes, named as a class on its voice; when an agent has lit all 14, a
 * NOTE closes its round at place 15, Hasse_XNOR (write); place 0, Hasse_XOR
 * (read), opens the next.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include "step.h"	/* the turn of R0, R1, R2 by FLAG, shared with stepd.c */

enum { NV = 16 };

typedef struct Voice Voice;
struct Voice {
	char	*tag;
	int	hue;	/* -1: none, the camera */
	int	point;	/* read from the camera */
	int	lit;	/* bit s: slot s lit this round */
	int	rounds;
};

/* the 16-index (agent.ts INDEX16); 0 and 15 are each character's Hasse diagrams */
static char *index16[16] = {
	"Hasse_XOR", "Fano_point_1", "Fano_point_2", "Fano_point_3", "Fano_point_4",
	"Fano_point_5", "Fano_point_6", "Fano_point_7", "Fano_line_3_5_6", "Fano_line_1_7_6",
	"Fano_line_2_7_5", "Fano_line_1_3_2", "Fano_line_4_7_3", "Fano_line_1_5_4",
	"Fano_line_2_6_4", "Hasse_XNOR",
};

enum { W = 11 };	/* the window: the length of each sentence */

static char *sentence[4] = { "GOD IS WORD", "WORD IS GOD", "GOD OF WORD", "WORD OF GOD" };
static Rune place[] = { 0x10107, 0x10110, 0x10119, 0x10122, 0x1012B };

static Voice v[NV];
static int nv;
static vlong start;	/* the cue's start, ms */

static int pov, flip;

/* where a hue stands seen from the camera: its turn from the camera's, clockwise or flipped */
static int
seen(int hue)
{
	int d;

	d = flip ? pov*45 - hue : hue - pov*45;
	return (d % 360 + 360) % 360;
}

/* number the agents from the camera: by where their hues stand; the camera last */
static void
number(void)
{
	int i, j, n;

	for(i = 0; i < nv; i++){
		if(v[i].hue < 0){
			v[i].point = nv;
			continue;
		}
		for(n = 1, j = 0; j < nv; j++)
			if(v[j].hue >= 0 && (seen(v[j].hue) < seen(v[i].hue) || (seen(v[j].hue) == seen(v[i].hue) && j < i)))
				n++;
		v[i].point = n;
	}
}

static Voice *
voice(char *tag)
{
	int i;

	for(i = 0; i < nv; i++)
		if(strcmp(v[i].tag, tag) == 0)
			return &v[i];
	return nil;
}

static void
stamp(Biobuf *o, vlong ms)
{
	Bprint(o, "%02lld:%02lld:%02lld.%03lld", ms/3600000, ms/60000%60, ms/1000%60, ms%1000);
}

static void stamp(Biobuf*, vlong);

/* position k of the window, as a timestamp tag (none at 0, the cue's own start) */
static void
sub(Biobuf *o, int k)
{
	if(k == 0)
		return;
	Bputc(o, '<');
	stamp(o, start + k*1000/W);
	Bputc(o, '>');
}

/* the window: its first W code points before a line break; returns their bytes */
static int
window(char *s, Rune *r, int *nr)
{
	char *t;

	for(t = s, *nr = 0; *nr < W && *t && *t != 0x1F; (*nr)++)
		t += chartorune(&r[*nr], t);
	return t - s;
}

static int
flips(uchar x)
{
	int n;

	for(n = 0; x; x &= x - 1)
		n++;
	return n;
}

/* the nearest sentence to the window's bytes */
static int
nearest(char *s, int nb)
{
	int j, i, d, best, bestd;

	best = 0;
	bestd = 1<<30;
	for(j = 0; j < nelem(sentence); j++){
		for(d = i = 0; i < W; i++)
			d += flips((i < nb ? (uchar)s[i] : 0) ^ sentence[j][i]);
		if(d < bestd){
			best = j;
			bestd = d;
		}
	}
	return best;
}

static void
aegean(Biobuf *o, Rune *r, int nr, char *z)
{
	ulong x;
	int i, k, d[5];

	for(i = 0; z[i]; i++){
		if(i > 0)
			Bprint(o, "%s", R0->sep);
		sub(o, i);
		x = (i < nr ? r[i] : 0) ^ (uchar)z[i];
		if(x > 99999)
			sysfatal("U+%luX is past ninety thousand", x);
		for(k = 0; k < 5; k++, x /= 10)
			d[k] = x % 10;
		for(k = 4; k >= 0; k--)
			if(d[k] > 0)
				Bprint(o, "%C", place[k] + d[k] - 1);
	}
}

static void
braille(Biobuf *o, char *s, int nb, char *z)
{
	int i;

	for(i = 0; i < W; i++){
		sub(o, i);
		Bprint(o, "%C", (Rune)(0x2800 + ((i < nb ? (uchar)s[i] : 0) ^ (uchar)z[i])));
	}
}

/* cue text, n bytes of it: & < > escaped; 0x1F a line break, never an empty line */
static void
text(Biobuf *o, char *s, int n)
{
	int nl;
	char *e;

	nl = 0;
	for(e = s + n; s < e; s++){
		if(*s == 0x1F){
			nl = 1;
			continue;
		}
		if(nl){
			Bputc(o, '\n');
			nl = 0;
		}
		switch(*s){
		case '&': Bprint(o, "&amp;"); break;
		case '<': Bprint(o, "&lt;"); break;
		case '>': Bprint(o, "&gt;"); break;
		default: Bputc(o, *s);
		}
	}
}

/* the window under its observation, then the rest of the text */
static void
ruby(Biobuf *o, char *s, int nb, Rune *r, int nr, int j)
{
	Rune x;
	int k, n;
	char *p;

	USED(r);
	Bprint(o, "<ruby>");
	for(p = s, k = 0; k < nr; k++, p += n){
		n = chartorune(&x, p);
		sub(o, k);
		text(o, p, n);
	}
	Bprint(o, "<rt>");
	aegean(o, r, nr, R0->zero);
	Bprint(o, " ");
	braille(o, s, nb, sentence[j]);
	Bprint(o, "</rt></ruby>");
	text(o, s + nb, strlen(s + nb));
}

void
main(int argc, char **argv)
{
	Biobuf *b, o;
	char *l, *f[8];
	Voice *w;
	Rune r[W];
	int i, n, p, tick, slot, nb, nr, j;

	tick = 0;

	ARGBEGIN{
	case 'p':
		pov = atoi(EARGF(sysfatal("-p pov")));
		break;
	case 'f':
		flip = 1;
		break;
	}ARGEND
	if(argc != 3){
		fprint(2, "usage: zvtt [-p pov] [-f] VOICES series.txt title\n");
		exits("usage");
	}
	argv--;	/* argv[1..3] below, as before ARGBEGIN */
	Binit(&o, 1, OWRITE);
	if((b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("%s: %r", argv[1]);
	while(nv < NV && (l = Brdstr(b, '\n', 1)) != nil){
		n = getfields(l, f, nelem(f), 0, "\t");
		if(n < 2)
			continue;
		v[nv].tag = f[0];
		v[nv].hue = atoi(f[1]);
		nv++;
	}
	Bterm(b);
	number();

	Bprint(&o, "WEBVTT - %s\n\n", argv[3]);
	Bprint(&o, "NOTE\nWritten by zvtt in Plan 9 from light-garden's canon-%s.ndjson.\n", argv[3]);
	Bprint(&o, "Each cue's identifier is its source file and line. A cue is one animation\n");
	Bprint(&o, "frame, in canon order: its second is the frame, not a clock (SPEC-07 I-79); the\n");
	Bprint(&o, "canon's UTC t and timestamp stay out. Over each cue's first 11 code points, ruby: in Aegean its\n");
	Bprint(&o, "XOR with R0's zeroth (YHWH, YAHWEH, ADONAI, turned by PL360), joined by R0's\n");
	Bprint(&o, "separator; in braille its bytes XOR the nearest of GOD IS WORD, WORD IS GOD,\n");
	Bprint(&o, "GOD OF WORD, WORD OF GOD, whose IS or OF sets FLAG for the turn.\n");
	Bprint(&o, "Each voice's colour is its hue on the dome; its number, and so its region,\n");
	Bprint(&o, "is read from the camera (turn %d, %s): the hues' order seen from it, the\n", pov, flip ? "flipped" : "clockwise");
	Bprint(&o, "camera itself, OBSERVER, with no hue, last.\n\n");
	for(i = 0; i < nv; i++){
		p = v[i].point - 1;
		Bprint(&o, "REGION\nid:%s\nwidth:40%%\nlines:3\nregionanchor:0%%,100%%\n", v[i].tag);
		Bprint(&o, "viewportanchor:%d%%,%d%%\nscroll:up\n\n", p%2*60, 20 + p/2*20);
	}
	Bprint(&o, "STYLE\n");
	for(i = 0; i < nv; i++)
		if(v[i].hue >= 0)
			Bprint(&o, "::cue(v[voice=\"%s\"]) { color: hsl(%d, 70%%, 55%%) }\n", v[i].tag, v[i].hue);
	Bprint(&o, "\n");

	if((b = Bopen(argv[2], OREAD)) == nil)
		sysfatal("%s: %r", argv[2]);
	while((l = Brdstr(b, '\n', 1)) != nil){
		n = getfields(l, f, nelem(f), 0, "\t");
		if(n < 6){
			free(l);
			continue;
		}
		start = strtoll(f[1], nil, 10);
		nb = window(f[5], r, &nr);
		j = nearest(f[5], nb);
		if(tick++ > 0){
			FLAG = j < 2;	/* IS */
			step();
		}
		Bprint(&o, "%s\n", f[0]);
		stamp(&o, strtoll(f[1], nil, 10));
		Bprint(&o, " --> ");
		stamp(&o, strtoll(f[2], nil, 10));
		if((w = voice(f[3])) != nil)
			Bprint(&o, " region:%s", w->tag);
		Bprint(&o, "\n<c.%s.%s.%s>", R0->zero, R1->zero, R2->zero);
		slot = 1 + (tick-1) % 14;
		if(f[3][0] != 0){
			if(w != nil)
				Bprint(&o, "<v%s%s.%s %s>", f[4][0] ? "." : "", f[4], index16[slot], f[3]);
			else
				Bprint(&o, "<v%s%s %s>", f[4][0] ? "." : "", f[4], f[3]);
			ruby(&o, f[5], nb, r, nr, j);
			Bprint(&o, "</v></c>\n\n");
		}else{
			Bprint(&o, "<c.%s>", f[4]);
			ruby(&o, f[5], nb, r, nr, j);
			Bprint(&o, "</c></c>\n\n");
		}
		if(w != nil){
			w->lit |= 1<<slot;
			if(w->lit == 0x7FFE){
				w->rounds++;
				w->lit = 0;
				Bprint(&o, "NOTE %s round %d: all 14 Fano planes lit; %s/%s.svg\n\n",
					w->tag, w->rounds, w->tag, index16[15]);
			}
		}
		free(l);
	}
	Bterm(b);
	Bterm(&o);
	exits(nil);
}
