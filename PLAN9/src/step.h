/*
 * step.h - the turn of the three named registers (step.c), shared by zvtt.c
 * and stepd.c so the two cannot drift. Wirth's PL360 example with R0, R1, R2
 * and FLAG predeclared; here the registers are the Aegean separators observing
 * YHWH, YAHWEH and ADONAI.
 */
typedef struct Reg Reg;
struct Reg {
	char	*sep;	/* the Aegean separator */
	char	*zero;	/* the zeroth it observes */
};

extern Reg reg[3];
extern Reg *R0, *R1, *R2;
extern int FLAG;

void	step(void);
void	stepreset(void);
