/*
 * zexpand - the language-feature binding of the wiki template bindings: a MediaWiki template call, {{Name|1|k=v}},
 * filling the template's parameters, {{{1}}} and {{{k|default}}}, as Wikipedia's own parser does it (Help:Template,
 * the author's pages). The mechanism, not MediaWiki's whole parser: the template is the one named UnboundBindTest,
 * its body given; no parser functions, magic words or other templates.
 *	zexpand [-n] body call	the call's text with every call to the template expanded, on standard output; -n,
 *				the falsifier, a positional value trimmed as a named one is
 * As the parser reads (its preprocessor's order, as the cases in in/wiki show it): comments dropped; the body's
 * <noinclude> dropped and <includeonly> kept, or only its <onlyinclude> parts; the page's <includeonly> dropped and
 * <noinclude> kept. A run of "{" or "[" of two or more opens a piece; a closing run pairs with the open piece on top,
 * three braces a parameter and two a call (two brackets a link, written back as text), what is left of the run
 * staying; "|" splits a piece's parts only inside braces, and the first "=" of a part names it; a piece left open
 * is written back as text, its closed children still expanded. A call's positional values are kept as written, its
 * named ones and their names trimmed, a later value for a name winning; a parameter's name is trimmed; a parameter
 * given is its value (empty is given), else its default, else written back.
 */
#include <u.h>
#include <libc.h>

enum { Text, Eq, Tpl, Arg };

typedef struct Node Node;
typedef struct List List;
struct List
{
	Node	**n;
	int	len, cap;
};
struct Node
{
	int	type;
	char	*s;		/* Text, Eq */
	List	*parts;		/* Tpl, Arg: its parts */
	int	nparts;
	int	*eq;		/* Tpl: for each part, the index of its first "=", or -1 */
};

typedef struct Buf Buf;
struct Buf
{
	char	*s;
	long	len, cap;
};

static char Name[] = "UnboundBindTest";
static int trimall;	/* -n, the falsifier: positional values trimmed as named ones are */

static void*
emalloc(ulong n)
{
	void *p;

	if((p = mallocz(n, 1)) == nil)
		sysfatal("malloc: %r");
	return p;
}

static void
addc(Buf *b, char *s, long n)
{
	if(b->len + n + 1 > b->cap){
		b->cap = 2 * (b->len + n + 1);
		if((b->s = realloc(b->s, b->cap)) == nil)
			sysfatal("realloc: %r");
	}
	memmove(b->s + b->len, s, n);
	b->len += n;
	b->s[b->len] = 0;
}

static void
adds(Buf *b, char *s)
{
	addc(b, s, strlen(s));
}

static void
push(List *l, Node *n)
{
	if(l->len == l->cap){
		l->cap = l->cap ? 2 * l->cap : 8;
		if((l->n = realloc(l->n, l->cap * sizeof l->n[0])) == nil)
			sysfatal("realloc: %r");
	}
	l->n[l->len++] = n;
}

static Node*
text(char *s, long n, int type)
{
	Node *x;

	x = emalloc(sizeof *x);
	x->type = type;
	x->s = emalloc(n + 1);
	memmove(x->s, s, n);
	return x;
}

static void
addtext(List *l, char *s, long n)
{
	Node *x;
	char *t;

	if(n == 0)
		return;
	if(l->len > 0 && (x = l->n[l->len-1])->type == Text){
		t = emalloc(strlen(x->s) + n + 1);
		strcpy(t, x->s);
		memmove(t + strlen(x->s), s, n);
		free(x->s);
		x->s = t;
		return;
	}
	push(l, text(s, n, Text));
}

/* ---------------------------------------------------------------- comments and inclusion */

/* the text with each region from open to close removed (close missing: to the end); keep, the tags alone */
static char*
strip(char *s, char *open, char *close, int keep)
{
	Buf b;
	char *p, *q;

	memset(&b, 0, sizeof b);
	adds(&b, "");
	for(;;){
		if((p = strstr(s, open)) == nil){
			adds(&b, s);
			return b.s;
		}
		addc(&b, s, p - s);
		p += strlen(open);
		q = strstr(p, close);
		if(keep)
			addc(&b, p, q ? q - p : strlen(p));
		if(q == nil)
			return b.s;
		s = q + strlen(close);
	}
}

static char*
body(char *s)
{
	Buf b;
	char *p, *q;

	s = strip(s, "<!--", "-->", 0);
	if(strstr(s, "<onlyinclude>") != nil){
		memset(&b, 0, sizeof b);
		adds(&b, "");
		for(p = s; (p = strstr(p, "<onlyinclude>")) != nil; p = q + strlen("</onlyinclude>")){
			p += strlen("<onlyinclude>");
			if((q = strstr(p, "</onlyinclude>")) == nil){
				adds(&b, p);
				break;
			}
			addc(&b, p, q - p);
		}
		s = b.s;
	}
	s = strip(s, "<noinclude>", "</noinclude>", 0);
	return strip(s, "<includeonly>", "</includeonly>", 1);
}

static char*
page(char *s)
{
	s = strip(s, "<!--", "-->", 0);
	s = strip(s, "<includeonly>", "</includeonly>", 0);
	s = strip(s, "<noinclude>", "</noinclude>", 1);
	return strip(s, "<onlyinclude>", "</onlyinclude>", 1);
}

/* ---------------------------------------------------------------- the pieces */

typedef struct Piece Piece;
struct Piece
{
	char	open;
	int	count;
	List	*parts;
	int	nparts, cap;
	int	*eq;
};

static void
newpart(Piece *p)
{
	if(p->nparts == p->cap){
		p->cap = p->cap ? 2 * p->cap : 4;
		p->parts = realloc(p->parts, p->cap * sizeof p->parts[0]);
		p->eq = realloc(p->eq, p->cap * sizeof p->eq[0]);
		if(p->parts == nil || p->eq == nil)
			sysfatal("realloc: %r");
	}
	memset(&p->parts[p->nparts], 0, sizeof p->parts[0]);
	p->eq[p->nparts] = -1;
	p->nparts++;
}

/* a piece's parts written back into l: its opening run, its parts with "|" between, and close if closed */
static void
writeback(List *l, Piece *p, int n, char close)
{
	int i, j;

	for(i = 0; i < n; i++)
		addtext(l, &p->open, 1);
	for(i = 0; i < p->nparts; i++){
		if(i > 0)
			addtext(l, "|", 1);
		for(j = 0; j < p->parts[i].len; j++)
			if(p->parts[i].n[j]->type == Text || p->parts[i].n[j]->type == Eq)
				addtext(l, p->parts[i].n[j]->s, strlen(p->parts[i].n[j]->s));
			else
				push(l, p->parts[i].n[j]);
	}
	if(close)
		for(i = 0; i < n; i++)
			addtext(l, &close, 1);
}

static List*
preprocess(char *s)
{
	Piece *stack[1024], *p, *q;
	int sp, run, m;
	List *root, *acc;
	Node *x;
	long k;

	root = emalloc(sizeof *root);
	sp = 0;
	for(k = 0; s[k]; ){
		acc = sp ? &stack[sp-1]->parts[stack[sp-1]->nparts-1] : root;
		if(s[k] == '{' || s[k] == '['){
			for(run = 0; s[k+run] == s[k]; run++)
				;
			if(run >= 2){
				if(sp == nelem(stack))
					sysfatal("too deep");
				p = emalloc(sizeof *p);
				p->open = s[k];
				p->count = run;
				newpart(p);
				stack[sp++] = p;
			}else
				addtext(acc, s + k, run);
			k += run;
			continue;
		}
		if(sp && (s[k] == '}' && stack[sp-1]->open == '{' || s[k] == ']' && stack[sp-1]->open == '[')){
			p = stack[sp-1];
			for(run = 0; s[k+run] == s[k] && run < p->count; run++)
				;
			m = p->open == '{' ? (run >= 3 ? 3 : run == 2 ? 2 : 0) : (run >= 2 ? 2 : 0);
			if(m == 0){
				addtext(acc, s + k, run);
				k += run;
				continue;
			}
			sp--;
			acc = sp ? &stack[sp-1]->parts[stack[sp-1]->nparts-1] : root;
			p->count -= m;
			x = nil;
			if(p->open == '{'){
				x = emalloc(sizeof *x);
				x->type = m == 3 ? Arg : Tpl;
				x->parts = p->parts;
				x->nparts = p->nparts;
				x->eq = p->eq;
			}
			if(p->count >= 2){
				q = emalloc(sizeof *q);
				q->open = p->open;
				q->count = p->count;
				newpart(q);
				if(x)
					push(&q->parts[0], x);
				else
					writeback(&q->parts[0], p, m, ']');
				stack[sp++] = q;
			}else{
				if(p->count > 0)
					addtext(acc, &p->open, p->count);
				if(x)
					push(acc, x);
				else
					writeback(acc, p, m, ']');
			}
			k += m;
			continue;
		}
		if(sp && stack[sp-1]->open == '{' && s[k] == '|'){
			newpart(stack[sp-1]);
			k++;
			continue;
		}
		if(sp && stack[sp-1]->open == '{' && s[k] == '=' && stack[sp-1]->eq[stack[sp-1]->nparts-1] < 0){
			p = stack[sp-1];
			p->eq[p->nparts-1] = p->parts[p->nparts-1].len;
			push(&p->parts[p->nparts-1], text("=", 1, Eq));
			k++;
			continue;
		}
		addtext(acc, s + k, 1);
		k++;
	}
	while(sp > 0){
		p = stack[--sp];
		acc = sp ? &stack[sp-1]->parts[stack[sp-1]->nparts-1] : root;
		writeback(acc, p, p->count, 0);
	}
	return root;
}

/* ---------------------------------------------------------------- expansion */

typedef struct Frame Frame;
struct Frame
{
	char	**name, **value;
	int	n;
};

static char *tbody;

static void expand(Buf*, List*, int, int, Frame*);

static char*
trim(char *s)
{
	char *e;

	while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v')
		s++;
	e = s + strlen(s);
	while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\v'))
		e--;
	*e = 0;
	return s;
}

static char*
str(List *l, int from, int to, Frame *f)
{
	Buf b;

	memset(&b, 0, sizeof b);
	adds(&b, "");
	expand(&b, l, from, to, f);
	return b.s;
}

/* the node unexpanded, as written */
static void
raw(Buf *b, Node *x)
{
	int i, j;

	if(x->type == Text || x->type == Eq){
		adds(b, x->s);
		return;
	}
	adds(b, x->type == Arg ? "{{{" : "{{");
	for(i = 0; i < x->nparts; i++){
		if(i > 0)
			adds(b, "|");
		for(j = 0; j < x->parts[i].len; j++)
			raw(b, x->parts[i].n[j]);
	}
	adds(b, x->type == Arg ? "}}}" : "}}");
}

static int
isname(char *s)
{
	char *t, *p;

	s = trim(strdup(s));
	if(cistrncmp(s, "subst:", 6) == 0)
		return 0;
	if(cistrncmp(s, "Template:", 9) == 0)
		s = trim(s + 9);
	for(p = s; *p; p++)
		if(*p == '_')
			*p = ' ';
	t = strdup(s);
	if(*t >= 'a' && *t <= 'z')
		*t += 'A' - 'a';
	return strcmp(t, Name) == 0;
}

static void
set(Frame *f, char *name, char *value)
{
	int i;

	for(i = 0; i < f->n; i++)
		if(strcmp(f->name[i], name) == 0){
			f->value[i] = value;
			return;
		}
	f->name = realloc(f->name, (f->n + 1) * sizeof f->name[0]);
	f->value = realloc(f->value, (f->n + 1) * sizeof f->value[0]);
	if(f->name == nil || f->value == nil)
		sysfatal("realloc: %r");
	f->name[f->n] = name;
	f->value[f->n] = value;
	f->n++;
}

static void
node(Buf *b, Node *x, Frame *f)
{
	Frame g;
	char *name, num[16];
	int i, pos;

	switch(x->type){
	case Text:
	case Eq:
		adds(b, x->s);
		return;
	case Arg:
		name = trim(str(&x->parts[0], 0, x->parts[0].len, f));
		for(i = 0; f && i < f->n; i++)
			if(strcmp(f->name[i], name) == 0){
				adds(b, f->value[i]);
				return;
			}
		if(x->nparts >= 2){
			expand(b, &x->parts[1], 0, x->parts[1].len, f);
			return;
		}
		adds(b, "{{{");
		expand(b, &x->parts[0], 0, x->parts[0].len, f);
		adds(b, "}}}");
		return;
	case Tpl:
		if(!isname(str(&x->parts[0], 0, x->parts[0].len, f))){
			raw(b, x);
			return;
		}
		memset(&g, 0, sizeof g);
		pos = 1;
		for(i = 1; i < x->nparts; i++)
			if(x->eq[i] >= 0)
				set(&g, trim(str(&x->parts[i], 0, x->eq[i], f)), trim(str(&x->parts[i], x->eq[i] + 1, x->parts[i].len, f)));
			else{
				snprint(num, sizeof num, "%d", pos++);
				set(&g, strdup(num), trimall ? trim(str(&x->parts[i], 0, x->parts[i].len, f)) : str(&x->parts[i], 0, x->parts[i].len, f));
			}
		expand(b, preprocess(tbody), 0, -1, &g);
		return;
	}
}

static void
expand(Buf *b, List *l, int from, int to, Frame *f)
{
	int i;

	if(to < 0)
		to = l->len;
	for(i = from; i < to; i++)
		node(b, l->n[i], f);
}

static char*
slurp(char *file)
{
	Buf b;
	int fd, n;
	char buf[8192];

	if((fd = open(file, OREAD)) < 0)
		sysfatal("open %s: %r", file);
	memset(&b, 0, sizeof b);
	adds(&b, "");
	while((n = read(fd, buf, sizeof buf)) > 0)
		addc(&b, buf, n);
	close(fd);
	return b.s;
}

void
main(int argc, char **argv)
{
	Buf b;

	ARGBEGIN{
	case 'n':
		trimall = 1;
		break;
	default:
		fprint(2, "usage: zexpand [-n] body call\n");
		exits("usage");
	}ARGEND
	if(argc != 2){
		fprint(2, "usage: zexpand [-n] body call\n");
		exits("usage");
	}
	tbody = body(slurp(argv[0]));
	memset(&b, 0, sizeof b);
	adds(&b, "");
	expand(&b, preprocess(page(slurp(argv[1]))), 0, -1, nil);
	write(1, b.s, b.len);
	exits(nil);
}
