/*
 * proof - run every ported record; print one line per record and a line
 * for each assertion that fails or is a JavaScript host assertion.
 *
 *	proof [id ...]
 */
#include "proof.h"

static char *cur;
static int nok, nfail, nhost, tfail;

void
ok(int cond, char *msg)
{
	if(cond)
		nok++;
	else{
		nfail++;
		print("%s FAIL: %s\n", cur, msg);
	}
}

void
host(char *msg)
{
	nhost++;
	print("%s host, not ported: %s\n", cur, msg);
}

static int
wanted(char *id, int argc, char **argv)
{
	int i;

	if(argc == 0)
		return 1;
	for(i = 0; i < argc; i++)
		if(strcmp(argv[i], id) == 0)
			return 1;
	return 0;
}

void
main(int argc, char *argv[])
{
	Record *r;
	int n;

	setfcr(getfcr() & ~(FPZDIV|FPINVAL|FPOVFL));	/* IEEE results, as JS has them */
	n = 0;
	for(r = records; r->id != nil; r++){
		if(!wanted(r->id, argc-1, argv+1))
			continue;
		cur = r->id;
		nok = nfail = nhost = 0;
		r->run();
		print("%s %s: %d hold, %d fail, %d host - %s\n", r->id, nfail ? "FAILS" : "holds", nok, nfail, nhost, r->claim);
		tfail += nfail;
		n++;
	}
	print("proof: %d records, %d failing assertions\n", n, tfail);
	exits(tfail ? "fail" : nil);
}

u32int
compareexchange(u32int *p, u32int exp, u32int repl)
{
	u32int old;

	old = *p;
	if(old == exp && cas32(p, exp, repl))
		return exp;
	return old;
}
