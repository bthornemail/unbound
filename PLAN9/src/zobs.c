/*
 * zobs - omi-dom's shared/observer-sync.js in Plan 9 C.
 *	zobs test	the module's four selfTest assertions, as it makes them
 *	zobs read	the check it names but does not make: observers that read
 *	zobs unchecked	the same without the second epoch read: the falsifier,
 *			which must disagree, or the check proves nothing
 * The module registers observers (id, position), folds a knot's ruler by XOR
 * (knotDigest), and reports every observer consistent: invariants() computes
 * the digest once and copies it, with consistent true, into each observer's
 * entry, never reading anything per observer. So all_consistent holds whenever
 * one observer is registered.
 * zobs read lets the observers read for themselves: one writer proc changes
 * the ruler 2000 times, marking each change through an epoch by cas32 (odd
 * while it writes, even when done) and recording the digest of each even
 * epoch; four observer procs read the ruler and fold their own digest,
 * reading again when the epoch was odd or moved meanwhile. Every (epoch,
 * digest) an observer reports must be the writer's for that epoch.
 */
#include <u.h>
#include <libc.h>
#include <thread.h>

/* ---- the module, as it is ---- */
typedef struct Obs Obs;
struct Obs {
	char	*id;
	double	x, y, z;
};
static Obs obs[16];
static int nobs;

static void
regist(char *id, double x, double y, double z)
{
	int i;

	for(i = 0; i < nobs; i++)
		if(strcmp(obs[i].id, id) == 0)
			break;
	if(i == nobs)
		nobs++;
	obs[i].id = id;
	obs[i].x = x; obs[i].y = y; obs[i].z = z;
}

static ulong
knotdigest(int *ruler, int n)
{
	int i, d;

	for(d = 0, i = 0; i < n; i++)
		d ^= ruler[i];
	return d;
}

/* invariants(): one digest, copied to each observer, each consistent */
static int
invariants(int *ruler, int n, ulong *each, int *allConsistent)
{
	ulong d;
	int i;

	d = knotdigest(ruler, n);
	for(i = 0; i < nobs; i++)
		each[i] = d;		/* consistent: true, as the module writes it */
	*allConsistent = nobs > 0;
	return d;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

/* ---- observers that read ---- */
enum { NOBS = 4, NWRITE = 2000, LEN = 8 };
static int ruler[LEN];
static long epoch;			/* odd while the writer writes */
static ulong logged[2*NWRITE+2];	/* the writer's digest at each even epoch */
static long reports, wrong;
static int writing = 1;
static int recheck = 1;	/* zobs unchecked clears it */
static Channel *finished;

static void
writer(void *)
{
	int k, i;
	long e;

	for(k = 1; k <= NWRITE; k++){
		e = epoch;
		cas32((u32int*)&epoch, e, e+1);	/* odd: writing */
		for(i = 0; i < LEN; i++)
			ruler[i] = (k * 2654435761UL + i * 40503) & 0xffff;
		logged[e+2] = knotdigest(ruler, LEN);
		cas32((u32int*)&epoch, e+1, e+2);	/* even: done */
		if(k % 50 == 0)
			sleep(0);
	}
	writing = 0;
	sendul(finished, 0);
	threadexits(nil);
}

static void
observer(void *)
{
	long e1, e2;
	int copy[LEN], i;
	ulong d;

	while(writing){
		e1 = epoch;
		if(e1 & 1)
			continue;
		for(i = 0; i < LEN; i++)
			copy[i] = ruler[i];
		e2 = epoch;
		if(recheck && e1 != e2)
			continue;	/* the writer moved during the read: read again */
		d = knotdigest(copy, LEN);
		ainc(&reports);
		if(e1 > 0 && d != logged[e1])
			ainc(&wrong);
	}
	sendul(finished, 1);
	threadexits(nil);
}

static void
readers(int want)
{
	int i;

	logged[0] = knotdigest(ruler, LEN);
	finished = chancreate(sizeof(ulong), NOBS+1);
	for(i = 0; i < NOBS; i++)
		proccreate(observer, nil, 8192);
	proccreate(writer, nil, 8192);
	for(i = 0; i < NOBS+1; i++)
		recvul(finished);
	if(want)	/* the check: no read may disagree */
		print("%s %d observers made %ld reads of a ruler changed %d times: %ld disagreed with the writer\n",
			wrong == 0 && reports > 0 ? "ok" : "FAIL", NOBS, reports, NWRITE, wrong);
	else		/* the falsifier: without the second epoch read, some must */
		print("%s without the second epoch read, %ld of %ld reads disagreed with the writer\n",
			wrong > 0 ? "ok" : "FAIL (the check would prove nothing)", wrong, reports);
	fails += want ? (wrong != 0 || reports == 0) : wrong == 0;
}

void
threadmain(int argc, char *argv[])
{
	ulong each[16];
	int all, r[4] = { 1, 2, 3, 4 };
	ulong d;

	if(argc != 2)
		sysfatal("usage: zobs test | read");
	if(strcmp(argv[1], "test") == 0){
		nobs = 0;
		regist("A", 0, 0, 0);
		regist("B", 1, 2, 3);
		check("list 2", nobs == 2);
		d = invariants(r, 4, each, &all);
		check("all consistent", all);
		check("same digest both", each[0] == each[1]);
		check("digest xor", d == (1 ^ 2 ^ 3 ^ 4));
		/* the finding: consistent holds with no reads, for any positions */
		regist("C", -1e9, 7, 0.5);
		invariants(r, 4, each, &all);
		check("consistent for any position: the module never reads one", all && each[2] == each[0]);
	}else if(strcmp(argv[1], "read") == 0)
		readers(1);
	else if(strcmp(argv[1], "unchecked") == 0){
		recheck = 0;
		readers(0);
	}
	else
		sysfatal("usage: zobs test | read");
	threadexitsall(fails ? "fail" : nil);
}
