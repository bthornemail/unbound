/*
 * proof.h - the frame for the I-series checks, ported from boxedPINE
 * proofs/js/catalog to Plan 9 C. Each record is one function; each JS
 * assertion is one call, in the same order, with the same message.
 */
#include <u.h>
#include <libc.h>

typedef struct Record Record;
struct Record {
	char	*id;
	char	*claim;
	void	(*run)(void);
};

/* an assertion: holds or fails, by the JS check's own message */
void	ok(int cond, char *msg);
/* an assertion about the JavaScript host itself (its error types, BigInt
 * mixing): reported, not ported, never replaced */
void	host(char *msg);

extern Record records[];

/* Atomics.compareExchange on 32-bit cells, through Plan 9's cas32: returns
 * the cell's previous value, and writes repl only if it was exp */
u32int	compareexchange(u32int *p, u32int exp, u32int repl);
