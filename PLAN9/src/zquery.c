/*
 * zquery - the PL360 example (Wirth, 1968, as quoted by Wikipedia) as the
 * query over one face of the tetrahedron: three files in R0, R1, R2.
 *
 *	BEGIN INTEGER BUCKET;
 *	     IF FLAG THEN
 *	     BEGIN BUCKET := R0; R0 := R1; R1 := R2; R2 := BUCKET; END ELSE
 *	     BEGIN BUCKET := R2; R2 := R1; R1 := R0; R0 := BUCKET; END
 *	     RESET(FLAG);
 *	END
 *
 *	zquery <three lines
 *
 * The registers hold pointers to the files, so a step moves files and copies
 * no byte. Checks the step and the face's parity, failing on the first that
 * does not hold.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static char *R0, *R1, *R2;
static int FLAG;

static void
step(void)
{
	char *BUCKET;

	if(FLAG){
		BUCKET = R0; R0 = R1; R1 = R2;
		R2 = BUCKET;
	}else{
		BUCKET = R2; R2 = R1; R1 = R0;
		R0 = BUCKET;
	}
	FLAG = 0;	/* RESET(FLAG) */
}

static void
fail(char *s)
{
	fprint(2, "zquery: FAIL %s\n", s);
	exits("fail");
}

/* the face closes: R0 ^ R1, R1 ^ R2 and R2 ^ R0 XOR to zero, byte by byte */
static int
closes(void)
{
	int n, i, a, b, c;

	n = strlen(R0);
	if(strlen(R1) > n) n = strlen(R1);
	if(strlen(R2) > n) n = strlen(R2);
	for(i = 0; i < n; i++){
		a = i < strlen(R0) ? R0[i] : 0;
		b = i < strlen(R1) ? R1[i] : 0;
		c = i < strlen(R2) ? R2[i] : 0;
		if(((a ^ b) ^ (b ^ c) ^ (c ^ a)) != 0)
			return 0;
	}
	return 1;
}

static void
show(char *what)
{
	print("%-28s R0 \"%s\"  R1 \"%s\"  R2 \"%s\"  FLAG %d\n", what, R0, R1, R2, FLAG);
}

void
main(void)
{
	Biobuf in;
	char *a, *b, *c;
	int k;

	Binit(&in, 0, OREAD);
	if((a = Brdstr(&in, '\n', 1)) == nil || (b = Brdstr(&in, '\n', 1)) == nil || (c = Brdstr(&in, '\n', 1)) == nil)
		sysfatal("need three lines");
	R0 = a; R1 = b; R2 = c;
	show("start");

	FLAG = 1; step(); show("FLAG set, one step");
	if(R0 != b || R1 != c || R2 != a) fail("FLAG set does not turn R0 <- R1 <- R2 <- R0");
	if(FLAG != 0) fail("FLAG is not reset");
	if(!closes()) fail("the face does not close after a turn");

	step(); show("FLAG clear, one step");
	if(R0 != a || R1 != b || R2 != c) fail("set then clear does not return to the start");

	for(k = 0; k < 3; k++){ FLAG = 0; step(); }
	show("FLAG clear, three steps");
	if(R0 != a || R1 != b || R2 != c) fail("three clear steps do not return to the start");
	if(FLAG != 0) fail("FLAG is not reset");
	if(!closes()) fail("the face does not close");

	print("zquery: the PL360 step turns the face one way with FLAG and back without; FLAG always ends reset; the face's parity closes in every position\n");
	exits(nil);
}
