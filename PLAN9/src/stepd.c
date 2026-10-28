/*
 * stepd - the turn of the three named registers (step.c) as a 9P file
 * server, the same step() zvtt.c ticks with.
 *	stepd [-m mtpt]		(default /n/step)
 *	R0 R1 R2	read: the zeroth the register holds and its Aegean
 *			separator (YHWH 𐄀, YAHWEH 𐄁, ADONAI 𐄂 at the start);
 *			a write is refused
 *	FLAG		write IS: FLAG set, one turn forward (R0 := R1 := R2 :=
 *			R0), then RESET(FLAG); write OF: FLAG clear, one turn
 *			back; anything else is refused and nothing turns.
 *			read: the direction the last write took, IS or OF, or
 *			nothing before the first
 * One write, one turn: the direction is spent by the step it makes.
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "step.h"

enum { Qr0, Qr1, Qr2, Qflag };

static char last[4];	/* "IS", "OF" or "" */

static void
fsread(Req *r)
{
	char buf[64];
	Reg *g;

	switch((uintptr)r->fid->file->aux){
	case Qr0: g = R0; break;
	case Qr1: g = R1; break;
	case Qr2: g = R2; break;
	case Qflag:
		snprint(buf, sizeof buf, "%s\n", last);
		readstr(r, buf);
		respond(r, nil);
		return;
	default:
		respond(r, "no such register");
		return;
	}
	snprint(buf, sizeof buf, "%s %s\n", g->zero, g->sep);
	readstr(r, buf);
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	char buf[16];
	int n;

	if((uintptr)r->fid->file->aux != Qflag){
		respond(r, "a register is read-only: write IS or OF to FLAG");
		return;
	}
	n = r->ifcall.count;
	if(n >= sizeof buf)
		n = sizeof buf - 1;
	memmove(buf, r->ifcall.data, n);
	while(n > 0 && (buf[n-1] == '\n' || buf[n-1] == ' '))
		n--;
	buf[n] = 0;
	if(strcmp(buf, "IS") == 0)
		FLAG = 1;
	else if(strcmp(buf, "OF") == 0)
		FLAG = 0;
	else{
		respond(r, "FLAG takes IS or OF");
		return;
	}
	strcpy(last, buf);
	step();		/* one turn, then RESET(FLAG) */
	r->ofcall.count = r->ifcall.count;
	respond(r, nil);
}

Srv fs = {
	.read = fsread,
	.write = fswrite,
};

void
main(int argc, char *argv[])
{
	char *mtpt;

	mtpt = "/n/step";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("-m mtpt"));
		break;
	}ARGEND
	stepreset();
	fs.tree = alloctree(nil, nil, DMDIR|0555, nil);
	createfile(fs.tree->root, "R0", nil, 0444, (void*)Qr0);
	createfile(fs.tree->root, "R1", nil, 0444, (void*)Qr1);
	createfile(fs.tree->root, "R2", nil, 0444, (void*)Qr2);
	createfile(fs.tree->root, "FLAG", nil, 0666, (void*)Qflag);
	postmountsrv(&fs, nil, mtpt, MREPL);
	exits(nil);
}
