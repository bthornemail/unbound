/*
 * steptest - apply a sequence of IS and OF to the shared step() (step.c, as
 * zvtt.c ticks) and print the registers' zeroths, R0 R1 R2: what stepd must
 * show after the same writes.
 *	steptest IS OF IS ...
 */
#include <u.h>
#include <libc.h>
#include "step.h"

void
main(int argc, char *argv[])
{
	int i;

	stepreset();
	for(i = 1; i < argc; i++){
		if(strcmp(argv[i], "IS") == 0)
			FLAG = 1;
		else if(strcmp(argv[i], "OF") == 0)
			FLAG = 0;
		else
			sysfatal("IS or OF, not %s", argv[i]);
		step();
	}
	print("%s %s %s\n", R0->zero, R1->zero, R2->zero);
	exits(nil);
}
