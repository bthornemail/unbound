/*
 * zgate - metaverse-build's authority gate (~/devops/metaverse-build
 * invariants/authority: AuthorityProjection.hs, validateAuthority, and
 * gate/AuthorityGate.hs) in Plan 9 C. The identity is $ID_PREFIX; the trace,
 * standard input. An empty prefix is InvalidSchemaPrefix (the module's
 * placeholder schema check: not null), "unknown" UnknownAuthority, "cross"
 * CrossDomainEscalation; any other prefix lets the trace through, every byte
 * as it came. On a violation: "HALT: violation" on standard error, zero bytes
 * on standard output, and the violation as the exit status: Haskell's
 * Either AuthorityViolation ValidatedTrace is Plan 9's exit string, nil for
 * Right. The whole trace is read before anything is written, so a halt emits
 * nothing.
 *	zgate		the gate
 *	zgate -e	the falsifier: the trace written as it is read, the check
 *			after, so a halt has already emitted it
 */
#include <u.h>
#include <libc.h>

static char*
violation(char *p)
{
	if(p == nil)
		return "ID_PREFIX: getEnv: does not exist (no environment variable)";
	if(*p == 0)
		return "InvalidSchemaPrefix";
	if(strcmp(p, "unknown") == 0)
		return "UnknownAuthority";
	if(strcmp(p, "cross") == 0)
		return "CrossDomainEscalation";
	return nil;
}

void
main(int argc, char *argv[])
{
	char *p, *v, *buf;
	long n, m, cap;
	int early;

	early = 0;
	ARGBEGIN{
	case 'e':
		early = 1;
		break;
	default:
		sysfatal("usage: zgate [-e]");
	}ARGEND
	p = getenv("ID_PREFIX");
	cap = 8192;
	buf = malloc(cap);
	n = 0;
	while((m = read(0, buf+n, cap-n)) > 0){
		if(early)
			write(1, buf+n, m);
		if((n += m) == cap)
			buf = realloc(buf, cap *= 2);
	}
	if((v = violation(p)) != nil){
		if(p == nil)
			fprint(2, "zgate: %s\n", v);
		else
			fprint(2, "HALT: %s\n", v);
		exits(v);
	}
	if(!early)
		write(1, buf, n);
	exits(nil);
}
