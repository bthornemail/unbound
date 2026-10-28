/*
 * zlazy - ask whether a sentence belongs to a production of a grammar in
 * Wirth syntax notation, expanding only what the question reaches.
 *
 *	zlazy grammar.wsn PRODUCTION sentence
 *
 * Prints yes or no, and how many (production, position) pairs were expanded.
 * Each pair is expanded once and remembered; a production never asked about
 * is never expanded. An identifier with no production matches nothing, as
 * WSN leaves letter and character undefined. Exit status: "" for yes.
 */
#include <u.h>
#include <libc.h>

enum { Lit, Id, Seq, Alt, Opt, Rep };

typedef struct Node Node;
struct Node {
	int	type;
	Rune	*s;		/* Lit: the runes; Id: the name */
	int	n;		/* Lit: length */
	int	id;		/* Id: production index, -1 if undefined */
	Node	**kid;
	int	nkid;
};

typedef struct Prod Prod;
struct Prod {
	Rune	*name;
	Node	*expr;
	uchar	**memo;		/* memo[pos]: end positions, nil until expanded */
	uchar	*busy;
};

static Rune *g;			/* the grammar text */
static int gp, line;
static Prod prods[1024];
static int nprods;
static Rune *sent;		/* the sentence */
static int slen, expanded;

static void
bad(char *what)
{
	sysfatal("grammar line %d: %s", line, what);
}

static void
space(void)
{
	for(; g[gp] == ' ' || g[gp] == '\t' || g[gp] == '\n' || g[gp] == '\r'; gp++)
		if(g[gp] == '\n')
			line++;
}

static int
isletter(Rune c)
{
	return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z';
}

static Node*
node(int type)
{
	Node *e;

	e = mallocz(sizeof *e, 1);
	e->type = type;
	return e;
}

static void
add(Node *e, Node *k)
{
	e->kid = realloc(e->kid, (e->nkid + 1) * sizeof e->kid[0]);
	e->kid[e->nkid++] = k;
}

static Node *expression(void);

static Node*
factor(void)
{
	Node *e;
	int st, i;
	Rune close;

	space();
	if(isletter(g[gp])){
		e = node(Id);
		for(st = gp; isletter(g[gp]); gp++)
			;
		e->s = runesmprint("%.*S", gp - st, g + st);
		return e;
	}
	if(g[gp] == '"'){
		e = node(Lit);
		e->s = malloc((runestrlen(g + gp) + 1) * sizeof(Rune));
		gp++;
		for(i = 0;; gp++){
			if(g[gp] == 0 || g[gp] == '\n')
				bad("unterminated literal");
			if(g[gp] == '"'){
				if(g[gp+1] != '"')
					break;
				gp++;
			}
			e->s[i++] = g[gp];
		}
		gp++;
		e->n = i;
		if(i == 0)
			bad("empty literal");
		return e;
	}
	if(g[gp] == '[' || g[gp] == '(' || g[gp] == '{'){
		e = node(g[gp] == '[' ? Opt : g[gp] == '{' ? Rep : Seq);
		close = g[gp] == '[' ? ']' : g[gp] == '{' ? '}' : ')';
		gp++;
		add(e, expression());
		space();
		if(g[gp] != close)
			bad("unclosed bracket");
		gp++;
		return e;
	}
	return nil;
}

static Node*
term(void)
{
	Node *e, *f;

	e = node(Seq);
	while((f = factor()) != nil)
		add(e, f);
	if(e->nkid == 0)
		bad("expected a factor");
	return e;
}

static Node*
expression(void)
{
	Node *e;

	e = node(Alt);
	add(e, term());
	for(space(); g[gp] == '|'; space()){
		gp++;
		add(e, term());
	}
	return e;
}

static void
resolve(Node *e)
{
	int i;

	if(e->type == Id){
		e->id = -1;
		for(i = 0; i < nprods; i++)
			if(runestrcmp(prods[i].name, e->s) == 0)
				e->id = i;
	}
	for(i = 0; i < e->nkid; i++)
		resolve(e->kid[i]);
}

static void match(Node*, int, uchar*);

/* the end positions of production p from pos, expanded once */
static uchar*
prod(int p, int pos)
{
	Prod *pr;

	pr = &prods[p];
	if(pr->memo[pos] != nil)
		return pr->memo[pos];
	if(pr->busy[pos])
		return nil;	/* left recursion: no progress this way */
	pr->busy[pos] = 1;
	expanded++;
	pr->memo[pos] = mallocz(slen + 1, 1);
	match(pr->expr, pos, pr->memo[pos]);
	pr->busy[pos] = 0;
	return pr->memo[pos];
}

static void
match(Node *e, int pos, uchar *out)
{
	uchar *cur, *nxt, *all, *r;
	int i, k, more;

	switch(e->type){
	case Lit:
		if(pos + e->n <= slen && memcmp(sent + pos, e->s, e->n * sizeof(Rune)) == 0)
			out[pos + e->n] = 1;
		break;
	case Id:
		if(e->id >= 0 && (r = prod(e->id, pos)) != nil)
			for(i = 0; i <= slen; i++)
				out[i] |= r[i];
		break;
	case Alt:
		for(k = 0; k < e->nkid; k++)
			match(e->kid[k], pos, out);
		break;
	case Opt:
		out[pos] = 1;
		match(e->kid[0], pos, out);
		break;
	case Seq:
	case Rep:
		cur = mallocz(slen + 1, 1);
		cur[pos] = 1;
		if(e->type == Seq){
			for(k = 0; k < e->nkid; k++){
				nxt = mallocz(slen + 1, 1);
				for(i = 0; i <= slen; i++)
					if(cur[i])
						match(e->kid[k], i, nxt);
				free(cur);
				cur = nxt;
			}
			for(i = 0; i <= slen; i++)
				out[i] |= cur[i];
		}else{
			all = mallocz(slen + 1, 1);
			all[pos] = 1;
			do{
				nxt = mallocz(slen + 1, 1);
				for(i = 0; i <= slen; i++)
					if(cur[i])
						match(e->kid[0], i, nxt);
				more = 0;
				for(i = 0; i <= slen; i++){
					cur[i] = nxt[i] && !all[i];
					if(cur[i])
						all[i] = more = 1;
				}
				free(nxt);
			}while(more);
			for(i = 0; i <= slen; i++)
				out[i] |= all[i];
			free(all);
		}
		free(cur);
		break;
	}
}

void
main(int argc, char *argv[])
{
	int fd, i, st, start;
	Dir *d;
	char *buf;
	uchar *r;

	if(argc != 4){
		fprint(2, "usage: zlazy grammar.wsn PRODUCTION sentence\n");
		exits("usage");
	}
	if((fd = open(argv[1], OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", argv[1]);
	buf = mallocz(d->length + 1, 1);
	if(readn(fd, buf, d->length) < 0)
		sysfatal("read %s: %r", argv[1]);
	close(fd);
	g = runesmprint("%s", buf);
	line = 1;
	for(space(); g[gp]; space()){
		if(!isletter(g[gp]))
			bad("expected an identifier");
		for(st = gp; isletter(g[gp]); gp++)
			;
		prods[nprods].name = runesmprint("%.*S", gp - st, g + st);
		space();
		if(g[gp] != '=')
			bad("expected =");
		gp++;
		prods[nprods].expr = expression();
		space();
		if(g[gp] != '.')
			bad("expected .");
		gp++;
		nprods++;
	}
	for(i = 0; i < nprods; i++)
		resolve(prods[i].expr);
	sent = runesmprint("%s", argv[3]);
	slen = runestrlen(sent);
	for(i = 0; i < nprods; i++){
		prods[i].memo = mallocz((slen + 1) * sizeof(uchar*), 1);
		prods[i].busy = mallocz(slen + 1, 1);
	}
	start = -1;
	for(i = 0; i < nprods; i++)
		if(strcmp(argv[2], smprint("%S", prods[i].name)) == 0)
			start = i;
	if(start < 0)
		sysfatal("no production %s", argv[2]);
	r = prod(start, 0);
	print("%s %s \"%s\": %s (%d expanded of %d productions x %d positions)\n",
		argv[1], argv[2], argv[3], r != nil && r[slen] ? "yes" : "no", expanded, nprods, slen + 1);
	exits(r != nil && r[slen] ? nil : "no");
}
