/*
 * zkit - Metaverse Kit's runtime CLIs that carry logic (~/devops/metaverse-kit tools/mv-init,
 * tools/mv-replay) in Plan 9 C. mv-host (runs the kit's Node server; Plan 9 serves the world with
 * mkworld.rc -s), mv-client (opens a browser) and mv-ext32 (posts to a server endpoint the Plan 9
 * world does not have) carry none to port.
 *	zkit init [--space S] [--world W]	mv-init: W/spaces/S/tiles/z0/x0/y0 with index.json and
 *					manifest.json, and W's .ulp-root, .ulp-scope, .ulp-ignore; a
 *					file already there is kept
 *	zkit replay [--world W] [--space S] [--tile T] [--after E] [--out F]
 *					mv-replay, literal (the author's choice): a tile's events are one
 *					log, W/spaces/S/tiles/T/events, every event in order, as the Plan 9
 *					world keeps them (mkfs), no segment named by a hash; --after E, the
 *					events after E; --out F, written there and counted
 *	zkit -n ...			the falsifier: init writes over what is there, replay keeps E
 * An event's id is its JSON "event_id" (as the kit's tilestore keeps it) or, in the grammar's
 * line form (wsn/mkevent.wsn, as mkfs keeps it), the word after event_id.
 */
#include <u.h>
#include <libc.h>

static int naive;

static char*
slurp(char *file)
{
	int fd;
	Dir *d;
	char *s;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		return nil;
	s = mallocz(d->length + 1, 1);
	readn(fd, s, d->length);
	close(fd);
	free(d);
	return s;
}

static void
spit(char *file, char *s)
{
	int fd;

	if((fd = create(file, OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", file);
	write(fd, s, strlen(s));
	close(fd);
}

static void
mkdirp(char *d)
{
	char *s, *q;
	int c;

	s = strdup(d);
	for(q = s; ; q++){
		if((*q == '/' || *q == 0) && q > s){
			c = *q;
			*q = 0;
			if(access(s, AEXIST) < 0)
				close(create(s, OREAD, DMDIR|0777));
			*q = c;
		}
		if(*q == 0)
			break;
	}
	free(s);
}

static char*
absolute(char *p)
{
	char wd[512];

	if(p[0] == '/')
		return p;
	if(getwd(wd, sizeof wd) == nil)
		sysfatal("getwd: %r");
	while(p[0] == '.' && p[1] == '/')
		p += 2;
	return smprint("%s/%s", wd, p);
}

/* the value after a flag, as the kit's parseArgs takes it: the last one given, if it has a value */
static char*
flag(int argc, char **argv, char *name, char *def)
{
	int i;

	for(i = 0; i < argc; i++)
		if(strcmp(argv[i], name) == 0 && i + 1 < argc && argv[i+1][0])
			def = argv[++i];
	return def;
}

static void
ensure(char *file, char *content)
{
	char *d;

	d = strdup(file);
	*strrchr(d, '/') = 0;
	mkdirp(d);
	if(naive || access(file, AEXIST) < 0)
		spit(file, content);
}

static void
init(int argc, char **argv)
{
	char *space, *world, *tile;

	space = flag(argc, argv, "--space", "demo");
	world = absolute(flag(argc, argv, "--world", "world"));
	tile = smprint("%s/spaces/%s/tiles/z0/x0/y0", world, space);
	mkdirp(tile);
	ensure(smprint("%s/index.json", tile), "{}\n");
	ensure(smprint("%s/manifest.json", tile), "{\n  \"tile_id\": \"z0/x0/y0\",\n  \"segments\": []\n}\n");
	ensure(smprint("%s/.ulp-root", world), "invariants:\n  - adjacency\n  - exclusion\n  - consistency\n  - boundary_discipline\n  - authority_nontransfer\nscope:\n  realm: team\n  authority: source\n  boundary: interior\n");
	ensure(smprint("%s/.ulp-scope", world), "scope:\n  realm: team\n  authority: source\n  boundary: interior\n");
	ensure(smprint("%s/.ulp-ignore", world), "node_modules\n.DS_Store\n");
	print("Initialized world at %s\nSpace: %s\n", world, space);
	exits(nil);
}

/* an event's id: its JSON "event_id", or the word after event_id in the line form */
static char*
eventid(char *line)
{
	char *p, *e;

	if((p = strstr(line, "\"event_id\":\"")) != nil){
		p += 12;
		for(e = p; *e && *e != '"'; e++)
			if(*e == '\\' && e[1])
				e++;
		return smprint("%.*s", (int)(e - p), p);
	}
	if(strncmp(line, "event_id ", 9) == 0){
		p = line + 9;
		e = strchr(p, ' ');
		return e ? smprint("%.*s", (int)(e - p), p) : strdup(p);
	}
	return "";
}

static void
replay(int argc, char **argv)
{
	char *world, *space, *tile, *out, *after, *log, *l, *nl, *line;
	int found, n, fd;

	world = flag(argc, argv, "--world", "world");
	space = flag(argc, argv, "--space", "demo");
	tile = flag(argc, argv, "--tile", "z0/x0/y0");
	out = flag(argc, argv, "--out", "");
	after = flag(argc, argv, "--after", "");
	if((log = slurp(smprint("%s/spaces/%s/tiles/%s/events", absolute(world), space, tile))) == nil){
		fprint(2, "%r\n");
		exits("1");
	}
	fd = -1;
	if(*out && (fd = create(out, OWRITE, 0666)) < 0)
		sysfatal("create %s: %r", out);
	found = *after == 0;
	n = 0;
	for(l = log; *l; l = nl){
		nl = strchr(l, '\n');
		nl = nl ? nl + 1 : l + strlen(l);
		line = smprint("%.*s", (int)(nl - l - (nl[-1] == '\n')), l);
		if(*line == 0)
			continue;
		if(!found){
			if(strcmp(eventid(line), after) == 0){
				found = 1;
				if(!naive)
					continue;
			}else
				continue;
		}
		fprint(fd >= 0 ? fd : 1, "%s\n", line);
		n++;
	}
	if(fd >= 0){
		close(fd);
		print("Wrote %d events to %s\n", n, out);
	}
	exits(nil);
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'n':
		naive = 1;
		break;
	}ARGEND
	if(argc >= 1 && strcmp(argv[0], "init") == 0)
		init(argc - 1, argv + 1);
	if(argc >= 1 && strcmp(argv[0], "replay") == 0)
		replay(argc - 1, argv + 1);
	sysfatal("usage: zkit [-n] init [--space S] [--world W] | replay [--world W] [--space S] [--tile T] [--after E] [--out F]");
}
