/*
 * manboy - Knuth's man or boy test (ALGOL Bulletin 17, 1964), in Plan 9 C.
 *
 *	manboy
 *
 * A procedure is a closure: its function and the frame it was made in.
 * B decrements the k of the A that made it, through that pointer, which is
 * the call by name the test checks. Prints A(k, 1, -1, -1, 1, 0) for k = 0
 * to 10, and fails unless it is 1 0 -2 0 1 0 1 -1 -10 -30 -67.
 */
#include <u.h>
#include <libc.h>

typedef struct Arg Arg;
struct Arg {
	int	(*fn)(Arg*);
	int	*k;
	Arg	*x1, *x2, *x3, *x4, *x5;
};

static int
eval(Arg *a)
{
	return a->fn(a);
}

static int A(Arg*);

static int
B(Arg *a)
{
	int k;
	Arg b;

	k = *a->k -= 1;
	b.fn = B;
	b.k = &k;
	b.x1 = a;
	b.x2 = a->x1;
	b.x3 = a->x2;
	b.x4 = a->x3;
	b.x5 = a->x4;
	return A(&b);
}

static int
A(Arg *a)
{
	return *a->k <= 0 ? eval(a->x4) + eval(a->x5) : B(a);
}

static int one(Arg*) { return 1; }
static int minusone(Arg*) { return -1; }
static int zero(Arg*) { return 0; }

void
main(void)
{
	static int want[] = { 1, 0, -2, 0, 1, 0, 1, -1, -10, -30, -67 };
	Arg a, f1, f_1, f0;
	int k, kk, r;

	f1.fn = one;
	f_1.fn = minusone;
	f0.fn = zero;
	for(k = 0; k < nelem(want); k++){
		kk = k;
		a.fn = B;	/* every argument A is given is a B closure, the first too */
		a.k = &kk;
		a.x1 = &f1;
		a.x2 = &f_1;
		a.x3 = &f_1;
		a.x4 = &f1;
		a.x5 = &f0;
		r = A(&a);
		print("%d %d\n", k, r);
		if(r != want[k])
			sysfatal("k = %d: %d, not %d", k, r, want[k]);
	}
	print("man\n");
	exits(nil);
}
