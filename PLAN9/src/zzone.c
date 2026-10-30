/*
 * zzone - metaverse-build's zones (~/devops/metaverse-build runtime/zones) in
 * Plan 9 C, one subcommand for each of its six tools, each as the tool does it:
 *	zzone authority policy zone actor	authority-check.py: OK, or HALT: ZoneNotAuthorized (3)
 *	zzone route snapshot event		route-event.py: the zone of the event's target entity
 *	zzone interest zone [radius]		interest.py: the tiles around tile-x-y within the radius
 *	zzone materialize snapshot outdir	zone-materialize.py: one snapshot a zone, and the index
 *	zzone migrate src entity zone transfer [srcout]	migrate-entity.py: an entity out as a transfer
 *	zzone apply dest transfer out		apply-migration.py: a transfer into a zone
 *	zzone -n authority ...			the falsifier: the wildcard "*" not read
 * Files are written as canonical JSON, keys sorted, no spaces. migrate and apply print
 * what they wrote, where the tools print its sha256: the literal, no hash standing for it.
 * materialize carries the tools' source_snapshot_hash, the sha256 of the canonical source,
 * as the files carry it: data in the files (libsec sha2_256), not an identity anything is
 * compared by. A malformed input writes its reason and exits "2". As in zworld, strings
 * are compared and written as their raw literals and numbers as their digits.
 */
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>

enum { Null, True, False, Num, Str, Arr, Obj };

typedef struct J J;
struct J {
	int	t;
	char	*s;	/* Num: digits; Str: the literal with its quotes */
	J	**v;
	char	**k;	/* Obj: keys, each a literal with its quotes */
	int	n;
};

static int nostar;
static char *p;

static J*
mk(int t)
{
	J *j;

	j = mallocz(sizeof *j, 1);
	j->t = t;
	return j;
}

static void
ws(void)
{
	while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
}

static char*
lit(void)
{
	char *b;

	b = p++;
	while(*p && *p != '"'){
		if(*p == '\\' && p[1])
			p++;
		p++;
	}
	p++;
	return smprint("%.*s", (int)(p - b), b);
}

static void
add(J *j, char *k, J *v)
{
	j->v = realloc(j->v, (j->n+1) * sizeof(J*));
	j->k = realloc(j->k, (j->n+1) * sizeof(char*));
	j->k[j->n] = k;
	j->v[j->n++] = v;
}

static J*
val(void)
{
	J *j;
	char *b, *k;

	ws();
	switch(*p){
	case '{':
		j = mk(Obj);
		p++; ws();
		if(*p == '}'){ p++; return j; }
		for(;;){
			ws(); k = lit(); ws(); p++;
			add(j, k, val());
			ws();
			if(*p++ == '}')
				return j;
		}
	case '[':
		j = mk(Arr);
		p++; ws();
		if(*p == ']'){ p++; return j; }
		for(;;){
			add(j, nil, val());
			ws();
			if(*p++ == ']')
				return j;
		}
	case '"':
		j = mk(Str);
		j->s = lit();
		return j;
	case 't': p += 4; return mk(True);
	case 'f': p += 5; return mk(False);
	case 'n': p += 4; return mk(Null);
	}
	j = mk(Num);
	b = p;
	while(*p && strchr("-+.0123456789eE", *p))
		p++;
	j->s = smprint("%.*s", (int)(p - b), b);
	return j;
}

static J*
get(J *o, char *k)
{
	int i, n;

	if(o == nil || o->t != Obj)
		return nil;
	n = strlen(k);
	for(i = 0; i < o->n; i++)
		if(strlen(o->k[i]) == n+2 && memcmp(o->k[i]+1, k, n) == 0)
			return o->v[i];
	return nil;
}

static void
put(J *o, char *k, J *v)
{
	int i, n;

	n = strlen(k);
	for(i = 0; i < o->n; i++)
		if(strlen(o->k[i]) == n+2 && memcmp(o->k[i]+1, k, n) == 0){
			o->v[i] = v;
			return;
		}
	add(o, smprint("\"%s\"", k), v);
}

static int isstr(J *j) { return j != nil && j->t == Str; }
static int filled(J *j) { return isstr(j) && strlen(j->s) > 2; }
static char *bare(J *j) { return smprint("%.*s", (int)strlen(j->s)-2, j->s+1); }
static J *orn(J *j) { return j ? j : mk(Null); }
static char *keyname(char *k) { return smprint("%.*s", (int)strlen(k)-2, k+1); }

static J*
str(char *s)
{
	J *j;

	j = mk(Str);
	j->s = smprint("\"%s\"", s);
	return j;
}

/* Python's str() of a value, as the tools' sort keys take it */
static char*
pystr(J *j)
{
	if(j == nil || j->t == Null) return "None";
	if(j->t == True) return "True";
	if(j->t == False) return "False";
	if(j->t == Str) return bare(j);
	return j->s ? j->s : "?";
}

/* Python's == on two JSON values */
static int
eq(J *a, J *b)
{
	int i;

	if(a == nil || b == nil)
		return a == b;
	if(a->t != b->t || a->n != b->n)
		return 0;
	switch(a->t){
	case Num: case Str: return strcmp(a->s, b->s) == 0;
	case Arr:
		for(i = 0; i < a->n; i++)
			if(!eq(a->v[i], b->v[i]))
				return 0;
		return 1;
	case Obj:
		for(i = 0; i < a->n; i++)
			if(!eq(a->v[i], get(b, keyname(a->k[i]))))
				return 0;
		return 1;
	}
	return 1;
}

static int
keycmp(void *a, void *b)
{
	char *x, *y;
	int m, n, c;

	x = *(char**)a; y = *(char**)b;
	m = strlen(x) - 2; n = strlen(y) - 2;
	c = memcmp(x+1, y+1, m < n ? m : n);
	return c ? c : m - n;
}

/* the canonical JSON of j, appended to the growing buffer */
typedef struct Buf Buf;
struct Buf { char *s; long n, cap; };

static void
bput(Buf *b, char *s)
{
	long m;

	m = strlen(s);
	if(b->n + m + 1 > b->cap){
		b->cap = (b->n + m + 1) * 2;
		b->s = realloc(b->s, b->cap);
	}
	memmove(b->s + b->n, s, m+1);
	b->n += m;
}

static void
canon(Buf *b, J *j)
{
	char **ks;
	int i, h;

	switch(j->t){
	case Null: bput(b, "null"); return;
	case True: bput(b, "true"); return;
	case False: bput(b, "false"); return;
	case Num: case Str: bput(b, j->s); return;
	case Arr:
		bput(b, "[");
		for(i = 0; i < j->n; i++){
			if(i) bput(b, ",");
			canon(b, j->v[i]);
		}
		bput(b, "]");
		return;
	}
	ks = malloc((j->n+1) * sizeof(char*));
	memmove(ks, j->k, j->n * sizeof(char*));
	qsort(ks, j->n, sizeof(char*), keycmp);
	bput(b, "{");
	for(i = 0; i < j->n; i++){
		if(i) bput(b, ",");
		for(h = 0; j->k[h] != ks[i]; h++)
			;
		bput(b, ks[i]);
		bput(b, ":");
		canon(b, j->v[h]);
	}
	bput(b, "}");
	free(ks);
}

static char*
canonical(J *j)
{
	Buf b;

	memset(&b, 0, sizeof b);
	bput(&b, "");
	canon(&b, j);
	return b.s;
}

static void
fail(char *why)
{
	fprint(2, "%s\n", why);
	exits("2");
}

static char*
slurp(char *file)
{
	int fd;
	Dir *d;
	char *s;

	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", file);
	s = mallocz(d->length + 1, 1);
	if(readn(fd, s, d->length) != d->length)
		sysfatal("read %s: %r", file);
	close(fd);
	free(d);
	return s;
}

static J*
load(char *file)
{
	p = slurp(file);
	return val();
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

/* the state's entities, as the tools take them: state.get("entities", []) */
static J*
entities(J *snap, char *what)
{
	J *e;

	e = get(get(snap, "state"), "entities");
	if(e == nil)
		e = mk(Arr);
	if(e->t != Arr)
		fail(smprint("invalid %s: entities not list", what));
	return e;
}

/* stable, by Python's str() of each entity's id */
static void
sortbyid(J *a)
{
	int i, j;
	J *x;

	for(i = 1; i < a->n; i++){
		x = a->v[i];
		for(j = i; j > 0 && strcmp(pystr(get(a->v[j-1], "id")), pystr(get(x, "id"))) > 0; j--)
			a->v[j] = a->v[j-1];
		a->v[j] = x;
	}
}

static void
authority(int argc, char **argv)
{
	J *pol, *l;
	char *keys[2];
	int i, k;

	if(argc != 3)
		fail("usage: authority-check.py <policy.json> <zone> <actor>");
	pol = load(argv[0]);
	keys[0] = argv[1];
	keys[1] = nostar ? nil : "*";
	for(k = 0; k < 2; k++){
		if(keys[k] == nil || (l = get(pol, keys[k])) == nil || l->t != Arr)
			continue;
		for(i = 0; i < l->n; i++)
			if(isstr(l->v[i]) && strcmp(bare(l->v[i]), argv[2]) == 0){
				print("OK\n");
				exits(nil);
			}
	}
	fprint(2, "HALT: ZoneNotAuthorized\n");
	exits("3");
}

static void
route(int argc, char **argv)
{
	J *snap, *ev, *ents, *t, *id, *z;
	int i;

	if(argc != 2)
		fail("usage: route-event.py <snapshot.json> <event.json>");
	snap = load(argv[0]);
	ev = load(argv[1]);
	ents = entities(snap, "snapshot");
	t = get(ev, "type");
	if(isstr(t) && strcmp(bare(t), "ENTITY_CREATE") == 0)
		id = get(ev, "id");
	else{
		id = get(ev, "entity");	/* entity or id: a falsy entity gives way */
		if(id == nil || id->t == Null || id->t == False || (isstr(id) && strlen(id->s) == 2) || (id->t == Num && atof(id->s) == 0))
			id = get(ev, "id");
	}
	if(!isstr(id))
		fail("event missing target entity id");
	z = nil;
	for(i = 0; i < ents->n; i++)
		if(ents->v[i]->t == Obj && eq(get(ents->v[i], "id"), id)){
			z = get(ents->v[i], "zone");
			break;
		}
	print("%s\n", filled(z) ? bare(z) : "zone-unknown");
	exits(nil);
}

/* a decimal integer as Python's int() takes digits: an optional minus, then digits */
static int
integer(char *s, char **end, vlong *v)
{
	char *b;
	int neg;

	neg = *s == '-';
	if(neg)
		s++;
	b = s;
	*v = 0;
	while(*s >= '0' && *s <= '9')
		*v = *v * 10 + (*s++ - '0');
	if(s == b)
		return 0;
	if(neg)
		*v = -*v;
	*end = s;
	return 1;
}

static void
interest(int argc, char **argv)
{
	char *s;
	vlong x, y, r, dx, dy;

	if(argc < 1)
		fail("usage: interest.py <zone> [radius]");
	r = 1;
	if(argc > 1 && !integer(argv[1], &s, &r))
		sysfatal("radius");
	s = argv[0];
	if(strncmp(s, "tile-", 5) != 0 || !integer(s+5, &s, &x) || *s != '-' || !integer(s+1, &s, &y) || *s != 0){
		print("%s\n", argv[0]);
		exits(nil);
	}
	for(dx = -r; dx <= r; dx++)
		for(dy = -r; dy <= r; dy++)
			print("tile-%lld-%lld\n", x+dx, y+dy);
	exits(nil);
}

static void
materialize(int argc, char **argv)
{
	J *snap, *ents, *e, *z, *zones, *index, *zs, *g, *zi, *st;
	char *src, hex[2*SHA2_256dlen+1], *path, *name;
	uchar dig[SHA2_256dlen];
	int i, j;

	if(argc != 2)
		fail("usage: zone-materialize.py <snapshot.json> <out_dir>");
	snap = load(argv[0]);
	ents = entities(snap, "snapshot");
	zones = mk(Obj);	/* zone -> its entities, in the order zones first appear */
	for(i = 0; i < ents->n; i++){
		e = ents->v[i];
		if(e->t != Obj)
			continue;
		z = get(e, "zone");
		name = filled(z) ? bare(z) : "zone-unknown";
		if((g = get(zones, name)) == nil){
			g = mk(Arr);
			put(zones, name, g);
		}
		add(g, nil, e);
	}
	for(i = 0; i < zones->n; i++)
		sortbyid(zones->v[i]);
	src = canonical(snap);
	sha2_256((uchar*)src, strlen(src), dig, nil);
	for(i = 0; i < SHA2_256dlen; i++)
		sprint(hex+2*i, "%.2x", dig[i]);
	if(access(argv[1], AEXIST) < 0)
		close(create(argv[1], OREAD, DMDIR|0777));
	index = mk(Obj);
	put(index, "source_snapshot", str(argv[0]));
	put(index, "source_hash", str(hex));
	zi = mk(Obj);
	put(index, "zones", zi);
	for(i = 0; i < zones->n; i++){
		name = keyname(zones->k[i]);
		path = smprint("%s/%s.snapshot.json", argv[1], name);
		zs = mk(Obj);
		put(zs, "world", orn(get(snap, "world")));
		put(zs, "zone", str(name));
		st = mk(Obj);
		put(st, "entities", zones->v[i]);
		put(zs, "state", st);
		put(zs, "source_snapshot_hash", str(hex));
		spit(path, canonical(zs));
		g = mk(Obj);
		put(g, "snapshot", str(path));
		j = zones->v[i]->n;
		put(g, "entity_count", (e = mk(Num), e->s = smprint("%d", j), e));
		put(zi, name, g);
	}
	path = smprint("%s/zone-index.json", argv[1]);
	spit(path, canonical(index));
	print("%s\n", path);
	exits(nil);
}

static void
migrate(int argc, char **argv)
{
	J *src, *ents, *found, *rest, *zone, *t, *ns, *st;
	char *s;
	int i;

	if(argc < 4)
		fail("usage: migrate-entity.py <source_zone_snapshot> <entity_id> <dest_zone> <out_transfer> [out_source_snapshot]");
	src = load(argv[0]);
	ents = entities(src, "source snapshot");
	zone = get(src, "zone");
	if(!filled(zone))
		fail("invalid source snapshot: missing zone");
	found = nil;
	rest = mk(Arr);
	for(i = 0; i < ents->n; i++)
		if(ents->v[i]->t == Obj && isstr(get(ents->v[i], "id")) && strcmp(bare(get(ents->v[i], "id")), argv[1]) == 0)
			found = ents->v[i];	/* the last match, as the tool keeps it */
		else
			add(rest, nil, ents->v[i]);
	if(found == nil || found->n == 0)
		fail("entity not found in source zone");
	t = mk(Obj);
	put(t, "entity", found);
	put(t, "from", zone);
	put(t, "to", str(argv[2]));
	put(t, "source_snapshot_hash", orn(get(src, "source_snapshot_hash")));
	s = canonical(t);
	spit(argv[3], s);
	if(argc > 4){
		ns = mk(Obj);
		put(ns, "world", orn(get(src, "world")));
		put(ns, "zone", zone);
		st = mk(Obj);
		put(st, "entities", rest);
		put(ns, "state", st);
		put(ns, "source_snapshot_hash", orn(get(src, "source_snapshot_hash")));
		spit(argv[4], canonical(ns));
	}
	print("%s", s);
	exits(nil);
}

static void
apply(int argc, char **argv)
{
	J *dest, *t, *ent, *to, *ents, *copy, *nd, *st;
	char *s;
	int i;

	if(argc < 3)
		fail("usage: apply-migration.py <dest_zone_snapshot> <transfer.json> <out_dest_snapshot>");
	dest = load(argv[0]);
	t = load(argv[1]);
	ent = get(t, "entity");
	if(ent == nil || ent->t != Obj)
		fail("invalid transfer: missing entity");
	to = get(t, "to");
	if(!filled(to))
		fail("invalid transfer: missing dest zone");
	ents = entities(dest, "destination snapshot");
	for(i = 0; i < ents->n; i++)
		if(ents->v[i]->t == Obj && eq(orn(get(ents->v[i], "id")), orn(get(ent, "id"))))
			fail("entity already exists in destination");
	copy = mk(Obj);
	for(i = 0; i < ent->n; i++)
		add(copy, ent->k[i], ent->v[i]);
	put(copy, "zone", to);
	add(ents, nil, copy);
	sortbyid(ents);
	nd = mk(Obj);
	put(nd, "world", orn(get(dest, "world")));
	put(nd, "zone", orn(get(dest, "zone")));
	st = mk(Obj);
	put(st, "entities", ents);
	put(nd, "state", st);
	put(nd, "source_snapshot_hash", orn(get(dest, "source_snapshot_hash")));
	s = canonical(nd);
	spit(argv[2], s);
	print("%s", s);
	exits(nil);
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'n':
		nostar = 1;
		break;
	default:
		sysfatal("usage: zzone [-n] authority|route|interest|materialize|migrate|apply args");
	}ARGEND
	if(argc < 1)
		sysfatal("usage: zzone [-n] authority|route|interest|materialize|migrate|apply args");
	if(strcmp(argv[0], "authority") == 0) authority(argc-1, argv+1);
	if(strcmp(argv[0], "route") == 0) route(argc-1, argv+1);
	if(strcmp(argv[0], "interest") == 0) interest(argc-1, argv+1);
	if(strcmp(argv[0], "materialize") == 0) materialize(argc-1, argv+1);
	if(strcmp(argv[0], "migrate") == 0) migrate(argc-1, argv+1);
	if(strcmp(argv[0], "apply") == 0) apply(argc-1, argv+1);
	sysfatal("unknown subcommand %s", argv[0]);
}
