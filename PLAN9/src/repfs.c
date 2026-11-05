/*
 * repfs - every number a path that resolves (the author, 2026-11-04: "the paths should be generated as read and write
 * now because every path has a guarented resolve"; "we dont need to name anything because it auto names as its
 * literal equaviln"). A 9P file server of the rationals, posted as /srv/repetend, mounted at /n/repetend:
 *	I		the integer I
 *	I/P		I.P, P the digits after the point
 *	I/P/R		I.P(R), R the repetend: (I P R - I P) / (10^|P| (10^|R| - 1)), path-is-query's linear equation
 *	I.P(R)		the same number written as a literal, in one name ("0.(142857)", "1.2(34)", "3.25")
 *	N÷D		the number as a fraction (÷, U+00F7, since / divides the path)
 * Every node is a directory; reading it lists its literal equivalents, the reduced fraction N÷D and the decimal
 * I.P(R) found by long division, each itself a name that walks back to the same node. Nothing else is named. The
 * qid is the number itself, the reduced numerator and denominator as num<<32 | den (both under 2^32), so equal
 * numbers are one file whatever path reached them; no hash. The root lists nothing: its children are every number.
 *	repfs [-m mtpt]
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum { Nseg = 3, Seglen = 19, Maxper = 1000 };

typedef struct Node Node;
struct Node {
	int	root;
	uvlong	num, den;
	int	nseg;		/* digit segments walked: 0 to 3; -1 when reached by a literal */
	char	seg[Nseg][Seglen];
	char	name[64];
};

static uvlong
gcd(uvlong a, uvlong b)
{
	uvlong t;

	while(b){
		t = a % b;
		a = b;
		b = t;
	}
	return a;
}

static uvlong
p10(int n)
{
	uvlong v;

	v = 1;
	while(n-- > 0)
		v *= 10;
	return v;
}

static int
digits(char *s)
{
	if(*s == 0)
		return 0;
	for(; *s; s++)
		if(*s < '0' || *s > '9')
			return 0;
	return 1;
}

static uvlong
atouv(char *s, int n)
{
	uvlong v;

	v = 0;
	while(n-- > 0)
		v = v*10 + *s++ - '0';
	return v;
}

/* I, P, R (R nil for none) as a reduced fraction; nil or why not */
static char*
value(char *i, int ni, char *p, int np, char *r, int nr, uvlong *num, uvlong *den)
{
	uvlong n, d, g, ip;

	if(ni + np + nr > 18)
		return "more than 18 digits";
	ip = atouv(i, ni) * p10(np) + atouv(p, np);
	if(nr == 0){
		n = ip;
		d = p10(np);
	}else{
		n = ip * p10(nr) + atouv(r, nr) - ip;
		d = p10(np) * (p10(nr) - 1);
	}
	g = gcd(n, d);
	if(g == 0)
		g = 1;
	n /= g;
	d /= g;
	if(n >= 1ULL<<32 || d >= 1ULL<<32)
		return "beyond a qid: numerator and denominator must be under 2^32";
	*num = n;
	*den = d;
	return nil;
}

/* a literal name, I.P(R) or N÷D, as a fraction; nil or why not */
static char*
literal(char *s, uvlong *num, uvlong *den)
{
	char *dot, *open, *close, *div, *p, *r;
	int ni, np, nr;
	uvlong n, d, g;

	if((div = strstr(s, "÷")) != nil){
		*div = 0;
		p = div + strlen("÷");
		if(!digits(s) || !digits(p) || strlen(s) > 10 || strlen(p) > 10){
			strcpy(div, "÷");
			return "not a fraction N÷D";
		}
		n = atouv(s, strlen(s));
		d = atouv(p, strlen(p));
		strcpy(div, "÷");
		if(d == 0)
			return "a fraction over zero";
		g = gcd(n, d);
		if(g == 0)
			g = 1;
		n /= g;
		d /= g;
		if(n >= 1ULL<<32 || d >= 1ULL<<32)
			return "beyond a qid: numerator and denominator must be under 2^32";
		*num = n;
		*den = d;
		return nil;
	}
	dot = strchr(s, '.');
	ni = dot ? dot - s : strlen(s);
	if(ni == 0)
		return "not a number";
	for(p = s; p < s + ni; p++)
		if(*p < '0' || *p > '9')
			return "not a number";
	if(dot == nil)
		return value(s, ni, "", 0, "", 0, num, den);
	p = dot + 1;
	open = strchr(p, '(');
	np = open ? open - p : strlen(p);
	for(r = p; r < p + np; r++)
		if(*r < '0' || *r > '9')
			return "not a number";
	if(open == nil)
		return value(s, ni, p, np, "", 0, num, den);
	r = open + 1;
	close = strchr(r, ')');
	if(close == nil || close[1] != 0)
		return "a repetend opened and not closed";
	nr = close - r;
	if(nr == 0)
		return "an empty repetend";
	for(p = r; p < close; p++)
		if(*p < '0' || *p > '9')
			return "not a number";
	return value(s, ni, dot + 1, np, r, nr, num, den);
}

/* the decimal literal of n/d by long division, I.P(R); nil when the repetend is longer than Maxper */
static char*
decimal(uvlong n, uvlong d)
{
	uvlong d1, rem, x;
	int a, b, pre, per, k;
	char *s, *e;

	d1 = d;
	for(a = 0; d1 % 2 == 0; a++)
		d1 /= 2;
	for(b = 0; d1 % 5 == 0; b++)
		d1 /= 5;
	pre = a > b ? a : b;
	per = 0;
	if(d1 > 1){
		x = 10 % d1;
		for(per = 1; x != 1; per++){
			if(per > Maxper)
				return nil;
			x = x * 10 % d1;
		}
	}
	s = emalloc9p(32 + pre + per);
	e = seprint(s, s + 32, "%llud", n / d);
	rem = n % d;
	if(pre + per > 0)
		*e++ = '.';
	for(k = 0; k < pre + per; k++){
		if(k == pre)
			*e++ = '(';
		rem *= 10;
		*e++ = '0' + rem / d;
		rem %= d;
	}
	if(per > 0)
		*e++ = ')';
	*e = 0;
	return s;
}

static Qid
qidof(Node *n)
{
	Qid q;

	q.type = QTDIR;
	q.vers = 0;
	q.path = n->root ? 0 : n->num << 32 | n->den;
	return q;
}

static void
fill(Dir *d, char *name, Qid q)
{
	memset(d, 0, sizeof *d);
	d->name = estrdup9p(name);
	d->uid = estrdup9p("repfs");
	d->gid = estrdup9p("repfs");
	d->muid = estrdup9p("");
	d->qid = q;
	d->mode = DMDIR|0555;
}

static void
fsattach(Req *r)
{
	Node *n;

	n = emalloc9p(sizeof *n);
	memset(n, 0, sizeof *n);
	n->root = 1;
	strcpy(n->name, "/");
	r->fid->aux = n;
	r->fid->qid = qidof(n);
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static char*
fsclone(Fid *old, Fid *new)
{
	Node *n;

	n = emalloc9p(sizeof *n);
	*n = *(Node*)old->aux;
	new->aux = n;
	return nil;
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	Node *n;
	char *err;
	int k, l;

	n = fid->aux;
	if(strcmp(name, "..") == 0){
		if(n->nseg > 1){
			n->nseg--;
			err = value(n->seg[0], strlen(n->seg[0]), n->nseg > 1 ? n->seg[1] : "", n->nseg > 1 ? strlen(n->seg[1]) : 0, "", 0, &n->num, &n->den);
			if(err != nil)
				return err;
			strecpy(n->name, n->name + sizeof n->name, n->seg[n->nseg-1]);
		}else{
			memset(n, 0, sizeof *n);
			n->root = 1;
			strcpy(n->name, "/");
		}
		*qid = qidof(n);
		fid->qid = *qid;
		return nil;
	}
	if(strlen(name) >= sizeof n->name)
		return "a name longer than 63 bytes";
	if(digits(name) && (n->root || (n->nseg > 0 && n->nseg < Nseg))){
		/* a digit segment: I, then P, then R */
		k = n->root ? 0 : n->nseg;
		l = strlen(name);
		if(l >= Seglen)
			return "a segment longer than 18 digits";
		strcpy(n->seg[k], name);
		err = value(n->seg[0], strlen(n->seg[0]), k >= 1 ? n->seg[1] : "", k >= 1 ? strlen(n->seg[1]) : 0, k >= 2 ? n->seg[2] : "", k >= 2 ? strlen(n->seg[2]) : 0, &n->num, &n->den);
		if(err != nil)
			return err;
		n->nseg = k + 1;
		n->root = 0;
	}else{
		/* a literal, from any node: the number it writes */
		if(digits(name))
			return "the repetend closes the number: no fourth digit segment";
		if((err = literal(name, &n->num, &n->den)) != nil)
			return err;
		n->nseg = -1;
		n->root = 0;
	}
	strcpy(n->name, name);
	*qid = qidof(n);
	fid->qid = *qid;
	return nil;
}

static int
gen(int i, Dir *d, void *aux)
{
	Node *n;
	char *s;

	n = aux;
	if(n->root)
		return -1;
	if(i == 0){
		s = smprint("%llud÷%llud", n->num, n->den);
		fill(d, s, qidof(n));
		free(s);
		return 0;
	}
	if(i == 1 && (s = decimal(n->num, n->den)) != nil){
		fill(d, s, qidof(n));
		free(s);
		return 0;
	}
	return -1;
}

static void
fsread(Req *r)
{
	dirread9p(r, gen, r->fid->aux);
	respond(r, nil);
}

static void
fsstat(Req *r)
{
	Node *n;

	n = r->fid->aux;
	fill(&r->d, n->name, qidof(n));
	respond(r, nil);
}

static void
fsopen(Req *r)
{
	if((r->ifcall.mode & 3) != OREAD){
		respond(r, "every number is read: walk to write one");
		return;
	}
	respond(r, nil);
}

static void
fsdestroyfid(Fid *fid)
{
	free(fid->aux);
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
	char *mtpt;

	mtpt = "/n/repetend";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("usage: repfs [-m mtpt]"));
		break;
	}ARGEND
	postmountsrv(&fs, "repetend", mtpt, MREPL);
	exits(nil);
}
