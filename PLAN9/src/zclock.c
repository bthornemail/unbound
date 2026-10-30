/*
 * zclock - the algorithmic clock (boxed omi-files/atomic-kernel: kernel.py, dev-docs/CyclicClock.v,
 * algorithmic_clock_whitepaper_v2.md) in Plan 9 C; I-248. A state is one 16-bit word; a tick XORs
 * B[t mod 8] in and advances by the delta law, rotl 1 ^ rotl 3 ^ rotr 2 ^ 0x1D1D.
 *	zclock replay seed steps	kernel.py replay: one line a tick, t state position orbit
 *					offset phase diff width density texture
 *	zclock sweep			how many of the 65,536 states are back after 8 ticks and after 16
 *	zclock spins n			for each prime q below n but 2 and 5: q, T = ord_q(10), and how many
 *					steps k of 1..T CyclicClock.v's recover_step gives back from encode_step
 *	zclock block q			R = (10^T - 1)/q, its T digits
 *	zclock -b ...			the falsifier: no B injected
 * The cyclic numbers are libmp integers; recover_step is the file's: (B mod q) times the inverse of
 * R mod q found by trying, k = 0 read as q, none when R mod q has no inverse.
 */
#include <u.h>
#include <libc.h>
#include <mp.h>

enum { Mask = 0xFFFF, C = 0x1D1D, T8 = 8, W36 = 36 };

static int B[T8] = { 0, 1, 3, 6, 9, 8, 6, 3 };
static int inject = 1;

static uint rotl(uint x, int n) { return ((x << n) | (x >> (16 - n))) & Mask; }
static uint rotr(uint x, int n) { return ((x >> n) | (x << (16 - n))) & Mask; }
static uint next(uint x) { return (rotl(x, 1) ^ rotl(x, 3) ^ rotr(x, 2) ^ C) & Mask; }
static uint tick(uint x, int t) { return next(inject ? x ^ B[t % T8] : x); }

static void
replay(uint x, int steps)
{
	int t, w, d, e, i;
	long pos;

	pos = 0;
	for(t = 0; t < steps; t++){
		for(w = 16; w > 0 && (x >> (w-1) & 1) == 0; w--)
			;
		for(d = 0, e = 0, i = 0; i < 16; i++){
			d += x >> i & 1;
			e += (x >> i & 1) != (x >> (i+1) % 16 & 1);
		}
		print("%d 0x%04X %ld %ld %ld %d %d %d %d %d\n", t, x, pos, pos / W36, pos % W36, t % T8, B[t % T8], w, d, e);
		pos += B[t % T8];
		x = tick(x, t);
	}
}

static void
sweep(void)
{
	uint x, s;
	int t, n8, n16;

	n8 = n16 = 0;
	for(x = 0; x <= Mask; x++){
		s = x;
		for(t = 0; t < 16; t++){
			s = tick(s, t);
			if(t == 7 && s == x)
				n8++;
		}
		if(s == x)
			n16++;
	}
	print("8 %d\n16 %d\n", n8, n16);
}

static int
prime(int n)
{
	int d;

	if(n < 2)
		return 0;
	for(d = 2; d*d <= n; d++)
		if(n % d == 0)
			return 0;
	return 1;
}

static int
order(int q)
{
	int k, v;

	for(k = 1, v = 10 % q; v != 1; k++)
		v = v * 10 % q;
	return k;
}

/* R = (10^T - 1)/q, and M = 10^T - 1 */
static mpint*
cyclic(int q, int T, mpint *M)
{
	mpint *ten, *R, *Q;
	int i;

	ten = uitomp(10, nil);
	uitomp(1, M);
	for(i = 0; i < T; i++)
		mpmul(M, ten, M);
	mpsub(M, mpone, M);
	R = mpnew(0);
	Q = uitomp(q, nil);
	mpdiv(M, Q, R, nil);
	mpfree(ten);
	mpfree(Q);
	return R;
}

static uint
mod(mpint *b, int q)
{
	mpint *Q, *r;
	uint v;

	Q = uitomp(q, nil);
	r = mpnew(0);
	mpdiv(b, Q, nil, r);
	v = mptoui(r);
	mpfree(Q);
	mpfree(r);
	return v;
}

static void
spins(int n)
{
	int q, T, k, inv, got, ok;
	uint rq;
	mpint *M, *R, *K, *Rk, *Bk;

	M = mpnew(0);
	K = mpnew(0);
	Rk = mpnew(0);
	Bk = mpnew(0);
	for(q = 3; q < n; q++){
		if(q == 5 || !prime(q))
			continue;
		T = order(q);
		R = cyclic(q, T, M);
		rq = mod(R, q);
		for(inv = 1; inv < q && rq * inv % q != 1; inv++)
			;
		ok = 0;
		for(k = 1; k <= T; k++){
			uitomp(k, K);
			mpmul(R, K, Rk);
			mpdiv(Rk, M, nil, Bk);	/* encode_step: R k mod (10^T - 1) */
			if(inv == q)
				continue;	/* recover_step: no inverse, None */
			got = mod(Bk, q) * inv % q;
			if((got == 0 ? q : got) == k)
				ok++;
		}
		print("%d %d %d\n", q, T, ok);
		mpfree(R);
	}
}

static void
block(int q)
{
	mpint *M, *R;
	char *s;
	int T, i;

	T = order(q);
	M = mpnew(0);
	R = cyclic(q, T, M);
	s = mptoa(R, 10, nil, 0);
	for(i = strlen(s); i < T; i++)
		print("0");
	print("%s\n", s);
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'b':
		inject = 0;
		break;
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zclock [-b] replay seed steps | sweep | spins n | block q");
	if(strcmp(argv[0], "replay") == 0 && argc == 3)
		replay(strtoul(argv[1], nil, 0) & Mask, atoi(argv[2]));
	else if(strcmp(argv[0], "sweep") == 0)
		sweep();
	else if(strcmp(argv[0], "spins") == 0 && argc == 2)
		spins(atoi(argv[1]));
	else if(strcmp(argv[0], "block") == 0 && argc == 2)
		block(atoi(argv[1]));
	else
		sysfatal("usage: zclock [-b] replay seed steps | sweep | spins n | block q");
	exits(nil);
}
