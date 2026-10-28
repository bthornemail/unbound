/*
 * stack - the stack element of wsn/stack.wsn as a 9P file server, the live
 * form of cast.rc's grammar. It holds at most one character.
 *	stack [-m mtpt]		(default /n/stack)
 *	cast	write a zeroth (YHWH, YAHWEH, ...): the characters it accepts are
 *		its feature pipeline's, as zcast.c writes them (consonants upper,
 *		vowels lowered 0x20, digits kept); read: the zeroth and those
 *		characters. Before a cast is written nothing is accepted: WSN
 *		leaves letter undefined
 *		(https://en.wikipedia.org/wiki/Wirth_syntax_notation).
 *	in	write one character: taken in when the element is empty and the
 *		cast accepts it; refused otherwise ("full", "not in the cast")
 *	out	read: the held character, unchanged, and the element is empty
 *		again; refused when empty
 *	state	read: "empty" or "full" and the held character
 * The element never changes a character: the 0x20 is the cast's, before a
 * character comes in.
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum { Qcast, Qin, Qout, Qstate };

static char zeroth[64];
static char accepted[64];	/* the cast's characters, as zcast.c orders them */
static int held = -1;		/* -1: empty */

/* terms.mjs classify, one character: its class's form, or -1 */
static int
classify(int c)
{
	if(c >= '0' && c <= '9')
		return c;
	if(c >= 'a' && c <= 'z')
		c -= 0x20;
	if(c < 'A' || c > 'Z')
		return -1;
	if(strchr("AEIOU", c) != nil)
		return c + 0x20;
	return c;
}

static void
cast(char *z)
{
	char *s;
	int c, n;

	snprint(zeroth, sizeof zeroth, "%s", z);
	n = 0;
	for(s = zeroth; *s && n < sizeof accepted - 1; s++)
		if((c = classify(*s)) >= 0 && strchr(accepted, c) == nil)
			accepted[n++] = c, accepted[n] = 0;
	accepted[n] = 0;
}

static void
fsread(Req *r)
{
	char buf[128];

	switch((uintptr)r->fid->file->aux){
	case Qcast:
		snprint(buf, sizeof buf, "%s %s\n", zeroth[0] ? zeroth : "none", accepted);
		readstr(r, buf);
		break;
	case Qstate:
		if(held < 0)
			snprint(buf, sizeof buf, "empty\n");
		else
			snprint(buf, sizeof buf, "full %c\n", held);
		readstr(r, buf);
		break;
	case Qout:
		if(r->ifcall.offset > 0){
			r->ofcall.count = 0;
			break;
		}
		if(held < 0){
			respond(r, "empty");
			return;
		}
		buf[0] = held;
		held = -1;
		r->ofcall.count = r->ifcall.count < 1 ? 0 : 1;
		memmove(r->ofcall.data, buf, r->ofcall.count);
		break;
	default:
		respond(r, "not readable");
		return;
	}
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	char buf[64];
	int n, c;

	n = r->ifcall.count;
	if(n >= sizeof buf)
		n = sizeof buf - 1;
	memmove(buf, r->ifcall.data, n);
	while(n > 0 && buf[n-1] == '\n')
		n--;
	buf[n] = 0;
	switch((uintptr)r->fid->file->aux){
	case Qcast:
		accepted[0] = 0;
		cast(buf);
		break;
	case Qin:
		if(n != 1){
			respond(r, "one character");
			return;
		}
		c = buf[0];
		if(held >= 0){
			respond(r, "full");
			return;
		}
		if(strchr(accepted, c) == nil){
			respond(r, "not in the cast");
			return;
		}
		held = c;
		break;
	default:
		respond(r, "not writable");
		return;
	}
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

	mtpt = "/n/stack";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("-m mtpt"));
		break;
	}ARGEND
	fs.tree = alloctree(nil, nil, DMDIR|0555, nil);
	createfile(fs.tree->root, "cast", nil, 0666, (void*)Qcast);
	createfile(fs.tree->root, "in", nil, 0222, (void*)Qin);
	createfile(fs.tree->root, "out", nil, 0444, (void*)Qout);
	createfile(fs.tree->root, "state", nil, 0444, (void*)Qstate);
	postmountsrv(&fs, nil, mtpt, MREPL);
	exits(nil);
}
