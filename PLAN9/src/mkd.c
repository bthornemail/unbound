/*
 * mkd - the Metaverse Kit world that mkfs serves, over HTTP, one request per
 * connection, for aux/listen1 (stdin and stdout are the connection):
 *	aux/listen1 -t tcp!*!8001 mkd
 * It reads /n/mk, mounting /srv/mk there if it is not yet.
 *	GET  /tiles			each tile, SPACE Z/X/Y, one a line
 *	GET  /events?SPACE&Z/X/Y	the tile's events, one a line
 *	GET  /state?SPACE&Z/X/Y		the tile's state, mkfs's fold, one node a line
 *	POST /events?SPACE&Z/X/Y	append the body's lines: taken whole when
 *					the grammar accepts every line, else 400
 *					and nothing appended
 * Only POST writes. Every answer allows any origin, for boxedPINE's space/.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

static Biobuf in, out;

static void
reply(char *status, char *body, long n)
{
	Bprint(&out, "HTTP/1.0 %s\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %ld\r\n", status, n);
	Bprint(&out, "Access-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
	Bwrite(&out, body, n);
	Bterm(&out);
	exits(nil);
}

static void
fail(char *status, char *why)
{
	char *s;

	s = smprint("%s\n", why);
	reply(status, s, strlen(s));
}

static char*
readall(char *file, long *n)
{
	int fd, m;
	long cap;
	char *s;

	if((fd = open(file, OREAD)) < 0)
		return nil;
	cap = 8192;
	s = malloc(cap);
	*n = 0;
	while((m = read(fd, s + *n, cap - *n)) > 0)
		if((*n += m) == cap)
			s = realloc(s, cap *= 2);
	close(fd);
	return s;
}

/* SPACE&Z/X/Y as the tile's directory, or nil */
static char*
tiledir(char *q)
{
	char *f[2];

	if(q == nil || getfields(q, f, 2, 0, "&") != 2 || strstr(f[0], "..") || strstr(f[1], "..") || strchr(f[0], '/'))
		return nil;
	return smprint("/n/mk/spaces/%s/tiles/%s", f[0], f[1]);
}

static void
tiles(void)
{
	Dir *s, *z, *x, *y;
	int ns, nz, nx, ny, a, b, c, d, fd;
	char *body, *p;

	body = strdup("");
	if((fd = open("/n/mk/spaces", OREAD)) < 0)
		fail("500 Internal Server Error", "no world at /n/mk");
	ns = dirreadall(fd, &s);
	close(fd);
	for(a = 0; a < ns; a++){
		p = smprint("/n/mk/spaces/%s/tiles", s[a].name);
		fd = open(p, OREAD); nz = fd < 0 ? 0 : dirreadall(fd, &z); close(fd);
		for(b = 0; b < nz; b++){
			p = smprint("/n/mk/spaces/%s/tiles/%s", s[a].name, z[b].name);
			fd = open(p, OREAD); nx = fd < 0 ? 0 : dirreadall(fd, &x); close(fd);
			for(c = 0; c < nx; c++){
				p = smprint("/n/mk/spaces/%s/tiles/%s/%s", s[a].name, z[b].name, x[c].name);
				fd = open(p, OREAD); ny = fd < 0 ? 0 : dirreadall(fd, &y); close(fd);
				for(d = 0; d < ny; d++)
					body = smprint("%s%s %s/%s/%s\n", body, s[a].name, z[b].name, x[c].name, y[d].name);
			}
		}
	}
	reply("200 OK", body, strlen(body));
}

void
main(void)
{
	char *l, *f[3], *path, *q, *d, *s, *body;
	long n, len;
	int fd;

	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	if(access("/n/mk/spaces", AEXIST) < 0){
		if((fd = open("/srv/mk", ORDWR)) < 0 || mount(fd, -1, "/n/mk", MREPL, "") < 0)
			fail("500 Internal Server Error", "no world: start mkfs");
	}
	if((l = Brdstr(&in, '\n', 1)) == nil || tokenize(l, f, 3) < 2)
		fail("400 Bad Request", "no request");
	len = 0;
	while((s = Brdstr(&in, '\n', 1)) != nil && s[0] != '\r' && s[0] != 0){
		if(cistrncmp(s, "Content-Length:", 15) == 0)
			len = atol(s + 15);
		free(s);
	}
	path = f[1];
	if((q = strchr(path, '?')) != nil)
		*q++ = 0;
	if(strcmp(path, "/tiles") == 0)
		tiles();
	if(strcmp(path, "/events") != 0 && strcmp(path, "/state") != 0)
		fail("404 Not Found", "GET /tiles, /events?SPACE&Z/X/Y or /state?SPACE&Z/X/Y; POST /events?SPACE&Z/X/Y");
	if((d = tiledir(q)) == nil || access(d, AEXIST) < 0)
		fail("404 Not Found", "no such tile: ?SPACE&Z/X/Y");
	if(strcmp(f[0], "POST") == 0){
		if(strcmp(path, "/events") != 0)
			fail("405 Method Not Allowed", "the state is the fold of the events: POST to /events");
		body = malloc(len + 1);
		if(len < 0 || Bread(&in, body, len) != len)
			fail("400 Bad Request", "the body is shorter than its Content-Length");
		if((fd = open(smprint("%s/events", d), OWRITE)) < 0)
			fail("500 Internal Server Error", "cannot open the events");
		if(write(fd, body, len) != len){
			fail("400 Bad Request", smprint("%r"));
		}
		close(fd);
		fail("200 OK", smprint("appended %ld bytes", len));
	}
	if(strcmp(f[0], "GET") != 0)
		fail("405 Method Not Allowed", "GET or POST");
	if((s = readall(smprint("%s%s", d, path), &n)) == nil)
		fail("500 Internal Server Error", "cannot read the tile");
	reply("200 OK", s, n);
}
