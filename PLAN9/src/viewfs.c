/*
 * viewfs - the autonomous view: any file in the name space read through a circuit, as a Braille stream, and nothing
 * changed (the author, 2026-11-04: "every interaction calls the pi proof resolution on every bitfield, it makes any
 * data into a bralle strem which cn be extreacted as any other stream, nothing fuckig chanes, the observer calss
 * differnt views, like callin fucking differnet ciricutruittryt"; "nothing has a fuking value until the request has
 * bene resolved"). A 9P file server posted as /srv/view, mounted at /n/view:
 *	/n/view/braille/PATH		each byte of PATH as its 8-dot cell, U+2800 + byte
 *	/n/view/xor/YHWH/PATH		each byte XOR the key laid again and again (//g), as cells; YAHWEH the same
 *	/n/view/xnor/YHWH/PATH		the complement of xor; YAHWEH the same
 *	/n/view/pixels/PATH		PIXELATE's pixels: each block of 64 bytes as 64 places, the butterfly's six
 *					stages, stage i XORing each place holding bit i with the place without it
 *					(zpixelate.c's cast, every bit-plane at once), as cells
 *	/n/view/spectrum/PATH		PIXELATE's spectrum: for each block of 64 and each bit-plane k from 0 to 7,
 *					for each a in 0..63 the count of places x where bit k of byte x differs
 *					from the parity of a & x (its Walsh bin is 64 - 2 x that), the count as a cell
 * PATH is any path of the server's name space (the one it was started in), its directories listed as they are,
 * lengths not given: a view has no value until it is read. Nothing is written: a file is opened OREAD only, its
 * bytes read when a read asks and the view made then. The operation is lower case, the constant upper case. A qid
 * is the file's own, its top byte the circuit's place in the list above (literal, no hash).
 *	viewfs [-m mtpt]
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum { Braille = 1, Xor, Xnor, Pixels, Spectrum, Maxfile = 16*1024*1024 };
static char *circuit[] = { nil, "braille", "xor", "xnor", "pixels", "spectrum" };
static char *keys[] = { "YHWH", "YAHWEH" };
static char *mtpt = "/n/view";

typedef struct Node Node;
struct Node {
	int	c;		/* the circuit, 0 at the root */
	char	*key;		/* xor and xnor: the key, nil until chosen */
	char	*path;		/* the real path, nil above it */
	char	*view;		/* made at the first read */
	long	nview;
	int	made;
};

static Node*
newnode(void)
{
	Node *n;

	n = emalloc9p(sizeof *n);
	memset(n, 0, sizeof *n);
	return n;
}

static int
inside(Node *n)
{
	return n->path != nil;
}

static Qid
qidof(Node *n)
{
	Qid q;
	Dir *d;
	int k;

	q.vers = 0;
	q.type = QTDIR;
	if(!inside(n)){
		k = 0;
		if(n->key != nil)
			k = strcmp(n->key, keys[0]) == 0 ? 1 : 2;
		q.path = (uvlong)n->c << 8 | k;
		return q;
	}
	if((d = dirstat(n->path)) == nil){
		q.path = 0;
		return q;
	}
	q = d->qid;
	q.path = (q.path & 0x00FFFFFFFFFFFFFFULL) | (uvlong)n->c << 56;
	free(d);
	return q;
}

static void
fill(Dir *d, char *name, Qid q)
{
	memset(d, 0, sizeof *d);
	d->name = estrdup9p(name);
	d->uid = estrdup9p("view");
	d->gid = estrdup9p("view");
	d->muid = estrdup9p("");
	d->qid = q;
	d->mode = q.type & QTDIR ? DMDIR|0555 : 0444;
}

static void
fsattach(Req *r)
{
	Node *n;

	n = newnode();
	r->fid->aux = n;
	r->fid->qid = qidof(n);
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static char*
fsclone(Fid *old, Fid *new)
{
	Node *n, *o;

	o = old->aux;
	n = newnode();
	n->c = o->c;
	n->key = o->key;
	n->path = o->path ? estrdup9p(o->path) : nil;
	new->aux = n;
	return nil;
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	Node *n;
	char *p, *s;
	int i;
	Dir *d;

	n = fid->aux;
	if(strcmp(name, "..") == 0){
		if(inside(n) && strcmp(n->path, "/") != 0){
			if((s = strrchr(n->path, '/')) == n->path)
				s[1] = 0;
			else
				*s = 0;
		}else if(inside(n)){
			free(n->path);
			n->path = nil;
			if(n->key == nil)
				n->c = 0;
		}else if(n->key != nil)
			n->key = nil;
		else
			n->c = 0;
	}else if(n->c == 0){
		for(i = 1; i < nelem(circuit); i++)
			if(strcmp(name, circuit[i]) == 0)
				break;
		if(i == nelem(circuit))
			return "no such circuit: braille, xor, xnor, pixels, spectrum";
		n->c = i;
		if(i != Xor && i != Xnor)
			n->path = estrdup9p("/");
	}else if(!inside(n)){
		for(i = 0; i < nelem(keys); i++)
			if(strcmp(name, keys[i]) == 0)
				break;
		if(i == nelem(keys))
			return "no such key: YHWH or YAHWEH";
		n->key = keys[i];
		n->path = estrdup9p("/");
	}else{
		p = strcmp(n->path, "/") == 0 ? smprint("/%s", name) : smprint("%s/%s", n->path, name);
		if(strncmp(p, mtpt, strlen(mtpt)) == 0 && (p[strlen(mtpt)] == 0 || p[strlen(mtpt)] == '/')){
			free(p);
			return "a view of the view is not read";
		}
		if((d = dirstat(p)) == nil){
			free(p);
			return "no such file in the name space";
		}
		free(d);
		free(n->path);
		n->path = p;
	}
	*qid = qidof(n);
	fid->qid = *qid;
	return nil;
}

static uchar*
slurp(char *path, long *n)
{
	int fd, m;
	long cap;
	uchar *b;

	if((fd = open(path, OREAD)) < 0)
		return nil;
	cap = 8192;
	b = emalloc9p(cap);
	*n = 0;
	while((m = read(fd, b + *n, cap - *n)) > 0){
		*n += m;
		if(*n == cap){
			if(cap >= Maxfile)
				break;
			cap *= 2;
			b = erealloc9p(b, cap);
		}
	}
	close(fd);
	return b;
}

static char*
cells(uchar *v, long n, long *len)
{
	char *s, *e;
	Rune r;
	long i;

	s = emalloc9p(n * 3 + 1);
	e = s;
	for(i = 0; i < n; i++){
		r = 0x2800 + v[i];
		e += runetochar(e, &r);
	}
	*len = e - s;
	return s;
}

static int
parity(int x)
{
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return x & 1;
}

/* the view, made at the first read */
static char*
make(Node *n, long *len)
{
	uchar *b, *v, blk[64];
	long nb, nv, i, o;
	int k, s, p, a, x, cnt, klen;
	char *view;

	if((b = slurp(n->path, &nb)) == nil)
		return nil;
	switch(n->c){
	case Braille:
		v = b;
		nv = nb;
		break;
	case Xor:
	case Xnor:
		klen = strlen(n->key);
		for(i = 0; i < nb; i++)
			b[i] ^= n->key[i % klen] ^ (n->c == Xnor ? 0xFF : 0);
		v = b;
		nv = nb;
		break;
	case Pixels:
		nv = (nb + 63) / 64 * 64;
		v = emalloc9p(nv);
		memset(v, 0, nv);
		memmove(v, b, nb);
		for(o = 0; o < nv; o += 64)
			for(s = 0; s < 6; s++)
				for(p = 0; p < 64; p++)
					if(p >> s & 1)
						v[o + p] ^= v[o + (p ^ 1 << s)];
		free(b);
		break;
	case Spectrum:
		nv = (nb + 63) / 64 * 512;
		v = emalloc9p(nv);
		for(o = 0; o < (nb + 63) / 64; o++){
			memset(blk, 0, sizeof blk);
			memmove(blk, b + o * 64, nb - o * 64 < 64 ? nb - o * 64 : 64);
			for(k = 0; k < 8; k++)
				for(a = 0; a < 64; a++){
					cnt = 0;
					for(x = 0; x < 64; x++)
						if((blk[x] >> k & 1) != parity(a & x))
							cnt++;
					v[o * 512 + k * 64 + a] = cnt;
				}
		}
		free(b);
		break;
	default:
		free(b);
		return nil;
	}
	view = cells(v, nv, len);
	free(v);
	return view;
}

static void
fsopen(Req *r)
{
	Node *n;

	n = r->fid->aux;
	if((r->ifcall.mode & 3) != OREAD || (r->ifcall.mode & OTRUNC)){
		respond(r, "nothing changes: a view is read");
		return;
	}
	if(!(r->fid->qid.type & QTDIR) && access(n->path, AREAD) < 0){
		respond(r, "the file cannot be read");
		return;
	}
	respond(r, nil);
}

static int
genroot(int i, Dir *d, void *aux)
{
	Node *n, m;

	n = aux;
	memset(&m, 0, sizeof m);
	if(n->c == 0){
		if(i + 1 >= nelem(circuit))
			return -1;
		m.c = i + 1;
		fill(d, circuit[i + 1], qidof(&m));
		return 0;
	}
	if(i >= nelem(keys))
		return -1;
	m.c = n->c;
	m.key = keys[i];
	fill(d, keys[i], qidof(&m));
	return 0;
}

typedef struct Listing Listing;
struct Listing {
	Node	*n;
	Dir	*d;
	long	nd;
};

static int
genreal(int i, Dir *d, void *aux)
{
	Listing *l;
	Qid q;

	l = aux;
	if(i >= l->nd)
		return -1;
	q = l->d[i].qid;
	q.path = (q.path & 0x00FFFFFFFFFFFFFFULL) | (uvlong)l->n->c << 56;
	fill(d, l->d[i].name, q);
	return 0;
}

static void
fsread(Req *r)
{
	Node *n;
	Listing l;
	int fd;

	n = r->fid->aux;
	if(!inside(n)){
		dirread9p(r, genroot, n);
		respond(r, nil);
		return;
	}
	if(r->fid->qid.type & QTDIR){
		if((fd = open(n->path, OREAD)) < 0){
			respond(r, "the directory cannot be read");
			return;
		}
		l.n = n;
		l.nd = dirreadall(fd, &l.d);
		close(fd);
		dirread9p(r, genreal, &l);
		free(l.d);
		respond(r, nil);
		return;
	}
	if(!n->made){
		n->view = make(n, &n->nview);
		n->made = 1;
	}
	if(n->view == nil){
		respond(r, "the file cannot be read");
		return;
	}
	readbuf(r, n->view, n->nview);
	respond(r, nil);
}

static void
fsstat(Req *r)
{
	Node *n;
	char *s;

	n = r->fid->aux;
	if(inside(n))
		s = strcmp(n->path, "/") == 0 ? "/" : strrchr(n->path, '/') + 1;
	else if(n->key != nil)
		s = n->key;
	else
		s = n->c ? circuit[n->c] : "/";
	fill(&r->d, s, qidof(n));
	respond(r, nil);
}

static void
fsdestroyfid(Fid *fid)
{
	Node *n;

	if((n = fid->aux) != nil){
		free(n->path);
		free(n->view);
		free(n);
	}
}

Srv fs = {
	.attach = fsattach,
	.clone = fsclone,
	.walk1 = fswalk1,
	.open = fsopen,
	.read = fsread,
	.stat = fsstat,
	.destroyfid = fsdestroyfid,
};

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("usage: viewfs [-m mtpt]"));
		break;
	}ARGEND
	postmountsrv(&fs, "view", mtpt, MREPL);
	exits(nil);
}
