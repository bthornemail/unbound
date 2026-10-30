/*
 * zorder - the delta law's period at every width W = 2^n, proved and checked
 * in Plan 9 C (boxedPINE proofs I-246). L = x + x^3 + x^-2 over GF(2) mod
 * x^W - 1: squaring doubles L's three exponents and nothing else, so L^(2^k)
 * is known from three integers mod W. At 2^k = W/2 they are W/2, 3W/2 = W/2
 * and -W = 0: the pair cancels and L^(W/2) = 1; at W/4 three distinct non-zero
 * exponents remain, so the order is exactly W/2. With a constant C, the
 * W/2-th step adds (1 + L)^(W/2 - 1) C, and 1 + L = y^3 (1 + y + y^2) with
 * y = x + 1, x^W - 1 = y^W: zero exactly when 3(W/2 - 1) >= W, W >= 8.
 *	zorder cert	the certificate for n = 2 to 62, one line a width
 *	zorder run	the law itself at 8, 16, 32 and 64 bits, several constants
 *	zorder four	the falsifier: at W = 4, how many of 16 constants survive
 */
#include <u.h>
#include <libc.h>

/* the exponents of L^(2^k) mod 2^n that survive cancellation in pairs, in e; their count */
static int
exponents(int n, int k, uvlong *e)
{
	uvlong W, m, raw[3];
	int i, j, c, cnt;

	W = (uvlong)1 << n;
	m = (uvlong)1 << k;
	raw[0] = m & (W-1);
	raw[1] = (3*m) & (W-1);
	raw[2] = (W - ((2*m) & (W-1))) & (W-1);	/* -2m mod W */
	cnt = 0;
	for(i = 0; i < 3; i++){
		c = 0;
		for(j = 0; j < 3; j++)
			if(raw[j] == raw[i])
				c++;
		if(c & 1){
			for(j = 0; j < cnt; j++)
				if(e[j] == raw[i])
					break;
			if(j == cnt)
				e[cnt++] = raw[i];
		}
	}
	return cnt;
}

static uvlong
rotl(uvlong x, int s, int W)
{
	uvlong M;

	M = W == 64 ? ~(uvlong)0 : ((uvlong)1 << W) - 1;
	return ((x << s) | (x >> (W - s))) & M;
}

static uvlong
rotr(uvlong x, int s, int W)
{
	uvlong M;

	M = W == 64 ? ~(uvlong)0 : ((uvlong)1 << W) - 1;
	return ((x >> s) | (x << (W - s))) & M;
}

static uvlong
delta(uvlong x, uvlong c, int W)
{
	return rotl(x, 1, W) ^ rotl(x, 3, W) ^ rotr(x, 2, W) ^ c;
}

static uvlong
steps(uvlong x, uvlong c, int W, int k)
{
	while(k-- > 0)
		x = delta(x, c, W);
	return x;
}

void
main(int argc, char *argv[])
{
	uvlong e[3], M, x, c;
	uvlong cs[] = { 0, 0x1D, 0x5A3C, 0x1D1D1D1D1D1D1D1DULL, ~(uvlong)0 };
	uvlong xs[] = { 1, 0x2B, 0x9E3779B97F4A7C15ULL };
	int n, cnt, fail, W, i, j, surv;

	if(argc != 2)
		sysfatal("usage: zorder cert|run|four");
	fail = 0;
	if(strcmp(argv[1], "cert") == 0){
		for(n = 2; n <= 62; n++){
			cnt = exponents(n, n-1, e);
			if(cnt != 1 || e[0] != 0){
				print("FAIL W = 2^%d: L^(W/2) != 1\n", n);
				fail = 1;
			}
			cnt = exponents(n, n-2, e);
			if(cnt != 3 || e[0] == 0 || e[1] == 0 || e[2] == 0){
				print("FAIL W = 2^%d: L^(W/4) = 1\n", n);
				fail = 1;
			}
			/* S = 0 iff 3(W/2 - 1) >= W iff W >= 8 */
			if((n >= 3) != (3*(((uvlong)1 << (n-1)) - 1) >= ((uvlong)1 << n))){
				print("FAIL W = 2^%d: the constant's term\n", n);
				fail = 1;
			}
		}
		if(!fail)
			print("ok n = 2 to 62: L^(W/2) = 1, L^(W/4) != 1; the constant drops out iff W >= 8\n");
	}else if(strcmp(argv[1], "run") == 0){
		for(W = 8; W <= 64; W *= 2){
			M = W == 64 ? ~(uvlong)0 : ((uvlong)1 << W) - 1;
			for(i = 0; i < nelem(cs); i++)
				for(j = 0; j < nelem(xs); j++){
					x = xs[j] & M;
					c = cs[i] & M;
					if(steps(x, c, W, W/2) != x){
						print("FAIL W = %d C = %llux x = %llux not back at W/2\n", W, c, x);
						fail = 1;
					}
				}
			if(steps(1, 0, W, W/4) == 1){
				print("FAIL W = %d back at W/4\n", W);
				fail = 1;
			}
		}
		if(!fail)
			print("ok W = 8, 16, 32, 64: every word tried back at W/2 for every constant tried, none at W/4\n");
	}else if(strcmp(argv[1], "four") == 0){
		surv = 0;
		for(c = 0; c < 16; c++)
			if(steps(0, c, 4, 2) != 0)
				surv++;
		print("%s W = 4: %d of 16 constants survive the second step\n", surv > 0 ? "ok" : "FAIL", surv);
		fail = surv == 0;
	}else
		sysfatal("usage: zorder cert|run|four");
	exits(fail ? "fail" : nil);
}
