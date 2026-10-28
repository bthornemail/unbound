/*
 * step - the turn of the three named registers, Wirth's PL360 example
 * (PL360, 1968, as quoted by Wikipedia), with R0, R1, R2 and FLAG predeclared:
 *
 *	BEGIN INTEGER BUCKET;
 *	     IF FLAG THEN
 *	     BEGIN BUCKET := R0; R0 := R1; R1 := R2; R2 := BUCKET; END ELSE
 *	     BEGIN BUCKET := R2; R2 := R1; R1 := R0; R0 := BUCKET; END
 *	     RESET(FLAG);
 *	END
 *
 * The registers are the Aegean separators 10100, 10101, 10102 (_CODE/U), each
 * observing a zeroth: YHWH, YAHWEH, ADONAI. Moved here from zvtt.c (unbound
 * aa5ef66) unchanged, so zvtt.c and stepd.c share one turn.
 */
#include <u.h>
#include <libc.h>
#include "step.h"

Reg reg[3] = {
	{ "\xF0\x90\x84\x80", "YHWH" },	/* U+10100 */
	{ "\xF0\x90\x84\x81", "YAHWEH" },	/* U+10101 */
	{ "\xF0\x90\x84\x82", "ADONAI" },	/* U+10102 */
};
Reg *R0 = &reg[0], *R1 = &reg[1], *R2 = &reg[2];
int FLAG;

void
step(void)
{
	Reg *BUCKET;

	if(FLAG){
		BUCKET = R0; R0 = R1; R1 = R2;
		R2 = BUCKET;
	}else{
		BUCKET = R2; R2 = R1; R1 = R0;
		R0 = BUCKET;
	}
	FLAG = 0;	/* RESET(FLAG) */
}

/* the registers as they start: YHWH, YAHWEH, ADONAI */
void
stepreset(void)
{
	R0 = &reg[0];
	R1 = &reg[1];
	R2 = &reg[2];
	FLAG = 0;
}
