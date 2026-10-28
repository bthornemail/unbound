/*
 * zbar - omi-dom's shared/worker-barrier.js in Plan 9 C: n workers each run a
 * function on a slot and post (index, result); the parent counts the posts and,
 * at the nth, XORs the partials into a digest. Worker threads become libthread
 * procs, postMessage a channel.
 *	zbar test	the module's two selfTest assertions, and its values
 *	zbar race	100 rounds of 8 procs that finish in any order: every
 *			round's digest must be the XOR of all eight results
 *	zbar falsify	the same, finishing at the first post instead of the nth:
 *			some round must disagree, or the race proves nothing
 * The partials are kept by index, not by arrival, so the order of finishing
 * cannot change them; XOR makes the digest independent of it too.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>

typedef struct Post Post;
struct Post {
	int	index;
	int	result;
};

typedef struct Work Work;
struct Work {
	int	index;
	int	slot;
	int	(*fn)(int);
	int	delay;		/* ms before posting: the race's order of finishing */
	Channel	*c;
};

static void
worker(void *a)
{
	Work *w;
	Post p;

	w = a;
	if(w->delay)
		sleep(w->delay);
	p.index = w->index;
	p.result = w->fn(w->slot);
	send(w->c, &p);
	threadexits(nil);
}

/* barrier(n, slots, fn): the digest once `need` posts have arrived (n, as the module) */
static ulong
barrier(int n, int *slots, int nslots, int (*fn)(int), int *partials, int need, int *delays)
{
	Channel *c;
	Work *w;
	Post p;
	ulong digest;
	int i, completed;

	c = chancreate(sizeof(Post), n);
	w = mallocz(n * sizeof *w, 1);
	for(i = 0; i < n; i++){
		partials[i] = 0;
		w[i].index = i;
		w[i].slot = slots[i % nslots];
		w[i].fn = fn;
		w[i].delay = delays ? delays[i] : 0;
		w[i].c = c;
		proccreate(worker, &w[i], 8192);
	}
	digest = 0;
	for(completed = 0; completed < n; ){
		recv(c, &p);
		partials[p.index] = p.result;
		if(++completed == need){
			for(digest = 0, i = 0; i < n; i++)
				digest ^= partials[i];
			if(need < n)	/* the falsifier: the rest still post; drain them */
				while(completed++ < n)
					recv(c, &p);
			break;
		}
	}
	chanfree(c);
	free(w);
	return digest;
}

static int times7(int s) { return s * 7; }
static int odd(int s) { return s * 2 + 1; }

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

enum { NR = 8, ROUNDS = 100 };

static int
race(int need)
{
	int slots[NR], partials[NR], delays[NR], r, i, bad;
	ulong d, ref;

	srand(1);
	for(i = 0; i < NR; i++)
		slots[i] = 1 << i;	/* each result a different bit: a missing one shows */
	for(ref = 0, i = 0; i < NR; i++)
		ref ^= odd(slots[i]);
	for(bad = 0, r = 0; r < ROUNDS; r++){
		for(i = 0; i < NR; i++)
			delays[i] = nrand(4);
		d = barrier(NR, slots, NR, odd, partials, need, delays);
		bad += d != ref;
	}
	return bad;
}

void
threadmain(int argc, char *argv[])
{
	int slots[4] = { 0, 1, 2, 3 }, partials[4], bad;
	ulong d, ref;
	int i;

	if(argc != 2)
		sysfatal("usage: zbar test | race | falsify");
	if(strcmp(argv[1], "test") == 0){
		d = barrier(4, slots, 4, times7, partials, 4, nil);
		for(ref = 0, i = 0; i < 4; i++)
			ref ^= slots[i] * 7;
		check("partials length 4", nelem(partials) == 4);
		check(smprint("digest match %lud vs %lud", d, ref), d == ref);
		print("values: partials %d %d %d %d, digest %lud\n", partials[0], partials[1], partials[2], partials[3], d);
	}else if(strcmp(argv[1], "race") == 0){
		bad = race(NR);
		print("%s %d rounds of %d procs finishing in any order: the digest of all eight each time (%d rounds otherwise)\n",
			bad ? "FAIL" : "ok", ROUNDS, NR, bad);
		fails += bad != 0;
	}else if(strcmp(argv[1], "falsify") == 0){
		bad = race(1);
		print("%s finishing at the first post, %d of %d rounds disagreed\n",
			bad ? "ok" : "FAIL (the race would prove nothing)", bad, ROUNDS);
		fails += bad == 0;
	}else
		sysfatal("usage: zbar test | race | falsify");
	threadexitsall(fails ? "fail" : nil);
}
