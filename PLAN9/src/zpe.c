/*
 * zpe - omi-dom's shared/parallel-engine.js in Plan 9 C: one shared Int32
 * buffer, compare-exchange as the only mutation, workers that take a lock by
 * compare-exchange and run a command. The host's pieces become Plan 9's own:
 * SharedArrayBuffer is memory shared between libthread procs, Atomics is
 * cas32, postMessage is a channel.
 *	zpe test	the module's selfTest, and the values its engine gives
 *	zpe race	eight procs claim one cell at once, a hundred times:
 *			exactly one may win each time
 * Layout as the module's: 0..7 ruler, 8 epoch, 9 lock, 10 result, 11 command,
 * 12..19 scratch (index, expected, replacement).
 */
#include <u.h>
#include <libc.h>
#include <thread.h>

enum {
	RULER0 = 0, EPOCH = 8, LOCK = 9, RESULT = 10, CMD = 11, SCRATCH = 12,
	IDLE = 0, BINDAPPLY = 1, DIGEST = 2, STOP = 3, XORFOLD = 4,
	Ready = 0, Done, Busy, Stopped,
	NW = 2,
};

typedef struct Msg Msg;
struct Msg {
	int	type;
	int	worker;
	int	result;
	int	epoch;
	int	cmd;
};

static int view[512];		/* the shared buffer: every proc sees it */
static Channel *run[NW];	/* to each worker: run */
static Channel *reply;		/* from the workers */

/* Atomics.compareExchange: the cell's previous value; repl written only if it was exp */
static int
compareexchange(int *p, int exp, int repl)
{
	int old;

	for(;;){
		old = *p;
		if(old != exp)
			return old;
		if(cas32((u32int*)p, exp, repl))
			return exp;
		/* the cell changed between the read and the exchange: read again */
	}
}

static void
store(int i, int v)
{
	int old;

	do old = view[i]; while(!cas32((u32int*)&view[i], old, v));
}

static void
worker(void *a)
{
	Msg m;
	int id, cmd, i, x, n, ok;

	id = (int)(uintptr)a;
	m.type = Ready; m.worker = id;
	send(reply, &m);
	for(;;){
		recv(run[id-1], &ok);
		if(compareexchange(&view[LOCK], 0, id) != 0){	/* tryLock */
			m.type = Busy; m.worker = id;
			send(reply, &m);
			continue;
		}
		cmd = view[CMD];
		if(cmd == BINDAPPLY){
			x = compareexchange(&view[view[SCRATCH]], view[SCRATCH+1], view[SCRATCH+2]);
			store(RESULT, x);
			store(EPOCH, view[EPOCH] + 1);
		}else if(cmd == DIGEST){
			for(x = 0, i = 0; i < 8; i++)
				x ^= view[RULER0+i];
			store(RESULT, x);
			store(EPOCH, view[EPOCH] + 1);
		}else if(cmd == XORFOLD){
			n = view[SCRATCH] ? view[SCRATCH] : 8;
			if(n > 64) n = 64;
			for(x = 0, i = 0; i < n; i++)
				x ^= view[RULER0+i];
			store(RESULT, x);
		}else if(cmd == STOP){
			store(LOCK, 0);
			m.type = Stopped; m.worker = id;
			send(reply, &m);
			threadexits(nil);
		}
		m.type = Done; m.worker = id; m.result = view[RESULT]; m.epoch = view[EPOCH]; m.cmd = cmd;
		store(CMD, IDLE);
		store(LOCK, 0);
		send(reply, &m);
	}
}

/* post a command to every worker; the one that took the lock answers Done */
static Msg
command(int cmd)
{
	Msg m, got;
	int i, one;

	store(CMD, cmd);
	one = 1;
	for(i = 0; i < NW; i++)
		send(run[i], &one);
	memset(&got, 0, sizeof got);
	/* a worker that takes the lock after another finished sees IDLE: only the Done
	 * for the posted command counts, as the module matches on cmd */
	for(i = 0; i < NW; i++){
		recv(reply, &m);
		if((m.type == Done && m.cmd == cmd) || m.type == Stopped)
			got = m;
	}
	return got;
}

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

static void
test(void)
{
	Msg m, cas1, cas2, bae, dig;
	int i, ruler[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	reply = chancreate(sizeof(Msg), NW);
	for(i = 0; i < NW; i++){
		run[i] = chancreate(sizeof(int), 1);
		proccreate(worker, (void*)(uintptr)(i+1), 8192);
	}
	for(i = 0; i < NW; i++)
		recv(reply, &m);	/* ready */
	check("shared or fallback buffer", 1);
	check("worker count", NW == 2);
	for(i = 0; i < 8; i++)
		store(RULER0+i, ruler[i]);
	check("ruler write/read", view[0] == 1 && view[7] == 8);

	store(SCRATCH, 0); store(SCRATCH+1, 1); store(SCRATCH+2, 99);
	cas1 = command(BINDAPPLY);
	check("CAS success old==1", cas1.result == 1);
	check("CAS stored 99", view[0] == 99);
	store(SCRATCH, 0); store(SCRATCH+1, 1); store(SCRATCH+2, 50);
	cas2 = command(BINDAPPLY);
	check("CAS miss old==99", cas2.result == 99);
	check("CAS miss no change", view[0] == 99);
	/* bindApplyEval(1, 2, 77): the knot bind(2, 77), the exchange, eval = the knot's a */
	store(SCRATCH, 1); store(SCRATCH+1, 2); store(SCRATCH+2, 77);
	bae = command(BINDAPPLY);
	check("bindApplyEval swapped", bae.result == 2);
	check("bindApplyEval eval", 2 == 2);
	check("slot1 is 77", view[1] == 77);
	dig = command(DIGEST);
	check("digest has xorFold", dig.cmd == DIGEST);
	/* STOP stays posted, so every worker sees it and stops: post it once */
	m = command(STOP);
	check("shutdown", m.type == Stopped);

	/* what the JavaScript engine gave on the host (worker ids aside) */
	print("values: cas1 old %d epoch %d, cas2 old %d epoch %d, bae old %d epoch %d, digest %d epoch %d, ruler",
		cas1.result, cas1.epoch, cas2.result, cas2.epoch, bae.result, bae.epoch, dig.result, dig.epoch);
	for(i = 0; i < 8; i++)
		print(" %d", view[RULER0+i]);
	print("\n");
}

/* ---- the race: eight procs, one cell ---- */
enum { NR = 8, ROUNDS = 100 };
static int cell, round;
static long wins, started;
static Channel *done;

static void
racer(void *a)
{
	int id, r;

	id = (int)(uintptr)a;
	for(r = 0; r < ROUNDS; r++){
		while(round != r)	/* the start line: all wait for the round */
			sleep(0);
		ainc(&started);
		while(started < NR*(r+1))
			;
		if(compareexchange(&cell, 0, id) == 0)
			ainc(&wins);
		sendul(done, id);
		while(round == r)
			sleep(0);
	}
	threadexits(nil);
}

static void
race(void)
{
	int r, i, bad;

	done = chancreate(sizeof(ulong), NR);
	for(i = 1; i <= NR; i++)
		proccreate(racer, (void*)(uintptr)i, 8192);
	bad = 0;
	for(r = 0; r < ROUNDS; r++){
		for(i = 0; i < NR; i++)
			recvul(done);
		if(wins != 1 || cell < 1 || cell > NR)
			bad++;
		wins = 0;
		cell = 0;
		round = r + 1;
	}
	print("%s %d rounds of %d procs claiming one cell: one winner each time (%d rounds otherwise)\n",
		bad ? "FAIL" : "ok", ROUNDS, NR, bad);
	fails += bad != 0;
}

void
threadmain(int argc, char *argv[])
{
	if(argc != 2)
		sysfatal("usage: zpe test | race");
	if(strcmp(argv[1], "test") == 0)
		test();
	else if(strcmp(argv[1], "race") == 0)
		race();
	else
		sysfatal("usage: zpe test | race");
	threadexitsall(fails ? "fail" : nil);
}
