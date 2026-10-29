/*
 * zak - the author's atomic-kernel (boxed/omi-files/atomic-kernel), the first
 * project after the delta law, in Plan 9 C. Its three versions of the delta:
 *	zak delta W seed steps	16d-v1 and ak-law-coq-v1: rotl(x,1) ^ rotl(x,3) ^
 *				rotr(x,2) ^ C at width W (16, 32, 64, 128, 256), C the
 *				byte 1D repeated to the width; each step's state and band
 *				(bit length, ones, places where a bit differs from the next)
 *	zak kernel seed steps	kernel-v1, kernel.py: 16 bits, C 0x1D1D, each tick
 *				XORs B[t mod 8] in before the law, B the digits of 1/73;
 *				position, its orbit and offset (divmod by W = 36), phase
 *	zak law			why the period is half the width: the linear part
 *				x + x^3 + x^(W-2) over GF(2), squared until it is 1;
 *				and whether the constant drops out over that many steps
 * The law at width W = 2^m: raising L to the power W/2 multiplies each shift
 * by W/2, and 1, 3 and W-2 times W/2 are W/2, W/2 and 0 modulo W, so
 * L^(W/2) = x^(W/2) + x^(W/2) + 1 = 1. Period 8 is the law at 16 bits;
 * at 256 it is 128. This is the scaling.
 */
#include <u.h>
#include <libc.h>

enum { MAXW = 256, NW = MAXW/64 };

typedef struct Word Word;
struct Word {
	uvlong	w[NW];
};

static int width;

static int
bit(Word *x, int i)
{
	return x->w[i/64] >> (i%64) & 1;
}

static void
setbit(Word *x, int i, int b)
{
	if(b)
		x->w[i/64] |= 1ULL << (i%64);
	else
		x->w[i/64] &= ~(1ULL << (i%64));
}

/* rotl by k at the width: bit i goes to i + k */
static Word
rotl(Word x, int k)
{
	Word r;
	int i;

	memset(&r, 0, sizeof r);
	k %= width;
	for(i = 0; i < width; i++)
		setbit(&r, (i + k) % width, bit(&x, i));
	return r;
}

static Word
xor(Word a, Word b)
{
	int i;

	for(i = 0; i < NW; i++)
		a.w[i] ^= b.w[i];
	return a;
}

static Word
constant(void)
{
	Word c;
	int i;

	memset(&c, 0, sizeof c);
	for(i = 0; i < width; i += 8)
		c.w[i/64] |= 0x1DULL << (i%64);
	return c;
}

static Word
lin(Word x)
{
	return xor(xor(rotl(x, 1), rotl(x, 3)), rotl(x, width - 2));
}

static Word
delta(Word x)
{
	return xor(lin(x), constant());
}

static int
eq(Word a, Word b)
{
	return memcmp(&a, &b, sizeof a) == 0;
}

static int
iszero(Word a)
{
	Word z;

	memset(&z, 0, sizeof z);
	return eq(a, z);
}

static Word
parse(char *s)
{
	Word x;
	int i, n, d;

	memset(&x, 0, sizeof x);
	if(s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2;
	n = strlen(s);
	for(i = 0; i < n; i++){
		d = s[n-1-i];
		d = d >= 'a' ? d - 'a' + 10 : d >= 'A' ? d - 'A' + 10 : d - '0';
		if(4*i < MAXW)
			x.w[4*i/64] |= (uvlong)(d & 15) << (4*i % 64);
	}
	for(i = width; i < MAXW; i++)
		setbit(&x, i, 0);
	return x;
}

static void
band(Word x, int *len, int *ones, int *texture)
{
	int i;

	*len = *ones = *texture = 0;
	for(i = 0; i < width; i++){
		if(bit(&x, i)){
			*len = i + 1;
			(*ones)++;
		}
		*texture += bit(&x, i) != bit(&x, (i + 1) % width);
	}
}

static char*
hex(Word x)
{
	char *s, *p;
	int i, d;

	s = malloc(width/4 + 3);
	p = s;
	*p++ = '0';
	*p++ = 'x';
	for(i = width/4 - 1; i >= 0; i--){
		d = x.w[4*i/64] >> (4*i % 64) & 15;
		*p++ = "0123456789ABCDEF"[d];
	}
	*p = 0;
	return s;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

void
main(int argc, char *argv[])
{
	static int B[8] = { 0, 1, 3, 6, 9, 8, 6, 3 };
	Word x, s, c, d;
	int t, steps, len, ones, tex, pos, W, k, j, e[3], ex[MAXW], n, half;
	char buf[64], *b;

	if(argc == 5 && strcmp(argv[1], "delta") == 0){
		width = atoi(argv[2]);
		if(width < 16 || width > MAXW || width % 8)
			sysfatal("width 16 to 256");
		x = parse(argv[3]);
		steps = atoi(argv[4]);
		for(t = 0; t < steps; t++){
			band(x, &len, &ones, &tex);
			print("%d %s %d %d %d\n", t, hex(x), len, ones, tex);
			x = delta(x);
		}
		exits(nil);
	}
	if(argc == 4 && strcmp(argv[1], "kernel") == 0){
		width = 16;
		x = parse(argv[2]);
		steps = atoi(argv[3]);
		for(W = 0, k = 0; k < 8; k++)
			W += B[k];
		for(pos = 0, t = 0; t < steps; t++){
			band(x, &len, &ones, &tex);
			print("%d %s %d %d %d %d %d %d %d %d\n", t, hex(x), pos, pos / W, pos % W, t % 8, B[t % 8], len, ones, tex);
			pos += B[t % 8];
			memset(&d, 0, sizeof d);
			d.w[0] = B[t % 8];
			x = delta(xor(x, d));
		}
		exits(nil);
	}
	if(argc == 2 && strcmp(argv[1], "law") == 0){
		for(width = 16; width <= MAXW; width *= 2){
			half = width / 2;
			/* L^(2^j): each shift times 2^j modulo the width, kept when it occurs an odd number of times */
			e[0] = 1; e[1] = 3; e[2] = width - 2;
			for(j = 1; (1 << j) <= half; j++){
				memset(ex, 0, sizeof ex);
				for(k = 0; k < 3; k++)
					ex[(e[k] << j) % width] ^= 1;
				for(n = 0, k = 0; k < width; k++)
					n += ex[k];
				if((1 << j) == half / 2)
					check(smprint("width %d: L^%d is not 1 (%d terms)", width, half/2, n), !(n == 1 && ex[0]));
			}
			b = buf;
			for(k = 0; k < width; k++)
				if(ex[k])
					b = seprint(b, buf + sizeof buf, " x^%d", k);
			check(smprint("width %d: L^%d =%s, which is 1", width, half, buf), n == 1 && ex[0] == 1);
			/* the constant over half the width of steps: the sum of L^k (C), k below W/2 */
			c = constant();
			memset(&s, 0, sizeof s);
			for(x = c, k = 0; k < half; k++){
				s = xor(s, x);
				x = lin(x);
			}
			check(smprint("width %d: C = %s drops out, so every state returns after %d steps", width, hex(c), half), iszero(s));
		}
		/* kernel-v1: B XORed in each tick. Since L^8 = 1, eight ticks add one D to
		 * every state, the same D; sixteen add it twice, which cancels */
		width = 16;
		{
			Word z, y, D;
			int p, same, back;

			memset(&z, 0, sizeof z);
			for(D = z, t = 0; t < 8; t++){
				d = z;
				d.w[0] = B[t];
				D = delta(xor(D, d));
			}
			for(same = back = 0, p = 0; p < 65536; p++){
				memset(&x, 0, sizeof x);
				x.w[0] = p;
				for(y = x, t = 0; t < 16; t++){
					d = z;
					d.w[0] = B[t % 8];
					y = delta(xor(y, d));
					if(t == 7)
						same += eq(xor(y, x), D);
				}
				back += eq(y, x);
			}
			check(smprint("kernel-v1: eight ticks add D = %s to every state (%d of 65536), not 0, so the state does not return in 8", hex(D), same), same == 65536 && !iszero(D));
			check(smprint("kernel-v1: sixteen ticks return every state (%d of 65536), while position advances 36 every 8", back), back == 65536);
		}
		exits(fails ? "fail" : nil);
	}
	if(argc == 2 && strcmp(argv[1], "period") == 0){
		/* at every width that holds whole bytes: the law commutes with rotation, so
		 * L^k is 1 exactly when it returns the one bit e0; and K^k is the identity
		 * when, besides, the constant has dropped out over those k steps */
		for(width = 16; width <= MAXW; width += 8){
			Word e0, y;
			int lk;

			memset(&e0, 0, sizeof e0);
			e0.w[0] = 1;
			for(y = lin(e0), lk = 1; !eq(y, e0) && lk < 65536; lk++)
				y = lin(y);
			for(n = width; n % 2 == 0; n /= 2)
				;
			if(!eq(y, e0)){
				print("width %d = %d x %d: L does not return within 65536\n", width, width / n, n);
				continue;
			}
			c = constant();
			memset(&s, 0, sizeof s);
			for(x = c, k = 0; k < lk; k++){
				s = xor(s, x);
				x = lin(x);
			}
			print("width %d = %d x %d: L returns in %d, and C %s\n", width, width / n, n, lk,
				iszero(s) ? "drops out: every state returns in that many" : "does not drop out");
		}
		exits(nil);
	}
	sysfatal("usage: zak delta W seed steps | kernel seed steps | law | period");
}
