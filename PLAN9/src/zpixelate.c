/*
 * zpixelate - PIXELATE, the author's "both together ... like the pi being resolved from the inside in the omi pi
 * proof, its causal": a polynomial over F2 in x1 to x6, written as wsn/poly.wsn and poly.rules say (Metaverse Kit's
 * written form), cast to its pixels (by the butterfly, as an FFT is computed) and its spectrum.
 *	zpixelate [-n] polys	a line for each line of polys: written, its 64 pixels and its 64 bins, a tab between;
 *				refused, why: not written, out of order, repeated (as omi-types' OMI.Poly says)
 * The pixels: the 64 points, a term (a 6-bit mask, x_i its bit i - 1) lighting the points at or above it, its light
 * cone; 6 stages, stage i XORing each point holding bit i with the point below it without (causal: a pixel only from
 * the terms below it). The spectrum, the Walsh-Hadamard transform of the pixels as +1 and -1: bin a is 64 - 2 x the
 * points where the pixels and the linear function a.x differ, counted here, its signs parity phases (no pi: it
 * enters only at a projection, as in the omi pi proof). -n, the falsifier: the cones taken downward, each pixel
 * from the terms at or above it.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static int down;
static Biobuf out;

/* a term: "1", or x_d joined by "*", each later than the last; its mask, or -1 */
static int
term(char *s, char *e)
{
	int m, last;

	if(e - s == 1 && *s == '1')
		return 0;
	m = 0;
	last = 0;
	for(;;){
		if(e - s < 2 || s[0] != 'x' || s[1] < '1' || s[1] > '6' || s[1] - '0' <= last)
			return -1;
		last = s[1] - '0';
		m |= 1 << (last - 1);
		s += 2;
		if(s == e)
			return m;
		if(*s != '*')
			return -1;
		s++;
	}
}

static int
parity(int v)
{
	int k;

	for(k = 0; v; v &= v - 1)
		k ^= 1;
	return k;
}

static void
cast(char *l)
{
	int *t, nt, i, j, p, f[64], d, m;
	char *s, *e;

	if((t = malloc((strlen(l) + 1) * sizeof t[0])) == nil)
		sysfatal("malloc: %r");
	nt = 0;
	if(strcmp(l, "0") != 0)
		for(s = l;; s = e + 1){
			for(e = s; *e && *e != '+'; e++)
				;
			if((m = term(s, e)) < 0){
				Bprint(&out, "not written\n");
				free(t);
				return;
			}
			t[nt++] = m;
			if(*e == 0)
				break;
		}
	for(i = 0; i < nt; i++)
		for(j = i + 1; j < nt; j++)
			if(t[i] == t[j]){
				Bprint(&out, "repeated\n");
				free(t);
				return;
			}
	for(i = 0; i + 1 < nt; i++)
		if(t[i] > t[i+1]){
			Bprint(&out, "out of order\n");
			free(t);
			return;
		}
	memset(f, 0, sizeof f);
	for(i = 0; i < nt; i++)
		f[t[i]] ^= 1;
	for(i = 0; i < 6; i++)
		for(p = 0; p < 64; p++)
			if((p >> i & 1) != down)
				f[p] ^= f[p ^ 1 << i];
	for(p = 0; p < 64; p++)
		Bputc(&out, '0' + f[p]);
	Bputc(&out, '\t');
	for(i = 0; i < 64; i++){
		for(d = 0, p = 0; p < 64; p++)
			d += f[p] != parity(i & p);
		Bprint(&out, i ? " %d" : "%d", 64 - 2 * d);
	}
	Bputc(&out, '\n');
	free(t);
}

void
main(int argc, char **argv)
{
	Biobuf *in;
	char *l;

	ARGBEGIN{
	case 'n':
		down = 1;
		break;
	default:
		fprint(2, "usage: zpixelate [-n] polys\n");
		exits("usage");
	}ARGEND
	if(argc != 1){
		fprint(2, "usage: zpixelate [-n] polys\n");
		exits("usage");
	}
	if((in = Bopen(argv[0], OREAD)) == nil)
		sysfatal("open %s: %r", argv[0]);
	Binit(&out, 1, OWRITE);
	while((l = Brdstr(in, '\n', 1)) != nil){
		cast(l);
		free(l);
	}
	Bterm(&out);
	exits(nil);
}
