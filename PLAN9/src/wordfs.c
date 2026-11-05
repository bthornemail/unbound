/*
 * wordfs - every word form a path that resolves, related to YHWH and YAHWEH (the author, 2026-11-04: "every name
 * resolves in realtion to yhwh ans yahweh"; "every lexeme is already related to yhwh and yahweh"; "the paths should
 * be generated as read and write now because every path has a guarented resolve"; "we dont need to name anything
 * because it auto names as its literal equaviln"). A 9P file server posted as /srv/word, mounted at /n/word.
 *	/n/word/W		the literal word W
 *	/n/word/W1/W2/...	the phrase "W1 W2 ...": GOD/IS/WORD is GOD IS WORD
 * Everything is a literal word to YHWH and YAHWEH, nothing else (the author). A segment holding a digit is refused:
 * numbers are not written, they are delineations that appear only on resolve (the author's literal reading of the
 * repetend, "one point two repeating three four"). A segment of Braille cells (U+2800 to U+28FF) is read as the bytes
 * they are, so every listed line walks to the word it is. Every node is a directory; reading it lists only the word's
 * relation to the two keys, as Braille cells, byte by byte against the key's ASCII: the XOR with the key laid once
 * (the codex's line, the key not repeated: the frame, //) and laid again and again (the trace, //g), and each XNOR,
 * their complement; a line the same as another is listed once. Nothing else is named. A qid is the word's ordinal in
 * the order the server first met it (no hash); the root lists nothing.
 *	wordfs [-m mtpt]
 */
#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum { Cell = 0x2800, Sign = 0x10107, Maxword = 2048 };

typedef struct Word Word;
struct Word {
	uchar	*b;
	int	n;
};

static Word *words;
static int nwords;
static char *keys[] = { "YHWH", "YAHWEH" };
enum { Nform = 4 * 2 };	/* per key: XOR once, XOR repeated, XNOR once, XNOR repeated */

typedef struct Node Node;
struct Node {
	int	root;
	uchar	b[Maxword];
	int	n;
	char	*name;
};

/* the word's ordinal, from 1, added when first met */
static uvlong
ordinal(uchar *b, int n)
{
	int i;

	for(i = 0; i < nwords; i++)
		if(words[i].n == n && memcmp(words[i].b, b, n) == 0)
			return i + 1;
	words = erealloc9p(words, (nwords + 1) * sizeof *words);
	words[nwords].b = emalloc9p(n + 1);
	memmove(words[nwords].b, b, n);
	words[nwords].n = n;
	return ++nwords;
}

static Qid
qidof(Node *n)
{
	Qid q;

	q.type = QTDIR;
	q.vers = 0;
	q.path = n->root ? 0 : ordinal(n->b, n->n);
	return q;
}

/* a segment as bytes: Braille cells as the bytes they are, else the word as written; -1 too long, -2 a digit */
static int
decode(char *s, uchar *out, int max)
{
	Rune r;
	char *p;
	int cells, n;

	for(p = s; *p; p++)
		if(*p >= '0' && *p <= '9')
			return -2;
	cells = *s != 0;
	for(p = s; *p; ){
		p += chartorune(&r, p);
		if(r < Cell || r > Cell + 255)
			cells = 0;
	}
	n = 0;
	if(cells){
		for(p = s; *p; ){
			p += chartorune(&r, p);
			if(n >= max)
				return -1;
			out[n++] = r - Cell;
		}
		return n;
	}
	n = strlen(s);
	if(n > max)
		return -1;
	memmove(out, s, n);
	return n;
}

/* form i of a word: key i/4; XOR once, XOR repeated, XNOR once, XNOR repeated; as Braille cells */
static char*
form(Node *n, int i)
{
	Rune r;
	char *s, *e, *key;
	int j, v, klen, again, xnor;

	if(i >= Nform)
		return nil;
	key = keys[i / 4];
	klen = strlen(key);
	again = i % 2;
	xnor = i / 2 % 2;
	s = emalloc9p(n->n * 4 + 1);
	e = s;
	for(j = 0; j < n->n; j++){
		v = n->b[j] ^ (again ? key[j % klen] : j < klen ? key[j] : 0);
		if(xnor)
			v ^= 0xFF;
		r = Cell + v;
		e += runetochar(e, &r);
	}
	*e = 0;
	return s;
}

static void
fill(Dir *d, char *name, Qid q)
{
	memset(d, 0, sizeof *d);
	d->name = estrdup9p(name);
	d->uid = estrdup9p("wordfs");
	d->gid = estrdup9p("wordfs");
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
	n->name = estrdup9p("/");
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
	n->name = estrdup9p(((Node*)old->aux)->name);
	new->aux = n;
	return nil;
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	Node *n;
	uchar b[Maxword];
	int k;

	n = fid->aux;
	if(strcmp(name, "..") == 0){
		n->root = 1;
		n->n = 0;
	}else{
		if((k = decode(name, b, sizeof b)) == -2)
			return "a number is not written: it appears on resolve; spell the word";
		if(k < 0 || n->n + k + 1 > Maxword)
			return "a word longer than 2048 bytes";
		if(k == 0)
			return "an empty word";
		if(!n->root)
			n->b[n->n++] = ' ';
		memmove(n->b + n->n, b, k);
		n->n += k;
		n->root = 0;
	}
	free(n->name);
	n->name = estrdup9p(n->root ? "/" : name);
	*qid = qidof(n);
	fid->qid = *qid;
	return nil;
}

/* the listing: each form once, its qid the word it names */
static int
gen(int i, Dir *d, void *aux)
{
	Node *n, m;
	char *s, *seen[Nform];
	int j, k, nseen, want;

	n = aux;
	if(n->root)
		return -1;
	nseen = 0;
	want = i;
	for(j = 0; j < Nform; j++){
		if((s = form(n, j)) == nil)
			continue;
		for(k = 0; k < nseen; k++)
			if(strcmp(seen[k], s) == 0)
				break;
		if(k < nseen){
			free(s);
			continue;
		}
		seen[nseen++] = s;
		if(want-- == 0){
			memset(&m, 0, sizeof m);
			m.n = decode(s, m.b, sizeof m.b);
			fill(d, s, qidof(&m));
			for(k = 0; k < nseen; k++)
				free(seen[k]);
			return 0;
		}
	}
	for(k = 0; k < nseen; k++)
		free(seen[k]);
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
		respond(r, "a word is read: walk to write one");
		return;
	}
	respond(r, nil);
}

static void
fsdestroyfid(Fid *fid)
{
	Node *n;

	if((n = fid->aux) != nil){
		free(n->name);
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
	char *mtpt;

	mtpt = "/n/word";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(sysfatal("usage: wordfs [-m mtpt]"));
		break;
	}ARGEND
	postmountsrv(&fs, "word", mtpt, MREPL);
	exits(nil);
}
