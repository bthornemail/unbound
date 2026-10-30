/*
 * zworld - metaverse-build's world fold (~/devops/metaverse-build
 * runtime/world/apply-event.py) in Plan 9 C. A snapshot (JSON) and its events
 * (one JSON object a line) go in; the events are applied in order, each
 * checked as the fold checks it, and the new snapshot comes out as canonical
 * JSON, keys sorted, no spaces: the literal the fold writes to its out file.
 * The fold prints that snapshot's sha256 as its identity; here the snapshot
 * itself is printed (no hash is identity). A HALT (MissingActor,
 * UnknownEntity, MissingOwner, NotOwner) writes "HALT: why" on standard error,
 * nothing on standard output, and exits "3"; a malformed input writes its
 * reason and exits "2", as the fold's exit codes 3 and 2.
 *	zworld snapshot events
 *	zworld -n snapshot events	the falsifier: no owner check
 * Strings are compared and written as their raw literals and numbers as their
 * digits, so inputs whose strings need JSON escapes or whose numbers Python
 * would rewrite (1e2, 1.0) are outside what this port claims.
 */
#include <u.h>
#include <libc.h>

enum { Null, True, False, Num, Str, Arr, Obj };

typedef struct J J;
struct J {
	int	t;
	char	*s;	/* Num: digits; Str: the literal with its quotes */
	J	**v;	/* Arr, Obj: values */
	char	**k;	/* Obj: keys, each a literal with its quotes */
	int	n;
};

static int noowner;
static char *p;		/* the parser's cursor */

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
lit(void)	/* a string literal, quotes kept, escapes kept as written */
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
			ws(); k = lit(); ws(); p++;	/* : */
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

/* the value at key k (a bare name), or nil */
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

static int
isstr(J *j)
{
	return j != nil && j->t == Str;
}

static int
filled(J *j)	/* a non-empty string, as Python's isinstance(x, str) and x */
{
	return isstr(j) && strlen(j->s) > 2;
}

static int
same(J *a, J *b)	/* two strings equal */
{
	return isstr(a) && isstr(b) && strcmp(a->s, b->s) == 0;
}

static J*
str(char *s)
{
	J *j;

	j = mk(Str);
	j->s = smprint("\"%s\"", s);
	return j;
}

/* keys compared as Python compares strings: by their characters, a prefix first */
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

static void
out(J *j)
{
	char **ks;
	int i, h;

	switch(j->t){
	case Null: print("null"); return;
	case True: print("true"); return;
	case False: print("false"); return;
	case Num: case Str: print("%s", j->s); return;
	case Arr:
		print("[");
		for(i = 0; i < j->n; i++){
			if(i) print(",");
			out(j->v[i]);
		}
		print("]");
		return;
	}
	ks = malloc((j->n+1) * sizeof(char*));
	memmove(ks, j->k, j->n * sizeof(char*));
	qsort(ks, j->n, sizeof(char*), keycmp);
	print("{");
	for(i = 0; i < j->n; i++){
		if(i) print(",");
		for(h = 0; j->k[h] != ks[i]; h++)
			;
		print("%s:", ks[i]);
		out(j->v[h]);
	}
	print("}");
}

static void
halt(char *why)
{
	fprint(2, "HALT: %s\n", why);
	exits("3");
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

static J *entities;
static int nextint;

static J*
find(J *id)	/* the entity with this id, or nil */
{
	int i;

	if(!isstr(id))
		return nil;
	for(i = 0; i < entities->n; i++)
		if(same(get(entities->v[i], "id"), id))
			return entities->v[i];
	return nil;
}

static void
actor(J *ev)
{
	if(!filled(get(ev, "actor")))
		halt("MissingActor");
}

static J*
need(J *id)
{
	J *e;

	if((e = find(id)) == nil)
		halt("UnknownEntity");
	return e;
}

static void
owner(J *e, J *ev)
{
	J *o;

	if(noowner)
		return;
	o = get(e, "owner");
	if(o == nil || o->t == Null)
		halt("MissingOwner");
	if(!same(get(ev, "actor"), o))
		halt("NotOwner");
}

static J*
comps(J *e)
{
	J *c;

	if((c = get(e, "components")) == nil || c->t != Arr){
		c = mk(Arr);
		put(e, "components", c);
	}
	return c;
}

static void
apply(J *ev)
{
	J *t, *id, *ty, *e, *c, *in, *o, *cid, *d, *pa, *z;
	char *et, *s;
	int i;

	t = get(ev, "type");
	et = isstr(t) ? smprint("%.*s", (int)strlen(t->s)-2, t->s+1) : "None";
	if(strcmp(et, "ENTITY_CREATE") == 0){
		actor(ev);
		id = get(ev, "id");
		if(!isstr(id))
			fail("ENTITY_CREATE requires id");
		in = get(ev, "internal");
		o = get(ev, "owner");
		if(!filled(o))
			halt("MissingOwner");
		if((e = find(id)) != nil){
			if(get(e, "owner") == nil || get(e, "owner")->t == Null)
				put(e, "owner", o);
			else if(!same(get(e, "owner"), o))
				halt("NotOwner");
		}else{
			e = mk(Obj);
			put(e, "id", id);
			if(in == nil || in->t == Null){
				s = smprint("e%d", nextint++);
				in = str(s);
			}
			put(e, "internal", in);
			put(e, "owner", o);
			put(e, "zone", mk(Null));
			put(e, "components", mk(Arr));
			add(entities, nil, e);
		}
	}else if(strcmp(et, "ENTITY_DESTROY") == 0){
		actor(ev);
		e = need(get(ev, "id"));
		owner(e, ev);
		for(i = 0; i < entities->n; i++)
			if(entities->v[i] == e)
				break;
		memmove(entities->v+i, entities->v+i+1, (entities->n-i-1) * sizeof(J*));
		entities->n--;
	}else if(strcmp(et, "COMPONENT_ATTACH") == 0 || strcmp(et, "COMPONENT_UPDATE") == 0 || strcmp(et, "COMPONENT_DETACH") == 0){
		actor(ev);
		id = get(ev, "entity");
		ty = get(ev, "component");
		if(!isstr(id) || !isstr(ty))
			fail(smprint("%s requires entity and component", et));
		e = need(id);
		owner(e, ev);
		c = comps(e);
		if(et[10] == 'A'){	/* ATTACH */
			cid = get(ev, "cid");
			if(cid == nil || cid->t == Null || (isstr(cid) && strlen(cid->s) == 2)){
				in = get(e, "internal");
				s = isstr(in) ? smprint("%.*s", (int)strlen(in->s)-2, in->s+1) : "None";
				cid = str(smprint("%s.c%d", s, c->n + 1));
			}
			d = get(ev, "data");
			if(d == nil)
				d = mk(Obj);
			o = mk(Obj);
			put(o, "id", cid);
			put(o, "type", ty);
			put(o, "data", d);
			add(c, nil, o);
		}else if(et[10] == 'U'){	/* UPDATE */
			pa = get(ev, "patch");
			for(i = 0; i < c->n; i++)
				if(same(get(c->v[i], "type"), ty)){
					d = get(c->v[i], "data");
					if(d == nil || d->t != Obj)
						d = mk(Obj);
					if(pa != nil && pa->t == Obj){
						int j;
						for(j = 0; j < pa->n; j++)
							put(d, smprint("%.*s", (int)strlen(pa->k[j])-2, pa->k[j]+1), pa->v[j]);
					}
					put(c->v[i], "data", d);
					break;
				}
		}else{			/* DETACH */
			for(i = 0; i < c->n; i++)
				if(same(get(c->v[i], "type"), ty)){
					memmove(c->v+i, c->v+i+1, (c->n-i-1) * sizeof(J*));
					c->n--;
					break;
				}
		}
	}else if(strcmp(et, "ZONE_MOVE") == 0){
		actor(ev);
		id = get(ev, "entity");
		z = get(ev, "zone");
		if(!isstr(id) || !isstr(z))
			fail("ZONE_MOVE requires entity and zone");
		e = need(id);
		owner(e, ev);
		put(e, "zone", z);
	}else
		fail(smprint("unknown event type: %s", et));
}

void
main(int argc, char *argv[])
{
	J *snap, *state, *ns, *ts, *w, *in;
	char *evs, *l, *nl;
	int i, n;

	ARGBEGIN{
	case 'n':
		noowner = 1;
		break;
	default:
		sysfatal("usage: zworld [-n] snapshot events");
	}ARGEND
	if(argc != 2)
		sysfatal("usage: zworld [-n] snapshot events");
	p = slurp(argv[0]);
	snap = val();
	state = get(snap, "state");
	if(state == nil || state->t != Obj)
		fail("invalid snapshot: missing state");
	entities = get(state, "entities");
	if(entities == nil)
		entities = mk(Arr);	/* as the fold's state.get("entities", []): not put back */
	if(entities->t != Arr)
		fail("invalid snapshot: entities not list");
	nextint = 1;
	for(i = 0; i < entities->n; i++){
		in = get(entities->v[i], "internal");
		if(isstr(in) && in->s[1] == 'e' && strlen(in->s) > 3){
			char *q;
			for(q = in->s+2; *q != '"' && *q >= '0' && *q <= '9'; q++)
				;
			if(*q == '"' && (n = atoi(in->s+2) + 1) > nextint)
				nextint = n;
		}
	}
	evs = slurp(argv[1]);
	for(l = evs; *l; l = nl){
		if((nl = strchr(l, '\n')) == nil)
			nl = l + strlen(l);
		else
			*nl++ = 0;
		p = l; ws();
		if(*p == 0)
			continue;
		apply(val());
	}
	ns = mk(Obj);
	w = get(snap, "world");
	put(ns, "world", w ? w : mk(Null));
	put(ns, "state", state);
	ts = get(snap, "trace_seed");
	put(ns, "trace_seed", ts ? ts : mk(Arr));
	out(ns);
	exits(nil);
}
