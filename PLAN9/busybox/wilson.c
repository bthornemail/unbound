/*
 * wilson - (n-1)! mod n = n-1 exactly when n is prime; the compare-exchange
 * on the factorial ladder selects prime vs composite (B-WILSON.sh).
 */
#include <u.h>
#include <libc.h>

static int
isprime(int n)
{
	int i;

	if(n < 2)
		return 0;
	for(i = 2; i * i <= n; i++)
		if(n % i == 0)
			return 0;
	return 1;
}

static int
wilson(int n)
{
	int f, k;

	for(f = 1, k = 1; k < n; k++)
		f = (f * k) % n;
	return f;
}

static void
fail(char *s)
{
	fprint(2, "FAIL(WILSON): %s\n", s);
	exits("fail");
}

void
main(void)
{
	int n, r, cas;

	/* assert: Wilson's law across the ladder n = 2..60, and the CAS selection matches primality */
	for(n = 2; n <= 60; n++){
		r = wilson(n);
		cas = r == n - 1;	/* compareExchange(expected = n-1) */
		if(isprime(n)){
			if(r != n - 1)
				fail(smprint("prime %d: (n-1)! mod n = %d, expected %d", n, r, n - 1));
			if(!cas)
				fail(smprint("CAS misclassified prime %d", n));
		}else if(n == 4){
			if(r != 2)
				fail(smprint("n=4 (the Catch): expected 2, got %d", r));
			if(cas)
				fail("n=4 should not read as prime");
		}else{
			if(r != 0)
				fail(smprint("composite %d: expected 0, got %d", n, r));
			if(cas)
				fail(smprint("CAS misclassified composite %d", n));
		}
	}
	print("WILSON: (n-1)! mod n = n-1 iff prime; CAS selects prime vs composite; n=4 the lone Catch - passed\n");
	exits(nil);
}
