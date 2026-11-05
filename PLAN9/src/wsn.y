%{
/*
 * wsn.y - Wirth syntax notation read by an LALR parser: the same grammar zwsn.c reads by recursive descent, given to
 * two LALR generators that must agree, Plan 9's yacc in the guest and GNU Bison on the host (the author, 2026-11-04,
 * of the LALR parser, DeRemer 1969: "yes lets do that ... wirth or css proto cues the easiest public form").
 *
 *	SYNTAX     = { PRODUCTION } .
 *	PRODUCTION = IDENTIFIER "=" EXPRESSION "." .
 *	EXPRESSION = TERM { "|" TERM } .
 *	TERM       = FACTOR { FACTOR } .
 *	FACTOR     = IDENTIFIER | LITERAL | "[" EXPRESSION "]" | "(" EXPRESSION ")" | "{" EXPRESSION "}" .
 *	IDENTIFIER = letter { letter } .
 *	LITERAL    = """" character { character } """" .
 *
 * The lexer is zwsn's: an identifier is ASCII letters only; a literal is quoted, a doubled quote standing for one,
 * never empty, never across a line; anything else is a token no rule takes. For each file it prints, as zwsn does,
 * "name: n productions", or "name:line: rejected" at the line of the first token that no input could continue
 * (both parsers have the correct-prefix property, so both stop there).
 *
 *	yacc wsn.y && 8c -DPLAN9 y.tab.c && 8l -o zlalr y.tab.8	(Plan 9)
 *	bison -y wsn.y && cc -o zlalr y.tab.c				(host)
 *	zlalr file ...
 */
#ifdef PLAN9
#include <u.h>
#include <libc.h>
#define PRINT print
#else
#include <stdio.h>
#include <stdlib.h>
#define PRINT printf
#endif

static char *s, *name;
static int line, tokline, nprod;
int yylex(void);
int yyparse(void);
void yyerror(char*);
%}

%token IDENT LIT BAD

%%

syntax:		/* empty */
	|	syntax production
	;
production:	IDENT '=' expression '.'	{ nprod++; }
	;
expression:	term
	|	expression '|' term
	;
term:		factor
	|	term factor
	;
factor:		IDENT
	|	LIT
	|	'[' expression ']'
	|	'(' expression ')'
	|	'{' expression '}'
	;

%%

static int
letter(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int
yylex(void)
{
	for(; *s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'; s++)
		if(*s == '\n')
			line++;
	tokline = line;
	if(*s == 0)
		return 0;
	if(letter(*s)){
		while(letter(*s))
			s++;
		return IDENT;
	}
	if(*s == '"'){
		s++;
		if(*s == '"' && s[1] != '"')
			return BAD;		/* empty literal */
		for(;;){
			if(*s == 0 || *s == '\n')
				return BAD;	/* unterminated */
			if(*s == '"'){
				if(s[1] != '"'){
					s++;
					return LIT;
				}
				s++;
			}
			s++;
		}
	}
	switch(*s){
	case '=': case '.': case '|': case '[': case ']': case '(': case ')': case '{': case '}':
		return *s++;
	}
	s++;
	return BAD;
}

void
yyerror(char *msg)
{
	(void)msg;
	PRINT("%s:%d: rejected\n", name, tokline);
}

static char*
readall(char *file)
{
	char *buf;
	long n;
#ifdef PLAN9
	int fd;
	Dir *d;
	if((fd = open(file, OREAD)) < 0 || (d = dirfstat(fd)) == nil)
		sysfatal("open %s: %r", file);
	buf = mallocz(d->length + 1, 1);
	n = readn(fd, buf, d->length);
	close(fd);
	free(d);
	if(n < 0)
		sysfatal("read %s: %r", file);
#else
	FILE *f = fopen(file, "rb");
	if(f == NULL){ fprintf(stderr, "open %s\n", file); exit(1); }
	fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
	buf = calloc(n + 1, 1);
	if(fread(buf, 1, n, f) != (size_t)n){ fprintf(stderr, "read %s\n", file); exit(1); }
	fclose(f);
#endif
	return buf;
}

#ifdef PLAN9
void
#else
int
#endif
main(int argc, char *argv[])
{
	int i;
	char *buf;

	for(i = 1; i < argc; i++){
		name = argv[i];
		buf = readall(name);
		s = buf;
		line = 1;
		nprod = 0;
		if(yyparse() == 0)
			PRINT("%s: %d productions\n", name, nprod);
		free(buf);
	}
#ifdef PLAN9
	exits(nil);
#else
	return 0;
#endif
}
