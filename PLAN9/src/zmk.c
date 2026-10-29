/*
 * zmk - Metaverse Kit's world fold (packages/shadow-canvas buildState, in
 * ~/devops/metaverse-kit) in Plan 9 C, over the event lines of
 * wsn/mkevent.wsn: one event a line, the envelope's fields in its order, each
 * named by its own field name. The events are put in order by timestamp, then
 * by event_id as JavaScript's localeCompare orders it, and applied one by one:
 * a transform, a property, a geometry, media, text or document is the last
 * one written; links are added and removed as a set; a delete leaves the node,
 * marked. Nothing is computed: every value is copied as its line wrote it.
 *	zmk		events on standard input, the state on standard output,
 *			one line per node in the order the nodes were made
 *	zmk -b		the same, event_ids ordered by their bytes instead (the
 *			falsifier: the kit's state must not come out)
 *	zmk -o		lines on standard input, put in localeCompare order
 */
#include <u.h>
#include <libc.h>
#include <bio.h>

enum { Maxtok = 512, Ntf = 10 };

typedef struct Event Event;
struct Event {
	char	**t;
	int	n;
	int	seq;
};

typedef struct Node Node;
struct Node {
	char	*id;
	char	*kind;
	int	deleted;
	char	*tf[Ntf];
	char	**key, **val;
	int	nprop;
	char	**rel, **to;
	int	nlink;
	char	*block[4];	/* geometry, media, text, document: their lines, as written */
};

static char *blockname[4] = { "geometry", "media", "text", "document" };
static int bytes;
static Node **nodes;
static int nnodes;

/*
 * localeCompare as Node's en-US collation orders the characters of ids:
 * first by base letters, case aside, punctuation not ignored, in the order
 * _ - : . / then the digits then the letters; then, base letters equal, the
 * first difference of case from the left, lowercase first.
 */
static char punct[] = "_-:./0123456789";

static int
primary(int c)
{
	char *p;

	if(c != 0 && (p = strchr(punct, c)) != nil)
		return p - punct;
	if(c >= 'a' && c <= 'z')
		return 15 + c - 'a';
	if(c >= 'A' && c <= 'Z')
		return 15 + c - 'A';
	return 100 + c;
}

static int
coll(char *a, char *b)
{
	int i, d;

	if(bytes)
		return strcmp(a, b);
	for(i = 0; a[i] && b[i]; i++)
		if((d = primary(a[i]) - primary(b[i])) != 0)
			return d;
	if(a[i] || b[i])
		return a[i] ? 1 : -1;
	for(i = 0; a[i]; i++)
		if(a[i] != b[i])
			return (a[i] >= 'A' && a[i] <= 'Z') ? 1 : -1;
	return 0;
}

static int
evcmp(void *va, void *vb)
{
	Event *a, *b;
	double x, y;
	int d;

	a = va;
	b = vb;
	x = atof(a->t[3]);
	y = atof(b->t[3]);
	if(x != y)
		return x < y ? -1 : 1;
	if((d = coll(a->t[1], b->t[1])) != 0)
		return d;
	return a->seq - b->seq;
}

static int
strpcmp(void *a, void *b)
{
	return coll(*(char**)a, *(char**)b);
}

static Node*
lookup(char *id)
{
	int i;

	for(i = 0; i < nnodes; i++)
		if(strcmp(nodes[i]->id, id) == 0)
			return nodes[i];
	return nil;
}

/* the node, if there and not deleted */
static Node*
live(char *id)
{
	Node *n;

	n = lookup(id);
	return n != nil && !n->deleted ? n : nil;
}

static void
setprop(Node *n, char *k, char *v)
{
	int i;

	for(i = 0; i < n->nprop; i++)
		if(strcmp(n->key[i], k) == 0){
			n->val[i] = v;
			return;
		}
	n->key = realloc(n->key, (n->nprop+1) * sizeof(char*));
	n->val = realloc(n->val, (n->nprop+1) * sizeof(char*));
	n->key[n->nprop] = k;
	n->val[n->nprop++] = v;
}

/* properties k v k v ... to the end of the line */
static void
props(Node *n, char **t, int i, int e)
{
	for(; i+1 < e; i += 2)
		setprop(n, t[i], t[i+1]);
}

static char*
join(char **t, int i, int e)
{
	char *s, *r;

	s = strdup("");
	for(; i < e; i++){
		r = smprint("%s%s%s", s, *s ? " " : "", t[i]);
		free(s);
		s = r;
	}
	return s;
}

static void
apply(Event *ev)
{
	char **t, *op;
	int i, n, j;
	Node *nd;

	t = ev->t;
	n = ev->n;
	op = t[11];
	/* past the envelope: scope, its policy, the invariants, previous_events, tile */
	for(i = 16; i < n && strcmp(t[i], "tile") != 0; i++)
		;
	i += 2;
	if(strcmp(op, "create_node") == 0){
		/* node_id N kind K transform 10 [properties ...] */
		if(lookup(t[i+1]) != nil)
			return;
		nd = mallocz(sizeof *nd, 1);
		nd->id = t[i+1];
		nd->kind = t[i+3];
		for(j = 0; j < Ntf; j++)
			nd->tf[j] = t[i+5+j];
		if(i+5+Ntf < n)
			props(nd, t, i+6+Ntf, n);
		nodes = realloc(nodes, (nnodes+1) * sizeof(Node*));
		nodes[nnodes++] = nd;
	}else if(strcmp(op, "update_transform") == 0){
		if((nd = live(t[i+1])) != nil)
			for(j = 0; j < Ntf; j++)
				nd->tf[j] = t[i+3+j];
	}else if(strcmp(op, "set_properties") == 0){
		if((nd = live(t[i+1])) != nil)
			props(nd, t, i+3, n);
	}else if(strcmp(op, "link_nodes") == 0){
		/* from_node F relation R to_node T */
		if((nd = live(t[i+1])) != nil){
			nd->rel = realloc(nd->rel, (nd->nlink+1) * sizeof(char*));
			nd->to = realloc(nd->to, (nd->nlink+1) * sizeof(char*));
			nd->rel[nd->nlink] = t[i+3];
			nd->to[nd->nlink++] = t[i+5];
		}
	}else if(strcmp(op, "unlink_nodes") == 0){
		if((nd = live(t[i+1])) != nil){
			int k = 0;
			for(j = 0; j < nd->nlink; j++)
				if(strcmp(nd->rel[j], t[i+3]) != 0 || strcmp(nd->to[j], t[i+5]) != 0){
					nd->rel[k] = nd->rel[j];
					nd->to[k++] = nd->to[j];
				}
			nd->nlink = k;
		}
	}else if(strcmp(op, "delete_node") == 0){
		if((nd = lookup(t[i+1])) != nil)
			nd->deleted = 1;
	}else if(strcmp(op, "physics_step") == 0){
		/* solver S tick N delta_time D updates (node_id N transform 10)... */
		for(i += 7; i + 3 + Ntf <= n; i += 3 + Ntf)
			if((nd = live(t[i+1])) != nil)
				for(j = 0; j < Ntf; j++)
					nd->tf[j] = t[i+3+j];
	}else{
		for(j = 0; j < 4; j++)
			if(strcmp(op, smprint("set_%s", blockname[j])) == 0){
				/* node_id N <block> kind K ref R ... to the end of the line */
				if((nd = live(t[i+1])) != nil)
					nd->block[j] = join(t, i+3, n);
				return;
			}
		/* merge, set_physics, macro.*: the fold leaves the state as it is */
	}
}

/* JavaScript puts a key that is an array index (0 to 2^32-2, no leading 0) first, in numeric order */
static int
isindex(char *k)
{
	char *p;

	if(k[0] == '0')
		return k[1] == 0;
	for(p = k; *p; p++)
		if(*p < '0' || *p > '9')
			return 0;
	return p > k && (p - k < 10 || (p - k == 10 && strcmp(k, "4294967295") < 0));
}

static int
numcmp(void *va, void *vb)
{
	char *a, *b;

	a = *(char**)va;
	b = *(char**)vb;
	if(strlen(a) != strlen(b))
		return strlen(a) < strlen(b) ? -1 : 1;
	return strcmp(a, b);
}

static char*
valof(Node *n, char *k)
{
	int i;

	for(i = 0; i < n->nprop; i++)
		if(strcmp(n->key[i], k) == 0)
			return n->val[i];
	return nil;
}

static void
show(Biobuf *o, Node *n)
{
	char **idx;
	int i, m;

	Bprint(o, "node_id %s kind %s%s transform", n->id, n->kind, n->deleted ? " deleted" : "");
	for(i = 0; i < Ntf; i++)
		Bprint(o, " %s", n->tf[i]);
	Bprint(o, " properties");
	idx = malloc((n->nprop+1) * sizeof(char*));
	for(i = m = 0; i < n->nprop; i++)
		if(isindex(n->key[i]))
			idx[m++] = n->key[i];
	qsort(idx, m, sizeof(char*), numcmp);
	for(i = 0; i < m; i++)
		Bprint(o, " %s %s", idx[i], valof(n, idx[i]));
	for(i = 0; i < n->nprop; i++)
		if(!isindex(n->key[i]))
			Bprint(o, " %s %s", n->key[i], n->val[i]);
	Bprint(o, " links");
	for(i = 0; i < n->nlink; i++)
		Bprint(o, " %s %s", n->rel[i], n->to[i]);
	for(i = 0; i < 4; i++)
		if(n->block[i] != nil)
			Bprint(o, " %s %s", blockname[i], n->block[i]);
	Bprint(o, "\n");
	free(idx);
}

void
main(int argc, char *argv[])
{
	Biobuf in, out;
	Event *ev;
	char *l, **lines;
	int nev, i, order;

	order = 0;
	ARGBEGIN{
	case 'b':
		bytes = 1;
		break;
	case 'o':
		order = 1;
		break;
	default:
		fprint(2, "usage: zmk [-b] [-o]\n");
		exits("usage");
	}ARGEND
	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	ev = nil;
	lines = nil;
	nev = 0;
	while((l = Brdstr(&in, '\n', 1)) != nil){
		if(*l == 0)
			continue;
		if(order){
			lines = realloc(lines, (nev+1) * sizeof(char*));
			lines[nev++] = l;
			continue;
		}
		ev = realloc(ev, (nev+1) * sizeof(Event));
		ev[nev].t = malloc(Maxtok * sizeof(char*));
		ev[nev].n = getfields(l, ev[nev].t, Maxtok, 1, " ");
		if(ev[nev].n < 20)
			sysfatal("line %d: not an event", nev+1);
		ev[nev].seq = nev;
		nev++;
	}
	if(order){
		qsort(lines, nev, sizeof(char*), strpcmp);
		for(i = 0; i < nev; i++)
			Bprint(&out, "%s\n", lines[i]);
		Bterm(&out);
		exits(nil);
	}
	qsort(ev, nev, sizeof(Event), evcmp);
	for(i = 0; i < nev; i++)
		apply(&ev[i]);
	for(i = 0; i < nnodes; i++)
		show(&out, nodes[i]);
	Bterm(&out);
	exits(nil);
}
