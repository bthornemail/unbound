/*
 * agentd - the running floor's agents over HTTP, one request per connection,
 * for aux/listen1 (stdin and stdout are the connection):
 *	aux/listen1 -t tcp!*!8000 agentd
 * It finds the floor among /proc's processes and reads it with mach(2), as
 * observe.c does. Three ways in, as the Observer has:
 *	GET  /agents			read (the default): the tick and each agent's
 *					tag, point, hue and state, as JSON
 *	GET  /constrain?TAG&spec	ask without writing: the K orbit's state that
 *					meets exact/ideal/min/max (observe.c's rule)
 *	POST /interject?TAG&state	write: stop the floor, write the state, start it
 * Only POST writes; a GET never changes the floor. Every answer allows any
 * origin, for boxedPINE's space/guest.html.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>

enum { NCH = 8, SZ = 16 };	/* floor.c's Ch: int point; char *tag; int hue; ushort state */

static Map *map;
static int pid;
static Biobuf out;

static void
reply(char *status, char *type, char *body)
{
	Bprint(&out, "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %ld\r\n", status, type, strlen(body));
	Bprint(&out, "Access-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n%s", body);
	Bterm(&out);
	exits(nil);
}

static void
fail(char *status, char *why)
{
	reply(status, "text/plain", smprint("%s\n", why));
}

/* the floor's pid: the process named floor in /proc */
static int
findfloor(void)
{
	Dir *d;
	int fd, n, i, f;
	char buf[32], *p;

	if((fd = open("/proc", OREAD)) < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	for(i = 0; i < n; i++){
		p = smprint("/proc/%s/status", d[i].name);
		f = open(p, OREAD);
		free(p);
		if(f < 0)
			continue;
		memset(buf, 0, sizeof buf);
		read(f, buf, sizeof buf - 1);
		close(f);
		if(strncmp(buf, "floor ", 6) == 0)
			return atoi(d[i].name);
	}
	return -1;
}

static void
ctl(char *msg)
{
	int fd;
	char *p;

	p = smprint("/proc/%d/ctl", pid);
	if((fd = open(p, OWRITE)) < 0 || write(fd, msg, strlen(msg)) < 0)
		fail("500 Internal Server Error", "cannot stop or start the floor");
	close(fd);
}

static uvlong
addr(char *name)
{
	Symbol s;

	if(lookup(nil, name, &s) < 0)
		fail("500 Internal Server Error", "the floor has no such symbol");
	return s.value;
}

static ulong
word(uvlong a)
{
	ulong v;

	if(get4(map, a, &v) < 0)
		fail("500 Internal Server Error", "cannot read the floor");
	return v;
}

static char*
str(uvlong a)
{
	static char buf[NCH][16];
	static int k;
	char *b;
	uchar c;
	int i;

	b = buf[k++ % NCH];
	for(i = 0; i < 15; i++){
		if(get1(map, a + i, &c, 1) < 0)
			break;
		if((b[i] = c) == 0)
			break;
	}
	b[i] = 0;
	return b;
}

static ushort
rotl(ushort x, int n)
{
	return x<<n | x>>(16-n);
}

/* floor.c and space/src/gnomon.ts K */
static ushort
K(ushort p)
{
	return rotl(p, 1) ^ rotl(p, 3) ^ rotl(p, 14) ^ 0x1D;
}

static int
flips(ushort x)
{
	int n;

	for(n = 0; x; x &= x - 1)
		n++;
	return n;
}

/* the agent's row in ch, by tag */
static uvlong
row(char *tag)
{
	uvlong ch;
	int i;

	ch = addr("ch");
	for(i = 0; i < NCH; i++)
		if(strcmp(str(word(ch + i*SZ + 4)), tag) == 0)
			return ch + i*SZ;
	fail("404 Not Found", "no such agent");
	return 0;
}

static void
agents(void)
{
	Fmt f;
	uvlong ch, e;
	ushort st;
	int i;

	ch = addr("ch");
	fmtstrinit(&f);
	fmtprint(&f, "{\"tick\":%lud,\"agents\":[", word(addr("tick")));
	for(i = 0; i < NCH; i++){
		e = ch + i*SZ;
		if(get2(map, e + 12, &st) < 0)
			fail("500 Internal Server Error", "cannot read a state");
		fmtprint(&f, "%s{\"tag\":\"%s\",\"point\":%ld,\"hue\":%ld,\"state\":\"%.4uX\"}",
			i ? "," : "", str(word(e + 4)), (long)word(e), (long)word(e + 8), st);
	}
	fmtprint(&f, "]}\n");
	reply("200 OK", "application/json", fmtstrflush(&f));
}

/* observe.c's constraints: exact, min, max mandatory over the K orbit; ideal the nearest */
static void
constrain(char *tag, char *spec)
{
	long c[4];
	char *f[4], *kv[2], *names[] = { "exact", "ideal", "min", "max" };
	ushort orbit[8], s, now;
	int i, j, n, best, bestd, d;

	if(get2(map, row(tag) + 12, &now) < 0)
		fail("500 Internal Server Error", "cannot read the state");
	for(j = 0; j < 4; j++)
		c[j] = -1;
	n = getfields(spec, f, nelem(f), 1, ",");
	for(i = 0; i < n; i++){
		if(getfields(f[i], kv, 2, 1, "=") != 2)
			fail("400 Bad Request", "a constraint is name=hex");
		for(j = 0; j < 4; j++)
			if(strcmp(kv[0], names[j]) == 0)
				c[j] = strtoul(kv[1], nil, 16);
	}
	for(s = now, i = 0; i < 8; i++, s = K(s))
		orbit[i] = s;
	best = -1;
	bestd = 17;
	for(i = 0; i < 8; i++){
		s = orbit[i];
		if((c[0] >= 0 && s != c[0]) || (c[2] >= 0 && s < c[2]) || (c[3] >= 0 && s > c[3]))
			continue;
		d = c[1] >= 0 ? flips(s ^ c[1]) : 0;
		if(d < bestd){
			best = i;
			bestd = d;
		}
	}
	if(best < 0)
		reply("200 OK", "application/json", smprint("{\"tag\":\"%s\",\"settings\":\"%.4uX\",\"OverconstrainedError\":true}\n", tag, now));
	reply("200 OK", "application/json", smprint("{\"tag\":\"%s\",\"settings\":\"%.4uX\",\"chosen\":\"%.4uX\",\"ticks\":%d,\"bitsFromIdeal\":%d}\n",
		tag, now, orbit[best], best, bestd));
}

static void
interject(char *tag, char *state)
{
	uvlong e;
	ushort st;

	e = row(tag);
	st = strtoul(state, nil, 16);
	ctl("stop");
	if(put2(map, e + 12, st) < 0){
		ctl("start");
		fail("500 Internal Server Error", "cannot write the state");
	}
	ctl("start");
	reply("200 OK", "application/json", smprint("{\"tag\":\"%s\",\"interjected\":\"%.4uX\"}\n", tag, st));
}

void
main(void)
{
	Biobuf in;
	Fhdr fh;
	char *l, *f[3], *path, *q, *a[2], *p;
	int fd, mem, n;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	if((l = Brdstr(&in, '\n', 1)) == nil)
		exits("no request");
	n = tokenize(l, f, nelem(f));
	while((p = Brdstr(&in, '\n', 1)) != nil && p[0] != 0 && p[0] != '\r')
		free(p);
	if(n < 2)
		fail("400 Bad Request", "no request line");
	path = f[1];
	q = strchr(path, '?');
	if(q != nil)
		*q++ = 0;

	if((pid = findfloor()) < 0)
		fail("503 Service Unavailable", "no floor is running");
	p = smprint("/proc/%d/text", pid);
	if((fd = open(p, OREAD)) < 0 || crackhdr(fd, &fh) == 0)
		fail("500 Internal Server Error", "cannot read the floor's text");
	machbytype(fh.type);
	if(syminit(fd, &fh) < 0)
		fail("500 Internal Server Error", "no symbols");
	p = smprint("/proc/%d/mem", pid);
	if((mem = open(p, ORDWR)) < 0 || (map = attachproc(pid, 0, mem, &fh)) == nil)
		fail("500 Internal Server Error", "cannot attach to the floor");

	if(strcmp(f[0], "GET") == 0 && strcmp(path, "/agents") == 0)
		agents();
	if(strcmp(f[0], "GET") == 0 && strcmp(path, "/constrain") == 0 && q != nil && getfields(q, a, 2, 1, "&") == 2)
		constrain(a[0], a[1]);
	if(strcmp(f[0], "POST") == 0 && strcmp(path, "/interject") == 0 && q != nil && getfields(q, a, 2, 1, "&") == 2)
		interject(a[0], a[1]);
	if(strcmp(path, "/interject") == 0)
		fail("405 Method Not Allowed", "an interjection writes: POST only");
	fail("404 Not Found", "GET /agents, GET /constrain?TAG&spec, POST /interject?TAG&state");
}
