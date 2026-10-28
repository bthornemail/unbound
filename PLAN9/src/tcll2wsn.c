/*
 * tcll2wsn - a TCLL1 grammar (Christopher's LL(1) parser generator, as in
 * the public-domain EULER compiler) to strict Wirth syntax notation.
 *
 *	tcll2wsn grammar.grm
 *
 * A defined name becomes an upper-case identifier, digits spelled out
 * (primary1 -> PRIMARYONE). An undefined name is a token and becomes a LITERAL
 * ("new"). An action, NAME!, becomes the identifier NAMEACTION (so procdef and
 * PROCDEF! stay apart), left undefined as WSN leaves letter and character undefined. "start : x ." is START = X . Several
 * productions for one name become one production of alternatives. # comments
 * are dropped.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

enum { Name, Action, Quote, Sym, End };

typedef struct Tok Tok;
struct Tok {
	int	type;
	char	*s;
};

static Tok tok[8192];
static int ntok;
static char *defined[512];
static int ndefined;

static int
isdef(char *s)
{
	int i;

	for(i = 0; i < ndefined; i++)
		if(strcmp(defined[i], s) == 0)
			return 1;
	return 0;
}

static char *digit[] = { "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE" };

static char*
ident(char *s)
{
	Fmt f;

	fmtstrinit(&f);
	for(; *s; s++)
		if(*s >= 'a' && *s <= 'z')
			fmtprint(&f, "%c", *s - 'a' + 'A');
		else if(*s >= 'A' && *s <= 'Z')
			fmtprint(&f, "%c", *s);
		else if(*s >= '0' && *s <= '9')
			fmtprint(&f, "%s", digit[*s - '0']);
	return fmtstrflush(&f);
}

/* n bytes from p (Plan 9's %.*s counts runes, not bytes) */
static char*
bytes(char *p, int n)
{
	char *s;

	s = malloc(n + 1);
	memmove(s, p, n);
	s[n] = 0;
	return s;
}

static void
lex(char *p)
{
	char *q;

	while(*p){
		if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'){
			p++;
			continue;
		}
		if(*p == '#'){
			while(*p && *p != '\n')
				p++;
			continue;
		}
		if(*p == '"'){
			for(q = p + 1; *q && *q != '"'; q++)
				;
			tok[ntok].type = Quote;
			tok[ntok++].s = bytes(p + 1, q - p - 1);
			p = *q ? q + 1 : q;
			continue;
		}
		if(isalpha(*p)){
			for(q = p; isalnum(*q); q++)
				;
			tok[ntok].type = *q == '!' ? Action : Name;
			tok[ntok++].s = bytes(p, q - p);
			p = *q == '!' ? q + 1 : q;
			continue;
		}
		tok[ntok].type = *p == '.' ? End : Sym;
		tok[ntok++].s = smprint("%c", *p == ':' ? '=' : *p);
		p++;
	}
}

void
main(int argc, char *argv[])
{
	Biobuf *b, out;
	char *text, *lhs[512];
	Fmt body[512];
	int nlhs, i, j, k, st;

	if(argc != 2){
		fprint(2, "usage: tcll2wsn grammar.grm\n");
		exits("usage");
	}
	if((b = Bopen(argv[1], OREAD)) == nil)
		sysfatal("open %s: %r", argv[1]);
	text = Brdstr(b, 0, 0);
	if(text == nil)
		text = "";
	Bterm(b);
	lex(text);
	/* the defined names: each one before = (or the : of start) */
	for(i = 0; i + 1 < ntok; i++)
		if(tok[i].type == Name && tok[i+1].type == Sym && strcmp(tok[i+1].s, "=") == 0 && (i == 0 || tok[i-1].type == End) && !isdef(tok[i].s))
			defined[ndefined++] = tok[i].s;
	Binit(&out, 1, OWRITE);
	nlhs = 0;
	for(i = 0; i < ntok; i = j + 1){
		if(tok[i].type != Name || i + 1 >= ntok || strcmp(tok[i+1].s, "=") != 0)
			sysfatal("expected name = at token %d (%s)", i, tok[i].s);
		for(j = i + 2; j < ntok && tok[j].type != End; j++)
			;
		for(k = 0; k < nlhs; k++)
			if(strcmp(lhs[k], tok[i].s) == 0)
				break;
		if(k == nlhs){
			lhs[nlhs] = tok[i].s;
			fmtstrinit(&body[nlhs++]);
		}else
			fmtprint(&body[k], " |");
		st = 0;
		for(k = 0; k < nlhs && strcmp(lhs[k], tok[i].s) != 0; k++)
			;
		fmtprint(&body[k], " (");
		for(st = i + 2; st < j; st++)
			switch(tok[st].type){
			case Name:
				if(isdef(tok[st].s))
					fmtprint(&body[k], " %s", ident(tok[st].s));
				else
					fmtprint(&body[k], " \"%s\"", tok[st].s);
				break;
			case Action:
				fmtprint(&body[k], " %sACTION", ident(tok[st].s));
				break;
			case Quote:
				fmtprint(&body[k], " \"%s\"", tok[st].s);
				break;
			case Sym:
				fmtprint(&body[k], " %s", tok[st].s);
				break;
			}
		fmtprint(&body[k], " )");
	}
	for(k = 0; k < nlhs; k++)
		Bprint(&out, "%s =%s .\n", ident(lhs[k]), fmtstrflush(&body[k]));
	Bterm(&out);
	exits(nil);
}
