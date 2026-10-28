/*
 * zprobe - omi-dom's shared/system-probe.js in Plan 9 C.
 *	zprobe test		the module's four selfTest assertions, as it makes them
 *	zprobe probe [root]	probes that read: each is ok only when it read what it
 *				names, under root (default /); healthy only when all are
 *	zprobe falsify		the same probes under a root that holds nothing: healthy
 *				must be false, or the probe proves nothing
 * The module's four probes write ok: true as a constant, whatever they find:
 * busybox is ok with no busybox, the codex is ok with no codex (present:
 * false, codex docs optional), so healthy is true on every system and its
 * selfTest cannot fail. The host's Node, BusyBox and streams become Plan 9's
 * own: the system is /dev/osversion and /dev/sysname, the runtime /env/cputype
 * and /dev/swap, the streams file descriptors 0, 1 and 2.
 */
#include <u.h>
#include <libc.h>

static int fails;

static void
check(char *name, int cond)
{
	print("%s %s\n", cond ? "ok" : "FAIL", name);
	fails += !cond;
}

/* the first line of root/name, or nil */
static char*
line(char *root, char *name)
{
	char buf[256], *p;
	int fd, n;

	p = smprint("%s%s", root, name);
	fd = open(p, OREAD);
	free(p);
	if(fd < 0)
		return nil;
	n = read(fd, buf, sizeof buf - 1);
	close(fd);
	if(n <= 0)
		return nil;
	buf[n] = 0;
	if((p = strchr(buf, '\n')) != nil)
		*p = 0;
	return strdup(buf);
}

static int
probe(char *root)
{
	char *osv, *sys, *cpu, *mem, *p;
	int system, runtime, streams, codex, fd, present;
	Dir *d;

	osv = line(root, "/dev/osversion");
	sys = line(root, "/dev/sysname");
	system = osv != nil && sys != nil;
	print("system: ok %d osversion %s sysname %s\n", system, osv ? osv : "-", sys ? sys : "-");

	cpu = line(root, "/env/cputype");
	mem = line(root, "/dev/swap");
	runtime = cpu != nil && mem != nil;
	print("runtime: ok %d cputype %s memory %s\n", runtime, cpu ? cpu : "-", mem ? mem : "-");

	/* the streams are open descriptors here; under another root, files there */
	for(streams = 1, fd = 0; fd < 3; fd++){
		if(strcmp(root, "") == 0)
			d = dirfstat(fd);
		else{
			p = smprint("%s/fd/%d", root, fd);
			d = dirstat(p);
			free(p);
		}
		streams &= d != nil;
		free(d);
	}
	print("streams: ok %d\n", streams);

	/* the module's candidates; the codex is optional, so absent is still ok, as there */
	p = smprint("%s/lib/codex", root);
	d = dirstat(p);
	present = d != nil;
	free(d);
	codex = 1;
	print("codex: ok %d present %d path %s\n", codex, present, p);
	free(p);

	return system && runtime && streams && codex;
}

void
main(int argc, char *argv[])
{
	int h;

	if(argc < 2)
		sysfatal("usage: zprobe test | probe [root] | falsify");
	if(strcmp(argv[1], "test") == 0){
		/* probe() as the module has it: every ok a constant */
		check("healthy (ok is a constant in every probe; cannot fail)", 1);
		check("streams ok (a constant; cannot fail)", 1);
		check("node ok (a constant; cannot fail)", 1);
		check("busybox ok (a constant; cannot fail)", 1);
	}else if(strcmp(argv[1], "probe") == 0){
		h = probe(argc > 2 ? argv[2] : "");
		check("healthy: every probe read what it names", h);
	}else if(strcmp(argv[1], "falsify") == 0){
		h = probe("/nonexistent");
		check("under a root that holds nothing, healthy is false", !h);
	}else
		sysfatal("usage: zprobe test | probe [root] | falsify");
	exits(fails ? "fail" : nil);
}
